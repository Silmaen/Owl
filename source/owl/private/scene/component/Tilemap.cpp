/**
 * @file Tilemap.cpp
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/TilemapAsset.h"
#include "scene/component/Tilemap.h"

#include <charconv>

namespace owl::scene::component {

namespace {
constexpr int32_t k_Empty = g_EmptyTileIndex;

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

void emitInline(YAML::Emitter& iEmitter, const scene::TilemapAsset& iAsset) {
	if (!iAsset.tilesetPath.empty())
		iEmitter << YAML::Key << "tilesetPath" << YAML::Value << iAsset.tilesetPath.generic_string();
	iEmitter << YAML::Key << "width" << YAML::Value << iAsset.width;
	iEmitter << YAML::Key << "height" << YAML::Value << iAsset.height;
	iEmitter << YAML::Key << "cellSize" << YAML::Value << iAsset.cellSize;
	iEmitter << YAML::Key << "layers" << YAML::Value << YAML::BeginSeq;
	for (const auto& layer: iAsset.layers) {
		iEmitter << YAML::BeginMap;
		iEmitter << YAML::Key << "name" << YAML::Value << layer.name;
		if (!layer.visible)
			iEmitter << YAML::Key << "visible" << YAML::Value << layer.visible;
		if (layer.parallax.x() != 1.f || layer.parallax.y() != 1.f) {
			iEmitter << YAML::Key << "parallax" << YAML::Value << YAML::Flow << YAML::BeginSeq << layer.parallax.x()
					 << layer.parallax.y() << YAML::EndSeq;
		}
		iEmitter << YAML::Key << "tiles" << YAML::Value << encodeTiles(layer.tiles);
		iEmitter << YAML::EndMap;
	}
	iEmitter << YAML::EndSeq;
}

auto readInline(const YAML::Node& iNode) -> shared<scene::TilemapAsset> {
	auto asset = mkShared<scene::TilemapAsset>();
	if (iNode["tilesetPath"])
		asset->tilesetPath = iNode["tilesetPath"].as<std::string>();
	if (iNode["width"])
		asset->width = std::max(1u, iNode["width"].as<uint32_t>());
	if (iNode["height"])
		asset->height = std::max(1u, iNode["height"].as<uint32_t>());
	if (iNode["cellSize"])
		asset->cellSize = iNode["cellSize"].as<float>();
	if (const auto layerNodes = iNode["layers"]; layerNodes && layerNodes.IsSequence()) {
		const size_t expected = static_cast<size_t>(asset->width) * asset->height;
		for (const auto& layerNode: layerNodes) {
			TilemapLayer layer;
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
			asset->layers.push_back(std::move(layer));
		}
	}
	return asset;
}

}// namespace

void Tilemap::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;
	if (!tilemapPath.empty()) {
		emitter << YAML::Key << "tilemapPath" << YAML::Value << tilemapPath.generic_string();
	} else if (asset) {
		emitter << YAML::Key << "inline" << YAML::Value << YAML::BeginMap;
		emitInline(emitter, *asset);
		emitter << YAML::EndMap;
	}
	emitter << YAML::EndMap;
}

void Tilemap::deserialize(const core::Serializer& iNode) {
	const auto& root = iNode.getImpl()->node;
	tilemapPath.clear();
	asset.reset();
	if (root["tilemapPath"]) {
		tilemapPath = root["tilemapPath"].as<std::string>();
		// `asset` is left null on purpose: scene loading resolves it via `Scene::loadAssetReferences`.
		return;
	}
	if (const auto inlineNode = root["inline"]; inlineNode && inlineNode.IsMap()) {
		asset = readInline(inlineNode);
	} else if (root["width"] || root["layers"]) {
		asset = readInline(root);
	}
}

}// namespace owl::scene::component
