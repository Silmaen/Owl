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
#include "platform/AtomicFile.h"
#include "scene/TilemapAsset.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

namespace owl::scene {

namespace {

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

constexpr std::array<core::MigrationStep, 1> g_tilemapMigrations{&migrateDenseTilesToTerrainText};
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

auto readPayload(const YAML::Node& iRoot, const codec::TilemapFileParts& iParts, TilemapAsset& ioParsed) -> bool {
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
	return codec::decodePayload(iParts.payload, ioParsed.layers);
}

auto parseHeader(const YAML::Node& iRoot, TilemapAsset& oParsed) -> bool {
	if (iRoot["tilesetPath"])
		oParsed.tilesetPath = iRoot["tilesetPath"].as<std::string>();
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
	return true;
}

}// namespace

auto TilemapAsset::format() -> const core::DocumentFormat& { return g_tilemapFormat; }

void TilemapAsset::resize(const uint32_t iWidth, const uint32_t iHeight) {
	const uint32_t newWidth = std::clamp(iWidth, 1u, maxGridSide);
	const uint32_t newHeight = std::clamp(iHeight, 1u, maxGridSide);
	for (auto& layer: layers) layer.resize({newWidth, newHeight});
	width = newWidth;
	height = newHeight;
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

auto TilemapAsset::getStorageBytes() const -> size_t {
	size_t bytes = 0;
	for (const auto& layer: layers) bytes += layer.getStorageBytes();
	return bytes;
}

auto TilemapAsset::hasSameContent(const TilemapAsset& iOther) const -> bool {
	if (tilesetPath != iOther.tilesetPath || width != iOther.width || height != iOther.height ||
		std::bit_cast<uint32_t>(cellSize) != std::bit_cast<uint32_t>(iOther.cellSize) ||
		layers.size() != iOther.layers.size())
		return false;
	return std::ranges::equal(
			layers, iOther.layers,
			[](const component::TilemapLayer& iFirst, const component::TilemapLayer& iSecond) -> bool {
				return iFirst.hasSameContent(iSecond);
			});
}

auto TilemapAsset::serializeToString(const std::string_view iName) const -> std::string {
	const auto payload = codec::encodePayload(layers);
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "Tilemap" << YAML::Value << std::string{iName};
	core::emitFormatVersion(emitter, g_tilemapFormat);
	if (!tilesetPath.empty())
		emitter << YAML::Key << "tilesetPath" << YAML::Value << tilesetPath.generic_string();
	emitter << YAML::Key << "width" << YAML::Value << width;
	emitter << YAML::Key << "height" << YAML::Value << height;
	emitter << YAML::Key << "cellSize" << YAML::Value << cellSize;
	emitter << YAML::Key << "layers" << YAML::Value << YAML::BeginSeq;
	for (const auto& layer: layers) emitLayerHeader(emitter, layer);
	emitter << YAML::EndSeq;
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
	if (*version >= 2 && !readPayload(root, parts, parsed))
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
