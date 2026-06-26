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
#include "data/voxel/BlockRunLength.h"
#include "data/voxel/Chunk.h"
#include "data/voxel/VoxelStructure.h"
#include "data/voxel/VoxelWorld.h"

namespace owl::data::voxel {

namespace {
constexpr int32_t k_ChunkSize = static_cast<int32_t>(g_ChunkSize);
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

auto VoxelStructure::metaAt(const int32_t iX, const int32_t iY, const int32_t iZ) const -> PackedMeta {
	if (iX < 0 || iY < 0 || iZ < 0 || iX >= size.x() || iY >= size.y() || iZ >= size.z())
		return g_DefaultMeta;
	const size_t index = static_cast<size_t>(iX) + static_cast<size_t>(iY) * static_cast<size_t>(size.x()) +
						 static_cast<size_t>(iZ) * static_cast<size_t>(size.x()) * static_cast<size_t>(size.y());
	return index < meta.size() ? meta[index] : g_DefaultMeta;
}

void VoxelStructure::forEachSolid(const std::function<void(const math::vec3i&, BlockId, PackedMeta)>& iVisitor) const {
	for (int32_t z = 0; z < size.z(); ++z)
		for (int32_t y = 0; y < size.y(); ++y)
			for (int32_t x = 0; x < size.x(); ++x) {
				if (const BlockId id = at(x, y, z); id != g_AirBlock)
					iVisitor(math::vec3i{x, y, z}, id, metaAt(x, y, z));
			}
}

auto VoxelStructure::serializeToString(const std::string_view iName) const -> std::string {
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "Structure" << YAML::Value << std::string{iName};
	emitter << YAML::Key << "Version" << YAML::Value << 1;
	emitter << YAML::Key << "Size" << YAML::Value << YAML::Flow << YAML::BeginSeq << size.x() << size.y() << size.z()
			<< YAML::EndSeq;
	emitter << YAML::Key << "Blocks" << YAML::Value << encodeBlockRuns(blocks, meta);
	emitter << YAML::EndMap;
	return emitter.c_str();
}

auto VoxelStructure::deserializeFromString(const std::string_view iYaml) -> bool {
	size = math::vec3i{0, 0, 0};
	blocks.clear();
	meta.clear();
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
		!blocksNode || !decodeBlockRuns(blocksNode.as<std::string>(), blocks, meta, volume())) {
		OWL_CORE_WARN("VoxelStructure: Blocks run count does not match Size volume.")
		size = math::vec3i{0, 0, 0};
		blocks.clear();
		meta.clear();
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
	structure.meta.assign(structure.volume(), g_DefaultMeta);
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
					structure.meta[index] = iChunk.getMeta(x, y, z);
				}
	};
	iWorld.forEachChunk(fill);
	return structure;
}

void VoxelStructure::stampInto(VoxelWorld& ioWorld, const math::vec3i& iOrigin) const {
	forEachSolid([&](const math::vec3i& iLocal, const BlockId iId, const PackedMeta iMeta) -> void {
		ioWorld.setBlock(math::vec3i{iOrigin.x() + iLocal.x(), iOrigin.y() + iLocal.y(), iOrigin.z() + iLocal.z()}, iId,
						 iMeta);
	});
}

}// namespace owl::data::voxel
