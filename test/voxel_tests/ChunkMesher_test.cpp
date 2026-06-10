/**
 * @file ChunkMesher_test.cpp
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <data/voxel/ChunkMesher.h>

#include <cmath>

using namespace owl;
using namespace owl::data::voxel;

namespace {
class ChunkMesherFixture : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};

struct Registry {
	BlockRegistry reg;
	BlockId stone = 0;
	BlockId glass = 0;
	BlockId marker = 0;

	Registry() {
		BlockType s;
		s.name = "stone";
		s.renderKind = BlockRenderKind::Opaque;
		s.solid = true;
		s.setAllFaces(1);
		stone = reg.registerBlock(s);
		BlockType g;
		g.name = "glass";
		g.renderKind = BlockRenderKind::Transparent;
		g.solid = true;
		g.setAllFaces(5);
		glass = reg.registerBlock(g);
		BlockType m;
		m.name = "marker";
		m.renderKind = BlockRenderKind::Opaque;
		m.solid = true;
		for (uint16_t f = 0; f < g_FaceCount; ++f) m.faceTextures[f] = static_cast<uint16_t>(100 + f);
		marker = reg.registerBlock(m);
	}
};

auto fEq(const float iA, const float iB) -> bool { return std::fabs(iA - iB) < 1e-4f; }

auto normalIs(const VoxelVertex& iVertex, const float iX, const float iY, const float iZ) -> bool {
	return fEq(iVertex.normal.x(), iX) && fEq(iVertex.normal.y(), iY) && fEq(iVertex.normal.z(), iZ);
}

auto quadsByNormal(const ChunkMesh& iMesh, const float iX, const float iY, const float iZ) -> size_t {
	size_t count = 0;
	for (size_t q = 0; q * 4 < iMesh.vertices.size(); ++q) {
		if (normalIs(iMesh.vertices[q * 4], iX, iY, iZ))
			++count;
	}
	return count;
}
}// namespace

TEST_F(ChunkMesherFixture, EmptyChunkProducesNoMesh) {
	const Registry r;
	const Chunk chunk;
	const ChunkMesh mesh = ChunkMesher::mesh(chunk, r.reg);
	EXPECT_TRUE(mesh.isEmpty());
	EXPECT_EQ(mesh.quadCount(), 0u);
}

TEST_F(ChunkMesherFixture, SingleBlockSixQuads) {
	const Registry r;
	Chunk chunk;
	chunk.setBlock(0, 0, 0, r.stone);
	const ChunkMesh mesh = ChunkMesher::mesh(chunk, r.reg);
	EXPECT_EQ(mesh.quadCount(), 6u);
	EXPECT_EQ(mesh.triangleCount(), 12u);
	EXPECT_EQ(mesh.vertices.size(), 24u);
	for (const float s: {-1.f, 1.f}) {
		EXPECT_EQ(quadsByNormal(mesh, s, 0, 0), 1u);
		EXPECT_EQ(quadsByNormal(mesh, 0, s, 0), 1u);
		EXPECT_EQ(quadsByNormal(mesh, 0, 0, s), 1u);
	}
}

TEST_F(ChunkMesherFixture, FullChunkGreedyMergesToSixQuads) {
	const Registry r;
	Chunk chunk;
	chunk.fill(r.stone);
	const ChunkMesh mesh = ChunkMesher::mesh(chunk, r.reg);
	// A solid cube exposes only its six outer faces; greedy meshing merges each
	// 16x16 face into a single quad (non-greedy would emit 6 * 256 quads).
	EXPECT_EQ(mesh.quadCount(), 6u);
}

TEST_F(ChunkMesherFixture, GreedyMergesFlatLayer) {
	const Registry r;
	Chunk chunk;
	for (int32_t z = 0; z < static_cast<int32_t>(g_ChunkSize); ++z)
		for (int32_t x = 0; x < static_cast<int32_t>(g_ChunkSize); ++x) chunk.setBlock(x, 0, z, r.stone);
	const ChunkMesh mesh = ChunkMesher::mesh(chunk, r.reg);
	// A 16x16x1 slab is a box: top + bottom + four side strips all merge to one quad each.
	EXPECT_EQ(mesh.quadCount(), 6u);
	EXPECT_EQ(quadsByNormal(mesh, 0, 1, 0), 1u);
	EXPECT_EQ(quadsByNormal(mesh, 0, -1, 0), 1u);
}

TEST_F(ChunkMesherFixture, AdjacentBlocksCullSharedFace) {
	const Registry r;
	Chunk chunk;
	chunk.setBlock(0, 0, 0, r.stone);
	chunk.setBlock(1, 0, 0, r.stone);
	const ChunkMesh mesh = ChunkMesher::mesh(chunk, r.reg);
	// A 2x1x1 box: the shared interior face is culled, the rest merge to a 6-quad box.
	EXPECT_EQ(mesh.quadCount(), 6u);
}

TEST_F(ChunkMesherFixture, SeparatedBlocksAreNotMerged) {
	const Registry r;
	Chunk chunk;
	chunk.setBlock(0, 0, 0, r.stone);
	chunk.setBlock(2, 0, 0, r.stone);
	const ChunkMesh mesh = ChunkMesher::mesh(chunk, r.reg);
	// Two separate cubes with a gap: no merging across the gap, two 6-quad boxes.
	EXPECT_EQ(mesh.quadCount(), 12u);
}

TEST_F(ChunkMesherFixture, AllFacesWindOutward) {
	const Registry r;
	Chunk chunk;
	chunk.setBlock(3, 4, 5, r.stone);
	const ChunkMesh mesh = ChunkMesher::mesh(chunk, r.reg);
	ASSERT_EQ(mesh.quadCount(), 6u);
	for (size_t q = 0; q < mesh.quadCount(); ++q) {
		const auto& v0 = mesh.vertices[mesh.indices[q * 6 + 0]];
		const auto& v1 = mesh.vertices[mesh.indices[q * 6 + 1]];
		const auto& v2 = mesh.vertices[mesh.indices[q * 6 + 2]];
		const math::vec3 e1 = v1.position - v0.position;
		const math::vec3 e2 = v2.position - v0.position;
		const math::vec3 cross{e1.y() * e2.z() - e1.z() * e2.y(), e1.z() * e2.x() - e1.x() * e2.z(),
							   e1.x() * e2.y() - e1.y() * e2.x()};
		const float dot = cross.x() * v0.normal.x() + cross.y() * v0.normal.y() + cross.z() * v0.normal.z();
		EXPECT_GT(dot, 0.f) << "quad " << q << " winds inward";
	}
}

TEST_F(ChunkMesherFixture, FaceTexturesPerFace) {
	const Registry r;
	Chunk chunk;
	chunk.setBlock(0, 0, 0, r.marker);
	const ChunkMesh mesh = ChunkMesher::mesh(chunk, r.reg);
	const auto texOfFace = [&](const float iX, const float iY, const float iZ) -> uint32_t {
		for (size_t q = 0; q * 4 < mesh.vertices.size(); ++q) {
			if (normalIs(mesh.vertices[q * 4], iX, iY, iZ))
				return mesh.vertices[q * 4].textureIndex;
		}
		return 0;
	};
	EXPECT_EQ(texOfFace(-1, 0, 0), 100u);// XNeg
	EXPECT_EQ(texOfFace(1, 0, 0), 101u);// XPos
	EXPECT_EQ(texOfFace(0, -1, 0), 102u);// YNeg
	EXPECT_EQ(texOfFace(0, 1, 0), 103u);// YPos
	EXPECT_EQ(texOfFace(0, 0, -1), 104u);// ZNeg
	EXPECT_EQ(texOfFace(0, 0, 1), 105u);// ZPos
}

TEST_F(ChunkMesherFixture, TransparentNeighborDoesNotCull) {
	const Registry r;
	Chunk chunk;
	chunk.setBlock(0, 0, 0, r.stone);
	chunk.setBlock(1, 0, 0, r.glass);
	const ChunkMesh mesh = ChunkMesher::mesh(chunk, r.reg);
	// Stone's +X face (toward the transparent glass) must still be emitted at x = 1.
	bool foundInterface = false;
	for (size_t q = 0; q * 4 < mesh.vertices.size(); ++q) {
		if (normalIs(mesh.vertices[q * 4], 1, 0, 0) && fEq(mesh.vertices[q * 4].position.x(), 1.f))
			foundInterface = true;
	}
	EXPECT_TRUE(foundInterface);
}

TEST_F(ChunkMesherFixture, FlatSurfaceIsFullyLit) {
	const Registry r;
	Chunk chunk;
	chunk.setBlock(5, 5, 5, r.stone);
	const ChunkMesh mesh = ChunkMesher::mesh(chunk, r.reg);
	ASSERT_FALSE(mesh.vertices.empty());
	for (const auto& v: mesh.vertices) EXPECT_TRUE(fEq(v.ao, 1.f)) << "isolated block must have no ambient occlusion";
}

TEST_F(ChunkMesherFixture, DiagonalNeighborOccludesCorner) {
	const Registry r;
	Chunk chunk;
	chunk.setBlock(0, 0, 0, r.stone);
	chunk.setBlock(0, 1, 1, r.stone);// diagonally above-and-forward: occludes the +Y face corner toward +Z
	const ChunkMesh mesh = ChunkMesher::mesh(chunk, r.reg);
	bool darkened = false;
	for (const auto& v: mesh.vertices) {
		if (normalIs(v, 0, 1, 0) && v.ao < 0.999f)
			darkened = true;
	}
	EXPECT_TRUE(darkened) << "a diagonal neighbour must darken the lower block's top face";
}

TEST_F(ChunkMesherFixture, OcclusionBreaksGreedyMerge) {
	const Registry r;
	Chunk flat;
	Chunk walled;
	for (int32_t z = 0; z < static_cast<int32_t>(g_ChunkSize); ++z) {
		for (int32_t x = 0; x < static_cast<int32_t>(g_ChunkSize); ++x) {
			flat.setBlock(x, 0, z, r.stone);
			walled.setBlock(x, 0, z, r.stone);
		}
	}
	walled.setBlock(8, 1, 8, r.stone);// a pillar on the floor casts AO on the surrounding top faces
	EXPECT_EQ(quadsByNormal(ChunkMesher::mesh(flat, r.reg), 0, 1, 0), 1u);
	EXPECT_GT(quadsByNormal(ChunkMesher::mesh(walled, r.reg), 0, 1, 0), 1u);
}

TEST_F(ChunkMesherFixture, MeshByKindSeparatesOpaqueAndTransparent) {
	const Registry r;
	Chunk chunk;
	chunk.setBlock(0, 0, 0, r.stone);
	chunk.setBlock(5, 0, 0, r.glass);
	const ChunkMeshSet set =
			ChunkMesher::meshByKind(chunk, r.reg, [](int32_t, int32_t, int32_t) -> BlockId { return g_AirBlock; });
	EXPECT_EQ(set.opaque.quadCount(), 6u);
	EXPECT_EQ(set.transparent.quadCount(), 6u);
	for (const auto& v: set.opaque.vertices) EXPECT_EQ(v.textureIndex, 1u);// stone face texture
	for (const auto& v: set.transparent.vertices) EXPECT_EQ(v.textureIndex, 5u);// glass face texture
}

TEST_F(ChunkMesherFixture, NeighborProviderCullsBoundaryFace) {
	const Registry r;
	const auto size = static_cast<int32_t>(g_ChunkSize);
	Chunk chunk;
	chunk.setBlock(size - 1, 5, 5, r.stone);
	const ChunkMesh isolated = ChunkMesher::mesh(chunk, r.reg);
	EXPECT_EQ(quadsByNormal(isolated, 1, 0, 0), 1u);
	const ChunkMesh bounded = ChunkMesher::mesh(chunk, r.reg, [&](const int32_t iX, int32_t, int32_t) -> BlockId {
		return iX >= size ? r.stone : g_AirBlock;
	});
	// The +X face at the chunk boundary is now culled by the opaque neighbour.
	EXPECT_EQ(quadsByNormal(bounded, 1, 0, 0), 0u);
}
