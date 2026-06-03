/**
 * @file ChunkMesher.cpp
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/voxel/ChunkMesher.h"

#include <array>

namespace owl::data::voxel {

namespace {
constexpr int32_t k_Size = static_cast<int32_t>(g_ChunkSize);

auto faceForAxisDir(const int32_t iAxis, const bool iPositive) -> BlockFace {
	switch (iAxis) {
		case 0:
			return iPositive ? BlockFace::XPos : BlockFace::XNeg;
		case 1:
			return iPositive ? BlockFace::YPos : BlockFace::YNeg;
		default:
			return iPositive ? BlockFace::ZPos : BlockFace::ZNeg;
	}
}

auto buildPos(const int32_t iAxis, const int32_t iU, const int32_t iV, const float iPlane, const float iUc,
			  const float iVc) -> math::vec3 {
	std::array<float, 3> comp{0.f, 0.f, 0.f};
	comp[static_cast<size_t>(iAxis)] = iPlane;
	comp[static_cast<size_t>(iU)] = iUc;
	comp[static_cast<size_t>(iV)] = iVc;
	return math::vec3{comp[0], comp[1], comp[2]};
}

auto buildNormal(const int32_t iAxis, const bool iPositive) -> math::vec3 {
	std::array<float, 3> comp{0.f, 0.f, 0.f};
	comp[static_cast<size_t>(iAxis)] = iPositive ? 1.f : -1.f;
	return math::vec3{comp[0], comp[1], comp[2]};
}

auto maskIndex(const int32_t iU, const int32_t iV) -> size_t {
	return static_cast<size_t>(iU) + static_cast<size_t>(iV) * g_ChunkSize;
}

auto sampleBlock(const Chunk& iChunk, const ChunkMesher::NeighborProvider& iNeighbor, const int32_t iX,
				 const int32_t iY, const int32_t iZ) -> BlockId {
	if (iX >= 0 && iX < k_Size && iY >= 0 && iY < k_Size && iZ >= 0 && iZ < k_Size)
		return iChunk.getBlock(iX, iY, iZ);
	return iNeighbor(iX, iY, iZ);
}

void buildSliceMask(const Chunk& iChunk, const BlockRegistry& iRegistry, const ChunkMesher::NeighborProvider& iNeighbor,
					const int32_t iAxis, const int32_t iU, const int32_t iV, const int32_t iLayer, const int32_t iStep,
					std::vector<BlockId>& oMask) {
	std::ranges::fill(oMask, g_AirBlock);
	for (int32_t vv = 0; vv < k_Size; ++vv) {
		for (int32_t uu = 0; uu < k_Size; ++uu) {
			std::array<int32_t, 3> coord{0, 0, 0};
			coord[static_cast<size_t>(iAxis)] = iLayer;
			coord[static_cast<size_t>(iU)] = uu;
			coord[static_cast<size_t>(iV)] = vv;
			const BlockId own = iChunk.getBlock(coord[0], coord[1], coord[2]);
			if (iRegistry.isAir(own))
				continue;
			coord[static_cast<size_t>(iAxis)] += iStep;
			const BlockId neighbor = sampleBlock(iChunk, iNeighbor, coord[0], coord[1], coord[2]);
			if (!iRegistry.isOpaque(neighbor) && neighbor != own)
				oMask[maskIndex(uu, vv)] = own;
		}
	}
}

auto rectWidth(const std::vector<BlockId>& iMask, const int32_t iU, const int32_t iV, const BlockId iBlock) -> int32_t {
	int32_t width = 1;
	while (iU + width < k_Size && iMask[maskIndex(iU + width, iV)] == iBlock) ++width;
	return width;
}

auto rowMatches(const std::vector<BlockId>& iMask, const int32_t iU, const int32_t iV, const int32_t iWidth,
				const BlockId iBlock) -> bool {
	for (int32_t k = 0; k < iWidth; ++k) {
		if (iMask[maskIndex(iU + k, iV)] != iBlock)
			return false;
	}
	return true;
}

auto rectHeight(const std::vector<BlockId>& iMask, const int32_t iU, const int32_t iV, const int32_t iWidth,
				const BlockId iBlock) -> int32_t {
	int32_t height = 1;
	while (iV + height < k_Size && rowMatches(iMask, iU, iV + height, iWidth, iBlock)) ++height;
	return height;
}

void clearRect(std::vector<BlockId>& ioMask, const int32_t iU, const int32_t iV, const int32_t iWidth,
			   const int32_t iHeight) {
	for (int32_t hh = 0; hh < iHeight; ++hh)
		for (int32_t ww = 0; ww < iWidth; ++ww) ioMask[maskIndex(iU + ww, iV + hh)] = g_AirBlock;
}

void emitQuad(ChunkMesh& ioMesh, const std::array<math::vec3, 4>& iPos, const math::vec3& iNormal, const float iW,
			  const float iH, const uint32_t iTexture, const bool iFlip) {
	const auto base = static_cast<uint32_t>(ioMesh.vertices.size());
	const std::array<math::vec2, 4> uv{math::vec2{0.f, 0.f}, math::vec2{iW, 0.f}, math::vec2{iW, iH},
									   math::vec2{0.f, iH}};
	for (size_t i = 0; i < 4; ++i)
		ioMesh.vertices.push_back(
				VoxelVertex{.position = iPos[i], .normal = iNormal, .uv = uv[i], .textureIndex = iTexture});
	const std::array<uint32_t, 6> order =
			iFlip ? std::array<uint32_t, 6>{0, 2, 1, 0, 3, 2} : std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3};
	for (const uint32_t idx: order) ioMesh.indices.push_back(base + idx);
}

void emitSliceQuads(std::vector<BlockId>& ioMask, ChunkMesh& ioMesh, const int32_t iAxis, const int32_t iU,
					const int32_t iV, const float iPlane, const math::vec3& iNormal, const BlockFace iFace,
					const BlockRegistry& iRegistry, const bool iFlip) {
	for (int32_t j = 0; j < k_Size; ++j) {
		for (int32_t i = 0; i < k_Size;) {
			const BlockId block = ioMask[maskIndex(i, j)];
			if (block == g_AirBlock) {
				++i;
				continue;
			}
			const int32_t width = rectWidth(ioMask, i, j, block);
			const int32_t height = rectHeight(ioMask, i, j, width, block);
			clearRect(ioMask, i, j, width, height);
			const std::array<math::vec3, 4> pos{
					buildPos(iAxis, iU, iV, iPlane, static_cast<float>(i), static_cast<float>(j)),
					buildPos(iAxis, iU, iV, iPlane, static_cast<float>(i + width), static_cast<float>(j)),
					buildPos(iAxis, iU, iV, iPlane, static_cast<float>(i + width), static_cast<float>(j + height)),
					buildPos(iAxis, iU, iV, iPlane, static_cast<float>(i), static_cast<float>(j + height))};
			const auto texture = static_cast<uint32_t>(iRegistry.get(block).faceTexture(iFace));
			emitQuad(ioMesh, pos, iNormal, static_cast<float>(width), static_cast<float>(height), texture, iFlip);
			i += width;
		}
	}
}

auto meshImpl(const Chunk& iChunk, const BlockRegistry& iRegistry, const ChunkMesher::NeighborProvider& iNeighbor)
		-> ChunkMesh {
	ChunkMesh out;
	std::vector<BlockId> mask(static_cast<size_t>(k_Size) * k_Size, g_AirBlock);
	for (int32_t axis = 0; axis < 3; ++axis) {
		const int32_t u = (axis + 1) % 3;
		const int32_t v = (axis + 2) % 3;
		for (int32_t dir = 0; dir < 2; ++dir) {
			const bool positive = dir == 1;
			const int32_t step = positive ? 1 : -1;
			const math::vec3 normal = buildNormal(axis, positive);
			const BlockFace face = faceForAxisDir(axis, positive);
			for (int32_t layer = 0; layer < k_Size; ++layer) {
				buildSliceMask(iChunk, iRegistry, iNeighbor, axis, u, v, layer, step, mask);
				const float plane = positive ? static_cast<float>(layer + 1) : static_cast<float>(layer);
				emitSliceQuads(mask, out, axis, u, v, plane, normal, face, iRegistry, !positive);
			}
		}
	}
	return out;
}
}// namespace

auto ChunkMesher::mesh(const Chunk& iChunk, const BlockRegistry& iRegistry, const NeighborProvider& iNeighbor)
		-> ChunkMesh {
	return meshImpl(iChunk, iRegistry, iNeighbor);
}

auto ChunkMesher::mesh(const Chunk& iChunk, const BlockRegistry& iRegistry) -> ChunkMesh {
	return meshImpl(iChunk, iRegistry, [](int32_t, int32_t, int32_t) -> BlockId { return g_AirBlock; });
}

}// namespace owl::data::voxel
