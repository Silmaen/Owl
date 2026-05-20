/**
 * @file TilemapAsset.cpp
 * @author Silmaen
 * @date 08/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/external/yaml.h"
#include "scene/TilemapAsset.h"

#include <charconv>
#include <fstream>
#include <sstream>

namespace owl::scene {

namespace {
constexpr int32_t k_Empty = scene::component::g_EmptyTileIndex;

void resizeLayerStorage(std::vector<int32_t>& ioTiles, const uint32_t iOldWidth, const uint32_t iOldHeight,
						const uint32_t iNewWidth, const uint32_t iNewHeight) {
	std::vector<int32_t> next(static_cast<size_t>(iNewWidth) * iNewHeight, k_Empty);
	const uint32_t copyW = std::min(iOldWidth, iNewWidth);
	const uint32_t copyH = std::min(iOldHeight, iNewHeight);
	for (uint32_t y = 0; y < copyH; ++y) {
		for (uint32_t x = 0; x < copyW; ++x) {
			const size_t srcIdx = static_cast<size_t>(y) * iOldWidth + x;
			const size_t dstIdx = static_cast<size_t>(y) * iNewWidth + x;
			if (srcIdx < ioTiles.size())
				next[dstIdx] = ioTiles[srcIdx];
		}
	}
	ioTiles = std::move(next);
}

auto encodeTiles(const std::vector<int32_t>& iTiles) -> std::string {
	std::string out;
	out.reserve(iTiles.size() * 3);
	for (size_t i = 0; i < iTiles.size(); ++i) {
		if (i > 0)
			out.push_back(',');
		out += std::to_string(iTiles[i]);
	}
	return out;
}

auto decodeTiles(const std::string& iEncoded, const size_t iExpected) -> std::vector<int32_t> {
	std::vector<int32_t> out(iExpected, k_Empty);
	if (iEncoded.empty())
		return out;
	size_t cursor = 0;
	size_t idx = 0;
	while (cursor <= iEncoded.size() && idx < iExpected) {
		const size_t comma = iEncoded.find(',', cursor);
		const size_t end = (comma == std::string::npos) ? iEncoded.size() : comma;
		int32_t value = k_Empty;
		(void) std::from_chars(iEncoded.data() + cursor, iEncoded.data() + end, value);
		out[idx++] = value;
		if (comma == std::string::npos)
			break;
		cursor = comma + 1;
	}
	return out;
}

}// namespace

void TilemapAsset::resize(const uint32_t iWidth, const uint32_t iHeight) {
	const uint32_t newWidth = std::max(1u, iWidth);
	const uint32_t newHeight = std::max(1u, iHeight);
	for (auto& layer: layers) resizeLayerStorage(layer.tiles, width, height, newWidth, newHeight);
	width = newWidth;
	height = newHeight;
}

auto TilemapAsset::addLayer(const std::string& iName) -> scene::component::TilemapLayer& {
	auto& layer = layers.emplace_back();
	layer.name = iName;
	layer.visible = true;
	layer.parallax = math::vec2{1.f, 1.f};
	layer.tiles.assign(static_cast<size_t>(width) * height, k_Empty);
	return layer;
}

auto TilemapAsset::getTile(const uint32_t iLayer, const uint32_t iX, const uint32_t iY) const -> int32_t {
	if (iLayer >= layers.size() || iX >= width || iY >= height)
		return k_Empty;
	const size_t idx = static_cast<size_t>(iY) * width + iX;
	if (idx >= layers[iLayer].tiles.size())
		return k_Empty;
	return layers[iLayer].tiles[idx];
}

void TilemapAsset::setTile(const uint32_t iLayer, const uint32_t iX, const uint32_t iY, const int32_t iValue) {
	if (iLayer >= layers.size() || iX >= width || iY >= height)
		return;
	auto& tiles = layers[iLayer].tiles;
	const size_t idx = static_cast<size_t>(iY) * width + iX;
	if (idx >= tiles.size())
		tiles.resize(static_cast<size_t>(width) * height, k_Empty);
	tiles[idx] = iValue;
}

auto TilemapAsset::serializeToString(const std::string_view iName) const -> std::string {
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "Tilemap" << YAML::Value << std::string{iName};
	emitter << YAML::Key << "Version" << YAML::Value << 1;
	if (!tilesetPath.empty())
		emitter << YAML::Key << "tilesetPath" << YAML::Value << tilesetPath.generic_string();
	emitter << YAML::Key << "width" << YAML::Value << width;
	emitter << YAML::Key << "height" << YAML::Value << height;
	emitter << YAML::Key << "cellSize" << YAML::Value << cellSize;
	emitter << YAML::Key << "layers" << YAML::Value << YAML::BeginSeq;
	for (const auto& layer: layers) {
		emitter << YAML::BeginMap;
		emitter << YAML::Key << "name" << YAML::Value << layer.name;
		if (!layer.visible)
			emitter << YAML::Key << "visible" << YAML::Value << layer.visible;
		if (layer.parallax.x() != 1.f || layer.parallax.y() != 1.f) {
			emitter << YAML::Key << "parallax" << YAML::Value << YAML::Flow << YAML::BeginSeq << layer.parallax.x()
					<< layer.parallax.y() << YAML::EndSeq;
		}
		emitter << YAML::Key << "tiles" << YAML::Value << encodeTiles(layer.tiles);
		emitter << YAML::EndMap;
	}
	emitter << YAML::EndSeq;
	emitter << YAML::EndMap;
	return std::string{emitter.c_str()};
}

auto TilemapAsset::deserializeFromString(const std::string_view iYaml) -> bool {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("TilemapAsset: failed to parse YAML — {}.", e.what())
		return false;
	}
	if (!root || !root.IsMap() || !root["Tilemap"])
		return false;
	TilemapAsset parsed;
	if (root["tilesetPath"])
		parsed.tilesetPath = root["tilesetPath"].as<std::string>();
	if (root["width"])
		parsed.width = std::max(1u, root["width"].as<uint32_t>());
	if (root["height"])
		parsed.height = std::max(1u, root["height"].as<uint32_t>());
	if (root["cellSize"])
		parsed.cellSize = root["cellSize"].as<float>();
	if (const auto layerNodes = root["layers"]; layerNodes && layerNodes.IsSequence()) {
		const size_t expected = static_cast<size_t>(parsed.width) * parsed.height;
		for (const auto& layerNode: layerNodes) {
			scene::component::TilemapLayer layer;
			if (layerNode["name"])
				layer.name = layerNode["name"].as<std::string>();
			if (layerNode["visible"])
				layer.visible = layerNode["visible"].as<bool>();
			if (const auto px = layerNode["parallax"]; px && px.IsSequence() && px.size() >= 2)
				layer.parallax = math::vec2{px[0].as<float>(), px[1].as<float>()};
			if (layerNode["tiles"])
				layer.tiles = decodeTiles(layerNode["tiles"].as<std::string>(), expected);
			else
				layer.tiles.assign(expected, k_Empty);
			parsed.layers.push_back(std::move(layer));
		}
	}
	*this = std::move(parsed);
	return true;
}

auto TilemapAsset::saveToFile(const std::filesystem::path& iPath, const std::string_view iName) const -> bool {
	std::ofstream out(iPath, std::ios::binary);
	if (!out.is_open()) {
		OWL_CORE_ERROR("TilemapAsset: failed to open '{}' for writing.", iPath.string())
		return false;
	}
	const auto displayName = iName.empty() ? iPath.stem().string() : std::string{iName};
	out << serializeToString(displayName);
	return out.good();
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
