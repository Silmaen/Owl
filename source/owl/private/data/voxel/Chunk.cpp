/**
 * @file Chunk.cpp
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/voxel/Chunk.h"

#include "data/voxel/BlockRunLength.h"

namespace owl::data::voxel {

auto worldToChunk(const math::vec3i& iWorld) -> math::vec3i {
	const auto size = static_cast<int32_t>(g_ChunkSize);
	return math::vec3i{floorDiv(iWorld.x(), size), floorDiv(iWorld.y(), size), floorDiv(iWorld.z(), size)};
}

auto worldToLocal(const math::vec3i& iWorld) -> math::vec3i {
	const auto size = static_cast<int32_t>(g_ChunkSize);
	const math::vec3i chunk = worldToChunk(iWorld);
	return math::vec3i{iWorld.x() - chunk.x() * size, iWorld.y() - chunk.y() * size, iWorld.z() - chunk.z() * size};
}

Chunk::Chunk() : m_blocks(g_ChunkVolume, g_AirBlock), m_meta(g_ChunkVolume, g_DefaultMeta) {}

Chunk::Chunk(const math::vec3i& iCoord)
	: m_coord{iCoord}, m_blocks(g_ChunkVolume, g_AirBlock), m_meta(g_ChunkVolume, g_DefaultMeta) {}

auto Chunk::getBlock(const int32_t iX, const int32_t iY, const int32_t iZ) const -> BlockId {
	const auto size = static_cast<int32_t>(g_ChunkSize);
	if (iX < 0 || iY < 0 || iZ < 0 || iX >= size || iY >= size || iZ >= size)
		return g_AirBlock;
	return m_blocks[localIndex(static_cast<uint32_t>(iX), static_cast<uint32_t>(iY), static_cast<uint32_t>(iZ))];
}

auto Chunk::getMeta(const int32_t iX, const int32_t iY, const int32_t iZ) const -> PackedMeta {
	const auto size = static_cast<int32_t>(g_ChunkSize);
	if (iX < 0 || iY < 0 || iZ < 0 || iX >= size || iY >= size || iZ >= size)
		return g_DefaultMeta;
	return m_meta[localIndex(static_cast<uint32_t>(iX), static_cast<uint32_t>(iY), static_cast<uint32_t>(iZ))];
}

void Chunk::setBlock(const int32_t iX, const int32_t iY, const int32_t iZ, const BlockId iBlock,
					 const PackedMeta iMeta) {
	const auto size = static_cast<int32_t>(g_ChunkSize);
	if (iX < 0 || iY < 0 || iZ < 0 || iX >= size || iY >= size || iZ >= size)
		return;
	const uint32_t idx = localIndex(static_cast<uint32_t>(iX), static_cast<uint32_t>(iY), static_cast<uint32_t>(iZ));
	if (m_blocks[idx] == iBlock && m_meta[idx] == iMeta)
		return;
	m_blocks[idx] = iBlock;
	m_meta[idx] = iMeta;
	m_dirty = true;
}

void Chunk::setMeta(const int32_t iX, const int32_t iY, const int32_t iZ, const PackedMeta iMeta) {
	const auto size = static_cast<int32_t>(g_ChunkSize);
	if (iX < 0 || iY < 0 || iZ < 0 || iX >= size || iY >= size || iZ >= size)
		return;
	const uint32_t idx = localIndex(static_cast<uint32_t>(iX), static_cast<uint32_t>(iY), static_cast<uint32_t>(iZ));
	if (m_meta[idx] == iMeta)
		return;
	m_meta[idx] = iMeta;
	m_dirty = true;
}

void Chunk::fill(const BlockId iBlock) {
	std::ranges::fill(m_blocks, iBlock);
	std::ranges::fill(m_meta, g_DefaultMeta);
	m_dirty = true;
}

auto Chunk::isEmpty() const -> bool {
	return std::ranges::all_of(m_blocks, [](const BlockId iBlock) -> bool { return iBlock == g_AirBlock; });
}

auto Chunk::encode() const -> std::string { return encodeBlockRuns(m_blocks, m_meta); }

auto Chunk::decode(const std::string_view iEncoded) -> bool {
	const bool ok = decodeBlockRuns(iEncoded, m_blocks, m_meta, g_ChunkVolume);
	m_dirty = false;
	return ok;
}

}// namespace owl::data::voxel
