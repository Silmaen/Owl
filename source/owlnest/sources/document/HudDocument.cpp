/**
 * @file HudDocument.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "HudDocument.h"

#include "../commands/HudCommands.h"

#include <scene/component/components.h>
#include <scene/ui/HudLayout.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <limits>
#include <ranges>
#include <utility>

namespace owl::nest {

namespace {

using scene::component::UiRect;
using scene::ui::HudNode;

// Distance in preview pixels a gesture must travel before it changes anything.
constexpr float dragThreshold = 2.f;
// Distance in screen pixels a widget keeps its size below, on a resize.
constexpr float minimumSize = 4.f;

auto isDescendantOf(const scene::Scene& iScene, const core::UUID iEntity, const core::UUID iAncestor) -> bool {
	const auto entity = iScene.findEntityByUUID(iEntity);
	if (!entity)
		return false;
	auto parent = entity.getComponent<scene::component::Hierarchy>().parentId;
	while (parent != core::UUID{0}) {
		if (parent == iAncestor)
			return true;
		const auto above = iScene.findEntityByUUID(parent);
		if (!above)
			return false;
		parent = above.getComponent<scene::component::Hierarchy>().parentId;
	}
	return false;
}

auto uuidOf(const scene::Scene& iScene, const entt::entity iEntity) -> core::UUID {
	return iScene.registry.get<scene::component::ID>(iEntity).id;
}

// "X,Y" units as `hud.set_rect` reads them.
auto unitsText(const UiRect::Units& iUnits) -> std::string {
	return std::format("{},{}", UiRect::getUnitName(iUnits.x), UiRect::getUnitName(iUnits.y));
}

auto toArg(const core::UUID iUuid) -> int64_t { return static_cast<int64_t>(static_cast<uint64_t>(iUuid)); }

auto parentArea(const scene::ui::HudLayout& iLayout, const HudNode& iNode, const math::vec2ui& iScreen)
		-> UiRect::Bounds {
	if (iNode.parent == HudNode::noParent)
		return {.position = {0.f, 0.f}, .size = {static_cast<float>(iScreen.x()), static_cast<float>(iScreen.y())}};
	return iLayout.getNodes()[iNode.parent].childArea();
}

auto toReference(const UiRect::Bounds& iBounds, const float iScale) -> UiRect::Bounds {
	return {.position = iBounds.position / iScale, .size = iBounds.size / iScale};
}

auto movesLeft(const HudHandle iHandle) -> bool {
	return iHandle == HudHandle::Left || iHandle == HudHandle::TopLeft || iHandle == HudHandle::BottomLeft;
}

auto movesRight(const HudHandle iHandle) -> bool {
	return iHandle == HudHandle::Right || iHandle == HudHandle::TopRight || iHandle == HudHandle::BottomRight;
}

auto movesTop(const HudHandle iHandle) -> bool {
	return iHandle == HudHandle::Top || iHandle == HudHandle::TopLeft || iHandle == HudHandle::TopRight;
}

auto movesBottom(const HudHandle iHandle) -> bool {
	return iHandle == HudHandle::Bottom || iHandle == HudHandle::BottomLeft || iHandle == HudHandle::BottomRight;
}

auto distanceToSegment(const math::vec2& iPoint, const math::vec2& iA, const math::vec2& iB) -> float {
	const math::vec2 edge = iB - iA;
	const float lengthSquared = edge.x() * edge.x() + edge.y() * edge.y();
	float t = 0.f;
	if (lengthSquared > 0.f)
		t = std::clamp(((iPoint.x() - iA.x()) * edge.x() + (iPoint.y() - iA.y()) * edge.y()) / lengthSquared, 0.f, 1.f);
	const math::vec2 closest = iA + edge * t;
	return std::hypot(iPoint.x() - closest.x(), iPoint.y() - closest.y());
}

auto snapUnit(const float iValue, const std::initializer_list<float> iStops, const float iRange) -> float {
	for (const float stop: iStops) {
		if (std::abs(iValue - stop) <= iRange)
			return stop;
	}
	return iValue;
}

}// namespace

auto snapToCandidates(const std::span<const float> iValues, const std::span<const float> iCandidates,
					  const float iThreshold) -> std::optional<std::pair<float, float>> {
	std::optional<std::pair<float, float>> best;
	float bestDistance = iThreshold;
	for (const float value: iValues) {
		for (const float candidate: iCandidates) {
			if (const float distance = std::abs(candidate - value); distance <= bestDistance) {
				bestDistance = distance;
				best = std::pair{candidate - value, candidate};
			}
		}
	}
	return best;
}

auto snapToGrid(const float iValue, const float iStep) -> float {
	if (iStep <= 0.f)
		return iValue;
	return std::round(iValue / iStep) * iStep;
}

void HudDocument::setResolution(const math::vec2ui& iResolution) {
	m_resolution = {std::clamp(iResolution.x(), minResolution, maxResolution),
					std::clamp(iResolution.y(), minResolution, maxResolution)};
}

auto HudDocument::findPreset() const -> std::optional<size_t> {
	const auto* const found = std::ranges::find(presets, m_resolution, &HudResolutionPreset::size);
	if (found == presets.end())
		return std::nullopt;
	return static_cast<size_t>(std::distance(presets.begin(), found));
}

void HudDocument::refreshLayout(scene::Scene& ioScene) const {
	ioScene.getHudLayout().compute(ioScene, m_resolution, true);
}

auto HudDocument::isSelected(const core::UUID iEntity) const -> bool {
	return std::ranges::find(m_selection, iEntity) != m_selection.end();
}

void HudDocument::select(const core::UUID iEntity, const HudSelectMode iMode) {
	if (iMode == HudSelectMode::Replace)
		m_selection.clear();
	if (iEntity == core::UUID{0})
		return;
	const auto found = std::ranges::find(m_selection, iEntity);
	if (found != m_selection.end()) {
		if (iMode != HudSelectMode::Toggle)
			return;
		m_selection.erase(found);
		return;
	}
	m_selection.push_back(iEntity);
}

void HudDocument::makePrimary(const core::UUID iEntity) {
	if (iEntity == core::UUID{0})
		return;
	std::erase(m_selection, iEntity);
	m_selection.push_back(iEntity);
}

void HudDocument::pruneSelection(const scene::Scene& iScene) {
	std::erase_if(m_selection,
				  [&iScene](const core::UUID iEntity) -> bool { return !iScene.findEntityByUUID(iEntity); });
}

auto HudDocument::pickWidget(const scene::Scene& iScene, const math::vec2& iPoint) -> core::UUID {
	const auto* node = iScene.getHudLayout().hitTest(iPoint);
	return node == nullptr ? core::UUID{0} : uuidOf(iScene, node->entity);
}

auto HudDocument::selectInBox(const scene::Scene& iScene, const UiRect::Bounds& iBox, const bool iAdditive) -> size_t {
	if (!iAdditive)
		m_selection.clear();
	size_t count = 0;
	for (const auto& node: iScene.getHudLayout().getNodes()) {
		const auto& b = node.bounds;
		const bool overlaps =
				b.position.x() < iBox.position.x() + iBox.size.x() && iBox.position.x() < b.position.x() + b.size.x() &&
				b.position.y() < iBox.position.y() + iBox.size.y() && iBox.position.y() < b.position.y() + b.size.y();
		if (!overlaps)
			continue;
		++count;
		select(uuidOf(iScene, node.entity), HudSelectMode::Add);
	}
	return count;
}

auto HudDocument::getGizmo(const scene::Scene& iScene) const -> std::optional<HudGizmo> {
	const auto primary = getPrimary();
	if (primary == core::UUID{0})
		return std::nullopt;
	const auto entity = iScene.findEntityByUUID(primary);
	if (!entity || !entity.hasComponent<UiRect>())
		return std::nullopt;
	const auto& layout = iScene.getHudLayout();
	const auto* node = layout.findNode(static_cast<entt::entity>(entity));
	if (node == nullptr)
		return std::nullopt;
	const auto& rect = entity.getComponent<UiRect>();
	HudGizmo gizmo;
	gizmo.bounds = node->bounds;
	gizmo.parent = parentArea(layout, *node, m_resolution);
	gizmo.pivot = {node->bounds.position.x() + node->bounds.size.x() * rect.pivot.x(),
				   node->bounds.position.y() + node->bounds.size.y() * rect.pivot.y()};
	gizmo.anchorMin = {gizmo.parent.position.x() + gizmo.parent.size.x() * rect.anchorMin.x(),
					   gizmo.parent.position.y() + gizmo.parent.size.y() * rect.anchorMin.y()};
	gizmo.anchorMax = {gizmo.parent.position.x() + gizmo.parent.size.x() * rect.anchorMax.x(),
					   gizmo.parent.position.y() + gizmo.parent.size.y() * rect.anchorMax.y()};
	gizmo.stretched = rect.anchorMin != rect.anchorMax;
	return gizmo;
}

auto HudDocument::hitHandle(const scene::Scene& iScene, const math::vec2& iPoint, const float iTolerance,
							const HudGrabMode iGrab) const -> HudHandle {
	if (m_selection.empty())
		return HudHandle::None;
	if (m_selection.size() > 1) {
		const auto& layout = iScene.getHudLayout();
		for (const auto uuid: m_selection) {
			const auto entity = iScene.findEntityByUUID(uuid);
			const auto* node = entity ? layout.findNode(static_cast<entt::entity>(entity)) : nullptr;
			if (node != nullptr && node->bounds.contains(iPoint))
				return HudHandle::Body;
		}
		return HudHandle::None;
	}
	const auto gizmo = getGizmo(iScene);
	if (!gizmo)
		return HudHandle::None;
	const auto near = [&iPoint, iTolerance](const math::vec2& iHandle) -> bool {
		return std::hypot(iPoint.x() - iHandle.x(), iPoint.y() - iHandle.y()) <= iTolerance;
	};
	const auto& b = gizmo->bounds;
	const auto usable = [&](const math::vec2& iHandle) -> bool {
		return iGrab != HudGrabMode::Normal || !b.contains(iHandle);
	};
	const auto anchorHandle = [&]() -> HudHandle {
		if (gizmo->stretched) {
			if (near(gizmo->anchorMin) && usable(gizmo->anchorMin))
				return HudHandle::AnchorMin;
			if (near(gizmo->anchorMax) && usable(gizmo->anchorMax))
				return HudHandle::AnchorMax;
			return HudHandle::None;
		}
		return near(gizmo->anchorMin) && usable(gizmo->anchorMin) ? HudHandle::AnchorBoth : HudHandle::None;
	};
	const auto pivotHandle = [&]() -> HudHandle {
		return near(gizmo->pivot) && usable(gizmo->pivot) ? HudHandle::Pivot : HudHandle::None;
	};
	const HudHandle first = iGrab == HudGrabMode::Anchors ? anchorHandle() : pivotHandle();
	if (first != HudHandle::None)
		return first;
	const HudHandle second = iGrab == HudGrabMode::Anchors ? pivotHandle() : anchorHandle();
	if (second != HudHandle::None)
		return second;
	const math::vec2 topLeft = b.position;
	const math::vec2 topRight{b.position.x() + b.size.x(), b.position.y()};
	const math::vec2 bottomLeft{b.position.x(), b.position.y() + b.size.y()};
	const math::vec2 bottomRight = b.position + b.size;
	if (near(topLeft))
		return HudHandle::TopLeft;
	if (near(topRight))
		return HudHandle::TopRight;
	if (near(bottomLeft))
		return HudHandle::BottomLeft;
	if (near(bottomRight))
		return HudHandle::BottomRight;
	if (distanceToSegment(iPoint, topLeft, topRight) <= iTolerance)
		return HudHandle::Top;
	if (distanceToSegment(iPoint, bottomLeft, bottomRight) <= iTolerance)
		return HudHandle::Bottom;
	if (distanceToSegment(iPoint, topLeft, bottomLeft) <= iTolerance)
		return HudHandle::Left;
	if (distanceToSegment(iPoint, topRight, bottomRight) <= iTolerance)
		return HudHandle::Right;
	return b.contains(iPoint) ? HudHandle::Body : HudHandle::None;
}

void HudDocument::collectCandidates(const scene::Scene& iScene, Drag& ioDrag) const {
	ioDrag.candidatesX.clear();
	ioDrag.candidatesY.clear();
	const auto addBox = [&ioDrag](const UiRect::Bounds& iBox) -> void {
		for (const float t: {0.f, 0.5f, 1.f}) {
			ioDrag.candidatesX.push_back(iBox.position.x() + iBox.size.x() * t);
			ioDrag.candidatesY.push_back(iBox.position.y() + iBox.size.y() * t);
		}
	};
	addBox({.position = {0.f, 0.f},
			.size = {static_cast<float>(m_resolution.x()), static_cast<float>(m_resolution.y())}});
	const auto& layout = iScene.getHudLayout();
	const auto nodes = layout.getNodes();
	const auto moving = [&](const HudNode& iNode) -> bool {
		const core::UUID uuid = uuidOf(iScene, iNode.entity);
		return std::ranges::any_of(ioDrag.items, [&](const DragItem& iItem) -> bool {
			return uuid == iItem.entity || isDescendantOf(iScene, uuid, iItem.entity);
		});
	};
	for (const auto& node: nodes) {
		if (!moving(node))
			addBox(node.bounds);
	}
	if (!ioDrag.items.empty()) {
		const auto entity = iScene.findEntityByUUID(ioDrag.items.back().entity);
		if (const auto* primary = entity ? layout.findNode(static_cast<entt::entity>(entity)) : nullptr;
			primary != nullptr && primary->parent != HudNode::noParent)
			addBox(nodes[primary->parent].childArea());
	}
}

auto HudDocument::beginDrag(scene::Scene& ioScene, const HudHandle iHandle, const math::vec2& iPoint) -> bool {
	m_drag.reset();
	m_guidesX.clear();
	m_guidesY.clear();
	if (iHandle == HudHandle::None || m_selection.empty())
		return false;
	refreshLayout(ioScene);
	const auto& layout = ioScene.getHudLayout();
	Drag drag;
	drag.handle = iHandle;
	drag.start = iPoint;
	std::vector<core::UUID> carried;
	if (iHandle == HudHandle::Body) {
		for (const auto uuid: m_selection) {
			const bool covered = std::ranges::any_of(m_selection, [&](const core::UUID iOther) -> bool {
				return iOther != uuid && isDescendantOf(ioScene, uuid, iOther);
			});
			if (!covered)
				carried.push_back(uuid);
		}
	} else {
		carried.push_back(getPrimary());
	}
	for (const auto uuid: carried) {
		const auto entity = ioScene.findEntityByUUID(uuid);
		if (!entity || !entity.hasComponent<UiRect>())
			continue;
		const auto* node = layout.findNode(static_cast<entt::entity>(entity));
		if (node == nullptr)
			continue;
		drag.items.push_back({.entity = uuid,
							  .rect = entity.getComponent<UiRect>(),
							  .bounds = node->bounds,
							  .frame = {.parent = toReference(parentArea(layout, *node, m_resolution), node->scale),
										.screen = math::vec2{static_cast<float>(m_resolution.x()),
															 static_cast<float>(m_resolution.y())} /
												  node->scale},
							  .scale = node->scale});
	}
	if (drag.items.empty())
		return false;
	drag.current = drag.items.back().rect;
	collectCandidates(ioScene, drag);
	m_drag = std::move(drag);
	return true;
}

auto HudDocument::snapAxis(const std::span<const float> iValues, const std::span<const float> iCandidates,
						   const float iScale, std::vector<float>& oGuides) const -> float {
	if (m_snap.edges) {
		if (const auto stuck = snapToCandidates(iValues, iCandidates, m_snap.threshold); stuck) {
			oGuides.push_back(stuck->second);
			return stuck->first;
		}
	}
	if (m_snap.pixelGrid && !iValues.empty()) {
		const float reference = iValues.front() / iScale;
		return (snapToGrid(reference, m_snap.gridStep) - reference) * iScale;
	}
	return 0.f;
}

auto HudDocument::computeRect(const math::vec2& iPoint, const bool iSnap) -> UiRect {
	auto& drag = *m_drag;
	const auto& item = drag.items.back();
	const auto& b = item.bounds;
	const math::vec2 delta = iPoint - drag.start;
	const auto fit = [&item](const UiRect& iRect, const UiRect::Bounds& iScreen) -> UiRect {
		return commands::solveHudRect(iRect, item.frame, toReference(iScreen, item.scale));
	};
	switch (drag.handle) {
		case HudHandle::Pivot:
			{
				math::vec2 pivot{(iPoint.x() - b.position.x()) / std::max(1.f, b.size.x()),
								 (iPoint.y() - b.position.y()) / std::max(1.f, b.size.y())};
				if (iSnap) {
					pivot.x() = snapUnit(pivot.x(), {0.f, 0.5f, 1.f}, m_snap.threshold / std::max(1.f, b.size.x()));
					pivot.y() = snapUnit(pivot.y(), {0.f, 0.5f, 1.f}, m_snap.threshold / std::max(1.f, b.size.y()));
				}
				UiRect rect = item.rect;
				rect.pivot = pivot;
				return fit(rect, b);
			}
		case HudHandle::AnchorMin:
		case HudHandle::AnchorMax:
		case HudHandle::AnchorBoth:
			{
				const auto area = UiRect::Bounds{.position = item.frame.parent.position * item.scale,
												 .size = item.frame.parent.size * item.scale};
				math::vec2 anchor{
						std::clamp((iPoint.x() - area.position.x()) / std::max(1.f, area.size.x()), 0.f, 1.f),
						std::clamp((iPoint.y() - area.position.y()) / std::max(1.f, area.size.y()), 0.f, 1.f)};
				if (iSnap) {
					anchor.x() = snapUnit(anchor.x(), {0.f, 0.25f, 0.5f, 0.75f, 1.f},
										  m_snap.threshold / std::max(1.f, area.size.x()));
					anchor.y() = snapUnit(anchor.y(), {0.f, 0.25f, 0.5f, 0.75f, 1.f},
										  m_snap.threshold / std::max(1.f, area.size.y()));
				}
				UiRect rect = item.rect;
				if (drag.handle == HudHandle::AnchorMin) {
					rect.anchorMin = {std::min(anchor.x(), rect.anchorMax.x()),
									  std::min(anchor.y(), rect.anchorMax.y())};
				} else if (drag.handle == HudHandle::AnchorMax) {
					rect.anchorMax = {std::max(anchor.x(), rect.anchorMin.x()),
									  std::max(anchor.y(), rect.anchorMin.y())};
				} else {
					rect.anchorMin = anchor;
					rect.anchorMax = anchor;
				}
				return fit(rect, b);
			}
		case HudHandle::Body:
		case HudHandle::None:
			return item.rect;
		case HudHandle::Left:
		case HudHandle::Right:
		case HudHandle::Top:
		case HudHandle::Bottom:
		case HudHandle::TopLeft:
		case HudHandle::TopRight:
		case HudHandle::BottomLeft:
		case HudHandle::BottomRight:
			break;
	}
	float left = b.position.x();
	float right = b.position.x() + b.size.x();
	float top = b.position.y();
	float bottom = b.position.y() + b.size.y();
	const float least = minimumSize;
	const auto stick = [&](const float iValue, const std::vector<float>& iCandidates,
						   std::vector<float>& oGuides) -> float {
		const std::array<float, 1> values{iValue};
		return iValue + (iSnap ? snapAxis(values, iCandidates, item.scale, oGuides) : 0.f);
	};
	if (movesLeft(drag.handle))
		left = std::min(stick(left + delta.x(), drag.candidatesX, m_guidesX), right - least);
	if (movesRight(drag.handle))
		right = std::max(stick(right + delta.x(), drag.candidatesX, m_guidesX), left + least);
	if (movesTop(drag.handle))
		top = std::min(stick(top + delta.y(), drag.candidatesY, m_guidesY), bottom - least);
	if (movesBottom(drag.handle))
		bottom = std::max(stick(bottom + delta.y(), drag.candidatesY, m_guidesY), top + least);
	return fit(item.rect, {.position = {left, top}, .size = {right - left, bottom - top}});
}

void HudDocument::applyPreview(scene::Scene& ioScene, const math::vec2& iPoint, const bool iSnap) {
	auto& drag = *m_drag;
	m_guidesX.clear();
	m_guidesY.clear();
	if (drag.handle == HudHandle::Body) {
		const auto& primary = drag.items.back();
		math::vec2 delta = iPoint - drag.start;
		if (iSnap) {
			const auto& b = primary.bounds;
			const std::array<float, 3> xs{b.position.x() + delta.x(), b.position.x() + b.size.x() * 0.5f + delta.x(),
										  b.position.x() + b.size.x() + delta.x()};
			const std::array<float, 3> ys{b.position.y() + delta.y(), b.position.y() + b.size.y() * 0.5f + delta.y(),
										  b.position.y() + b.size.y() + delta.y()};
			delta.x() += snapAxis(xs, drag.candidatesX, primary.scale, m_guidesX);
			delta.y() += snapAxis(ys, drag.candidatesY, primary.scale, m_guidesY);
		}
		for (const auto& item: drag.items) {
			if (const auto entity = ioScene.findEntityByUUID(item.entity); entity) {
				auto& rect = entity.getComponent<UiRect>();
				rect = item.rect;
				rect.moveBy(delta / item.scale, item.frame);
			}
		}
		drag.current = primary.rect;
		drag.current.moveBy(delta / primary.scale, primary.frame);
		drag.movedBy = delta / primary.scale;
		return;
	}
	drag.current = computeRect(iPoint, iSnap);
	if (const auto entity = ioScene.findEntityByUUID(drag.items.back().entity); entity)
		entity.getComponent<UiRect>() = drag.current;
}

void HudDocument::updateDrag(scene::Scene& ioScene, const math::vec2& iPoint, const bool iSnap) {
	if (!m_drag)
		return;
	if (!m_drag->moved) {
		const math::vec2 away = iPoint - m_drag->start;
		if (std::hypot(away.x(), away.y()) < dragThreshold)
			return;
		m_drag->moved = true;
	}
	applyPreview(ioScene, iPoint, iSnap);
}

void HudDocument::restoreRects(scene::Scene& ioScene) const {
	if (!m_drag)
		return;
	for (const auto& item: m_drag->items) {
		if (const auto entity = ioScene.findEntityByUUID(item.entity); entity && entity.hasComponent<UiRect>())
			entity.getComponent<UiRect>() = item.rect;
	}
}

auto HudDocument::endDrag(scene::Scene& ioScene, const commands::CommandTarget& iTarget) -> bool {
	if (!m_drag)
		return false;
	restoreRects(ioScene);
	const Drag drag = std::move(*m_drag);
	m_drag.reset();
	m_guidesX.clear();
	m_guidesY.clear();
	if (!drag.moved)
		return false;
	if (drag.handle == HudHandle::Body) {
		std::string entities;
		for (const auto& item: drag.items)
			entities += std::format("{}{}", entities.empty() ? "" : ",", static_cast<uint64_t>(item.entity));
		return iTarget.execute("hud.move", {{"entities", entities}, {"delta", drag.movedBy}}).has_value();
	}
	const auto& rect = drag.current;
	return iTarget
			.execute("hud.set_rect", {{"entity", toArg(drag.items.back().entity)},
									  {"anchor_min", rect.anchorMin},
									  {"anchor_max", rect.anchorMax},
									  {"pivot", rect.pivot},
									  {"size", rect.size},
									  {"offset", rect.anchorOffset},
									  {"size_unit", unitsText(rect.sizeUnit)},
									  {"offset_unit", unitsText(rect.offsetUnit)}})
			.has_value();
}

void HudDocument::cancelDrag(scene::Scene& ioScene) {
	restoreRects(ioScene);
	m_drag.reset();
	m_guidesX.clear();
	m_guidesY.clear();
}

auto HudDocument::addWidget(scene::Scene& ioScene, const commands::CommandTarget& iTarget, const std::string& iKind,
							const math::vec2& iPoint, const std::string& iTexture)
		-> expected<core::UUID, commands::CommandFailure> {
	refreshLayout(ioScene);
	const auto& layout = ioScene.getHudLayout();
	const auto nodes = layout.getNodes();
	const auto* hit = layout.hitTest(iPoint);
	const HudNode* container = hit;
	while (container != nullptr) {
		const auto entity = scene::Entity{container->entity, &ioScene};
		if (entity.hasComponent<scene::component::UiWindow>() || entity.hasComponent<scene::component::UiPanel>() ||
			entity.hasComponent<scene::component::UiScrollView>())
			break;
		container = container->parent == HudNode::noParent ? nullptr : &nodes[container->parent];
	}
	commands::CommandArgs args{{"kind", iKind}};
	math::vec2 origin{0.f, 0.f};
	const math::vec2 screen{static_cast<float>(m_resolution.x()), static_cast<float>(m_resolution.y())};
	float scale = scene::component::Canvas{}.getScale(screen);
	if (container != nullptr) {
		args.set("parent", toArg(uuidOf(ioScene, container->entity)));
		origin = container->childArea().position;
		scale = container->scale;
	} else if (hit != nullptr) {
		args.set("parent", toArg(uuidOf(ioScene, hit->canvas)));
		scale = hit->scale;
	} else if (const auto canvases = ioScene.registry.view<scene::component::Canvas>(); !canvases.empty()) {
		const auto first = *std::ranges::min_element(canvases, {}, [&canvases](const entt::entity iCanvas) -> int32_t {
			return canvases.get<scene::component::Canvas>(iCanvas).sortOrder;
		});
		args.set("parent", toArg(uuidOf(ioScene, first)));
		scale = canvases.get<scene::component::Canvas>(first).getScale(screen);
	}
	args.set("anchor", math::vec2{0.f, 0.f});
	args.set("offset", (iPoint - origin) / scale);
	if (!iTexture.empty())
		args.set("texture", iTexture);
	const auto result = iTarget.execute("hud.add_widget", args);
	if (!result)
		return unexpected<commands::CommandFailure>{result.error()};
	select(result->entity);
	return result->entity;
}

auto HudDocument::deleteSelection(const commands::CommandTarget& iTarget) -> bool {
	if (m_selection.empty())
		return false;
	std::string entities;
	for (const auto uuid: m_selection)
		entities += std::format("{}{}", entities.empty() ? "" : ",", static_cast<uint64_t>(uuid));
	if (!iTarget.execute("hud.delete", {{"entities", entities}}))
		return false;
	m_selection.clear();
	return true;
}

auto HudDocument::moveSelection(const commands::CommandTarget& iTarget, const math::vec2& iDelta) -> bool {
	if (m_selection.empty())
		return false;
	std::string entities;
	for (const auto uuid: m_selection)
		entities += std::format("{}{}", entities.empty() ? "" : ",", static_cast<uint64_t>(uuid));
	return iTarget.execute("hud.move", {{"entities", entities}, {"delta", iDelta}}).has_value();
}

auto HudDocument::reparentPrimary(const commands::CommandTarget& iTarget, const core::UUID iParent,
								  const int64_t iIndex) const -> bool {
	if (getPrimary() == core::UUID{0})
		return false;
	commands::CommandArgs args{{"entity", toArg(getPrimary())}, {"parent", toArg(iParent)}};
	if (iIndex >= 0)
		args.set("index", iIndex);
	return iTarget.execute("hud.reparent", args).has_value();
}

auto HudDocument::reorderPrimary(const commands::CommandTarget& iTarget, const int64_t iDelta) const -> bool {
	if (getPrimary() == core::UUID{0} || !iTarget.isValid())
		return false;
	const auto entity = iTarget.scene->findEntityByUUID(getPrimary());
	if (!entity)
		return false;
	const auto parentId = entity.getComponent<scene::component::Hierarchy>().parentId;
	const auto parent = iTarget.scene->findEntityByUUID(parentId);
	if (!parent)
		return false;
	const auto& siblings = parent.getComponent<scene::component::Hierarchy>().childrenIds;
	const auto at = std::ranges::find(siblings, getPrimary());
	if (at == siblings.end())
		return false;
	const auto last = static_cast<int64_t>(siblings.size()) - 1;
	const auto index = std::clamp(static_cast<int64_t>(std::distance(siblings.begin(), at)) + iDelta, int64_t{0}, last);
	return iTarget
			.execute("hud.reparent", {{"entity", toArg(getPrimary())},
									  {"parent", toArg(parentId)},
									  {"index", index},
									  {"keep_bounds", false}})
			.has_value();
}

}// namespace owl::nest
