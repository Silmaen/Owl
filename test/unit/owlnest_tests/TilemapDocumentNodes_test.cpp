/**
 * @file TilemapDocumentNodes_test.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/TilemapDocument.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <scene/TilemapTerrain.h>

#include <cstdint>
#include <tuple>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::test;

namespace {

class TilemapDocumentNodesTest : public NestTest {
protected:
	void SetUp() override {
		NestTest::SetUp();
		m_document.asset().width = 16;
		m_document.asset().height = 16;
		m_document.asset().addLayer("ground").fill(1);
	}

	/// Whether every cell of the map has one of the 19 slopes.
	[[nodiscard]] auto allSlopesValid() const -> bool {
		const auto& asset = m_document.asset();
		for (uint32_t y = 0; y < asset.height; ++y) {
			for (uint32_t x = 0; x < asset.width; ++x) {
				if (!scene::terrain::deriveSlope(asset.getCellCorners(x, y)))
					return false;
			}
		}
		return true;
	}

	TilemapDocument m_document;
};

}// namespace

TEST_F(TilemapDocumentNodesTest, CellCornerNodes) {
	EXPECT_EQ(TilemapDocument::getCellCornerNode(3, 4, 0), (math::vec2ui{3, 4}));
	EXPECT_EQ(TilemapDocument::getCellCornerNode(3, 4, 1), (math::vec2ui{4, 4}));
	EXPECT_EQ(TilemapDocument::getCellCornerNode(3, 4, 2), (math::vec2ui{4, 5}));
	EXPECT_EQ(TilemapDocument::getCellCornerNode(3, 4, 3), (math::vec2ui{3, 5}));
}

TEST_F(TilemapDocumentNodesTest, SelectionToggleAndBounds) {
	m_document.selectNode({2, 2}, false);
	m_document.selectNode({3, 2}, true);
	EXPECT_EQ(m_document.getSelectedNodes().size(), 2u);
	m_document.selectNode({3, 2}, true);
	ASSERT_EQ(m_document.getSelectedNodes().size(), 1u);
	EXPECT_EQ(m_document.getSelectedNodes()[0], (math::vec2ui{2, 2}));
	m_document.selectNode({17, 0}, true);
	EXPECT_EQ(m_document.getSelectedNodes().size(), 1u);
	m_document.selectNode({16, 16}, false);
	EXPECT_EQ(m_document.getSelectedNodes().size(), 1u);
	m_document.clearNodeSelection();
	EXPECT_TRUE(m_document.getSelectedNodes().empty());
}

TEST_F(TilemapDocumentNodesTest, SetNodeHeightAdaptsTheNeighboursAndUndoes) {
	const scene::TilemapAsset saved = m_document.asset();
	EXPECT_TRUE(m_document.setNodeHeight(8, 8, 3));
	const auto& asset = m_document.asset();
	EXPECT_EQ(asset.getCornerHeight(8, 8), 3);
	EXPECT_EQ(asset.getCornerHeight(7, 8), 2);
	EXPECT_EQ(asset.getCornerHeight(6, 8), 1);
	EXPECT_TRUE(allSlopesValid());
	EXPECT_EQ(m_document.tilemapUndoManager().undoDescription(), "Set node height");
	m_document.performUndo();
	EXPECT_TRUE(asset.hasSameContent(saved));
	EXPECT_FALSE(m_document.canUndo());
	m_document.performRedo();
	EXPECT_EQ(asset.getCornerHeight(8, 8), 3);
	EXPECT_EQ(asset.getCornerHeight(7, 8), 2);
	EXPECT_TRUE(allSlopesValid());
}

TEST_F(TilemapDocumentNodesTest, SameAsTheTerraformTools) {
	scene::TilemapAsset reference = m_document.asset();
	std::ignore = reference.setCornerHeight(5, 5, 2);
	EXPECT_TRUE(m_document.setNodeHeight(5, 5, 2));
	EXPECT_TRUE(m_document.asset().hasSameContent(reference));
}

TEST_F(TilemapDocumentNodesTest, UnchangedHeightPushesNothing) {
	EXPECT_FALSE(m_document.setNodeHeight(4, 4, 0));
	EXPECT_FALSE(m_document.canUndo());
	EXPECT_FALSE(m_document.setNodeHeight(40, 4, 2));
	EXPECT_FALSE(m_document.canUndo());
}

TEST_F(TilemapDocumentNodesTest, PreviewsMergeIntoOneUndoStep) {
	const scene::TilemapAsset saved = m_document.asset();
	EXPECT_TRUE(m_document.previewNodeHeight(6, 6, 1));
	EXPECT_TRUE(m_document.previewNodeHeight(6, 6, 2));
	EXPECT_TRUE(m_document.previewNodeHeight(6, 6, 3));
	EXPECT_FALSE(m_document.canUndo());
	EXPECT_TRUE(m_document.commitTerrainEdit("Edit corner heights"));
	EXPECT_FALSE(m_document.commitTerrainEdit("Edit corner heights"));
	EXPECT_EQ(m_document.asset().getCornerHeight(6, 6), 3);
	m_document.performUndo();
	EXPECT_TRUE(m_document.asset().hasSameContent(saved));
	EXPECT_FALSE(m_document.canUndo());
}

TEST_F(TilemapDocumentNodesTest, MoveSelectedNodesRaisesAndLowersTogether) {
	m_document.selectNode({4, 4}, false);
	m_document.selectNode({5, 4}, true);
	m_document.selectNode({10, 10}, true);
	const scene::TilemapAsset saved = m_document.asset();
	EXPECT_TRUE(m_document.moveSelectedNodes(2));
	const auto& asset = m_document.asset();
	EXPECT_EQ(asset.getCornerHeight(4, 4), 2);
	EXPECT_EQ(asset.getCornerHeight(5, 4), 2);
	EXPECT_EQ(asset.getCornerHeight(10, 10), 2);
	EXPECT_TRUE(allSlopesValid());
	EXPECT_EQ(m_document.tilemapUndoManager().undoDescription(), "Move nodes");
	EXPECT_TRUE(m_document.moveSelectedNodes(-1));
	EXPECT_EQ(asset.getCornerHeight(4, 4), 1);
	EXPECT_TRUE(m_document.moveSelectedNodes(-5));
	EXPECT_EQ(asset.getCornerHeight(10, 10), 0);
	EXPECT_FALSE(m_document.moveSelectedNodes(-1));
	m_document.performUndo();
	m_document.performUndo();
	m_document.performUndo();
	EXPECT_TRUE(asset.hasSameContent(saved));
	m_document.performRedo();
	m_document.performRedo();
	EXPECT_EQ(asset.getCornerHeight(4, 4), 1);
	EXPECT_TRUE(allSlopesValid());
}

TEST_F(TilemapDocumentNodesTest, NoSelectionMovesNothing) {
	EXPECT_FALSE(m_document.moveSelectedNodes(1));
	EXPECT_FALSE(m_document.canUndo());
}

TEST_F(TilemapDocumentNodesTest, CellCornerEditsDeriveTheSlope) {
	// Raising the back corner of cell (2, 2) makes it a "corner up" slope.
	const auto node = TilemapDocument::getCellCornerNode(2, 2, 0);
	EXPECT_TRUE(m_document.setNodeHeight(node.x(), node.y(), 1));
	const auto& asset = m_document.asset();
	EXPECT_NE(asset.getCellSlope(2, 2), scene::terrain::flatSlope);
	EXPECT_EQ(asset.getCellCorners(2, 2)[0], 1);
	EXPECT_EQ(asset.getCellCorners(1, 1)[2], 1);
	EXPECT_TRUE(allSlopesValid());
}
