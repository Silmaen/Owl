/**
 * @file HudLayout.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/ui/HudLayout.h"

#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/Canvas.h"
#include "scene/component/Hierarchy.h"
#include "scene/component/UiList.h"
#include "scene/component/UiPanel.h"
#include "scene/component/UiScrollView.h"
#include "scene/component/UiWindow.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <ranges>
#include <tuple>

namespace owl::scene::ui {

void HudLayout::compute(Scene& iScene, const math::vec2ui& iViewport, const bool iEditorMode) {
	OWL_PROFILE_FUNCTION()

	++m_computeCount;
	m_nodes.clear();
	m_scrolls.clear();
	m_viewport = iViewport;
	m_valid = true;
	m_screenReference = {0.f, 0.f};
	if (iViewport.x() == 0 || iViewport.y() == 0)
		return;
	m_canvases.clear();
	for (const auto view = iScene.registry.view<component::Canvas>(); const auto entity: view) {
		if (iScene.isEffectivelyVisible(Entity{entity, &iScene}, iEditorMode))
			m_canvases.emplace_back(view.get<component::Canvas>(entity).sortOrder, entity);
	}
	// The entity breaks the ties so the order does not depend on the storage order.
	std::ranges::sort(m_canvases);
	for (const auto& canvas: m_canvases | std::views::values) layoutCanvas(iScene, canvas, iEditorMode);
}

namespace {

auto alignOffset(const component::UiLayout::CrossAlign iAlign, const float iRoom, const float iSize) -> float {
	switch (iAlign) {
		case component::UiLayout::CrossAlign::Start:
			return 0.f;
		case component::UiLayout::CrossAlign::Center:
			return std::max(0.f, (iRoom - iSize) * 0.5f);
		case component::UiLayout::CrossAlign::End:
			return std::max(0.f, iRoom - iSize);
	}
	return 0.f;
}

// A window never leaves the screen; one larger than the screen keeps its top-left corner.
void clampToScreen(component::UiRect::Bounds& ioBounds, const math::vec2& iScreen) {
	ioBounds.position.x() = std::clamp(ioBounds.position.x(), 0.f, std::max(0.f, iScreen.x() - ioBounds.size.x()));
	ioBounds.position.y() = std::clamp(ioBounds.position.y(), 0.f, std::max(0.f, iScreen.y() - ioBounds.size.y()));
}

}// namespace

auto HudLayout::arrange(const component::UiLayout& iLayout, const math::vec2& iContent) -> math::vec2 {
	using Arrangement = component::UiLayout::Arrangement;
	m_slots.assign(m_layoutChildren.size(), {0.f, 0.f});
	const float pad = iLayout.padding;
	const math::vec2 inner{std::max(0.f, iContent.x() - 2.f * pad), std::max(0.f, iContent.y() - 2.f * pad)};
	math::vec2 cell = iLayout.cellSize;
	if (iLayout.arrangement == Arrangement::Grid) {
		for (const auto& child: m_layoutChildren | std::views::filter(&LayoutChild::placed)) {
			cell.x() = iLayout.cellSize.x() > 0.f ? cell.x() : std::max(cell.x(), child.size.x());
			cell.y() = iLayout.cellSize.y() > 0.f ? cell.y() : std::max(cell.y(), child.size.y());
		}
	}
	const size_t columns = std::max<size_t>(1, iLayout.columns);
	math::vec2 cursor{pad, pad};
	size_t placed = 0;
	for (size_t i = 0; i < m_layoutChildren.size(); ++i) {
		if (!m_layoutChildren[i].placed)
			continue;
		const auto& size = m_layoutChildren[i].size;
		switch (iLayout.arrangement) {
			case Arrangement::Row:
				m_slots[i] = {cursor.x(), pad + alignOffset(iLayout.crossAlign, inner.y(), size.y())};
				cursor.x() += size.x() + iLayout.spacing;
				break;
			case Arrangement::Column:
				m_slots[i] = {pad + alignOffset(iLayout.crossAlign, inner.x(), size.x()), cursor.y()};
				cursor.y() += size.y() + iLayout.spacing;
				break;
			case Arrangement::Grid:
				{
					const size_t column = placed % columns;
					const size_t row = placed / columns;
					const math::vec2 origin{pad + static_cast<float>(column) * (cell.x() + iLayout.spacing),
											pad + static_cast<float>(row) * (cell.y() + iLayout.spacing)};
					m_slots[i] = {origin.x() + alignOffset(iLayout.crossAlign, cell.x(), size.x()),
								  origin.y() + alignOffset(iLayout.crossAlign, cell.y(), size.y())};
					break;
				}
		}
		++placed;
	}
	if (placed == 0)
		return {0.f, 0.f};
	math::vec2 extent{0.f, 0.f};
	for (size_t i = 0; i < m_layoutChildren.size(); ++i) {
		if (!m_layoutChildren[i].placed)
			continue;
		extent.x() = std::max(extent.x(), m_slots[i].x() + m_layoutChildren[i].size.x());
		extent.y() = std::max(extent.y(), m_slots[i].y() + m_layoutChildren[i].size.y());
	}
	return {extent.x() + pad, extent.y() + pad};
}

void HudLayout::collectLayoutChildren(Scene& iScene, const entt::entity iEntity, const math::vec2& iContent,
									  const bool iEditorMode) {
	const component::UiRect::Frame frame{.parent = {.position = {0.f, 0.f}, .size = iContent},
										 .screen = m_screenReference};
	m_layoutChildren.clear();
	const auto* hierarchy = iScene.registry.try_get<component::Hierarchy>(iEntity);
	if (hierarchy == nullptr)
		return;
	for (const auto childId: hierarchy->childrenIds) {
		const Entity child = iScene.findEntityByUUID(childId);
		if (!child)
			continue;
		const auto entity = static_cast<entt::entity>(child);
		const auto* rect = iScene.registry.try_get<component::UiRect>(entity);
		const bool placed = rect != nullptr && !iScene.registry.all_of<component::Canvas>(entity) &&
							iScene.isEffectivelyVisible(child, iEditorMode);
		m_layoutChildren.push_back({.entity = entity,
									.size = placed ? rect->getPixelSize(frame) : math::vec2{0.f, 0.f},
									.placed = placed});
	}
}

auto HudLayout::measureLayout(Scene& iScene, const entt::entity iEntity, const math::vec2& iContent,
							  const bool iEditorMode) -> std::optional<math::vec2> {
	const auto* layout = iScene.registry.try_get<component::UiLayout>(iEntity);
	if (layout == nullptr)
		return std::nullopt;
	collectLayoutChildren(iScene, iEntity, iContent, iEditorMode);
	return arrange(*layout, iContent);
}

void HudLayout::pushChildren(Scene& iScene, const entt::entity iEntity, const size_t iParent, const bool iEditorMode) {
	const auto* hierarchy = iScene.registry.try_get<component::Hierarchy>(iEntity);
	if (hierarchy == nullptr)
		return;
	const auto* layout = iParent == HudNode::noParent ? nullptr : iScene.registry.try_get<component::UiLayout>(iEntity);
	if (layout == nullptr) {
		for (const auto childId: hierarchy->childrenIds | std::views::reverse) {
			if (const Entity child = iScene.findEntityByUUID(childId); child)
				m_pending.push_back({.entity = static_cast<entt::entity>(child), .parent = iParent});
		}
		return;
	}
	const auto& parent = m_nodes[iParent];
	const math::vec2 content = parent.content.size / parent.scale;
	collectLayoutChildren(iScene, iEntity, content, iEditorMode);
	std::ignore = arrange(*layout, content);
	for (size_t i = m_layoutChildren.size(); i-- > 0;)
		m_pending.push_back({.entity = m_layoutChildren[i].entity,
							 .parent = iParent,
							 .slotted = m_layoutChildren[i].placed,
							 .slot = m_slots[i]});
}

void HudLayout::layoutCanvas(Scene& iScene, const entt::entity iCanvas, const bool iEditorMode) {
	const math::vec2 screen{static_cast<float>(m_viewport.x()), static_cast<float>(m_viewport.y())};
	const float scale = iScene.registry.get<component::Canvas>(iCanvas).getScale(screen);
	const component::UiRect::Bounds root{.position = {0.f, 0.f}, .size = screen / scale};
	m_screenReference = root.size;
	m_pending.clear();
	pushChildren(iScene, iCanvas, HudNode::noParent, iEditorMode);
	while (!m_pending.empty()) {
		const auto [entity, parent, slotted, slot] = m_pending.back();
		m_pending.pop_back();
		if (iScene.registry.all_of<component::Canvas>(entity) ||
			!iScene.isEffectivelyVisible(Entity{entity, &iScene}, iEditorMode))
			continue;
		const auto* rect = iScene.registry.try_get<component::UiRect>(entity);
		if (rect == nullptr) {
			pushChildren(iScene, entity, parent, iEditorMode);
			continue;
		}
		// Laid out in reference pixels, the parent's content divided back by the scale.
		const auto parentBounds =
				parent == HudNode::noParent
						? root
						: component::UiRect::Bounds{.position = m_nodes[parent].childArea().position / scale,
													.size = m_nodes[parent].content.size / scale};
		const auto local =
				slotted ? component::UiRect::Bounds{.position = parentBounds.position + slot,
													.size = rect->getPixelSize(
															{.parent = parentBounds, .screen = m_screenReference})}
						: rect->computeBounds(parentBounds, m_screenReference);
		HudNode node{.entity = entity,
					 .canvas = iCanvas,
					 .parent = parent,
					 .bounds = {.position = local.position * scale, .size = local.size * scale},
					 .scale = scale,
					 .content = {},
					 .clip = {},
					 .clipped = false,
					 .clipsChildren = false,
					 .window = HudNode::noParent};
		const auto* window = iScene.registry.try_get<component::UiWindow>(entity);
		if (window != nullptr)
			clampToScreen(node.bounds, screen);
		node.content = node.bounds;
		if (window != nullptr) {
			const float bar = std::min(window->titleBarHeight * scale, node.bounds.size.y());
			node.content.position.y() += bar;
			node.content.size.y() -= bar;
			node.clipsChildren = true;
		} else if (const auto* panel = iScene.registry.try_get<component::UiPanel>(entity);
				   panel != nullptr && panel->clipChildren) {
			node.clipsChildren = true;
		}
		attachScroll(iScene, node, iEditorMode);
		if (parent != HudNode::noParent) {
			const auto& above = m_nodes[parent];
			node.clipped = above.clipsChildren || above.clipped;
			if (above.clipsChildren)
				node.clip = above.clipped ? above.content.intersect(above.clip) : above.content;
			else
				node.clip = above.clip;
			node.window = above.window;
			node.scrollAncestor = above.scroll != HudNode::noParent ? parent : above.scrollAncestor;
		}
		if (window != nullptr)
			node.window = m_nodes.size();
		m_nodes.push_back(node);
		pushChildren(iScene, entity, m_nodes.size() - 1, iEditorMode);
	}
}

namespace {

constexpr float g_minThumbReference = 16.f;

}// namespace

void HudLayout::attachScroll(Scene& iScene, HudNode& ioNode, const bool iEditorMode) {
	const auto* view = iScene.registry.try_get<component::UiScrollView>(ioNode.entity);
	auto* list = view == nullptr ? iScene.registry.try_get<component::UiList>(ioNode.entity) : nullptr;
	if (view == nullptr && list == nullptr)
		return;
	const float scale = ioNode.scale;
	HudScroll scroll;
	scroll.entity = ioNode.entity;
	scroll.scale = scale;
	scroll.isList = list != nullptr;
	bool vertical = true;
	bool horizontal = false;
	bool showBars = true;
	float barWidth = 0.f;
	math::vec2 content{0.f, 0.f};
	math::vec2 wanted{0.f, 0.f};
	if (view != nullptr) {
		vertical = view->vertical;
		horizontal = view->horizontal;
		showBars = view->showScrollBar;
		barWidth = view->scrollBarWidth * scale;
		scroll.trackColor = view->trackColor;
		scroll.thumbColor = view->thumbColor;
		wanted = view->scrollOffset * scale;
		content = view->contentSize * scale;
		if (content.x() <= 0.f || content.y() <= 0.f) {
			if (const auto extent = measureLayout(iScene, ioNode.entity, ioNode.bounds.size / scale, iEditorMode)) {
				content.x() = content.x() > 0.f ? content.x() : extent->x() * scale;
				content.y() = content.y() > 0.f ? content.y() : extent->y() * scale;
			}
		}
	} else {
		showBars = list->showScrollBar;
		barWidth = list->scrollBarWidth * scale;
		scroll.trackColor = list->trackColor;
		scroll.thumbColor = list->thumbColor;
		scroll.rowHeight = list->rowHeight * scale;
		scroll.rowSpacing = list->rowSpacing * scale;
		scroll.rowCount = list->itemCount;
		wanted = {0.f, list->scrollOffset * scale};
		content = {0.f, ui::listContentHeight(scroll.rowHeight, scroll.rowSpacing, list->itemCount)};
	}
	scroll.minThumb = g_minThumbReference * scale;
	scroll.geometry = computeScrollGeometry(ioNode.bounds, content, vertical, horizontal, showBars, barWidth);
	scroll.offset = {std::clamp(wanted.x(), 0.f, scroll.geometry.maxOffset.x()),
					 std::clamp(wanted.y(), 0.f, scroll.geometry.maxOffset.y())};
	if (list != nullptr && list->revealSelected) {
		if (list->selectedIndex >= 0) {
			const float pitch = scroll.rowHeight + scroll.rowSpacing;
			const float start = static_cast<float>(list->selectedIndex) * pitch;
			scroll.offset.y() = std::clamp(
					revealRange(scroll.offset.y(), scroll.geometry.viewport.size.y(), start, start + scroll.rowHeight),
					0.f, scroll.geometry.maxOffset.y());
		}
		list->revealSelected = false;
	}
	if (view != nullptr)
		iScene.registry.get<component::UiScrollView>(ioNode.entity).scrollOffset = scroll.offset / scale;
	else
		list->scrollOffset = scroll.offset.y() / scale;
	ioNode.content = scroll.geometry.viewport;
	ioNode.clipsChildren = true;
	ioNode.scrollOffset = view != nullptr ? scroll.offset : math::vec2{0.f, 0.f};
	ioNode.scroll = m_scrolls.size();
	m_scrolls.push_back(scroll);
}

auto HudScroll::verticalThumb() const -> component::UiRect::Bounds {
	if (!geometry.verticalBar)
		return {};
	const auto thumb = thumbOf(geometry.verticalTrack.size.y(), geometry.viewport.size.y(), geometry.content.y(),
							   offset.y(), minThumb);
	return {.position = {geometry.verticalTrack.position.x(), geometry.verticalTrack.position.y() + thumb.start},
			.size = {geometry.verticalTrack.size.x(), thumb.length}};
}

auto HudScroll::horizontalThumb() const -> component::UiRect::Bounds {
	if (!geometry.horizontalBar)
		return {};
	const auto thumb = thumbOf(geometry.horizontalTrack.size.x(), geometry.viewport.size.x(), geometry.content.x(),
							   offset.x(), minThumb);
	return {.position = {geometry.horizontalTrack.position.x() + thumb.start, geometry.horizontalTrack.position.y()},
			.size = {thumb.length, geometry.horizontalTrack.size.y()}};
}

void HudLayout::invalidate() { m_valid = false; }

auto HudLayout::isComputedFor(const math::vec2ui& iViewport) const -> bool {
	return m_valid && m_viewport == iViewport;
}

auto HudLayout::findNode(const entt::entity iEntity) const -> const HudNode* {
	const auto found = std::ranges::find(m_nodes, iEntity, &HudNode::entity);
	return found == m_nodes.end() ? nullptr : &*found;
}

auto HudLayout::hitTest(const math::vec2& iPoint) const -> const HudNode* {
	for (const auto& node: m_nodes | std::views::reverse) {
		if (node.isHit(iPoint))
			return &node;
	}
	return nullptr;
}

}// namespace owl::scene::ui
