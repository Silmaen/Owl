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

#include <algorithm>
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

void HudLayout::layoutCanvas(Scene& iScene, const entt::entity iCanvas, const bool iEditorMode) {
	const math::vec2 screen{static_cast<float>(m_viewport.x()), static_cast<float>(m_viewport.y())};
	const float scale = iScene.registry.get<component::Canvas>(iCanvas).getScale(screen);
	const component::UiRect::Bounds root{.position = {0.f, 0.f}, .size = screen / scale};
	const auto pushChildren = [&](const entt::entity iEntity, const size_t iParent) -> void {
		const auto* hierarchy = iScene.registry.try_get<component::Hierarchy>(iEntity);
		if (hierarchy == nullptr)
			return;
		for (const auto childId: hierarchy->childrenIds | std::views::reverse) {
			if (const Entity child = iScene.findEntityByUUID(childId); child)
				m_pending.push_back({.entity = static_cast<entt::entity>(child), .parent = iParent});
		}
	};
	m_pending.clear();
	pushChildren(iCanvas, HudNode::noParent);
	while (!m_pending.empty()) {
		const auto [entity, parent] = m_pending.back();
		m_pending.pop_back();
		if (iScene.registry.all_of<component::Canvas>(entity) ||
			!iScene.isEffectivelyVisible(Entity{entity, &iScene}, iEditorMode))
			continue;
		const auto* rect = iScene.registry.try_get<component::UiRect>(entity);
		if (rect == nullptr) {
			pushChildren(entity, parent);
			continue;
		}
		// Laid out in reference pixels, the parent's rectangle divided back by the scale.
		const auto parentBounds =
				parent == HudNode::noParent
						? root
						: component::UiRect::Bounds{.position = m_nodes[parent].bounds.position / scale,
													.size = m_nodes[parent].bounds.size / scale};
		const auto local = rect->computeBounds(parentBounds);
		m_nodes.push_back({.entity = entity,
						   .canvas = iCanvas,
						   .parent = parent,
						   .bounds = {.position = local.position * scale, .size = local.size * scale},
						   .scale = scale});
		pushChildren(entity, m_nodes.size() - 1);
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
		if (node.bounds.contains(iPoint))
			return &node;
	}
	return nullptr;
}

}// namespace owl::scene::ui
