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

// Maps an occlusion level (0 = most occluded, 3 = open) to a brightness multiplier baked into the vertex.
constexpr std::array<float, 4> k_AoCurve{0.45f, 0.62f, 0.80f, 1.0f};

enum struct BlockClass : uint8_t { All, Opaque, NonOpaque };

auto matchesClass(const BlockRegistry& iRegistry, const BlockId iBlock, const BlockClass iClass) -> bool {
	switch (iClass) {
		case BlockClass::Opaque:
			return iRegistry.isOpaque(iBlock);
		case BlockClass::NonOpaque:
			return !iRegistry.isOpaque(iBlock);
		case BlockClass::All:
			return true;
	}
	return true;
}

// One greedy-mask cell: the block whose face lives here plus the four corner occlusion levels that gate merging.
struct MaskCell {
	BlockId block = g_AirBlock;
	std::array<uint8_t, 4> ao{3, 3, 3, 3};

	auto operator==(const MaskCell& iOther) const -> bool = default;
};

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

auto occludesAt(const Chunk& iChunk, const BlockRegistry& iRegistry, const ChunkMesher::NeighborProvider& iNeighbor,
				const int32_t iAxis, const int32_t iU, const int32_t iV, const int32_t iLayer, const int32_t iUu,
				const int32_t iVv) -> bool {
	std::array<int32_t, 3> coord{0, 0, 0};
	coord[static_cast<size_t>(iAxis)] = iLayer;
	coord[static_cast<size_t>(iU)] = iUu;
	coord[static_cast<size_t>(iV)] = iVv;
	return iRegistry.isOpaque(sampleBlock(iChunk, iNeighbor, coord[0], coord[1], coord[2]));
}

auto cornerLevel(const bool iSide1, const bool iSide2, const bool iCorner) -> uint8_t {
	if (iSide1 && iSide2)
		return 0;
	return static_cast<uint8_t>(
			3 - (static_cast<int32_t>(iSide1) + static_cast<int32_t>(iSide2) + static_cast<int32_t>(iCorner)));
}

auto faceAo(const Chunk& iChunk, const BlockRegistry& iRegistry, const ChunkMesher::NeighborProvider& iNeighbor,
			const int32_t iAxis, const int32_t iU, const int32_t iV, const int32_t iOuterLayer, const int32_t iCu,
			const int32_t iCv) -> std::array<uint8_t, 4> {
	std::array<uint8_t, 4> ao{};
	constexpr std::array<std::array<int32_t, 2>, 4> corners{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
	for (size_t c = 0; c < 4; ++c) {
		const int32_t uOff = corners[c][0] == 0 ? -1 : 1;
		const int32_t vOff = corners[c][1] == 0 ? -1 : 1;
		const bool side1 = occludesAt(iChunk, iRegistry, iNeighbor, iAxis, iU, iV, iOuterLayer, iCu + uOff, iCv);
		const bool side2 = occludesAt(iChunk, iRegistry, iNeighbor, iAxis, iU, iV, iOuterLayer, iCu, iCv + vOff);
		const bool corner =
				occludesAt(iChunk, iRegistry, iNeighbor, iAxis, iU, iV, iOuterLayer, iCu + uOff, iCv + vOff);
		ao[c] = cornerLevel(side1, side2, corner);
	}
	return ao;
}

void buildSliceMask(const Chunk& iChunk, const BlockRegistry& iRegistry, const ChunkMesher::NeighborProvider& iNeighbor,
					const int32_t iAxis, const int32_t iU, const int32_t iV, const int32_t iLayer, const int32_t iStep,
					const BlockClass iClass, const bool iAmbientOcclusion, std::vector<MaskCell>& oMask) {
	std::ranges::fill(oMask, MaskCell{});
	for (int32_t vv = 0; vv < k_Size; ++vv) {
		for (int32_t uu = 0; uu < k_Size; ++uu) {
			std::array<int32_t, 3> coord{0, 0, 0};
			coord[static_cast<size_t>(iAxis)] = iLayer;
			coord[static_cast<size_t>(iU)] = uu;
			coord[static_cast<size_t>(iV)] = vv;
			const BlockId own = iChunk.getBlock(coord[0], coord[1], coord[2]);
			if (iRegistry.isAir(own) || !matchesClass(iRegistry, own, iClass))
				continue;
			const int32_t outer = iLayer + iStep;
			coord[static_cast<size_t>(iAxis)] = outer;
			const BlockId neighbor = sampleBlock(iChunk, iNeighbor, coord[0], coord[1], coord[2]);
			if (!iRegistry.isOpaque(neighbor) && neighbor != own)
				oMask[maskIndex(uu, vv)] = MaskCell{
						.block = own,
						.ao = iAmbientOcclusion ? faceAo(iChunk, iRegistry, iNeighbor, iAxis, iU, iV, outer, uu, vv)
												: std::array<uint8_t, 4>{3, 3, 3, 3}};
		}
	}
}

auto rectWidth(const std::vector<MaskCell>& iMask, const int32_t iU, const int32_t iV, const MaskCell& iCell)
		-> int32_t {
	int32_t width = 1;
	while (iU + width < k_Size && iMask[maskIndex(iU + width, iV)] == iCell) ++width;
	return width;
}

auto rowMatches(const std::vector<MaskCell>& iMask, const int32_t iU, const int32_t iV, const int32_t iWidth,
				const MaskCell& iCell) -> bool {
	for (int32_t k = 0; k < iWidth; ++k) {
		if (!(iMask[maskIndex(iU + k, iV)] == iCell))
			return false;
	}
	return true;
}

auto rectHeight(const std::vector<MaskCell>& iMask, const int32_t iU, const int32_t iV, const int32_t iWidth,
				const MaskCell& iCell) -> int32_t {
	int32_t height = 1;
	while (iV + height < k_Size && rowMatches(iMask, iU, iV + height, iWidth, iCell)) ++height;
	return height;
}

void clearRect(std::vector<MaskCell>& ioMask, const int32_t iU, const int32_t iV, const int32_t iWidth,
			   const int32_t iHeight) {
	for (int32_t hh = 0; hh < iHeight; ++hh)
		for (int32_t ww = 0; ww < iWidth; ++ww) ioMask[maskIndex(iU + ww, iV + hh)] = MaskCell{};
}

void emitQuad(ChunkMesh& ioMesh, const std::array<math::vec3, 4>& iPos, const math::vec3& iNormal, const float iW,
			  const float iH, const uint32_t iTexture, const std::array<uint8_t, 4>& iAo, const bool iFlip,
			  const bool iSwapUv) {
	const auto base = static_cast<uint32_t>(ioMesh.vertices.size());
	// X-axis faces map U->world-Y, V->world-Z, rotating the texture 90 deg; swap UV there to keep its vertical up.
	const std::array<math::vec2, 4> uv = iSwapUv ? std::array<math::vec2, 4>{math::vec2{0.f, 0.f}, math::vec2{0.f, iW},
																			 math::vec2{iH, iW}, math::vec2{iH, 0.f}}
												 : std::array<math::vec2, 4>{math::vec2{0.f, 0.f}, math::vec2{iW, 0.f},
																			 math::vec2{iW, iH}, math::vec2{0.f, iH}};
	for (size_t i = 0; i < 4; ++i)
		ioMesh.vertices.push_back(VoxelVertex{.position = iPos[i],
											  .normal = iNormal,
											  .uv = uv[i],
											  .textureIndex = iTexture,
											  .ao = k_AoCurve[iAo[i]]});
	// Flip the split diagonal when AO is asymmetric, so the dark corner doesn't bleed across the brighter triangle.
	const bool aoFlip = iAo[0] + iAo[2] > iAo[1] + iAo[3];
	std::array<uint32_t, 6> order =
			aoFlip ? std::array<uint32_t, 6>{1, 2, 3, 1, 3, 0} : std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3};
	if (iFlip) {
		for (size_t t = 0; t < 6; t += 3) std::swap(order[t + 1], order[t + 2]);
	}
	for (const uint32_t idx: order) ioMesh.indices.push_back(base + idx);
}

void emitSliceQuads(std::vector<MaskCell>& ioMask, ChunkMesh& ioMesh, const int32_t iAxis, const int32_t iU,
					const int32_t iV, const float iPlane, const math::vec3& iNormal, const BlockFace iFace,
					const BlockRegistry& iRegistry, const bool iFlip) {
	for (int32_t j = 0; j < k_Size; ++j) {
		for (int32_t i = 0; i < k_Size;) {
			const MaskCell cell = ioMask[maskIndex(i, j)];
			if (cell.block == g_AirBlock) {
				++i;
				continue;
			}
			const int32_t width = rectWidth(ioMask, i, j, cell);
			const int32_t height = rectHeight(ioMask, i, j, width, cell);
			clearRect(ioMask, i, j, width, height);
			const std::array<math::vec3, 4> pos{
					buildPos(iAxis, iU, iV, iPlane, static_cast<float>(i), static_cast<float>(j)),
					buildPos(iAxis, iU, iV, iPlane, static_cast<float>(i + width), static_cast<float>(j)),
					buildPos(iAxis, iU, iV, iPlane, static_cast<float>(i + width), static_cast<float>(j + height)),
					buildPos(iAxis, iU, iV, iPlane, static_cast<float>(i), static_cast<float>(j + height))};
			const auto texture = static_cast<uint32_t>(iRegistry.get(cell.block).faceTexture(iFace));
			emitQuad(ioMesh, pos, iNormal, static_cast<float>(width), static_cast<float>(height), texture, cell.ao,
					 iFlip, iAxis == 0);
			i += width;
		}
	}
}

auto meshImpl(const Chunk& iChunk, const BlockRegistry& iRegistry, const ChunkMesher::NeighborProvider& iNeighbor,
			  const BlockClass iClass, const bool iAmbientOcclusion) -> ChunkMesh {
	ChunkMesh out;
	std::vector<MaskCell> mask(static_cast<size_t>(k_Size) * k_Size);
	for (int32_t axis = 0; axis < 3; ++axis) {
		const int32_t u = (axis + 1) % 3;
		const int32_t v = (axis + 2) % 3;
		for (int32_t dir = 0; dir < 2; ++dir) {
			const bool positive = dir == 1;
			const int32_t step = positive ? 1 : -1;
			const math::vec3 normal = buildNormal(axis, positive);
			const BlockFace face = faceForAxisDir(axis, positive);
			for (int32_t layer = 0; layer < k_Size; ++layer) {
				buildSliceMask(iChunk, iRegistry, iNeighbor, axis, u, v, layer, step, iClass, iAmbientOcclusion, mask);
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
	return meshImpl(iChunk, iRegistry, iNeighbor, BlockClass::All, true);
}

auto ChunkMesher::mesh(const Chunk& iChunk, const BlockRegistry& iRegistry) -> ChunkMesh {
	return meshImpl(
			iChunk, iRegistry, [](int32_t, int32_t, int32_t) -> BlockId { return g_AirBlock; }, BlockClass::All, true);
}

auto ChunkMesher::meshByKind(const Chunk& iChunk, const BlockRegistry& iRegistry, const NeighborProvider& iNeighbor,
							 const bool iAmbientOcclusion) -> ChunkMeshSet {
	return ChunkMeshSet{.opaque = meshImpl(iChunk, iRegistry, iNeighbor, BlockClass::Opaque, iAmbientOcclusion),
						.transparent =
								meshImpl(iChunk, iRegistry, iNeighbor, BlockClass::NonOpaque, iAmbientOcclusion)};
}

}// namespace owl::data::voxel
