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
#include "scene/component/UiPanel.h"
#include "scene/component/UiWindow.h"

#include <algorithm>
#include <cstddef>
#include <ranges>

namespace owl::scene::ui {

void HudLayout::compute(Scene& iScene, const math::vec2ui& iViewport, const bool iEditorMode) {
	OWL_PROFILE_FUNCTION()

	++m_computeCount;
	m_nodes.clear();
	m_viewport = iViewport;
	m_valid = true;
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

void HudLayout::arrange(const component::UiLayout& iLayout, const math::vec2& iContent) {
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
	m_layoutChildren.clear();
	for (const auto childId: hierarchy->childrenIds) {
		const Entity child = iScene.findEntityByUUID(childId);
		if (!child)
			continue;
		const auto entity = static_cast<entt::entity>(child);
		const auto* rect = iScene.registry.try_get<component::UiRect>(entity);
		const bool placed = rect != nullptr && !iScene.registry.all_of<component::Canvas>(entity) &&
							iScene.isEffectivelyVisible(child, iEditorMode);
		m_layoutChildren.push_back(
				{.entity = entity, .size = placed ? rect->size : math::vec2{0.f, 0.f}, .placed = placed});
	}
	const auto& parent = m_nodes[iParent];
	arrange(*layout, parent.content.size / parent.scale);
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
						: component::UiRect::Bounds{.position = m_nodes[parent].content.position / scale,
													.size = m_nodes[parent].content.size / scale};
		const auto local =
				slotted ? component::UiRect::Bounds{.position = parentBounds.position + slot, .size = rect->size}
						: rect->computeBounds(parentBounds);
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
		if (parent != HudNode::noParent) {
			const auto& above = m_nodes[parent];
			node.clipped = above.clipsChildren || above.clipped;
			if (above.clipsChildren)
				node.clip = above.clipped ? above.content.intersect(above.clip) : above.content;
			else
				node.clip = above.clip;
			node.window = above.window;
		}
		if (window != nullptr)
			node.window = m_nodes.size();
		m_nodes.push_back(node);
		pushChildren(iScene, entity, m_nodes.size() - 1, iEditorMode);
	}
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
