/**
 * @file HudDocument_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/HudDocument.h"
#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/Canvas.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/Tag.h>
#include <scene/component/UiPanel.h>
#include <scene/component/UiRect.h>
#include <scene/ui/HudLayout.h>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::nest;
using scene::component::UiRect;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class HudDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		auto canvas = m_scene->createEntity("HUD");
		canvas.addComponent<scene::component::Canvas>();
		m_canvas = canvas.getUUID();
		m_panel = makeWidget("Panel", canvas, {200.f, 100.f}, {0.5f, 0.5f}, {0.f, 0.f}).getUUID();
		panelEntity().addComponent<scene::component::UiPanel>();
		m_other = makeWidget("Other", canvas, {100.f, 50.f}, {0.f, 0.f}, {100.f, 100.f}, {0.f, 0.f}).getUUID();
		m_document.refreshLayout(*m_scene);
	}

	void TearDown() override { core::Log::invalidate(); }

	auto makeWidget(const std::string& iName, const scene::Entity& iParent, const math::vec2& iSize,
					const math::vec2& iAnchor, const math::vec2& iOffset, const math::vec2& iPivot = {0.5f, 0.5f})
			-> scene::Entity {
		auto widget = m_scene->createEntity(iName);
		m_scene->setParent(widget, iParent);
		auto& rect = widget.addComponent<UiRect>();
		rect.size = iSize;
		rect.anchorMin = iAnchor;
		rect.anchorMax = iAnchor;
		rect.anchorOffset = iOffset;
		rect.pivot = iPivot;
		return widget;
	}

	[[nodiscard]] auto panelEntity() const -> scene::Entity { return m_scene->findEntityByUUID(m_panel); }

	[[nodiscard]] auto rectOf(const core::UUID iEntity) const -> UiRect {
		return m_scene->findEntityByUUID(iEntity).getComponent<UiRect>();
	}

	[[nodiscard]] auto boundsOf(const core::UUID iEntity) const -> UiRect::Bounds {
		m_document.refreshLayout(*m_scene);
		const auto entity = m_scene->findEntityByUUID(iEntity);
		return m_scene->getHudLayout().findNode(static_cast<entt::entity>(entity))->bounds;
	}

	[[nodiscard]] auto target() -> commands::CommandTarget {
		return {.registry = &m_registry, .scene = m_scene, .undo = &m_undo};
	}

	auto drag(const HudHandle iHandle, const math::vec2& iFrom, const math::vec2& iTo, const bool iSnap = true)
			-> bool {
		EXPECT_TRUE(m_document.beginDrag(*m_scene, iHandle, iFrom));
		m_document.updateDrag(*m_scene, iTo, iSnap);
		return m_document.endDrag(*m_scene, target());
	}

	/// Panel, Other.
	shared<scene::Scene> m_scene = mkShared<scene::Scene>();
	commands::CommandRegistry m_registry;
	SceneUndoManager m_undo;
	mutable HudDocument m_document;
	core::UUID m_canvas{0};
	core::UUID m_panel{0};
	core::UUID m_other{0};
};
OWL_DIAG_POP

}// namespace

TEST(HudSnapping, SticksToTheClosestCandidateWithinTheThreshold) {
	const std::array<float, 3> values{100.f, 150.f, 200.f};
	const std::array<float, 3> candidates{98.f, 205.f, 400.f};
	const auto stuck = snapToCandidates(values, candidates, 6.f);
	ASSERT_TRUE(stuck.has_value());
	EXPECT_FLOAT_EQ(stuck->first, -2.f);
	EXPECT_FLOAT_EQ(stuck->second, 98.f);
	EXPECT_FALSE(snapToCandidates(values, candidates, 1.f).has_value());
	EXPECT_FALSE(snapToCandidates(values, {}, 6.f).has_value());
}

TEST(HudSnapping, RoundsToTheGrid) {
	EXPECT_FLOAT_EQ(snapToGrid(10.4f, 1.f), 10.f);
	EXPECT_FLOAT_EQ(snapToGrid(10.6f, 1.f), 11.f);
	EXPECT_FLOAT_EQ(snapToGrid(13.f, 8.f), 16.f);
	EXPECT_FLOAT_EQ(snapToGrid(13.3f, 0.f), 13.3f);
}

TEST_F(HudDocumentTest, ResolutionIsClampedAndMatchesAPreset) {
	EXPECT_EQ(m_document.getResolution(), math::vec2ui(1280, 720));
	EXPECT_EQ(m_document.findPreset(), std::optional<size_t>{1});
	m_document.setResolution({1920, 1080});
	EXPECT_EQ(m_document.findPreset(), std::optional<size_t>{2});
	m_document.setResolution({1000, 3});
	EXPECT_EQ(m_document.getResolution(), math::vec2ui(1000, HudDocument::minResolution));
	EXPECT_FALSE(m_document.findPreset().has_value());
	m_document.setResolution({99999, 99999});
	EXPECT_EQ(m_document.getResolution(), math::vec2ui(HudDocument::maxResolution, HudDocument::maxResolution));
}

TEST_F(HudDocumentTest, BackdropKeepsItsColourAndImage) {
	EXPECT_EQ(m_document.getBackdropKind(), HudBackdropKind::Color);
	m_document.setBackdropKind(HudBackdropKind::Image);
	m_document.setBackdropImage("shots/level1.png");
	m_document.setBackdropColor({1.f, 0.f, 0.f, 1.f});
	EXPECT_EQ(m_document.getBackdropKind(), HudBackdropKind::Image);
	EXPECT_EQ(m_document.getBackdropImage(), "shots/level1.png");
	EXPECT_FLOAT_EQ(m_document.getBackdropColor().x(), 1.f);
}

TEST_F(HudDocumentTest, SelectionModes) {
	m_document.select(m_panel);
	m_document.select(m_other, HudSelectMode::Add);
	EXPECT_EQ(m_document.getSelection().size(), 2u);
	EXPECT_EQ(m_document.getPrimary(), m_other);
	m_document.select(m_other, HudSelectMode::Add);
	EXPECT_EQ(m_document.getSelection().size(), 2u);
	m_document.select(m_other, HudSelectMode::Toggle);
	EXPECT_EQ(m_document.getSelection().size(), 1u);
	EXPECT_TRUE(m_document.isSelected(m_panel));
	m_document.select(m_other);
	EXPECT_EQ(m_document.getSelection().size(), 1u);
	EXPECT_EQ(m_document.getPrimary(), m_other);
	m_document.select(core::UUID{0});
	EXPECT_TRUE(m_document.getSelection().empty());
	EXPECT_EQ(m_document.getPrimary(), core::UUID{0});
}

TEST_F(HudDocumentTest, MakePrimaryMovesAWidgetLastOrAddsIt) {
	m_document.setSelection({m_panel, m_other});
	m_document.makePrimary(m_panel);
	EXPECT_EQ(m_document.getSelection().size(), 2u);
	EXPECT_EQ(m_document.getPrimary(), m_panel);
	m_document.select(m_other);
	m_document.makePrimary(m_panel);
	EXPECT_EQ(m_document.getSelection().size(), 2u);
	EXPECT_EQ(m_document.getPrimary(), m_panel);
	m_document.makePrimary(core::UUID{0});
	EXPECT_EQ(m_document.getPrimary(), m_panel);
}

TEST_F(HudDocumentTest, PruneForgetsDeletedWidgets) {
	m_document.setSelection({m_panel, m_other});
	auto other = m_scene->findEntityByUUID(m_other);
	m_scene->destroyEntity(other);
	m_document.pruneSelection(*m_scene);
	ASSERT_EQ(m_document.getSelection().size(), 1u);
	EXPECT_EQ(m_document.getSelection().front(), m_panel);
}

TEST_F(HudDocumentTest, PicksTheWidgetOnTopAndSelectsInABox) {
	EXPECT_EQ(HudDocument::pickWidget(*m_scene, {600.f, 330.f}), m_panel);
	EXPECT_EQ(HudDocument::pickWidget(*m_scene, {120.f, 120.f}), m_other);
	EXPECT_EQ(HudDocument::pickWidget(*m_scene, {5.f, 5.f}), core::UUID{0});
	EXPECT_EQ(m_document.selectInBox(*m_scene, {.position = {500.f, 300.f}, .size = {260.f, 120.f}}, false), 1u);
	EXPECT_EQ(m_document.getPrimary(), m_panel);
	EXPECT_EQ(m_document.selectInBox(*m_scene, {.position = {90.f, 90.f}, .size = {30.f, 30.f}}, true), 1u);
	EXPECT_EQ(m_document.getSelection().size(), 2u);
	EXPECT_EQ(m_document.selectInBox(*m_scene, {.position = {0.f, 600.f}, .size = {50.f, 50.f}}, false), 0u);
	EXPECT_TRUE(m_document.getSelection().empty());
}

TEST_F(HudDocumentTest, GizmoGivesTheRectPivotAndAnchors) {
	EXPECT_FALSE(m_document.getGizmo(*m_scene).has_value());
	m_document.select(m_other);
	const auto gizmo = m_document.getGizmo(*m_scene);
	ASSERT_TRUE(gizmo.has_value());
	EXPECT_FLOAT_EQ(gizmo->bounds.position.x(), 100.f);
	EXPECT_FLOAT_EQ(gizmo->pivot.x(), 100.f);
	EXPECT_FLOAT_EQ(gizmo->anchorMin.x(), 0.f);
	EXPECT_FALSE(gizmo->stretched);
	m_document.select(m_panel);
	const auto centred = m_document.getGizmo(*m_scene);
	ASSERT_TRUE(centred.has_value());
	EXPECT_FLOAT_EQ(centred->pivot.x(), 640.f);
	EXPECT_FLOAT_EQ(centred->anchorMin.y(), 360.f);
}

TEST_F(HudDocumentTest, HitHandlesFollowTheGizmo) {
	m_document.select(m_panel);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {640.f, 360.f}, 6.f), HudHandle::Body);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {640.f, 360.f}, 6.f, HudGrabMode::Pivot), HudHandle::Pivot);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {640.f, 360.f}, 6.f, HudGrabMode::Anchors), HudHandle::AnchorBoth);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {540.f, 310.f}, 6.f), HudHandle::TopLeft);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {741.f, 410.f}, 6.f), HudHandle::BottomRight);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {640.f, 308.f}, 6.f), HudHandle::Top);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {640.f, 412.f}, 6.f), HudHandle::Bottom);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {538.f, 360.f}, 6.f), HudHandle::Left);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {742.f, 360.f}, 6.f), HudHandle::Right);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {20.f, 600.f}, 6.f), HudHandle::None);
}

TEST_F(HudDocumentTest, AnAnchorOutsideTheWidgetIsGrabbedWithoutModifier) {
	m_document.select(m_other);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {0.f, 0.f}, 6.f), HudHandle::AnchorBoth);
	m_document.select(m_other);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {150.f, 125.f}, 6.f), HudHandle::Body);
}

TEST_F(HudDocumentTest, SeveralSelectedWidgetsOnlyOfferTheBody) {
	m_document.setSelection({m_panel, m_other});
	EXPECT_EQ(m_document.hitHandle(*m_scene, {540.f, 310.f}, 6.f), HudHandle::Body);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {120.f, 120.f}, 6.f), HudHandle::Body);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {20.f, 600.f}, 6.f), HudHandle::None);
}

TEST_F(HudDocumentTest, MovingPreviewsThenRunsOneUndoableCommand) {
	m_document.select(m_panel);
	ASSERT_TRUE(m_document.beginDrag(*m_scene, HudHandle::Body, {600.f, 330.f}));
	EXPECT_TRUE(m_document.isDragging());
	m_document.updateDrag(*m_scene, {651.2f, 340.f}, true);
	EXPECT_FLOAT_EQ(rectOf(m_panel).anchorOffset.x(), 51.f);
	EXPECT_FLOAT_EQ(rectOf(m_panel).anchorOffset.y(), 10.f);
	EXPECT_FALSE(m_undo.canUndo());
	ASSERT_TRUE(m_document.endDrag(*m_scene, target()));
	EXPECT_FALSE(m_document.isDragging());
	EXPECT_FLOAT_EQ(rectOf(m_panel).anchorOffset.x(), 51.f);
	ASSERT_TRUE(m_undo.canUndo());
	m_undo.undo(*m_scene);
	EXPECT_FLOAT_EQ(rectOf(m_panel).anchorOffset.x(), 0.f);
}

TEST_F(HudDocumentTest, AClickWithoutMovingChangesNothing) {
	m_document.select(m_panel);
	EXPECT_FALSE(drag(HudHandle::Body, {600.f, 330.f}, {600.5f, 330.5f}));
	EXPECT_FALSE(m_undo.canUndo());
	EXPECT_FLOAT_EQ(rectOf(m_panel).anchorOffset.x(), 0.f);
}

TEST_F(HudDocumentTest, MovingSticksToTheEdgesOfTheSiblingsAndShowsTheGuide) {
	m_document.select(m_panel);
	ASSERT_TRUE(m_document.beginDrag(*m_scene, HudHandle::Body, {600.f, 330.f}));
	m_document.updateDrag(*m_scene, {262.f, 330.f}, true);
	EXPECT_FLOAT_EQ(rectOf(m_panel).anchorOffset.x(), -340.f);
	ASSERT_FALSE(m_document.getGuidesX().empty());
	EXPECT_FLOAT_EQ(m_document.getGuidesX().front(), 200.f);
	m_document.cancelDrag(*m_scene);
	EXPECT_FLOAT_EQ(rectOf(m_panel).anchorOffset.x(), 0.f);
	EXPECT_TRUE(m_document.getGuidesX().empty());
}

TEST_F(HudDocumentTest, HoldingTheModifierTurnsSnappingOff) {
	m_document.select(m_panel);
	ASSERT_TRUE(drag(HudHandle::Body, {600.f, 330.f}, {261.6f, 330.f}, false));
	EXPECT_NEAR(rectOf(m_panel).anchorOffset.x(), -338.4f, 1e-3f);
}

TEST_F(HudDocumentTest, SnappingCanBeSwitchedOffInTheSettings) {
	m_document.getSnap().pixelGrid = false;
	m_document.getSnap().edges = false;
	m_document.select(m_panel);
	ASSERT_TRUE(drag(HudHandle::Body, {600.f, 330.f}, {261.6f, 330.3f}));
	EXPECT_NEAR(rectOf(m_panel).anchorOffset.x(), -338.4f, 1e-3f);
	EXPECT_NEAR(rectOf(m_panel).anchorOffset.y(), 0.3f, 1e-3f);
}

TEST_F(HudDocumentTest, MovingAGroupShiftsEveryWidgetByTheSameDistance) {
	m_document.setSelection({m_other, m_panel});
	ASSERT_TRUE(drag(HudHandle::Body, {600.f, 330.f}, {620.f, 340.f}));
	EXPECT_FLOAT_EQ(rectOf(m_panel).anchorOffset.x(), 20.f);
	EXPECT_FLOAT_EQ(rectOf(m_other).anchorOffset.x(), 120.f);
	EXPECT_FLOAT_EQ(rectOf(m_other).anchorOffset.y(), 110.f);
	m_undo.undo(*m_scene);
	EXPECT_FLOAT_EQ(rectOf(m_panel).anchorOffset.x(), 0.f);
	EXPECT_FLOAT_EQ(rectOf(m_other).anchorOffset.x(), 100.f);
	EXPECT_FALSE(m_undo.canUndo());
}

TEST_F(HudDocumentTest, ResizingAnEdgeKeepsTheOppositeEdge) {
	m_document.select(m_panel);
	ASSERT_TRUE(drag(HudHandle::Right, {741.f, 360.f}, {771.4f, 360.f}));
	const auto bounds = boundsOf(m_panel);
	EXPECT_FLOAT_EQ(bounds.position.x(), 540.f);
	EXPECT_FLOAT_EQ(bounds.size.x(), 230.f);
	EXPECT_FLOAT_EQ(rectOf(m_panel).size.x(), 230.f);
	EXPECT_FLOAT_EQ(rectOf(m_panel).anchorOffset.x(), 15.f);
	EXPECT_FLOAT_EQ(bounds.size.y(), 100.f);
}

TEST_F(HudDocumentTest, ResizingACornerMovesBothEdgesAndStaysAboveAMinimumSize) {
	m_document.select(m_panel);
	ASSERT_TRUE(drag(HudHandle::TopLeft, {540.f, 310.f}, {900.f, 700.f}));
	const auto bounds = boundsOf(m_panel);
	EXPECT_GT(bounds.size.x(), 0.f);
	EXPECT_GT(bounds.size.y(), 0.f);
	EXPECT_FLOAT_EQ(bounds.position.x() + bounds.size.x(), 740.f);
	EXPECT_FLOAT_EQ(bounds.position.y() + bounds.size.y(), 410.f);
}

TEST_F(HudDocumentTest, ResizingSticksToTheSiblingEdges) {
	m_document.select(m_panel);
	ASSERT_TRUE(drag(HudHandle::Left, {540.f, 360.f}, {203.f, 360.f}));
	EXPECT_FLOAT_EQ(boundsOf(m_panel).position.x(), 200.f);
}

TEST_F(HudDocumentTest, DraggingThePivotKeepsTheWidgetInPlace) {
	m_document.select(m_panel);
	ASSERT_TRUE(drag(HudHandle::Pivot, {640.f, 360.f}, {541.f, 311.f}));
	const auto rect = rectOf(m_panel);
	EXPECT_FLOAT_EQ(rect.pivot.x(), 0.f);
	EXPECT_FLOAT_EQ(rect.pivot.y(), 0.f);
	const auto bounds = boundsOf(m_panel);
	EXPECT_FLOAT_EQ(bounds.position.x(), 540.f);
	EXPECT_FLOAT_EQ(bounds.position.y(), 310.f);
	EXPECT_FLOAT_EQ(rect.anchorOffset.x(), -100.f);
}

TEST_F(HudDocumentTest, DraggingAnAnchorKeepsTheWidgetInPlace) {
	m_document.select(m_panel);
	ASSERT_TRUE(drag(HudHandle::AnchorBoth, {640.f, 360.f}, {3.f, 4.f}));
	const auto rect = rectOf(m_panel);
	EXPECT_EQ(rect.getAnchor(), UiRect::Anchor::TopLeft);
	const auto bounds = boundsOf(m_panel);
	EXPECT_FLOAT_EQ(bounds.position.x(), 540.f);
	EXPECT_FLOAT_EQ(bounds.position.y(), 310.f);
	EXPECT_FLOAT_EQ(rect.anchorOffset.x(), 640.f);
}

TEST_F(HudDocumentTest, DraggingTheTwoAnchorsOfAStretchedWidgetKeepsItsBounds) {
	m_document.select(m_panel);
	ASSERT_TRUE(drag(HudHandle::AnchorBoth, {640.f, 360.f}, {0.f, 360.f}));
	m_document.refreshLayout(*m_scene);
	ASSERT_TRUE(drag(HudHandle::AnchorMax, {0.f, 360.f}, {1280.f, 360.f}));
	const auto rect = rectOf(m_panel);
	EXPECT_NE(rect.anchorMin, rect.anchorMax);
	EXPECT_FLOAT_EQ(rect.anchorMax.x(), 1.f);
	const auto bounds = boundsOf(m_panel);
	EXPECT_FLOAT_EQ(bounds.position.x(), 540.f);
	EXPECT_FLOAT_EQ(bounds.size.x(), 200.f);
	const auto gizmo = m_document.getGizmo(*m_scene);
	ASSERT_TRUE(gizmo.has_value());
	EXPECT_TRUE(gizmo->stretched);
	EXPECT_EQ(m_document.hitHandle(*m_scene, {1280.f, 360.f}, 6.f), HudHandle::AnchorMax);
}

TEST_F(HudDocumentTest, AGestureNeedsAHandleAndASelection) {
	EXPECT_FALSE(m_document.beginDrag(*m_scene, HudHandle::Body, {600.f, 330.f}));
	m_document.select(m_panel);
	EXPECT_FALSE(m_document.beginDrag(*m_scene, HudHandle::None, {600.f, 330.f}));
	EXPECT_FALSE(m_document.endDrag(*m_scene, target()));
	m_document.updateDrag(*m_scene, {1.f, 1.f}, true);
	EXPECT_FALSE(m_document.isDragging());
}

TEST_F(HudDocumentTest, AddWidgetPutsItInTheContainerUnderThePoint) {
	const auto added = m_document.addWidget(*m_scene, target(), "label", {600.f, 330.f});
	ASSERT_TRUE(added.has_value()) << added.error().message;
	EXPECT_EQ(m_document.getPrimary(), *added);
	const auto label = m_scene->findEntityByUUID(*added);
	EXPECT_EQ(label.getComponent<scene::component::Hierarchy>().parentId, m_panel);
	const auto bounds = boundsOf(*added);
	EXPECT_FLOAT_EQ(bounds.getCenter().x(), 600.f);
	EXPECT_FLOAT_EQ(bounds.getCenter().y(), 330.f);
	m_undo.undo(*m_scene);
	EXPECT_FALSE(m_scene->findEntityByUUID(*added));
}

TEST_F(HudDocumentTest, AddWidgetOnEmptySpaceGoesUnderTheCanvas) {
	const auto added = m_document.addWidget(*m_scene, target(), "button", {50.f, 650.f});
	ASSERT_TRUE(added.has_value()) << added.error().message;
	const auto button = m_scene->findEntityByUUID(*added);
	EXPECT_EQ(button.getComponent<scene::component::Hierarchy>().parentId, m_canvas);
	EXPECT_FLOAT_EQ(boundsOf(*added).getCenter().x(), 50.f);
	EXPECT_FLOAT_EQ(boundsOf(*added).getCenter().y(), 650.f);
}

TEST_F(HudDocumentTest, AddWidgetUnderAScaledCanvasUsesReferencePixels) {
	m_document.setResolution({1920, 1080});
	const auto added = m_document.addWidget(*m_scene, target(), "slider", {300.f, 900.f});
	ASSERT_TRUE(added.has_value()) << added.error().message;
	EXPECT_FLOAT_EQ(rectOf(*added).anchorOffset.x(), 200.f);
	EXPECT_FLOAT_EQ(rectOf(*added).anchorOffset.y(), 600.f);
	EXPECT_FLOAT_EQ(boundsOf(*added).getCenter().x(), 300.f);
}

TEST_F(HudDocumentTest, AddWidgetInAnEmptySceneCreatesTheCanvas) {
	auto empty = mkShared<scene::Scene>();
	SceneUndoManager undo;
	const commands::CommandTarget emptyTarget{.registry = &m_registry, .scene = empty, .undo = &undo};
	HudDocument document;
	const auto added = document.addWidget(*empty, emptyTarget, "panel", {100.f, 100.f});
	ASSERT_TRUE(added.has_value()) << added.error().message;
	EXPECT_EQ(empty->registry.view<scene::component::Canvas>().size(), 1u);
	document.refreshLayout(*empty);
	const auto entity = empty->findEntityByUUID(*added);
	EXPECT_FLOAT_EQ(empty->getHudLayout().findNode(static_cast<entt::entity>(entity))->bounds.getCenter().x(), 100.f);
}

TEST_F(HudDocumentTest, AddWidgetRefusesAnUnknownKind) {
	const auto added = m_document.addWidget(*m_scene, target(), "sprite", {10.f, 10.f});
	ASSERT_FALSE(added.has_value());
	EXPECT_EQ(added.error().error, commands::CommandError::InvalidArgument);
}

TEST_F(HudDocumentTest, DeleteSelectionRemovesTheGroupAsOneStep) {
	m_document.setSelection({m_panel, m_other});
	ASSERT_TRUE(m_document.deleteSelection(target()));
	EXPECT_TRUE(m_document.getSelection().empty());
	EXPECT_FALSE(m_scene->findEntityByUUID(m_panel));
	EXPECT_FALSE(m_scene->findEntityByUUID(m_other));
	m_undo.undo(*m_scene);
	EXPECT_TRUE(m_scene->findEntityByUUID(m_panel));
	EXPECT_TRUE(m_scene->findEntityByUUID(m_other));
	EXPECT_FALSE(m_document.deleteSelection(target()));
}

TEST_F(HudDocumentTest, MoveSelectionRunsHudMove) {
	m_document.setSelection({m_panel, m_other});
	ASSERT_TRUE(m_document.moveSelection(target(), {4.f, -2.f}));
	EXPECT_FLOAT_EQ(rectOf(m_other).anchorOffset.x(), 104.f);
	EXPECT_FLOAT_EQ(rectOf(m_panel).anchorOffset.y(), -2.f);
	EXPECT_FALSE(HudDocument{}.moveSelection(target(), {1.f, 1.f}));
}

TEST_F(HudDocumentTest, ReorderChangesTheDrawOrder) {
	m_document.select(m_panel);
	EXPECT_EQ(HudDocument::pickWidget(*m_scene, {600.f, 330.f}), m_panel);
	ASSERT_TRUE(m_document.reorderPrimary(target(), 1));
	const auto& children = m_scene->findEntityByUUID(m_canvas).getComponent<scene::component::Hierarchy>().childrenIds;
	ASSERT_EQ(children.size(), 2u);
	EXPECT_EQ(children.back(), m_panel);
	EXPECT_FALSE(m_document.reorderPrimary(target(), 5));
	ASSERT_TRUE(m_document.reorderPrimary(target(), -1));
	EXPECT_EQ(m_scene->findEntityByUUID(m_canvas).getComponent<scene::component::Hierarchy>().childrenIds.front(),
			  m_panel);
}

TEST_F(HudDocumentTest, ReparentPrimaryKeepsTheWidgetOnScreen) {
	m_document.select(m_other);
	const auto before = boundsOf(m_other);
	ASSERT_TRUE(m_document.reparentPrimary(target(), m_panel));
	EXPECT_EQ(m_scene->findEntityByUUID(m_other).getComponent<scene::component::Hierarchy>().parentId, m_panel);
	const auto after = boundsOf(m_other);
	EXPECT_FLOAT_EQ(after.position.x(), before.position.x());
	EXPECT_FLOAT_EQ(after.position.y(), before.position.y());
	m_undo.undo(*m_scene);
	EXPECT_EQ(m_scene->findEntityByUUID(m_other).getComponent<scene::component::Hierarchy>().parentId, m_canvas);
}
