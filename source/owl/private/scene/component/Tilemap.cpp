/**
 * @file Tilemap.cpp
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/Tileset.h"
#include "scene/component/Tilemap.h"

#include <charconv>
#include <sstream>

namespace owl::scene::component {

namespace {
/// Render-empty marker. Mirrors `g_EmptyTileIndex` but in scope of this TU for terseness.
constexpr int32_t k_Empty = g_EmptyTileIndex;

/**
 * @brief
 *  Resize a row-major flat layer buffer in place, copying preserved cells.
 */
void resizeLayerStorage(std::vector<int32_t>& ioTiles, uint32_t iOldWidth, uint32_t iOldHeight, uint32_t iNewWidth,
						uint32_t iNewHeight) {
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

/**
 * @brief
 *  Encode a flat tile buffer as a comma-separated decimal string.
 */
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

/**
 * @brief
 *  Decode a comma-separated tile string into a flat buffer of size `iExpected`.
 *
 * Missing or malformed entries become `g_EmptyTileIndex`. Surplus entries are
 * truncated to keep the layer aligned to the grid.
 */
auto decodeTiles(const std::string& iEncoded, size_t iExpected) -> std::vector<int32_t> {
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

void Tilemap::resize(const uint32_t iWidth, const uint32_t iHeight) {
	const uint32_t newWidth = std::max(1u, iWidth);
	const uint32_t newHeight = std::max(1u, iHeight);
	for (auto& layer: layers) resizeLayerStorage(layer.tiles, width, height, newWidth, newHeight);
	width = newWidth;
	height = newHeight;
}

auto Tilemap::addLayer(const std::string& iName) -> TilemapLayer& {
	auto& layer = layers.emplace_back();
	layer.name = iName;
	layer.visible = true;
	layer.parallax = math::vec2{1.f, 1.f};
	layer.tiles.assign(static_cast<size_t>(width) * height, k_Empty);
	return layer;
}

auto Tilemap::getTile(const uint32_t iLayer, const uint32_t iX, const uint32_t iY) const -> int32_t {
	if (iLayer >= layers.size() || iX >= width || iY >= height)
		return k_Empty;
	const size_t idx = static_cast<size_t>(iY) * width + iX;
	if (idx >= layers[iLayer].tiles.size())
		return k_Empty;
	return layers[iLayer].tiles[idx];
}

void Tilemap::setTile(const uint32_t iLayer, const uint32_t iX, const uint32_t iY, const int32_t iValue) {
	if (iX >= width || iY >= height)
		return;
	auto& tiles = layers.at(iLayer).tiles;
	const size_t idx = static_cast<size_t>(iY) * width + iX;
	if (idx >= tiles.size())
		tiles.resize(static_cast<size_t>(width) * height, k_Empty);
	tiles[idx] = iValue;
}

void Tilemap::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// Tilemap
	if (!tilesetPath.empty())
		iOut.getImpl()->emitter << YAML::Key << "tilesetPath" << YAML::Value << tilesetPath.generic_string();
	iOut.getImpl()->emitter << YAML::Key << "width" << YAML::Value << width;
	iOut.getImpl()->emitter << YAML::Key << "height" << YAML::Value << height;
	iOut.getImpl()->emitter << YAML::Key << "cellSize" << YAML::Value << cellSize;
	iOut.getImpl()->emitter << YAML::Key << "layers" << YAML::Value << YAML::BeginSeq;
	for (const auto& layer: layers) {
		iOut.getImpl()->emitter << YAML::BeginMap;
		iOut.getImpl()->emitter << YAML::Key << "name" << YAML::Value << layer.name;
		if (!layer.visible)
			iOut.getImpl()->emitter << YAML::Key << "visible" << YAML::Value << layer.visible;
		if (layer.parallax.x() != 1.f || layer.parallax.y() != 1.f) {
			iOut.getImpl()->emitter << YAML::Key << "parallax" << YAML::Value << YAML::Flow << YAML::BeginSeq
									<< layer.parallax.x() << layer.parallax.y() << YAML::EndSeq;
		}
		iOut.getImpl()->emitter << YAML::Key << "tiles" << YAML::Value << encodeTiles(layer.tiles);
		iOut.getImpl()->emitter << YAML::EndMap;
	}
	iOut.getImpl()->emitter << YAML::EndSeq;
	iOut.getImpl()->emitter << YAML::EndMap;// Tilemap
}

void Tilemap::deserialize(const core::Serializer& iNode) {
	const auto& root = iNode.getImpl()->node;
	if (root["tilesetPath"])
		tilesetPath = root["tilesetPath"].as<std::string>();
	if (root["width"])
		width = std::max(1u, root["width"].as<uint32_t>());
	if (root["height"])
		height = std::max(1u, root["height"].as<uint32_t>());
	if (root["cellSize"])
		cellSize = root["cellSize"].as<float>();
	layers.clear();
	if (const auto layerNodes = root["layers"]; layerNodes && layerNodes.IsSequence()) {
		const size_t expected = static_cast<size_t>(width) * height;
		for (const auto& layerNode: layerNodes) {
			TilemapLayer layer;
			if (layerNode["name"])
				layer.name = layerNode["name"].as<std::string>();
			if (layerNode["visible"])
				layer.visible = layerNode["visible"].as<bool>();
			if (const auto px = layerNode["parallax"]; px && px.IsSequence() && px.size() >= 2) {
				layer.parallax = math::vec2{px[0].as<float>(), px[1].as<float>()};
			}
			if (layerNode["tiles"])
				layer.tiles = decodeTiles(layerNode["tiles"].as<std::string>(), expected);
			else
				layer.tiles.assign(expected, k_Empty);
			layers.push_back(std::move(layer));
		}
	}
}

}// namespace owl::scene::component
