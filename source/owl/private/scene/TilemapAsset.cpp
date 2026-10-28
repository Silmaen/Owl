/**
 * @file TilemapAsset.cpp
 * @author Silmaen
 * @date 08/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "TilemapCodec.h"
#include "core/FormatVersionYaml.h"
#include "core/SerializerImpl.h"
#include "core/external/yaml.h"
#include "data/assets/AssetReference.h"
#include "platform/AtomicFile.h"
#include "scene/TilemapAsset.h"
#include "scene/TilemapTerrain.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <fstream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <utility>

namespace owl::scene {

namespace {

std::atomic<uint64_t> g_nextHeightGeneration{1};

constexpr auto g_tilesKeyV1 = "tiles";
constexpr auto g_terrainTextKey = "terrain";
constexpr auto g_payloadBytesKey = "payloadBytes";

// Format 1 kept one dense `tiles` string per layer: it becomes a `terrain` text plane, read once, never written.
auto migrateDenseTilesToTerrainText(const core::Serializer& ioDocument) -> bool {
	auto layers = ioDocument.getImpl()->document["layers"];
	if (!layers)
		return true;
	if (!layers.IsSequence()) {
		OWL_CORE_WARN("TilemapAsset: Layers of a format 1 tilemap are not a sequence.")
		return false;
	}
	for (auto layer: layers) {
		if (!layer.IsMap() || !layer[g_tilesKeyV1])
			continue;
		layer[g_terrainTextKey] = layer[g_tilesKeyV1].as<std::string>();
		layer.remove(g_tilesKeyV1);
	}
	return true;
}

// Format 2 to 3 only adds the height section of the binary payload: the header keeps its layout.
auto migrateHeaderToHeights(const core::Serializer& /*ioDocument*/) -> bool { return true; }

constexpr std::array<core::MigrationStep, 2> g_tilemapMigrations{&migrateDenseTilesToTerrainText,
																 &migrateHeaderToHeights};
constexpr core::DocumentFormat g_tilemapFormat{.name = "Tilemap", .migrations = g_tilemapMigrations};

void emitLayerHeader(YAML::Emitter& ioEmitter, const component::TilemapLayer& iLayer) {
	ioEmitter << YAML::BeginMap;
	ioEmitter << YAML::Key << "name" << YAML::Value << iLayer.name;
	if (!iLayer.visible)
		ioEmitter << YAML::Key << "visible" << YAML::Value << iLayer.visible;
	if (iLayer.parallax.x() != 1.f || iLayer.parallax.y() != 1.f) {
		ioEmitter << YAML::Key << "parallax" << YAML::Value << YAML::Flow << YAML::BeginSeq << iLayer.parallax.x()
				  << iLayer.parallax.y() << YAML::EndSeq;
	}
	ioEmitter << YAML::EndMap;
}

auto readLayerHeader(const YAML::Node& iNode, const math::vec2ui& iGridSize) -> component::TilemapLayer {
	component::TilemapLayer layer;
	if (iNode["name"])
		layer.name = iNode["name"].as<std::string>();
	if (iNode["visible"])
		layer.visible = iNode["visible"].as<bool>();
	if (const auto px = iNode["parallax"]; px && px.IsSequence() && px.size() >= 2)
		layer.parallax = math::vec2{px[0].as<float>(), px[1].as<float>()};
	if (const auto text = iNode[g_terrainTextKey]; text) {
		const size_t expected = static_cast<size_t>(iGridSize.x()) * iGridSize.y();
		layer.assign(iGridSize, codec::decodeTileText(text.as<std::string>(), expected));
	} else {
		layer.resize(iGridSize);
	}
	return layer;
}

auto readPayload(const YAML::Node& iRoot, const codec::TilemapFileParts& iParts, const uint32_t iVersion,
				 TilemapAsset& ioParsed) -> bool {
	if (!iRoot[g_payloadBytesKey]) {
		OWL_CORE_ERROR("TilemapAsset: Header has no `{}` (truncated file).", g_payloadBytesKey)
		return false;
	}
	const auto expected = iRoot[g_payloadBytesKey].as<uint64_t>();
	if (!iParts.hasPayload) {
		OWL_CORE_ERROR("TilemapAsset: Binary payload of {} bytes is missing (truncated file).", expected)
		return false;
	}
	if (iParts.payload.size() != expected) {
		OWL_CORE_ERROR("TilemapAsset: Binary payload holds {} bytes, the header announces {}.", iParts.payload.size(),
					   expected)
		return false;
	}
	std::vector<uint8_t> heights;
	const auto cornerCount = (static_cast<size_t>(ioParsed.width) + 1) * (static_cast<size_t>(ioParsed.height) + 1);
	if (!codec::decodePayload(iParts.payload, ioParsed.layers, heights, cornerCount, iVersion >= 3))
		return false;
	return ioParsed.setCornerHeights(std::move(heights));
}

auto parseHeader(const YAML::Node& iRoot, TilemapAsset& oParsed) -> bool {
	if (iRoot["tilesetPath"])
		oParsed.tilesetPath = data::assets::toAssetPath(iRoot["tilesetPath"].as<std::string>());
	if (iRoot["width"])
		oParsed.width = std::max(1u, iRoot["width"].as<uint32_t>());
	if (iRoot["height"])
		oParsed.height = std::max(1u, iRoot["height"].as<uint32_t>());
	if (iRoot["cellSize"])
		oParsed.cellSize = iRoot["cellSize"].as<float>();
	if (oParsed.width > TilemapAsset::maxGridSide || oParsed.height > TilemapAsset::maxGridSide) {
		OWL_CORE_ERROR("TilemapAsset: Grid of {}×{} cells exceeds the {} cells side limit.", oParsed.width,
					   oParsed.height, TilemapAsset::maxGridSide)
		return false;
	}
	if (const auto layerNodes = iRoot["layers"]; layerNodes) {
		if (!layerNodes.IsSequence()) {
			OWL_CORE_ERROR("TilemapAsset: `layers` is not a sequence.")
			return false;
		}
		const math::vec2ui gridSize{oParsed.width, oParsed.height};
		for (const auto& layerNode: layerNodes) oParsed.layers.push_back(readLayerHeader(layerNode, gridSize));
	}
	if (const auto node = iRoot["generator"]; node && node.IsMap()) {
		TilemapGenerator generator;
		if (node["graph"])
			generator.graphPath = data::assets::toAssetPath(node["graph"].as<std::string>());
		generator.seed = node["seed"].as<uint64_t>(0);
		generator.layer = node["layer"].as<uint32_t>(0);
		if (const auto origin = node["origin"]; origin && origin.IsSequence() && origin.size() == 2)
			generator.origin = {origin[0].as<uint32_t>(), origin[1].as<uint32_t>()};
		if (const auto size = node["size"]; size && size.IsSequence() && size.size() == 2)
			generator.size = {size[0].as<uint32_t>(), size[1].as<uint32_t>()};
		oParsed.generator = std::move(generator);
	}
	return true;
}

void emitGenerator(YAML::Emitter& ioEmitter, const TilemapGenerator& iGenerator) {
	ioEmitter << YAML::Key << "generator" << YAML::Value << YAML::BeginMap;
	ioEmitter << YAML::Key << "graph" << YAML::Value
			  << data::assets::toAssetReference(iGenerator.graphPath.generic_string());
	ioEmitter << YAML::Key << "seed" << YAML::Value << iGenerator.seed;
	ioEmitter << YAML::Key << "layer" << YAML::Value << iGenerator.layer;
	ioEmitter << YAML::Key << "origin" << YAML::Value << YAML::Flow << YAML::BeginSeq << iGenerator.origin.x()
			  << iGenerator.origin.y() << YAML::EndSeq;
	ioEmitter << YAML::Key << "size" << YAML::Value << YAML::Flow << YAML::BeginSeq << iGenerator.size.x()
			  << iGenerator.size.y() << YAML::EndSeq;
	ioEmitter << YAML::EndMap;
}

}// namespace

auto TilemapAsset::format() -> const core::DocumentFormat& { return g_tilemapFormat; }

void TilemapAsset::resize(const uint32_t iWidth, const uint32_t iHeight) {
	const uint32_t newWidth = std::clamp(iWidth, 1u, maxGridSide);
	const uint32_t newHeight = std::clamp(iHeight, 1u, maxGridSide);
	for (auto& layer: layers) layer.resize({newWidth, newHeight});
	if (!m_cornerHeights.empty()) {
		std::vector<uint8_t> resized((static_cast<size_t>(newWidth) + 1) * (newHeight + 1));
		for (uint32_t y = 0; y <= newHeight; ++y) {
			for (uint32_t x = 0; x <= newWidth; ++x)
				resized[static_cast<size_t>(y) * (newWidth + 1) + x] =
						getCornerHeight(std::min(x, width), std::min(y, height));
		}
		m_cornerHeights = std::move(resized);
	}
	width = newWidth;
	height = newHeight;
	stampAllHeights();
	refreshHeightSummary();
}

auto TilemapAsset::addLayer(const std::string& iName) -> component::TilemapLayer& {
	auto& layer = layers.emplace_back();
	layer.name = iName;
	layer.visible = true;
	layer.parallax = math::vec2{1.f, 1.f};
	layer.resize({width, height});
	return layer;
}

auto TilemapAsset::getTile(const uint32_t iLayer, const uint32_t iX, const uint32_t iY) const -> int32_t {
	if (iLayer >= layers.size() || iX >= width || iY >= height)
		return component::g_EmptyTileIndex;
	return layers[iLayer].getTile(iX, iY);
}

void TilemapAsset::setTile(const uint32_t iLayer, const uint32_t iX, const uint32_t iY, const int32_t iValue) {
	if (iLayer >= layers.size() || iX >= width || iY >= height)
		return;
	auto& layer = layers[iLayer];
	if (const math::vec2ui gridSize{width, height}; layer.getSize() != gridSize)
		layer.resize(gridSize);
	layer.setTile(iX, iY, iValue);
}

auto TilemapAsset::getCornerHeight(const uint32_t iX, const uint32_t iY) const -> uint8_t {
	if (m_cornerHeights.empty() || iX > width || iY > height)
		return 0;
	return m_cornerHeights[getCornerIndex(iX, iY)];
}

auto TilemapAsset::getCellCorners(const uint32_t iX, const uint32_t iY) const -> std::array<uint8_t, 4> {
	if (m_cornerHeights.empty() || iX >= width || iY >= height)
		return {};
	return {getCornerHeight(iX, iY), getCornerHeight(iX + 1, iY), getCornerHeight(iX + 1, iY + 1),
			getCornerHeight(iX, iY + 1)};
}

auto TilemapAsset::getCellSlope(const uint32_t iX, const uint32_t iY) const -> uint8_t {
	if (m_cornerHeights.empty())
		return terrain::flatSlope;
	return terrain::deriveSlope(getCellCorners(iX, iY)).value_or(terrain::flatSlope);
}

auto TilemapAsset::getCellBaseHeight(const uint32_t iX, const uint32_t iY) const -> uint8_t {
	if (m_cornerHeights.empty())
		return 0;
	const auto corners = getCellCorners(iX, iY);
	return *std::ranges::min_element(corners);
}

void TilemapAsset::flattenAround(const uint32_t iX, const uint32_t iY, const uint8_t iHeight,
								 std::vector<CornerHeightChange>& ioChanges) {
	if (m_cornerHeights.empty())
		m_cornerHeights.assign((static_cast<size_t>(width) + 1) * (height + 1), 0);
	const uint32_t columns = width + 1;
	std::vector<uint32_t> pending;
	const auto assign = [&](const uint32_t iIndex, const int iValue) -> void {
		const auto value = static_cast<uint8_t>(iValue);
		ioChanges.push_back({.index = iIndex, .before = m_cornerHeights[iIndex], .after = value});
		m_cornerHeights[iIndex] = value;
		stampCorner(iIndex);
		pending.push_back(iIndex);
	};
	if (const auto start = static_cast<uint32_t>(getCornerIndex(iX, iY)); m_cornerHeights[start] != iHeight)
		assign(start, iHeight);
	while (!pending.empty()) {
		const uint32_t index = pending.back();
		pending.pop_back();
		const int value = m_cornerHeights[index];
		const uint32_t cx = index % columns;
		const uint32_t cy = index / columns;
		const std::array<std::pair<bool, uint32_t>, 4> neighbours{{{cx > 0, index - 1},
																   {cx < width, index + 1},
																   {cy > 0, index - columns},
																   {cy < height, index + columns}}};
		for (const auto& [valid, other]: neighbours) {
			if (!valid)
				continue;
			const int otherValue = m_cornerHeights[other];
			if (otherValue > value + 1)
				assign(other, value + 1);
			else if (otherValue + 1 < value)
				assign(other, value - 1);
		}
	}
}

auto TilemapAsset::getChunkHeightGeneration(const math::vec2ui& iChunk) const -> uint64_t {
	const uint32_t columns = (width + component::TilemapLayer::chunkSize - 1) / component::TilemapLayer::chunkSize;
	const size_t index = static_cast<size_t>(iChunk.y()) * columns + iChunk.x();
	return iChunk.x() < columns && index < m_chunkHeightGenerations.size() ? m_chunkHeightGenerations[index] : 0;
}

void TilemapAsset::stampAllHeights() {
	const uint32_t side = component::TilemapLayer::chunkSize;
	const size_t count = static_cast<size_t>((width + side - 1) / side) * ((height + side - 1) / side);
	m_chunkHeightGenerations.assign(count, g_nextHeightGeneration.fetch_add(1));
}

void TilemapAsset::stampCorner(const size_t iIndex) {
	const uint32_t side = component::TilemapLayer::chunkSize;
	const uint32_t columns = (width + side - 1) / side;
	if (m_chunkHeightGenerations.size() != static_cast<size_t>(columns) * ((height + side - 1) / side))
		stampAllHeights();
	const auto cornerX = static_cast<uint32_t>(iIndex % (width + 1));
	const auto cornerY = static_cast<uint32_t>(iIndex / (width + 1));
	const uint64_t generation = g_nextHeightGeneration.fetch_add(1);
	// The corner belongs to the cells (x - 1, y - 1) to (x, y): at most four chunks.
	for (uint32_t cellY = cornerY > 0 ? cornerY - 1 : 0; cellY <= std::min(cornerY, height - 1); ++cellY) {
		for (uint32_t cellX = cornerX > 0 ? cornerX - 1 : 0; cellX <= std::min(cornerX, width - 1); ++cellX)
			m_chunkHeightGenerations[static_cast<size_t>(cellY / side) * columns + cellX / side] = generation;
	}
}

void TilemapAsset::refreshHeightSummary() {
	m_maxCornerHeight = m_cornerHeights.empty() ? uint8_t{0} : *std::ranges::max_element(m_cornerHeights);
	if (m_maxCornerHeight == 0)
		m_cornerHeights.clear();
}

void TilemapAsset::repairHeights() {
	if (m_cornerHeights.empty())
		return;
	const uint32_t columns = width + 1;
	std::vector<uint32_t> pending(m_cornerHeights.size());
	for (uint32_t i = 0; i < pending.size(); ++i) pending[i] = i;
	const auto raise = [&](const uint32_t iIndex, const int iValue) -> void {
		if (m_cornerHeights[iIndex] + 1 < iValue) {
			m_cornerHeights[iIndex] = static_cast<uint8_t>(iValue - 1);
			pending.push_back(iIndex);
		}
	};
	while (!pending.empty()) {
		const uint32_t index = pending.back();
		pending.pop_back();
		const int value = m_cornerHeights[index];
		const uint32_t cx = index % columns;
		const uint32_t cy = index / columns;
		if (cx > 0)
			raise(index - 1, value);
		if (cx < width)
			raise(index + 1, value);
		if (cy > 0)
			raise(index - columns, value);
		if (cy < height)
			raise(index + columns, value);
	}
}

auto TilemapAsset::setCornerHeight(const uint32_t iX, const uint32_t iY, const uint8_t iHeight)
		-> std::vector<CornerHeightChange> {
	std::vector<CornerHeightChange> changes;
	if (iX > width || iY > height)
		return changes;
	flattenAround(iX, iY, iHeight, changes);
	terrain::mergeCornerChanges(changes);
	refreshHeightSummary();
	return changes;
}

auto TilemapAsset::raiseCorner(const uint32_t iX, const uint32_t iY) -> std::vector<CornerHeightChange> {
	const uint8_t current = getCornerHeight(iX, iY);
	return current >= maxCornerHeight ? std::vector<CornerHeightChange>{}
									  : setCornerHeight(iX, iY, static_cast<uint8_t>(current + 1));
}

auto TilemapAsset::lowerCorner(const uint32_t iX, const uint32_t iY) -> std::vector<CornerHeightChange> {
	const uint8_t current = getCornerHeight(iX, iY);
	return current == 0 ? std::vector<CornerHeightChange>{}
						: setCornerHeight(iX, iY, static_cast<uint8_t>(current - 1));
}

auto TilemapAsset::terraformArea(const math::vec2ui& iFirst, const math::vec2ui& iLast, const int iDelta,
								 const uint8_t iLevel) -> std::vector<CornerHeightChange> {
	std::vector<CornerHeightChange> changes;
	const uint32_t x0 = std::min(iFirst.x(), iLast.x());
	const uint32_t y0 = std::min(iFirst.y(), iLast.y());
	const uint32_t x1 = std::min(std::max(iFirst.x(), iLast.x()), width);
	const uint32_t y1 = std::min(std::max(iFirst.y(), iLast.y()), height);
	if (x0 > x1 || y0 > y1)
		return changes;
	std::vector<uint8_t> original;
	for (uint32_t y = y0; y <= y1; ++y) {
		for (uint32_t x = x0; x <= x1; ++x) original.push_back(getCornerHeight(x, y));
	}
	size_t cursor = 0;
	for (uint32_t y = y0; y <= y1; ++y) {
		for (uint32_t x = x0; x <= x1; ++x) {
			const int before = original[cursor++];
			if (iDelta == 0) {
				flattenAround(x, y, iLevel, changes);
				continue;
			}
			const int target = std::clamp(before + iDelta, 0, int{maxCornerHeight});
			const int current = getCornerHeight(x, y);
			if ((iDelta > 0 && current < target) || (iDelta < 0 && current > target))
				flattenAround(x, y, static_cast<uint8_t>(target), changes);
		}
	}
	terrain::mergeCornerChanges(changes);
	refreshHeightSummary();
	return changes;
}

auto TilemapAsset::raiseArea(const math::vec2ui& iFirst, const math::vec2ui& iLast) -> std::vector<CornerHeightChange> {
	return terraformArea(iFirst, iLast, 1, 0);
}

auto TilemapAsset::lowerArea(const math::vec2ui& iFirst, const math::vec2ui& iLast) -> std::vector<CornerHeightChange> {
	return terraformArea(iFirst, iLast, -1, 0);
}

auto TilemapAsset::levelArea(const math::vec2ui& iFirst, const math::vec2ui& iLast, const uint8_t iHeight)
		-> std::vector<CornerHeightChange> {
	return terraformArea(iFirst, iLast, 0, iHeight);
}

void TilemapAsset::applyCornerChanges(const std::span<const CornerHeightChange> iChanges, const bool iUndo) {
	if (iChanges.empty())
		return;
	if (m_cornerHeights.empty())
		m_cornerHeights.assign((static_cast<size_t>(width) + 1) * (height + 1), 0);
	for (const auto& change: iChanges) {
		if (change.index < m_cornerHeights.size()) {
			m_cornerHeights[change.index] = iUndo ? change.before : change.after;
			stampCorner(change.index);
		}
	}
	refreshHeightSummary();
}

auto TilemapAsset::setCornerHeights(std::vector<uint8_t> iHeights) -> bool {
	if (!iHeights.empty() && iHeights.size() != (static_cast<size_t>(width) + 1) * (height + 1)) {
		OWL_CORE_ERROR("TilemapAsset: {} corner heights do not fit a {}x{} grid.", iHeights.size(), width, height)
		return false;
	}
	const auto given = iHeights;
	m_cornerHeights = std::move(iHeights);
	repairHeights();
	stampAllHeights();
	if (m_cornerHeights != given)
		OWL_CORE_WARN("TilemapAsset: Corner heights differing by more than one step were raised.")
	refreshHeightSummary();
	return true;
}

auto TilemapAsset::getStorageBytes() const -> size_t {
	size_t bytes = m_cornerHeights.size();
	for (const auto& layer: layers) bytes += layer.getStorageBytes();
	return bytes;
}

auto TilemapAsset::hasSameContent(const TilemapAsset& iOther) const -> bool {
	if (tilesetPath != iOther.tilesetPath || width != iOther.width || height != iOther.height ||
		std::bit_cast<uint32_t>(cellSize) != std::bit_cast<uint32_t>(iOther.cellSize) ||
		layers.size() != iOther.layers.size() || generator != iOther.generator)
		return false;
	if (m_cornerHeights != iOther.m_cornerHeights)
		return false;
	return std::ranges::equal(
			layers, iOther.layers,
			[](const component::TilemapLayer& iFirst, const component::TilemapLayer& iSecond) -> bool {
				return iFirst.hasSameContent(iSecond);
			});
}

auto TilemapAsset::serializeToString(const std::string_view iName) const -> std::string {
	const auto payload = codec::encodePayload(layers, m_cornerHeights);
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "Tilemap" << YAML::Value << std::string{iName};
	core::emitFormatVersion(emitter, g_tilemapFormat);
	if (!tilesetPath.empty())
		emitter << YAML::Key << "tilesetPath" << YAML::Value
				<< data::assets::toAssetReference(tilesetPath.generic_string());
	emitter << YAML::Key << "width" << YAML::Value << width;
	emitter << YAML::Key << "height" << YAML::Value << height;
	emitter << YAML::Key << "cellSize" << YAML::Value << cellSize;
	emitter << YAML::Key << "layers" << YAML::Value << YAML::BeginSeq;
	for (const auto& layer: layers) emitLayerHeader(emitter, layer);
	emitter << YAML::EndSeq;
	if (generator)
		emitGenerator(emitter, *generator);
	emitter << YAML::Key << g_payloadBytesKey << YAML::Value << static_cast<uint64_t>(payload.size());
	emitter << YAML::EndMap;
	std::string out{emitter.c_str()};
	out.append(codec::g_PayloadSeparator);
	out.append(payload);
	return out;
}

auto TilemapAsset::deserializeFromString(const std::string_view iText) -> bool {
	const auto parts = codec::splitFile(iText);
	YAML::Node root;
	try {
		root = YAML::Load(std::string{parts.header});
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("TilemapAsset: failed to parse YAML — {}.", e.what())
		return false;
	}
	if (!root || !root.IsMap() || !root["Tilemap"]) {
		OWL_CORE_WARN("TilemapAsset: Document is not a tilemap.")
		return false;
	}
	const auto version = core::upgradeYamlDocument(g_tilemapFormat, root, "<buffer>");
	if (!version) {
		OWL_CORE_ERROR("TilemapAsset: Document cannot be read.")
		return false;
	}
	TilemapAsset parsed;
	try {
		if (!parseHeader(root, parsed))
			return false;
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("TilemapAsset: Invalid header value — {}.", e.what())
		return false;
	}
	if (*version >= 2 && !readPayload(root, parts, *version, parsed))
		return false;
	*this = std::move(parsed);
	return true;
}

auto TilemapAsset::saveToFile(const std::filesystem::path& iPath, const std::string_view iName) const -> bool {
	const auto displayName = iName.empty() ? iPath.stem().string() : std::string{iName};
	if (const auto written = platform::writeFileAtomic(iPath, serializeToString(displayName)); !written) {
		OWL_CORE_ERROR("TilemapAsset: Failed to write '{}': {}.", iPath.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto TilemapAsset::loadFromFile(const std::filesystem::path& iPath) -> bool {
	std::ifstream in(iPath, std::ios::binary);
	if (!in.is_open()) {
		OWL_CORE_ERROR("TilemapAsset: failed to open '{}' for reading.", iPath.string())
		return false;
	}
	std::stringstream buffer;
	buffer << in.rdbuf();
	return deserializeFromString(buffer.str());
}

}// namespace owl::scene
