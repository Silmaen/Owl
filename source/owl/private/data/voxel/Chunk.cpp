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

#include <atomic>
#include <cstdint>

namespace owl::data::voxel {

namespace {
std::atomic<uint64_t> g_RevisionCounter{0};

auto countSolid(const std::vector<BlockId>& iBlocks) -> uint32_t {
	return static_cast<uint32_t>(
			std::ranges::count_if(iBlocks, [](const BlockId iBlock) -> bool { return iBlock != g_AirBlock; }));
}
}// namespace

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

Chunk::Chunk(const Chunk& iOther)
	: m_coord{iOther.m_coord}, m_blocks{iOther.m_blocks}, m_meta{iOther.m_meta}, m_dirty{iOther.m_dirty},
	  m_solidCount{iOther.m_solidCount}, m_revision{iOther.getRevision()} {}

auto Chunk::operator=(const Chunk& iOther) -> Chunk& {
	if (this == &iOther)
		return *this;
	m_coord = iOther.m_coord;
	m_blocks = iOther.m_blocks;
	m_meta = iOther.m_meta;
	m_dirty = iOther.m_dirty;
	m_solidCount = iOther.m_solidCount;
	m_revision = iOther.getRevision();
	return *this;
}

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
	if (m_blocks[idx] == g_AirBlock && iBlock != g_AirBlock)
		++m_solidCount;
	else if (m_blocks[idx] != g_AirBlock && iBlock == g_AirBlock)
		--m_solidCount;
	m_blocks[idx] = iBlock;
	m_meta[idx] = iMeta;
	m_dirty = true;
	m_revision = 0;
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
	m_revision = 0;
}

void Chunk::fill(const BlockId iBlock) {
	std::ranges::fill(m_blocks, iBlock);
	std::ranges::fill(m_meta, g_DefaultMeta);
	m_solidCount = iBlock == g_AirBlock ? 0 : g_ChunkVolume;
	m_dirty = true;
	m_revision = 0;
}

auto Chunk::getRevision() const -> uint64_t {
	if (m_revision == 0)
		m_revision = g_RevisionCounter.fetch_add(1, std::memory_order_relaxed) + 1;
	return m_revision;
}

auto Chunk::encode() const -> std::string { return encodeBlockRuns(m_blocks, m_meta); }

auto Chunk::decode(const std::string_view iEncoded) -> bool {
	const bool ok = decodeBlockRuns(iEncoded, m_blocks, m_meta, g_ChunkVolume);
	m_solidCount = countSolid(m_blocks);
	m_dirty = false;
	m_revision = 0;
	return ok;
}

}// namespace owl::data::voxel
