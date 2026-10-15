/**
 * @file TilemapCommands.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "TilemapCommands.h"

#include <scene/TilemapTerrain.h>

#include <algorithm>
#include <tuple>
#include <utility>

namespace owl::nest::commands {

namespace {

using scene::component::TilemapChunk;
using scene::component::TilemapChunkRef;
using scene::component::TilemapLayer;

auto chunkOf(const uint32_t iX, const uint32_t iY) -> math::vec2ui {
	return {iX / TilemapLayer::chunkSize, iY / TilemapLayer::chunkSize};
}

auto sameContent(const TilemapChunkRef& iFirst, const TilemapChunkRef& iSecond) -> bool {
	if (iFirst == iSecond)
		return true;
	static const TilemapChunk s_empty;
	const auto* first = iFirst ? iFirst.get() : &s_empty;
	const auto* second = iSecond ? iSecond.get() : &s_empty;
	return *first == *second;
}

}// namespace

ModifyTilemapChunksCommand::ModifyTilemapChunksCommand(std::vector<TilemapChunkRecord> iBefore,
													   std::string iDescription)
	: m_records{std::move(iBefore)}, m_description{std::move(iDescription)} {}

void ModifyTilemapChunksCommand::undo(scene::TilemapAsset& ioTarget) { swapChunks(ioTarget); }

void ModifyTilemapChunksCommand::redo(scene::TilemapAsset& ioTarget) { swapChunks(ioTarget); }

void ModifyTilemapChunksCommand::swapChunks(scene::TilemapAsset& ioTarget) {
	for (auto& [layerIndex, chunk, content]: m_records) {
		if (layerIndex >= ioTarget.layers.size())
			continue;
		auto& layer = ioTarget.layers[layerIndex];
		auto live = layer.getChunkRef(chunk);
		std::ignore = layer.setChunkRef(chunk, content);
		content = std::move(live);
	}
}

auto ModifyTilemapChunksCommand::getStoredBytes() const -> size_t {
	return static_cast<size_t>(std::ranges::count_if(
				   m_records,
				   [](const TilemapChunkRecord& iRecord) -> bool { return static_cast<bool>(iRecord.content); })) *
		   sizeof(TilemapChunk);
}

ModifyTilemapLayoutCommand::ModifyTilemapLayoutCommand(scene::TilemapAsset iBefore, scene::TilemapAsset iAfter,
													   std::string iDescription)
	: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)} {}

void ModifyTilemapLayoutCommand::undo(scene::TilemapAsset& ioTarget) { ioTarget = m_before; }

void ModifyTilemapLayoutCommand::redo(scene::TilemapAsset& ioTarget) { ioTarget = m_after; }

ModifyTerrainHeightsCommand::ModifyTerrainHeightsCommand(std::vector<scene::CornerHeightChange> iChanges,
														 std::string iDescription)
	: m_changes{std::move(iChanges)}, m_description{std::move(iDescription)} {}

void ModifyTerrainHeightsCommand::undo(scene::TilemapAsset& ioTarget) { ioTarget.applyCornerChanges(m_changes, true); }

void ModifyTerrainHeightsCommand::redo(scene::TilemapAsset& ioTarget) { ioTarget.applyCornerChanges(m_changes, false); }

void TerrainStroke::add(const std::vector<scene::CornerHeightChange>& iChanges) {
	m_changes.insert(m_changes.end(), iChanges.begin(), iChanges.end());
}

auto TerrainStroke::finish(const std::string& iDescription) -> uniq<ModifyTerrainHeightsCommand> {
	scene::terrain::mergeCornerChanges(m_changes);
	if (m_changes.empty())
		return nullptr;
	auto command = mkUniq<ModifyTerrainHeightsCommand>(std::move(m_changes), iDescription);
	m_changes.clear();
	return command;
}

auto TilemapStroke::setTile(scene::TilemapAsset& ioAsset, const uint32_t iLayer, const uint32_t iX, const uint32_t iY,
							const int32_t iValue) -> bool {
	if (iLayer >= ioAsset.layers.size() || ioAsset.getTile(iLayer, iX, iY) == iValue)
		return false;
	record(ioAsset, iLayer, iX, iY);
	return ioAsset.layers[iLayer].setTile(iX, iY, iValue);
}

auto TilemapStroke::setCell(scene::TilemapAsset& ioAsset, const uint32_t iLayer, const uint32_t iX, const uint32_t iY,
							const scene::component::TileCell& iCell) -> bool {
	if (iLayer >= ioAsset.layers.size() || ioAsset.layers[iLayer].getCell(iX, iY) == iCell)
		return false;
	record(ioAsset, iLayer, iX, iY);
	return ioAsset.layers[iLayer].setCell(iX, iY, iCell);
}

void TilemapStroke::record(const scene::TilemapAsset& iAsset, const uint32_t iLayer, const uint32_t iX,
						   const uint32_t iY) {
	const auto chunk = chunkOf(iX, iY);
	const bool known = std::ranges::any_of(m_before, [&](const TilemapChunkRecord& iRecord) -> bool {
		return iRecord.layer == iLayer && iRecord.chunk == chunk;
	});
	if (!known)
		m_before.push_back({.layer = iLayer, .chunk = chunk, .content = iAsset.layers[iLayer].getChunkRef(chunk)});
}

auto TilemapStroke::finish(const scene::TilemapAsset& iAsset, const std::string& iDescription)
		-> uniq<ModifyTilemapChunksCommand> {
	std::erase_if(m_before, [&iAsset](const TilemapChunkRecord& iRecord) -> bool {
		return iRecord.layer >= iAsset.layers.size() ||
			   sameContent(iRecord.content, iAsset.layers[iRecord.layer].getChunkRef(iRecord.chunk));
	});
	if (m_before.empty())
		return nullptr;
	auto command = mkUniq<ModifyTilemapChunksCommand>(std::move(m_before), iDescription);
	m_before.clear();
	return command;
}

}// namespace owl::nest::commands
