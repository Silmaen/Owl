/**
 * @file Tileset.cpp
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "platform/AtomicFile.h"
#include "scene/Tileset.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <utility>
#include <vector>

namespace owl::scene {

namespace {
constexpr std::array<core::MigrationStep, 0> g_tilesetMigrations{};
constexpr core::DocumentFormat g_tilesetFormat{.name = "Tileset", .migrations = g_tilesetMigrations};

void emitSlopeRow(YAML::Emitter& ioEmitter, const char* iKey, const std::array<int32_t, terrain::slopeCount>& iRow) {
	ioEmitter << YAML::Key << iKey << YAML::Value << YAML::Flow << YAML::BeginSeq;
	for (const int32_t tile: iRow) ioEmitter << tile;
	ioEmitter << YAML::EndSeq;
}

void emitSlopeSets(YAML::Emitter& ioEmitter, const std::vector<SlopeSet>& iSets) {
	if (iSets.empty())
		return;
	ioEmitter << YAML::Key << "slopes" << YAML::Value << YAML::BeginSeq;
	for (const auto& set: iSets) {
		ioEmitter << YAML::BeginMap << YAML::Key << "id" << YAML::Value << set.id;
		emitSlopeRow(ioEmitter, "tiles", set.tiles);
		if (std::ranges::any_of(set.foundations, [](const int32_t iTile) -> bool { return iTile >= 0; }))
			emitSlopeRow(ioEmitter, "foundations", set.foundations);
		ioEmitter << YAML::EndMap;
	}
	ioEmitter << YAML::EndSeq;
}

void readSlopeRow(const YAML::Node& iNode, std::array<int32_t, terrain::slopeCount>& oRow) {
	if (!iNode || !iNode.IsSequence())
		return;
	for (size_t i = 0; i < std::min<size_t>(iNode.size(), terrain::slopeCount); ++i)
		oRow[i] = std::max(-1, iNode[i].as<int32_t>());
}

void readSlopeSets(const YAML::Node& iNode, std::vector<SlopeSet>& oSets) {
	if (!iNode || !iNode.IsSequence())
		return;
	for (const auto& entry: iNode) {
		if (!entry.IsMap() || !entry["id"])
			continue;
		SlopeSet set;
		set.id = entry["id"].as<int32_t>();
		readSlopeRow(entry["tiles"], set.tiles);
		readSlopeRow(entry["foundations"], set.foundations);
		oSets.push_back(set);
	}
}

}// namespace

auto Tileset::format() -> const core::DocumentFormat& { return g_tilesetFormat; }

namespace {
constexpr auto g_FullAtlasUv = std::array<math::vec2, 4>{math::vec2{0.f, 0.f}, math::vec2{1.f, 0.f},
														 math::vec2{1.f, 1.f}, math::vec2{0.f, 1.f}};

const TileMeta g_DefaultTileMeta{};

}// namespace

void Tileset::resize(const uint32_t iColumns, const uint32_t iRows) {
	columns = std::max(1u, iColumns);
	rows = std::max(1u, iRows);
	tiles.assign(static_cast<size_t>(columns) * rows, TileMeta{});
}

void Tileset::addRow() {
	rows++;
	tiles.resize(static_cast<size_t>(columns) * rows);
}

void Tileset::removeRow() {
	if (rows <= 1)
		return;
	rows--;
	tiles.resize(static_cast<size_t>(columns) * rows);
}

void Tileset::addColumn() {
	const uint32_t newCols = columns + 1;
	std::vector<TileMeta> next(static_cast<size_t>(newCols) * rows);
	for (uint32_t r = 0; r < rows; ++r) {
		for (uint32_t c = 0; c < columns; ++c) {
			const size_t src = static_cast<size_t>(r) * columns + c;
			if (src < tiles.size())
				next[static_cast<size_t>(r) * newCols + c] = std::move(tiles[src]);
		}
	}
	columns = newCols;
	tiles = std::move(next);
}

void Tileset::removeColumn() {
	if (columns <= 1)
		return;
	const uint32_t newCols = columns - 1;
	std::vector<TileMeta> next(static_cast<size_t>(newCols) * rows);
	for (uint32_t r = 0; r < rows; ++r) {
		for (uint32_t c = 0; c < newCols; ++c) {
			const size_t src = static_cast<size_t>(r) * columns + c;
			if (src < tiles.size())
				next[static_cast<size_t>(r) * newCols + c] = std::move(tiles[src]);
		}
	}
	columns = newCols;
	tiles = std::move(next);
}

void Tileset::swapTiles(const uint32_t iLhs, const uint32_t iRhs) {
	if (iLhs == iRhs)
		return;
	if (iLhs >= tiles.size() || iRhs >= tiles.size())
		return;
	std::swap(tiles[iLhs], tiles[iRhs]);
}

auto Tileset::getTileUv(const uint32_t iIndex) const -> std::array<math::vec2, 4> {
	const uint32_t total = tileCount();
	if (total == 0 || iIndex >= total)
		return g_FullAtlasUv;
	const uint32_t col = iIndex % columns;
	const uint32_t row = iIndex / columns;
	const float atlasW = static_cast<float>(columns) * static_cast<float>(std::max(1u, tileWidth));
	const float atlasH = static_cast<float>(rows) * static_cast<float>(std::max(1u, tileHeight));
	const float halfU = 0.5f / atlasW;
	const float halfV = 0.5f / atlasH;
	const float u0 = static_cast<float>(col) / static_cast<float>(columns) + halfU;
	const float u1 = static_cast<float>(col + 1) / static_cast<float>(columns) - halfU;
	const float v0 = 1.f - static_cast<float>(row + 1) / static_cast<float>(rows) + halfV;
	const float v1 = 1.f - static_cast<float>(row) / static_cast<float>(rows) - halfV;
	return {math::vec2{u0, v0}, math::vec2{u1, v0}, math::vec2{u1, v1}, math::vec2{u0, v1}};
}

auto Tileset::getTileMeta(const uint32_t iIndex) const -> const TileMeta& {
	if (iIndex >= tiles.size())
		return g_DefaultTileMeta;
	return tiles[iIndex];
}

auto Tileset::isCollidable(const uint32_t iIndex) const -> bool { return getTileMeta(iIndex).collidable; }

auto Tileset::findSlopeSet(const int32_t iId) const -> const SlopeSet* {
	const auto found = std::ranges::find(slopeSets, iId, &SlopeSet::id);
	return found == slopeSets.end() ? nullptr : &*found;
}

auto Tileset::resolveTile(const int32_t iId, const uint8_t iSlope, const bool iFoundation) const -> int32_t {
	const auto* set = findSlopeSet(iId);
	if (set == nullptr || iSlope >= terrain::slopeCount)
		return iId;
	if (iFoundation && set->foundations[iSlope] >= 0)
		return set->foundations[iSlope];
	if (set->tiles[iSlope] >= 0)
		return set->tiles[iSlope];
	if (set->tiles[terrain::flatSlope] >= 0)
		return set->tiles[terrain::flatSlope];
	return iId;
}

auto Tileset::serializeToString(const std::string_view iName) const -> std::string {
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "Tileset" << YAML::Value << std::string{iName};
	core::emitFormatVersion(emitter, g_tilesetFormat);
	if (texture)
		emitter << YAML::Key << "texture" << YAML::Value << texture->getSerializeString();
	if (normalTexture)
		emitter << YAML::Key << "normalTexture" << YAML::Value << normalTexture->getSerializeString();
	if (roughnessTexture)
		emitter << YAML::Key << "roughnessTexture" << YAML::Value << roughnessTexture->getSerializeString();
	emitter << YAML::Key << "tileWidth" << YAML::Value << tileWidth;
	emitter << YAML::Key << "tileHeight" << YAML::Value << tileHeight;
	emitter << YAML::Key << "columns" << YAML::Value << columns;
	emitter << YAML::Key << "rows" << YAML::Value << rows;
	if (filterMode == renderer::gpu::FilterMode::Nearest)
		emitter << YAML::Key << "filterMode" << YAML::Value << "Nearest";
	if (kind == Kind::Isometric)
		emitter << YAML::Key << "kind" << YAML::Value << "Isometric";
	emitSlopeSets(emitter, slopeSets);

	// Sparse: emit only the entries that differ from the default.
	const uint32_t total = tileCount();
	const auto count = static_cast<std::vector<TileMeta>::difference_type>(std::min<size_t>(tiles.size(), total));
	const auto isSpecial = [](const TileMeta& m) -> bool {
		return m.collidable || !m.name.empty() || m.transparent || m.wallHeight != 1.f || m.connections.has_value();
	};
	if (std::any_of(tiles.begin(), tiles.begin() + count, isSpecial)) {
		emitter << YAML::Key << "tiles" << YAML::Value << YAML::BeginSeq;
		for (uint32_t i = 0; i < total && i < tiles.size(); ++i) {
			const auto& meta = tiles[i];
			if (!isSpecial(meta))
				continue;
			emitter << YAML::BeginMap;
			emitter << YAML::Key << "index" << YAML::Value << i;
			if (meta.collidable)
				emitter << YAML::Key << "collidable" << YAML::Value << true;
			if (!meta.name.empty())
				emitter << YAML::Key << "name" << YAML::Value << meta.name;
			if (meta.wallHeight != 1.f)
				emitter << YAML::Key << "wallHeight" << YAML::Value << meta.wallHeight;
			if (meta.transparent)
				emitter << YAML::Key << "transparent" << YAML::Value << true;
			if (meta.connections.has_value()) {
				emitter << YAML::Key << "connections" << YAML::Value << YAML::Flow << YAML::BeginSeq;
				for (const auto direction: nav::directions) {
					if ((*meta.connections & nav::directionBit(direction)) != 0)
						emitter << nav::directionNames[static_cast<size_t>(direction)];
				}
				emitter << YAML::EndSeq;
			}
			emitter << YAML::EndMap;
		}
		emitter << YAML::EndSeq;
	}

	emitter << YAML::EndMap;
	return std::string{emitter.c_str()};
}

auto Tileset::deserializeFromString(const std::string_view iYaml) -> bool {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("Tileset: failed to parse YAML — {}.", e.what())
		return false;
	}
	if (!root || !root.IsMap() || !root["Tileset"]) {
		OWL_CORE_WARN("Tileset: Document is not a tileset.")
		return false;
	}
	if (!core::upgradeYamlDocument(g_tilesetFormat, root, "<buffer>")) {
		OWL_CORE_ERROR("Tileset: Document cannot be read.")
		return false;
	}
	Tileset parsed;
	if (const auto fm = root["filterMode"]; fm && fm.IsScalar()) {
		if (const auto v = fm.as<std::string>(); v == "Nearest")
			parsed.filterMode = renderer::gpu::FilterMode::Nearest;
		else
			parsed.filterMode = renderer::gpu::FilterMode::Linear;
	}
	if (const auto kindNode = root["kind"]; kindNode && kindNode.IsScalar())
		parsed.kind = kindNode.as<std::string>() == "Isometric" ? Kind::Isometric : Kind::Orthogonal;
	if (root["texture"]) {
		parsed.texture =
				renderer::gpu::Texture2D::createFromSerializedForDeserialize(root["texture"].as<std::string>());
		if (parsed.texture)
			parsed.texture->setFilterMode(parsed.filterMode);
	}
	for (auto [key, map]:
		 {std::pair{"normalTexture", &parsed.normalTexture}, std::pair{"roughnessTexture", &parsed.roughnessTexture}}) {
		if (!root[key])
			continue;
		*map = renderer::gpu::Texture2D::createFromSerializedForDeserialize(root[key].as<std::string>());
		if (*map)
			(*map)->setFilterMode(parsed.filterMode);
	}
	if (root["tileWidth"])
		parsed.tileWidth = std::max(1u, root["tileWidth"].as<uint32_t>());
	if (root["tileHeight"])
		parsed.tileHeight = std::max(1u, root["tileHeight"].as<uint32_t>());
	if (root["columns"])
		parsed.columns = std::max(1u, root["columns"].as<uint32_t>());
	if (root["rows"])
		parsed.rows = std::max(1u, root["rows"].as<uint32_t>());
	parsed.tiles.assign(static_cast<size_t>(parsed.columns) * parsed.rows, TileMeta{});
	if (const auto tilesNode = root["tiles"]; tilesNode && tilesNode.IsSequence()) {
		for (const auto& entry: tilesNode) {
			if (!entry.IsMap() || !entry["index"])
				continue;
			const auto idx = entry["index"].as<uint32_t>();
			if (idx >= parsed.tiles.size())
				continue;
			TileMeta meta;
			if (entry["collidable"])
				meta.collidable = entry["collidable"].as<bool>();
			if (entry["name"])
				meta.name = entry["name"].as<std::string>();
			if (entry["wallHeight"])
				meta.wallHeight = std::clamp(entry["wallHeight"].as<float>(), 0.f, 8.f);
			if (entry["transparent"])
				meta.transparent = entry["transparent"].as<bool>();
			if (const auto connections = entry["connections"]; connections && connections.IsSequence()) {
				uint8_t mask = 0;
				for (const auto& side: connections) {
					if (const auto direction = nav::parseDirection(side.as<std::string>()); direction.has_value())
						mask = static_cast<uint8_t>(mask | nav::directionBit(*direction));
				}
				meta.connections = mask;
			}
			parsed.tiles[idx] = std::move(meta);
		}
	}
	readSlopeSets(root["slopes"], parsed.slopeSets);
	*this = std::move(parsed);
	return true;
}

auto Tileset::saveToFile(const std::filesystem::path& iPath, const std::string_view iName) const -> bool {
	const auto displayName = iName.empty() ? iPath.stem().string() : std::string{iName};
	if (const auto written = platform::writeFileAtomic(iPath, serializeToString(displayName)); !written) {
		OWL_CORE_ERROR("Tileset: Failed to write '{}': {}.", iPath.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto Tileset::loadFromFile(const std::filesystem::path& iPath) -> bool {
	std::ifstream in(iPath, std::ios::binary);
	if (!in.is_open()) {
		OWL_CORE_ERROR("Tileset: failed to open '{}' for reading.", iPath.string())
		return false;
	}
	std::stringstream buffer;
	buffer << in.rdbuf();
	return deserializeFromString(buffer.str());
}

}// namespace owl::scene
