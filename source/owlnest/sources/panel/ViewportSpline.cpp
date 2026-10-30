/**
 * @file ViewportSpline.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"
#include "Viewport.h"
#include "document/SceneDocument.h"

#include "../commands/CommandRegistry.h"
#include "../commands/DocumentCommands.h"

#include <input/Input.h>
#include <math/linAlgebra.h>
#include <math/matrixCreation.h>
#include <renderer/Renderer.h>
#include <scene/component/Spline.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace owl::nest::panel {

namespace {

using Handle = gui::component::SplineSelection::Handle;

auto transformPoint(const math::mat4& iMatrix, const math::vec3& iPoint) -> math::vec3 {
	const math::vec4 point = iMatrix * math::vec4{iPoint.x(), iPoint.y(), iPoint.z(), 1.f};
	return {point.x(), point.y(), point.z()};
}

// World position of a point or of one of its Bezier handles.
auto handlePosition(const math::SplinePoint& iPoint, const Handle iHandle) -> math::vec3 {
	switch (iHandle) {
		case Handle::Point:
			return iPoint.position;
		case Handle::TangentIn:
			return iPoint.position + iPoint.tangentIn;
		case Handle::TangentOut:
			return iPoint.position + iPoint.tangentOut;
	}
	return iPoint.position;
}

// A square marker facing the camera.
void drawMarker(const math::vec3& iCenter, const math::vec3& iRight, const math::vec3& iUp, const float iSize,
				const math::vec4& iColor, const int iEntityId) {
	const math::vec3 a = iCenter + (iRight + iUp) * iSize;
	const math::vec3 b = iCenter + (iRight - iUp) * iSize;
	const math::vec3 c = iCenter - (iRight + iUp) * iSize;
	const math::vec3 d = iCenter - (iRight - iUp) * iSize;
	for (const auto& [from, to]: {std::pair{a, b}, std::pair{b, c}, std::pair{c, d}, std::pair{d, a}})
		renderer::Renderer2D::drawLine({.point1 = from, .point2 = to, .color = iColor, .entityId = iEntityId});
}

// Distance from a ray to a segment, and the share of the segment where it is closest.
auto raySegment(const math::vec3& iOrigin, const math::vec3& iDirection, const math::vec3& iA, const math::vec3& iB)
		-> std::pair<float, float> {
	const math::vec3 segment = iB - iA;
	const math::vec3 offset = iA - iOrigin;
	const float aa = segment * segment;
	const float ab = segment * iDirection;
	const float ac = segment * offset;
	const float bc = iDirection * offset;
	const float denominator = aa - ab * ab;
	float share = denominator > 1e-8f ? (ab * bc - ac) / denominator : 0.f;
	share = std::clamp(share, 0.f, 1.f);
	const math::vec3 onSegment = iA + segment * share;
	const float along = std::max(0.f, (onSegment - iOrigin) * iDirection);
	return {(iOrigin + iDirection * along - onSegment).norm(), share};
}

}// namespace

void Viewport::renderSplineOverlay(const shared<scene::Scene>& iScene) const {
	if (m_parent == nullptr || mp_document == nullptr || mp_document->state() != SceneDocument::State::Edit)
		return;
	const scene::Entity selected = m_parent->getSelectedEntity();
	if (!selected || !selected.hasComponent<scene::component::Spline>())
		return;
	const auto& spline = selected.getComponent<scene::component::Spline>();
	auto& selection = gui::component::getSplineSelection();
	selection.clamp(selected.getUUID(), spline.curve.getPoints().size());
	const auto id = static_cast<int>(static_cast<uint32_t>(selected));
	const math::mat4 world = iScene->getWorldTransform(selected)();
	const math::mat4 view = getActiveCamera().getView();
	const math::vec3 right{view(0, 0), view(0, 1), view(0, 2)};
	const math::vec3 up{view(1, 0), view(1, 1), view(1, 2)};
	constexpr math::vec4 curveColor{0.3f, 0.9f, 0.55f, 1.f};
	constexpr math::vec4 laneColor{0.3f, 0.9f, 0.55f, 0.45f};
	constexpr math::vec4 pointColor{1.f, 0.8f, 0.25f, 1.f};
	constexpr math::vec4 handleColor{0.55f, 0.75f, 1.f, 1.f};
	constexpr math::vec4 selectedColor{1.f, 1.f, 1.f, 1.f};
	const auto& polyline = spline.curve.getPolyline();
	for (size_t index = 1; index < polyline.size(); ++index)
		renderer::Renderer2D::drawLine({.point1 = transformPoint(world, polyline[index - 1]),
										.point2 = transformPoint(world, polyline[index]),
										.color = curveColor,
										.entityId = id});
	const float length = spline.curve.getLength();
	for (const auto& lane: spline.lanes) {
		constexpr int steps = 64;
		math::vec3 previous = transformPoint(world, spline.sample(0.f, lane.name).position);
		for (int step = 1; step <= steps; ++step) {
			const float distance = length * static_cast<float>(step) / static_cast<float>(steps);
			const math::vec3 next = transformPoint(world, spline.sample(distance, lane.name).position);
			renderer::Renderer2D::drawLine({.point1 = previous, .point2 = next, .color = laneColor, .entityId = id});
			previous = next;
		}
	}
	const auto& points = spline.curve.getPoints();
	const bool bezier = spline.curve.getInterpolation() == math::SplineInterpolation::Bezier;
	for (size_t index = 0; index < points.size(); ++index) {
		const math::vec3 position = transformPoint(world, points[index].position);
		const float size = getLightMarkerRadius(position) * 0.6f;
		const bool picked = selection.contains(selected.getUUID(), index);
		drawMarker(position, right, up, size * (picked ? 1.5f : 1.f), picked ? selectedColor : pointColor, id);
		if (!bezier || !picked || selection.points.size() != 1)
			continue;
		for (const auto handle: {Handle::TangentIn, Handle::TangentOut}) {
			const math::vec3 tip = transformPoint(world, handlePosition(points[index], handle));
			const bool active = selection.handle == handle;
			renderer::Renderer2D::drawLine({.point1 = position, .point2 = tip, .color = handleColor, .entityId = id});
			drawMarker(tip, right, up, size * (active ? 1.2f : 0.8f), active ? selectedColor : handleColor, id);
		}
	}
}

auto Viewport::pickSplineHandle() const -> std::optional<std::pair<size_t, Handle>> {
	if (m_parent == nullptr || mp_document == nullptr)
		return std::nullopt;
	const auto& scene = mp_document->getActiveScene();
	const scene::Entity selected = m_parent->getSelectedEntity();
	const auto ray = getCursorRay();
	if (!scene || !selected || !selected.hasComponent<scene::component::Spline>() || !ray)
		return std::nullopt;
	const auto& [origin, direction] = *ray;
	const auto& spline = selected.getComponent<scene::component::Spline>();
	const auto& selection = gui::component::getSplineSelection();
	const math::mat4 world = scene->getWorldTransform(selected)();
	const auto& points = spline.curve.getPoints();
	const bool handles = spline.curve.getInterpolation() == math::SplineInterpolation::Bezier &&
						 selection.entity == selected.getUUID() && selection.points.size() == 1;
	std::optional<std::pair<size_t, Handle>> nearest;
	float nearestDistance = std::numeric_limits<float>::max();
	const auto test = [&](const size_t iIndex, const Handle iHandle) -> void {
		const math::vec3 point = transformPoint(world, handlePosition(points[iIndex], iHandle));
		const math::vec3 toPoint = point - origin;
		const float along = toPoint * direction;
		if (along <= 0.f)
			return;
		const float offAxis = (toPoint - direction * along).norm();
		if (offAxis <= getLightMarkerRadius(point) * 1.2f && along < nearestDistance) {
			nearestDistance = along;
			nearest = std::pair{iIndex, iHandle};
		}
	};
	for (size_t index = 0; index < points.size(); ++index) test(index, Handle::Point);
	if (handles && selection.points.front() < points.size()) {
		test(selection.points.front(), Handle::TangentIn);
		test(selection.points.front(), Handle::TangentOut);
	}
	return nearest;
}

auto Viewport::pickSplineDistance() const -> std::optional<float> {
	if (m_parent == nullptr || mp_document == nullptr)
		return std::nullopt;
	const auto& scene = mp_document->getActiveScene();
	const scene::Entity selected = m_parent->getSelectedEntity();
	const auto ray = getCursorRay();
	if (!scene || !selected || !selected.hasComponent<scene::component::Spline>() || !ray)
		return std::nullopt;
	const auto& [origin, direction] = *ray;
	const auto& curve = selected.getComponent<scene::component::Spline>().curve;
	const auto& polyline = curve.getPolyline();
	const math::mat4 world = scene->getWorldTransform(selected)();
	std::optional<float> best;
	float bestMiss = std::numeric_limits<float>::max();
	for (size_t index = 1; index < polyline.size(); ++index) {
		const math::vec3 a = transformPoint(world, polyline[index - 1]);
		const math::vec3 b = transformPoint(world, polyline[index]);
		const auto [miss, share] = raySegment(origin, direction, a, b);
		if (miss > getLightMarkerRadius(a + (b - a) * share) || miss >= bestMiss)
			continue;
		bestMiss = miss;
		const math::vec3 local = transformPoint(math::inverse(world), a + (b - a) * share);
		best = curve.getNearestDistance(local);
	}
	return best;
}

auto Viewport::onSplineClick() -> bool {
	const scene::Entity selected = m_parent != nullptr ? m_parent->getSelectedEntity() : scene::Entity{};
	auto& selection = gui::component::getSplineSelection();
	if (!selected || !selected.hasComponent<scene::component::Spline>()) {
		selection.clear();
		return false;
	}
	if (const auto handle = pickSplineHandle(); handle) {
		const bool add = input::Input::isKeyPressed(input::key::LeftControl) && handle->second == Handle::Point;
		if (handle->second == Handle::Point || !selection.contains(selected.getUUID(), handle->first))
			selection.select(selected.getUUID(), handle->first, add, handle->second);
		else
			selection.handle = handle->second;
		return true;
	}
	if (input::Input::isKeyPressed(input::key::LeftShift)) {
		if (const auto distance = pickSplineDistance(); distance) {
			const commands::CommandTarget target{.registry = &m_parent->getRegistry(),
												 .scene = mp_document->getActiveScene(),
												 .undo = mp_undoManager};
			const auto result = target.execute(
					"spline.insert_point", {{"entity", static_cast<int64_t>(static_cast<uint64_t>(selected.getUUID()))},
											{"distance", static_cast<double>(*distance)}});
			if (result) {
				if (const auto point = result->data.find("point"); point != result->data.end())
					if (const auto* index = std::get_if<int64_t>(&point->second); index != nullptr)
						selection.select(selected.getUUID(), static_cast<size_t>(*index));
			}
			return true;
		}
	}
	if (selection.entity == selected.getUUID() && !selection.points.empty()) {
		selection.clear();
		return true;
	}
	return false;
}

auto Viewport::renderSplineGizmo() -> bool {
	if (m_parent == nullptr || mp_document == nullptr)
		return false;
	const auto& scene = mp_document->getActiveScene();
	const scene::Entity selected = m_parent->getSelectedEntity();
	auto& selection = gui::component::getSplineSelection();
	if (!scene || !selected || !selected.hasComponent<scene::component::Spline>() ||
		selection.entity != selected.getUUID() || selection.points.empty() ||
		mp_document->state() != SceneDocument::State::Edit)
		return false;
	auto& curve = selected.getComponent<scene::component::Spline>().curve;
	const auto& points = curve.getPoints();
	selection.clamp(selected.getUUID(), points.size());
	if (selection.points.empty())
		return false;
	const math::mat4 world = scene->getWorldTransform(selected)();
	const Handle handle = selection.points.size() == 1 ? selection.handle : Handle::Point;
	math::vec3 centre{0.f, 0.f, 0.f};
	for (const auto index: selection.points) centre = centre + handlePosition(points[index], handle);
	centre = centre * (1.f / static_cast<float>(selection.points.size()));
	gui::Guizmo::initialize(m_lower, m_upper - m_lower,
							m_isometric || m_editorCamera.getProjectionMode() ==
												   renderer::CameraEditor::Projection::Orthographic);
	const auto& camera = getActiveCamera();
	math::mat4 cameraProjection = camera.getProjection();
	if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan)
		cameraProjection(1, 1) *= -1.f;
	math::mat4 gizmo = math::translate(math::identity<float, 4>(), transformPoint(world, centre));
	const bool snap = input::Input::isKeyPressed(input::key::LeftControl) || m_parent->getSettings().snapEnabled;
	gui::Guizmo::manipulate(camera.getView(), cameraProjection, gui::Guizmo::Type::Translation, gizmo,
							snap ? std::max(0.0001f, m_parent->getSettings().snapStep) : 0.f,
							gui::Guizmo::Space::World);
	const bool isUsing = gui::Guizmo::isUsing();
	if (isUsing && !m_splineWasUsing)
		m_splineBefore = points;
	if (isUsing && !m_splineBefore.empty()) {
		const math::vec3 moved = transformPoint(math::inverse(world), {gizmo(0, 3), gizmo(1, 3), gizmo(2, 3)});
		const math::vec3 before = [&]() -> math::vec3 {
			math::vec3 sum{0.f, 0.f, 0.f};
			for (const auto index: selection.points) sum = sum + handlePosition(m_splineBefore[index], handle);
			return sum * (1.f / static_cast<float>(selection.points.size()));
		}();
		const math::vec3 offset = moved - before;
		auto preview = m_splineBefore;
		for (const auto index: selection.points) {
			if (handle == Handle::Point)
				preview[index].position = preview[index].position + offset;
			else if (handle == Handle::TangentIn)
				preview[index].tangentIn = preview[index].tangentIn + offset;
			else
				preview[index].tangentOut = preview[index].tangentOut + offset;
		}
		curve.setPoints(std::move(preview));
	}
	if (!isUsing && m_splineWasUsing && !m_splineBefore.empty()) {
		const auto after = curve.getPoints();
		curve.setPoints(m_splineBefore);
		if (after != m_splineBefore) {
			const commands::CommandTarget target{.registry = &m_parent->getRegistry(),
												 .scene = scene,
												 .undo = mp_undoManager};
			const auto entity = static_cast<int64_t>(static_cast<uint64_t>(selected.getUUID()));
			bool done = false;
			if (handle == Handle::Point) {
				const auto first = selection.points.front();
				done = target.execute("spline.move_points",
									  {{"entity", entity},
									   {"points", commands::formatIndices(selection.points)},
									   {"offset", after[first].position - m_splineBefore[first].position}})
							   .has_value();
			} else {
				const auto index = selection.points.front();
				done = target.execute("spline.set_point", {{"entity", entity},
														   {"point", static_cast<int64_t>(index)},
														   {"tangent_in", after[index].tangentIn},
														   {"tangent_out", after[index].tangentOut}})
							   .has_value();
			}
			if (!done)
				curve.setPoints(after);
		}
		m_splineBefore.clear();
	}
	m_splineWasUsing = isUsing;
	return true;
}

}// namespace owl::nest::panel
