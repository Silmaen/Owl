/**
 * @file VoxelStructure.cpp
 * @author Silmaen
 * @date 12/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/external/yaml.h"
#include "data/voxel/Block.h"
#include "data/voxel/Chunk.h"
#include "data/voxel/VoxelStructure.h"
#include "data/voxel/VoxelWorld.h"

#include <charconv>

namespace owl::data::voxel {

namespace {
constexpr int32_t k_ChunkSize = static_cast<int32_t>(g_ChunkSize);

auto encodeRuns(const std::vector<BlockId>& iBlocks) -> std::string {
	std::string out;
	size_t i = 0;
	bool first = true;
	while (i < iBlocks.size()) {
		const BlockId value = iBlocks[i];
		size_t run = 1;
		while (i + run < iBlocks.size() && iBlocks[i + run] == value) ++run;
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

auto decodeRuns(const std::string_view iEncoded, std::vector<BlockId>& oBlocks, const size_t iVolume) -> bool {
	oBlocks.assign(iVolume, g_AirBlock);
	size_t cursor = 0;
	size_t written = 0;
	while (cursor < iEncoded.size() && written < iVolume) {
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
		for (size_t k = 0; k < run && written < iVolume; ++k) oBlocks[written++] = value;
		cursor = spaceEnd;
	}
	return written == iVolume;
}
}// namespace

auto VoxelStructure::volume() const -> size_t {
	if (size.x() <= 0 || size.y() <= 0 || size.z() <= 0)
		return 0;
	return static_cast<size_t>(size.x()) * static_cast<size_t>(size.y()) * static_cast<size_t>(size.z());
}

auto VoxelStructure::at(const int32_t iX, const int32_t iY, const int32_t iZ) const -> BlockId {
	if (iX < 0 || iY < 0 || iZ < 0 || iX >= size.x() || iY >= size.y() || iZ >= size.z())
		return g_AirBlock;
	const size_t index = static_cast<size_t>(iX) + static_cast<size_t>(iY) * static_cast<size_t>(size.x()) +
						 static_cast<size_t>(iZ) * static_cast<size_t>(size.x()) * static_cast<size_t>(size.y());
	return index < blocks.size() ? blocks[index] : g_AirBlock;
}

void VoxelStructure::forEachSolid(const std::function<void(const math::vec3i&, BlockId)>& iVisitor) const {
	for (int32_t z = 0; z < size.z(); ++z)
		for (int32_t y = 0; y < size.y(); ++y)
			for (int32_t x = 0; x < size.x(); ++x) {
				if (const BlockId id = at(x, y, z); id != g_AirBlock)
					iVisitor(math::vec3i{x, y, z}, id);
			}
}

auto VoxelStructure::serializeToString(const std::string_view iName) const -> std::string {
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "Structure" << YAML::Value << std::string{iName};
	emitter << YAML::Key << "Version" << YAML::Value << 1;
	emitter << YAML::Key << "Size" << YAML::Value << YAML::Flow << YAML::BeginSeq << size.x() << size.y() << size.z()
			<< YAML::EndSeq;
	emitter << YAML::Key << "Blocks" << YAML::Value << encodeRuns(blocks);
	emitter << YAML::EndMap;
	return emitter.c_str();
}

auto VoxelStructure::deserializeFromString(const std::string_view iYaml) -> bool {
	size = math::vec3i{0, 0, 0};
	blocks.clear();
	const YAML::Node node = YAML::Load(std::string{iYaml});
	const auto sizeNode = node["Size"];
	if (!sizeNode || !sizeNode.IsSequence() || sizeNode.size() < 3) {
		OWL_CORE_WARN("VoxelStructure: missing or malformed Size in structure document.")
		return false;
	}
	size = math::vec3i{sizeNode[0].as<int32_t>(), sizeNode[1].as<int32_t>(), sizeNode[2].as<int32_t>()};
	if (size.x() <= 0 || size.y() <= 0 || size.z() <= 0) {
		OWL_CORE_WARN("VoxelStructure: non-positive Size in structure document.")
		size = math::vec3i{0, 0, 0};
		return false;
	}
	if (const auto blocksNode = node["Blocks"];
		!blocksNode || !decodeRuns(blocksNode.as<std::string>(), blocks, volume())) {
		OWL_CORE_WARN("VoxelStructure: Blocks run count does not match Size volume.")
		size = math::vec3i{0, 0, 0};
		blocks.clear();
		return false;
	}
	return true;
}

auto VoxelStructure::captureFromWorld(const VoxelWorld& iWorld, const BlockRegistry& iRegistry) -> VoxelStructure {
	bool any = false;
	math::vec3i mn{0, 0, 0};
	math::vec3i mx{0, 0, 0};
	const auto bounds = [&](const math::vec3i& iCoord, const Chunk& iChunk) -> void {
		for (int32_t z = 0; z < k_ChunkSize; ++z)
			for (int32_t y = 0; y < k_ChunkSize; ++y)
				for (int32_t x = 0; x < k_ChunkSize; ++x) {
					if (iRegistry.isAir(iChunk.getBlock(x, y, z)))
						continue;
					const math::vec3i world{iCoord.x() * k_ChunkSize + x, iCoord.y() * k_ChunkSize + y,
											iCoord.z() * k_ChunkSize + z};
					if (!any) {
						mn = world;
						mx = world;
						any = true;
					} else {
						mn = math::vec3i{std::min(mn.x(), world.x()), std::min(mn.y(), world.y()),
										 std::min(mn.z(), world.z())};
						mx = math::vec3i{std::max(mx.x(), world.x()), std::max(mx.y(), world.y()),
										 std::max(mx.z(), world.z())};
					}
				}
	};
	iWorld.forEachChunk(bounds);
	if (!any)
		return {};
	VoxelStructure structure;
	structure.size = math::vec3i{mx.x() - mn.x() + 1, mx.y() - mn.y() + 1, mx.z() - mn.z() + 1};
	structure.blocks.assign(structure.volume(), g_AirBlock);
	const auto fill = [&](const math::vec3i& iCoord, const Chunk& iChunk) -> void {
		for (int32_t z = 0; z < k_ChunkSize; ++z)
			for (int32_t y = 0; y < k_ChunkSize; ++y)
				for (int32_t x = 0; x < k_ChunkSize; ++x) {
					const BlockId id = iChunk.getBlock(x, y, z);
					if (iRegistry.isAir(id))
						continue;
					const int32_t lx = iCoord.x() * k_ChunkSize + x - mn.x();
					const int32_t ly = iCoord.y() * k_ChunkSize + y - mn.y();
					const int32_t lz = iCoord.z() * k_ChunkSize + z - mn.z();
					const size_t index = static_cast<size_t>(lx) +
										 static_cast<size_t>(ly) * static_cast<size_t>(structure.size.x()) +
										 static_cast<size_t>(lz) * static_cast<size_t>(structure.size.x()) *
												 static_cast<size_t>(structure.size.y());
					structure.blocks[index] = id;
				}
	};
	iWorld.forEachChunk(fill);
	return structure;
}

void VoxelStructure::stampInto(VoxelWorld& ioWorld, const math::vec3i& iOrigin) const {
	forEachSolid([&](const math::vec3i& iLocal, const BlockId iId) -> void {
		ioWorld.setBlock(math::vec3i{iOrigin.x() + iLocal.x(), iOrigin.y() + iLocal.y(), iOrigin.z() + iLocal.z()},
						 iId);
	});
}

}// namespace owl::data::voxel
