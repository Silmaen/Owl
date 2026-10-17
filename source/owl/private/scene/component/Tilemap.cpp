/**
 * @file Tilemap.cpp
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "../TilemapCodec.h"
#include "core/SerializerImpl.h"
#include "scene/TilemapAsset.h"
#include "scene/component/Tilemap.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <utility>

namespace owl::scene::component {

namespace {
std::atomic<uint64_t> g_nextChunkGeneration{1};

auto newGeneration() -> uint64_t { return g_nextChunkGeneration.fetch_add(1, std::memory_order_relaxed); }

auto chunkCount(const uint32_t iCells) -> uint32_t {
	return (iCells + TilemapLayer::chunkSize - 1) / TilemapLayer::chunkSize;
}

auto toId(const int32_t iTile) -> uint16_t {
	if (iTile < 0 || iTile > g_MaxTileIndex)
		return TilemapChunk::emptyId;
	return static_cast<uint16_t>(iTile);
}

auto fromId(const uint16_t iId) -> int32_t {
	return iId == TilemapChunk::emptyId ? g_EmptyTileIndex : static_cast<int32_t>(iId);
}

auto isValidTile(const int32_t iTile) -> bool {
	return iTile == g_EmptyTileIndex || (iTile >= 0 && iTile <= g_MaxTileIndex);
}

auto localIndex(const uint32_t iX, const uint32_t iY) -> uint32_t {
	return (iY % TilemapLayer::chunkSize) * TilemapLayer::chunkSize + iX % TilemapLayer::chunkSize;
}

auto emptyChunk() -> const TilemapChunk& {
	static const TilemapChunk s_empty;
	return s_empty;
}

}// namespace

TilemapChunk::TilemapChunk() {
	terrain.fill(emptyId);
	overlay.fill(emptyId);
}

auto TilemapChunk::isEmpty() const -> bool {
	return std::ranges::all_of(terrain, [](const uint16_t iId) -> bool { return iId == emptyId; }) &&
		   std::ranges::all_of(overlay, [](const uint16_t iId) -> bool { return iId == emptyId; }) &&
		   std::ranges::all_of(flags, [](const uint8_t iFlags) -> bool { return iFlags == 0; });
}

auto TilemapChunk::getCell(const uint32_t iIndex) const -> TileCell {
	return {.terrain = fromId(terrain[iIndex]), .overlay = fromId(overlay[iIndex]), .flags = flags[iIndex]};
}

void TilemapChunk::setCell(const uint32_t iIndex, const TileCell& iCell) {
	terrain[iIndex] = toId(iCell.terrain);
	overlay[iIndex] = toId(iCell.overlay);
	flags[iIndex] = iCell.flags;
}

TilemapLayer::TilemapLayer(TilemapLayer&& ioOther) noexcept
	: name{std::move(ioOther.name)}, visible{ioOther.visible}, parallax{ioOther.parallax},
	  m_size{std::exchange(ioOther.m_size, math::vec2ui{0, 0})}, m_chunks{std::move(ioOther.m_chunks)},
	  m_chunkGenerations{std::move(ioOther.m_chunkGenerations)}, m_generation{std::exchange(ioOther.m_generation, 0)} {
	ioOther.m_chunks.clear();
	ioOther.m_chunkGenerations.clear();
}

auto TilemapLayer::operator=(TilemapLayer&& ioOther) noexcept -> TilemapLayer& {
	if (this == &ioOther)
		return *this;
	name = std::move(ioOther.name);
	visible = ioOther.visible;
	parallax = ioOther.parallax;
	m_size = std::exchange(ioOther.m_size, math::vec2ui{0, 0});
	m_chunks = std::move(ioOther.m_chunks);
	m_chunkGenerations = std::move(ioOther.m_chunkGenerations);
	m_generation = std::exchange(ioOther.m_generation, 0);
	ioOther.m_chunks.clear();
	ioOther.m_chunkGenerations.clear();
	return *this;
}

void TilemapLayer::restampAll() {
	m_generation = newGeneration();
	m_chunkGenerations.assign(static_cast<size_t>(chunkCount(m_size.x())) * chunkCount(m_size.y()), m_generation);
}

void TilemapLayer::stamp(const size_t iIndex) {
	m_generation = newGeneration();
	m_chunkGenerations[iIndex] = m_generation;
}

auto TilemapLayer::chunkIndex(const math::vec2ui& iChunk) const -> size_t {
	return static_cast<size_t>(iChunk.y()) * chunkCount(m_size.x()) + iChunk.x();
}

auto TilemapLayer::writableChunk(const size_t iIndex) -> TilemapChunk& {
	auto& slot = m_chunks[iIndex];
	if (!slot)
		slot = mkShared<TilemapChunk>();
	else if (slot.use_count() > 1)
		slot = mkShared<TilemapChunk>(*slot);
	return *slot;
}

auto TilemapLayer::hasCellsOutside(const math::vec2ui& iChunk, const TilemapChunk& iContent) const -> bool {
	const uint32_t usedW = std::min(chunkSize, m_size.x() - iChunk.x() * chunkSize);
	const uint32_t usedH = std::min(chunkSize, m_size.y() - iChunk.y() * chunkSize);
	if (usedW == chunkSize && usedH == chunkSize)
		return false;
	for (uint32_t y = 0; y < chunkSize; ++y) {
		for (uint32_t x = (y < usedH ? usedW : 0); x < chunkSize; ++x) {
			if (iContent.getCell(y * chunkSize + x) != TileCell{})
				return true;
		}
	}
	return false;
}

void TilemapLayer::clearOutside(const math::vec2ui& iChunk, TilemapChunk& ioContent) const {
	const uint32_t usedW = std::min(chunkSize, m_size.x() - iChunk.x() * chunkSize);
	const uint32_t usedH = std::min(chunkSize, m_size.y() - iChunk.y() * chunkSize);
	for (uint32_t y = 0; y < chunkSize; ++y) {
		for (uint32_t x = (y < usedH ? usedW : 0); x < chunkSize; ++x) ioContent.setCell(y * chunkSize + x, TileCell{});
	}
}

void TilemapLayer::resize(const math::vec2ui& iSize) {
	const auto oldGrid = getChunkGridSize();
	const math::vec2ui newGrid{chunkCount(iSize.x()), chunkCount(iSize.y())};
	std::vector<shared<TilemapChunk>> next(static_cast<size_t>(newGrid.x()) * newGrid.y());
	const uint32_t keepW = std::min(oldGrid.x(), newGrid.x());
	const uint32_t keepH = std::min(oldGrid.y(), newGrid.y());
	for (uint32_t cy = 0; cy < keepH; ++cy) {
		for (uint32_t cx = 0; cx < keepW; ++cx)
			next[static_cast<size_t>(cy) * newGrid.x() + cx] = std::move(m_chunks[chunkIndex({cx, cy})]);
	}
	m_chunks = std::move(next);
	m_size = iSize;
	for (uint32_t cy = 0; cy < keepH; ++cy) {
		for (uint32_t cx = 0; cx < keepW; ++cx) {
			const size_t index = chunkIndex({cx, cy});
			if (m_chunks[index] && hasCellsOutside({cx, cy}, *m_chunks[index]))
				clearOutside({cx, cy}, writableChunk(index));
		}
	}
	restampAll();
}

void TilemapLayer::assign(const math::vec2ui& iSize, const std::span<const int32_t> iTiles) {
	m_size = iSize;
	m_chunks.assign(static_cast<size_t>(chunkCount(iSize.x())) * chunkCount(iSize.y()), nullptr);
	const size_t count = std::min(iTiles.size(), static_cast<size_t>(iSize.x()) * iSize.y());
	for (size_t i = 0; i < count; ++i) {
		if (const uint16_t id = toId(iTiles[i]); id != TilemapChunk::emptyId) {
			const auto x = static_cast<uint32_t>(i % iSize.x());
			const auto y = static_cast<uint32_t>(i / iSize.x());
			writableChunk(chunkIndex({x / chunkSize, y / chunkSize})).terrain[localIndex(x, y)] = id;
		}
	}
	restampAll();
}

void TilemapLayer::fill(const int32_t iValue) {
	const auto grid = getChunkGridSize();
	m_chunks.assign(m_chunks.size(), nullptr);
	if (const uint16_t id = toId(iValue); id != TilemapChunk::emptyId) {
		for (uint32_t cy = 0; cy < grid.y(); ++cy) {
			for (uint32_t cx = 0; cx < grid.x(); ++cx) {
				auto& chunk = writableChunk(chunkIndex({cx, cy}));
				chunk.terrain.fill(id);
				clearOutside({cx, cy}, chunk);
			}
		}
	}
	restampAll();
}

auto TilemapLayer::getTile(const uint32_t iX, const uint32_t iY) const -> int32_t {
	if (iX >= m_size.x() || iY >= m_size.y())
		return g_EmptyTileIndex;
	const auto& chunk = m_chunks[chunkIndex({iX / chunkSize, iY / chunkSize})];
	return chunk ? fromId(chunk->terrain[localIndex(iX, iY)]) : g_EmptyTileIndex;
}

auto TilemapLayer::setTile(const uint32_t iX, const uint32_t iY, const int32_t iValue) -> bool {
	if (iX >= m_size.x() || iY >= m_size.y() || !isValidTile(iValue) || getTile(iX, iY) == iValue)
		return false;
	const size_t index = chunkIndex({iX / chunkSize, iY / chunkSize});
	writableChunk(index).terrain[localIndex(iX, iY)] = toId(iValue);
	stamp(index);
	return true;
}

auto TilemapLayer::getCell(const uint32_t iX, const uint32_t iY) const -> TileCell {
	if (iX >= m_size.x() || iY >= m_size.y())
		return {};
	const auto& chunk = m_chunks[chunkIndex({iX / chunkSize, iY / chunkSize})];
	return chunk ? chunk->getCell(localIndex(iX, iY)) : TileCell{};
}

auto TilemapLayer::setCell(const uint32_t iX, const uint32_t iY, const TileCell& iCell) -> bool {
	if (iX >= m_size.x() || iY >= m_size.y() || !isValidTile(iCell.terrain) || !isValidTile(iCell.overlay) ||
		getCell(iX, iY) == iCell)
		return false;
	const size_t index = chunkIndex({iX / chunkSize, iY / chunkSize});
	writableChunk(index).setCell(localIndex(iX, iY), iCell);
	stamp(index);
	return true;
}

auto TilemapLayer::getOccupiedCount() const -> size_t {
	size_t count = 0;
	for (const auto& chunk: m_chunks) {
		if (chunk)
			count += static_cast<size_t>(std::ranges::count_if(
					chunk->terrain, [](const uint16_t iId) -> bool { return iId != TilemapChunk::emptyId; }));
	}
	return count;
}

auto TilemapLayer::getChunkGridSize() const -> math::vec2ui { return {chunkCount(m_size.x()), chunkCount(m_size.y())}; }

auto TilemapLayer::getChunk(const math::vec2ui& iChunk) const -> const TilemapChunk* {
	const auto grid = getChunkGridSize();
	if (iChunk.x() >= grid.x() || iChunk.y() >= grid.y())
		return nullptr;
	return m_chunks[chunkIndex(iChunk)].get();
}

auto TilemapLayer::getChunkRef(const math::vec2ui& iChunk) const -> TilemapChunkRef {
	TilemapChunkRef ref;
	const auto grid = getChunkGridSize();
	if (iChunk.x() < grid.x() && iChunk.y() < grid.y())
		ref.m_chunk = m_chunks[chunkIndex(iChunk)];
	return ref;
}

auto TilemapLayer::setChunkRef(const math::vec2ui& iChunk, const TilemapChunkRef& iRef) -> bool {
	const auto grid = getChunkGridSize();
	if (iChunk.x() >= grid.x() || iChunk.y() >= grid.y())
		return false;
	const size_t index = chunkIndex(iChunk);
	m_chunks[index] = iRef.m_chunk;
	if (m_chunks[index] && hasCellsOutside(iChunk, *m_chunks[index]))
		clearOutside(iChunk, writableChunk(index));
	stamp(index);
	return true;
}

auto TilemapLayer::setChunk(const math::vec2ui& iChunk, const TilemapChunk& iContent) -> bool {
	const auto grid = getChunkGridSize();
	if (iChunk.x() >= grid.x() || iChunk.y() >= grid.y())
		return false;
	const size_t index = chunkIndex(iChunk);
	auto content = mkShared<TilemapChunk>(iContent);
	clearOutside(iChunk, *content);
	m_chunks[index] = content->isEmpty() ? nullptr : std::move(content);
	stamp(index);
	return true;
}

auto TilemapLayer::getAllocatedChunkCount() const -> size_t {
	return static_cast<size_t>(std::ranges::count_if(
			m_chunks, [](const shared<TilemapChunk>& iChunk) -> bool { return iChunk != nullptr; }));
}

auto TilemapLayer::getStorageBytes() const -> size_t {
	return getAllocatedChunkCount() * sizeof(TilemapChunk) + m_chunks.size() * sizeof(shared<TilemapChunk>) +
		   m_chunkGenerations.size() * sizeof(uint64_t);
}

auto TilemapLayer::getChunkGeneration(const math::vec2ui& iChunk) const -> uint64_t {
	const auto grid = getChunkGridSize();
	if (iChunk.x() >= grid.x() || iChunk.y() >= grid.y())
		return 0;
	return m_chunkGenerations[chunkIndex(iChunk)];
}

auto TilemapLayer::hasSameContent(const TilemapLayer& iOther) const -> bool {
	if (name != iOther.name || visible != iOther.visible || parallax != iOther.parallax || m_size != iOther.m_size)
		return false;
	const auto& empty = emptyChunk();
	for (size_t i = 0; i < m_chunks.size(); ++i) {
		const auto& mine = m_chunks[i];
		const auto& theirs = iOther.m_chunks[i];
		if (mine == theirs)
			continue;
		if ((mine ? *mine : empty) != (theirs ? *theirs : empty))
			return false;
	}
	return true;
}

namespace {

constexpr std::array<const char*, 3> g_modeNames{"rules", "connections", "cells"};

void emitDirections(YAML::Emitter& iEmitter, const uint8_t iMask) {
	iEmitter << YAML::Flow << YAML::BeginSeq;
	for (const auto direction: nav::directions) {
		if ((iMask & nav::directionBit(direction)) != 0)
			iEmitter << nav::directionNames[static_cast<size_t>(direction)];
	}
	iEmitter << YAML::EndSeq;
}

auto readDirections(const core::YamlNode& iNode, const uint8_t iDefault) -> uint8_t {
	if (!iNode || !iNode.isSequence())
		return iDefault;
	uint8_t mask = 0;
	for (const auto entry: iNode) {
		const auto name = entry.as<std::string>();
		for (const auto direction: nav::directions) {
			if (name == nav::directionNames[static_cast<size_t>(direction)])
				mask = static_cast<uint8_t>(mask | nav::directionBit(direction));
		}
	}
	return mask;
}

void emitNavigation(YAML::Emitter& iEmitter, const nav::NavProfile& iProfile) {
	iEmitter << YAML::Key << "navigation" << YAML::Value << YAML::BeginMap;
	iEmitter << YAML::Key << "mode" << YAML::Value << g_modeNames[static_cast<size_t>(iProfile.mode)];
	iEmitter << YAML::Key << "layer" << YAML::Value << iProfile.layer;
	iEmitter << YAML::Key << "source" << YAML::Value
			 << (iProfile.source == nav::NavSource::Overlay ? "overlay" : "terrain");
	iEmitter << YAML::Key << "maxRise" << YAML::Value << static_cast<uint32_t>(iProfile.maxRise);
	iEmitter << YAML::Key << "climbCost" << YAML::Value << iProfile.climbCost;
	iEmitter << YAML::Key << "allowSteep" << YAML::Value << iProfile.allowSteep;
	if (!iProfile.rules.empty()) {
		iEmitter << YAML::Key << "rules" << YAML::Value << YAML::BeginSeq;
		for (const auto& rule: iProfile.rules) {
			iEmitter << YAML::BeginMap << YAML::Flow;
			iEmitter << YAML::Key << "tile" << YAML::Value << rule.tile;
			iEmitter << YAML::Key << "cost" << YAML::Value << rule.cost;
			iEmitter << YAML::Key << "exits" << YAML::Value;
			emitDirections(iEmitter, rule.exits);
			iEmitter << YAML::Key << "enters" << YAML::Value;
			emitDirections(iEmitter, rule.enters);
			iEmitter << YAML::EndMap;
		}
		iEmitter << YAML::EndSeq;
	}
	if (!iProfile.overrides.empty()) {
		iEmitter << YAML::Key << "overrides" << YAML::Value << YAML::BeginSeq;
		for (const auto& entry: iProfile.overrides) {
			iEmitter << YAML::BeginMap << YAML::Flow;
			iEmitter << YAML::Key << "cell" << YAML::Value << YAML::Flow << YAML::BeginSeq << entry.x << entry.y
					 << YAML::EndSeq;
			if (entry.noExit != 0) {
				iEmitter << YAML::Key << "noExit" << YAML::Value;
				emitDirections(iEmitter, entry.noExit);
			}
			if (entry.noEnter != 0) {
				iEmitter << YAML::Key << "noEnter" << YAML::Value;
				emitDirections(iEmitter, entry.noEnter);
			}
			if (entry.cost > 0.f)
				iEmitter << YAML::Key << "cost" << YAML::Value << entry.cost;
			iEmitter << YAML::EndMap;
		}
		iEmitter << YAML::EndSeq;
	}
	iEmitter << YAML::EndMap;
}

auto readNavigation(const core::YamlNode& iNode) -> nav::NavProfile {
	nav::NavProfile profile;
	if (!iNode || !iNode.isMap())
		return profile;
	if (iNode["mode"]) {
		// A file without `mode` is a legacy profile: explicit rules.
		const auto name = iNode["mode"].as<std::string>();
		for (size_t i = 0; i < g_modeNames.size(); ++i) {
			if (name == g_modeNames[i])
				profile.mode = static_cast<nav::NavMode>(i);
		}
	}
	if (iNode["layer"])
		profile.layer = iNode["layer"].as<uint32_t>();
	if (iNode["source"])
		profile.source =
				iNode["source"].as<std::string>() == "terrain" ? nav::NavSource::Terrain : nav::NavSource::Overlay;
	if (iNode["maxRise"])
		profile.maxRise = static_cast<uint8_t>(
				std::min<uint32_t>(iNode["maxRise"].as<uint32_t>(), nav::NavProfile::maxRiseLimit));
	if (iNode["climbCost"])
		profile.climbCost = iNode["climbCost"].as<float>();
	if (iNode["allowSteep"])
		profile.allowSteep = iNode["allowSteep"].as<bool>();
	if (const auto rules = iNode["rules"]; rules && rules.isSequence()) {
		for (const auto ruleNode: rules) {
			if (!ruleNode["tile"])
				continue;
			nav::NavTileRule rule;
			rule.tile = ruleNode["tile"].as<int32_t>();
			if (ruleNode["cost"])
				rule.cost = ruleNode["cost"].as<float>();
			rule.exits = readDirections(ruleNode["exits"], nav::allDirections);
			rule.enters = readDirections(ruleNode["enters"], nav::allDirections);
			profile.rules.push_back(rule);
		}
	}
	if (const auto overrides = iNode["overrides"]; overrides && overrides.isSequence()) {
		for (const auto entryNode: overrides) {
			const auto cell = entryNode["cell"];
			if (!cell || !cell.isSequence() || cell.size() < 2)
				continue;
			nav::NavCellOverride entry{.x = cell[0].as<uint32_t>(), .y = cell[1].as<uint32_t>()};
			entry.noExit = readDirections(entryNode["noExit"], 0);
			entry.noEnter = readDirections(entryNode["noEnter"], 0);
			if (entryNode["cost"])
				entry.cost = std::clamp(entryNode["cost"].as<float>(), 0.f, 64.f);
			if (!entry.isNeutral())
				profile.overrides.push_back(entry);
		}
	}
	return profile;
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
		iEmitter << YAML::Key << "tiles" << YAML::Value << codec::encodeTerrainText(layer);
		iEmitter << YAML::EndMap;
	}
	iEmitter << YAML::EndSeq;
}

auto readInline(const core::YamlNode& iNode) -> shared<scene::TilemapAsset> {
	auto asset = mkShared<scene::TilemapAsset>();
	if (iNode["tilesetPath"])
		asset->tilesetPath = iNode["tilesetPath"].as<std::string>();
	if (iNode["width"])
		asset->width = std::clamp(iNode["width"].as<uint32_t>(), 1u, scene::TilemapAsset::maxGridSide);
	if (iNode["height"])
		asset->height = std::clamp(iNode["height"].as<uint32_t>(), 1u, scene::TilemapAsset::maxGridSide);
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
				layer.assign(gridSize, codec::decodeTileText(layerNode["tiles"].as<std::string>(), expected));
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
	if (!navigation.isEmpty())
		emitNavigation(emitter, navigation);
	emitter << YAML::EndMap;
}

void Tilemap::deserialize(const core::Serializer& iNode) {
	const auto& root = iNode.getImpl()->node;
	tilemapPath.clear();
	asset.reset();
	navigation = readNavigation(root["navigation"]);
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
