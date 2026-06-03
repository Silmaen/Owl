/**
 * @file Chunk.cpp
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/voxel/Chunk.h"

#include <charconv>

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

Chunk::Chunk() : m_blocks(g_ChunkVolume, g_AirBlock) {}

Chunk::Chunk(const math::vec3i& iCoord) : m_coord{iCoord}, m_blocks(g_ChunkVolume, g_AirBlock) {}

auto Chunk::getBlock(const int32_t iX, const int32_t iY, const int32_t iZ) const -> BlockId {
	const auto size = static_cast<int32_t>(g_ChunkSize);
	if (iX < 0 || iY < 0 || iZ < 0 || iX >= size || iY >= size || iZ >= size)
		return g_AirBlock;
	return m_blocks[localIndex(static_cast<uint32_t>(iX), static_cast<uint32_t>(iY), static_cast<uint32_t>(iZ))];
}

void Chunk::setBlock(const int32_t iX, const int32_t iY, const int32_t iZ, const BlockId iBlock) {
	const auto size = static_cast<int32_t>(g_ChunkSize);
	if (iX < 0 || iY < 0 || iZ < 0 || iX >= size || iY >= size || iZ >= size)
		return;
	const uint32_t idx = localIndex(static_cast<uint32_t>(iX), static_cast<uint32_t>(iY), static_cast<uint32_t>(iZ));
	if (m_blocks[idx] == iBlock)
		return;
	m_blocks[idx] = iBlock;
	m_dirty = true;
}

void Chunk::fill(const BlockId iBlock) {
	std::ranges::fill(m_blocks, iBlock);
	m_dirty = true;
}

auto Chunk::isEmpty() const -> bool {
	return std::ranges::all_of(m_blocks, [](const BlockId iBlock) -> bool { return iBlock == g_AirBlock; });
}

auto Chunk::encode() const -> std::string {
	std::string out;
	size_t i = 0;
	bool first = true;
	while (i < m_blocks.size()) {
		const BlockId value = m_blocks[i];
		size_t run = 1;
		while (i + run < m_blocks.size() && m_blocks[i + run] == value) ++run;
		if (!first)
			out.push_back(' ');
		out += std::to_string(run);
		out.push_back('x');
		out += std::to_string(value);
		first = false;
		i += run;
	}
	return out;
}

auto Chunk::decode(const std::string_view iEncoded) -> bool {
	m_blocks.assign(g_ChunkVolume, g_AirBlock);
	m_dirty = false;
	size_t cursor = 0;
	size_t written = 0;
	while (cursor < iEncoded.size() && written < g_ChunkVolume) {
		while (cursor < iEncoded.size() && iEncoded[cursor] == ' ') ++cursor;
		if (cursor >= iEncoded.size())
			break;
		const size_t sep = iEncoded.find('x', cursor);
		if (sep == std::string_view::npos)
			return false;
		size_t spaceEnd = iEncoded.find(' ', sep);
		if (spaceEnd == std::string_view::npos)
			spaceEnd = iEncoded.size();
		size_t run = 0;
		BlockId value = g_AirBlock;
		if (std::from_chars(iEncoded.data() + cursor, iEncoded.data() + sep, run).ec != std::errc{})
			return false;
		if (std::from_chars(iEncoded.data() + sep + 1, iEncoded.data() + spaceEnd, value).ec != std::errc{})
			return false;
		for (size_t k = 0; k < run && written < g_ChunkVolume; ++k) m_blocks[written++] = value;
		cursor = spaceEnd;
	}
	return written == g_ChunkVolume;
}

}// namespace owl::data::voxel
