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

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <span>
#include <utility>

namespace owl::scene::component {

namespace {
constexpr int32_t k_Empty = g_EmptyTileIndex;

std::atomic<uint64_t> g_nextChunkGeneration{1};

auto newGeneration() -> uint64_t { return g_nextChunkGeneration.fetch_add(1, std::memory_order_relaxed); }

auto chunkCount(const uint32_t iCells) -> uint32_t {
	return (iCells + TilemapLayer::chunkSize - 1) / TilemapLayer::chunkSize;
}

}// namespace

TilemapLayer::TilemapLayer(TilemapLayer&& ioOther) noexcept
	: name{std::move(ioOther.name)}, visible{ioOther.visible}, parallax{ioOther.parallax},
	  m_size{std::exchange(ioOther.m_size, math::vec2ui{0, 0})}, m_tiles{std::move(ioOther.m_tiles)},
	  m_chunkGenerations{std::move(ioOther.m_chunkGenerations)}, m_generation{std::exchange(ioOther.m_generation, 0)} {
	ioOther.m_tiles.clear();
	ioOther.m_chunkGenerations.clear();
}

auto TilemapLayer::operator=(TilemapLayer&& ioOther) noexcept -> TilemapLayer& {
	if (this == &ioOther)
		return *this;
	name = std::move(ioOther.name);
	visible = ioOther.visible;
	parallax = ioOther.parallax;
	m_size = std::exchange(ioOther.m_size, math::vec2ui{0, 0});
	m_tiles = std::move(ioOther.m_tiles);
	m_chunkGenerations = std::move(ioOther.m_chunkGenerations);
	m_generation = std::exchange(ioOther.m_generation, 0);
	ioOther.m_tiles.clear();
	ioOther.m_chunkGenerations.clear();
	return *this;
}

void TilemapLayer::restampAll() {
	m_generation = newGeneration();
	m_chunkGenerations.assign(static_cast<size_t>(chunkCount(m_size.x())) * chunkCount(m_size.y()), m_generation);
}

void TilemapLayer::resize(const math::vec2ui& iSize) {
	std::vector<int32_t> next(static_cast<size_t>(iSize.x()) * iSize.y(), k_Empty);
	const uint32_t copyW = std::min(m_size.x(), iSize.x());
	const uint32_t copyH = std::min(m_size.y(), iSize.y());
	for (uint32_t y = 0; y < copyH; ++y) {
		const auto src = m_tiles.begin() + static_cast<ptrdiff_t>(static_cast<size_t>(y) * m_size.x());
		std::copy_n(src, copyW, next.begin() + static_cast<ptrdiff_t>(static_cast<size_t>(y) * iSize.x()));
	}
	m_tiles = std::move(next);
	m_size = iSize;
	restampAll();
}

void TilemapLayer::assign(const math::vec2ui& iSize, const std::span<const int32_t> iTiles) {
	m_size = iSize;
	m_tiles.assign(static_cast<size_t>(iSize.x()) * iSize.y(), k_Empty);
	std::copy_n(iTiles.begin(), std::min(iTiles.size(), m_tiles.size()), m_tiles.begin());
	restampAll();
}

void TilemapLayer::fill(const int32_t iValue) {
	std::ranges::fill(m_tiles, iValue);
	restampAll();
}

auto TilemapLayer::getTile(const uint32_t iX, const uint32_t iY) const -> int32_t {
	if (iX >= m_size.x() || iY >= m_size.y())
		return k_Empty;
	return m_tiles[static_cast<size_t>(iY) * m_size.x() + iX];
}

auto TilemapLayer::setTile(const uint32_t iX, const uint32_t iY, const int32_t iValue) -> bool {
	if (iX >= m_size.x() || iY >= m_size.y())
		return false;
	auto& cell = m_tiles[static_cast<size_t>(iY) * m_size.x() + iX];
	if (cell == iValue)
		return false;
	cell = iValue;
	m_generation = newGeneration();
	m_chunkGenerations[static_cast<size_t>(iY / chunkSize) * chunkCount(m_size.x()) + iX / chunkSize] = m_generation;
	return true;
}

auto TilemapLayer::getOccupiedCount() const -> size_t {
	return static_cast<size_t>(std::ranges::count_if(m_tiles, [](const int32_t iTile) -> bool { return iTile >= 0; }));
}

auto TilemapLayer::getChunkGridSize() const -> math::vec2ui { return {chunkCount(m_size.x()), chunkCount(m_size.y())}; }

auto TilemapLayer::getChunkGeneration(const math::vec2ui& iChunk) const -> uint64_t {
	const auto grid = getChunkGridSize();
	if (iChunk.x() >= grid.x() || iChunk.y() >= grid.y())
		return 0;
	return m_chunkGenerations[static_cast<size_t>(iChunk.y()) * grid.x() + iChunk.x()];
}

namespace {

auto encodeTiles(const std::span<const int32_t> iTiles) -> std::string {
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
		iEmitter << YAML::Key << "tiles" << YAML::Value << encodeTiles(layer.getTiles());
		iEmitter << YAML::EndMap;
	}
	iEmitter << YAML::EndSeq;
}

auto readInline(const core::YamlNode& iNode) -> shared<scene::TilemapAsset> {
	auto asset = mkShared<scene::TilemapAsset>();
	if (iNode["tilesetPath"])
		asset->tilesetPath = iNode["tilesetPath"].as<std::string>();
	if (iNode["width"])
		asset->width = std::max(1u, iNode["width"].as<uint32_t>());
	if (iNode["height"])
		asset->height = std::max(1u, iNode["height"].as<uint32_t>());
	if (iNode["cellSize"])
		asset->cellSize = iNode["cellSize"].as<float>();
	if (const auto layerNodes = iNode["layers"]; layerNodes && layerNodes.isSequence()) {
		const size_t expected = static_cast<size_t>(asset->width) * asset->height;
		const math::vec2ui gridSize{asset->width, asset->height};
		for (const auto layerNode: layerNodes) {
			TilemapLayer layer;
			if (layerNode["name"])
				layer.name = layerNode["name"].as<std::string>();
			if (layerNode["visible"])
				layer.visible = layerNode["visible"].as<bool>();
			if (const auto px = layerNode["parallax"]; px && px.isSequence() && px.size() >= 2)
				layer.parallax = math::vec2{px[0].as<float>(), px[1].as<float>()};
			if (layerNode["tiles"])
				layer.assign(gridSize, decodeTiles(layerNode["tiles"].as<std::string>(), expected));
			else
				layer.resize(gridSize);
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
	if (const auto inlineNode = root["inline"]; inlineNode && inlineNode.isMap()) {
		asset = readInline(inlineNode);
	} else if (root["width"] || root["layers"]) {
		asset = readInline(root);
	}
}

}// namespace owl::scene::component
