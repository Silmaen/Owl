/**
 * @file ViewportWalkArea.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"
#include "Viewport.h"
#include "document/SceneDocument.h"

#include "../commands/CommandRegistry.h"
#include "../commands/WalkAreaCommands.h"

#include <input/Input.h>
#include <math/matrixCreation.h>
#include <renderer/Renderer.h>
#include <scene/component/WalkArea.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace owl::nest::panel {

namespace {

using Vertex = gui::component::WalkAreaSelection::Vertex;
using WalkArea = scene::component::WalkArea;

auto toWorld(const math::mat4& iWorld, const math::vec2& iPoint) -> math::vec3 {
	const math::vec4 point = iWorld * math::vec4{iPoint.x(), iPoint.y(), 0.f, 1.f};
	return {point.x(), point.y(), point.z()};
}

void drawMarker(const math::vec3& iCenter, const float iSize, const math::vec4& iColor, const int iEntityId) {
	const math::vec3 a = iCenter + math::vec3{-iSize, -iSize, 0.f};
	const math::vec3 b = iCenter + math::vec3{iSize, -iSize, 0.f};
	const math::vec3 c = iCenter + math::vec3{iSize, iSize, 0.f};
	const math::vec3 d = iCenter + math::vec3{-iSize, iSize, 0.f};
	for (const auto& [from, to]: {std::pair{a, b}, std::pair{b, c}, std::pair{c, d}, std::pair{d, a}})
		renderer::Renderer2D::drawLine({.point1 = from, .point2 = to, .color = iColor, .entityId = iEntityId});
}

auto closestOnSegment(const math::vec2& iA, const math::vec2& iB, const math::vec2& iPoint) -> math::vec2 {
	const math::vec2 segment = iB - iA;
	const float lengthSq = segment.normSq();
	if (lengthSq <= 0.f)
		return iA;
	return iA + segment * std::clamp(((iPoint - iA) * segment) / lengthSq, 0.f, 1.f);
}

// Calls `iVisit(vertex, ring)` for every vertex of every ring of the area.
template<typename Visit>
void forEachVertex(const WalkArea& iArea, Visit&& iVisit) {
	for (size_t polygon = 0; polygon < iArea.polygons.size(); ++polygon) {
		const size_t rings = iArea.polygons[polygon].holes.size() + 1;
		for (size_t ring = 0; ring < rings; ++ring) {
			const auto& points = *iArea.getRing(polygon, ring);
			for (size_t index = 0; index < points.size(); ++index)
				iVisit(Vertex{.polygon = polygon, .ring = ring, .index = index}, points);
		}
	}
}

auto entityArg(const scene::Entity& iEntity) -> int64_t {
	return static_cast<int64_t>(static_cast<uint64_t>(iEntity.getUUID()));
}

}// namespace

void Viewport::renderWalkAreaOverlay(const shared<scene::Scene>& iScene) const {
	if (m_parent == nullptr || mp_document == nullptr || mp_document->state() != SceneDocument::State::Edit)
		return;
	const scene::Entity selected = m_parent->getSelectedEntity();
	if (!selected || !selected.hasComponent<WalkArea>())
		return;
	const auto& area = selected.getComponent<WalkArea>();
	auto& selection = gui::component::getWalkAreaSelection();
	selection.clamp(selected.getUUID(), area);
	const auto id = static_cast<int>(static_cast<uint32_t>(selected));
	const math::mat4 world = iScene->getWorldTransform(selected)();
	constexpr math::vec4 outlineColor{0.3f, 0.95f, 0.45f, 1.f};
	constexpr math::vec4 holeColor{1.f, 0.55f, 0.2f, 1.f};
	constexpr math::vec4 triangleColor{0.3f, 0.95f, 0.45f, 0.25f};
	constexpr math::vec4 vertexColor{1.f, 0.8f, 0.25f, 1.f};
	constexpr math::vec4 selectedColor{1.f, 1.f, 1.f, 1.f};
	const auto& mesh = area.getMesh();
	for (const auto& triangle: mesh.getTriangles()) {
		for (size_t edge = 0; edge < 3; ++edge) {
			if (triangle.neighbours[edge] < 0)
				continue;
			renderer::Renderer2D::drawLine(
					{.point1 = toWorld(world, mesh.getVertices()[triangle.vertices[edge]]),
					 .point2 = toWorld(world, mesh.getVertices()[triangle.vertices[(edge + 1) % 3]]),
					 .color = triangleColor,
					 .entityId = id});
		}
	}
	forEachVertex(area, [&](const Vertex& iVertex, const std::vector<math::vec2>& iRing) -> void {
		const math::vec3 position = toWorld(world, iRing[iVertex.index]);
		const math::vec3 next = toWorld(world, iRing[(iVertex.index + 1) % iRing.size()]);
		renderer::Renderer2D::drawLine({.point1 = position,
										.point2 = next,
										.color = iVertex.ring == 0 ? outlineColor : holeColor,
										.entityId = id});
		const float size = (m_isometric ? 0.15f : getLightMarkerRadius(position)) * 0.5f;
		const bool picked = selection.contains(selected.getUUID(), iVertex);
		drawMarker(position, size * (picked ? 1.5f : 1.f), picked ? selectedColor : vertexColor, id);
	});
	if (area.depthScale) {
		// The two heights of the scale by depth, as dashed lines across the floors.
		float minX = std::numeric_limits<float>::max();
		float maxX = -std::numeric_limits<float>::max();
		for (const auto& vertex: mesh.getVertices()) {
			minX = std::min(minX, vertex.x());
			maxX = std::max(maxX, vertex.x());
		}
		for (const float height: {area.nearY, area.farY}) {
			for (float x = minX; x < maxX; x += 0.5f)
				renderer::Renderer2D::drawLine({.point1 = toWorld(world, {x, height}),
												.point2 = toWorld(world, {std::min(x + 0.25f, maxX), height}),
												.color = {0.55f, 0.75f, 1.f, 0.8f},
												.entityId = id});
		}
	}
}

auto Viewport::pickWalkAreaPoint() const -> std::optional<math::vec2> {
	if (m_parent == nullptr || mp_document == nullptr)
		return std::nullopt;
	const auto& scene = mp_document->getActiveScene();
	const scene::Entity selected = m_parent->getSelectedEntity();
	const auto ray = getCursorRay();
	if (!scene || !selected || !selected.hasComponent<WalkArea>() || !ray)
		return std::nullopt;
	const math::mat4 toLocal = math::inverse(scene->getWorldTransform(selected)());
	const auto& [origin, direction] = *ray;
	const math::vec4 localOrigin = toLocal * math::vec4{origin.x(), origin.y(), origin.z(), 1.f};
	const math::vec4 localDirection = toLocal * math::vec4{direction.x(), direction.y(), direction.z(), 0.f};
	if (std::abs(localDirection.z()) <= 1e-6f)
		return std::nullopt;
	const float along = -localOrigin.z() / localDirection.z();
	return math::vec2{localOrigin.x() + localDirection.x() * along, localOrigin.y() + localDirection.y() * along};
}

auto Viewport::pickWalkAreaVertex() const -> std::optional<Vertex> {
	const auto point = pickWalkAreaPoint();
	if (!point)
		return std::nullopt;
	const auto& scene = mp_document->getActiveScene();
	const scene::Entity selected = m_parent->getSelectedEntity();
	const math::mat4 world = scene->getWorldTransform(selected)();
	const math::vec3 cursor = toWorld(world, *point);
	std::optional<Vertex> nearest;
	float nearestDistance = std::numeric_limits<float>::max();
	forEachVertex(selected.getComponent<WalkArea>(),
				  [&](const Vertex& iVertex, const std::vector<math::vec2>& iRing) -> void {
					  const math::vec3 position = toWorld(world, iRing[iVertex.index]);
					  const float distance = (position - cursor).norm();
					  const float radius = m_isometric ? 0.2f : getLightMarkerRadius(position) * 0.8f;
					  if (distance <= radius && distance < nearestDistance) {
						  nearestDistance = distance;
						  nearest = iVertex;
					  }
				  });
	return nearest;
}

auto Viewport::onWalkAreaClick() -> bool {
	const scene::Entity selected = m_parent != nullptr ? m_parent->getSelectedEntity() : scene::Entity{};
	auto& selection = gui::component::getWalkAreaSelection();
	if (!selected || !selected.hasComponent<WalkArea>()) {
		selection.clear();
		return false;
	}
	if (const auto vertex = pickWalkAreaVertex(); vertex) {
		selection.select(selected.getUUID(), *vertex, input::Input::isKeyPressed(input::key::LeftControl));
		return true;
	}
	const auto point = pickWalkAreaPoint();
	if (!point)
		return false;
	const commands::CommandTarget target{.registry = &m_parent->getRegistry(),
										 .scene = mp_document->getActiveScene(),
										 .undo = mp_undoManager};
	const auto& area = selected.getComponent<WalkArea>();
	if (input::Input::isKeyPressed(input::key::LeftShift)) {
		// The edge nearest to the cursor, within the pick radius.
		const math::mat4 world = mp_document->getActiveScene()->getWorldTransform(selected)();
		std::optional<Vertex> edge;
		math::vec2 onEdge = *point;
		float best = std::numeric_limits<float>::max();
		forEachVertex(area, [&](const Vertex& iVertex, const std::vector<math::vec2>& iRing) -> void {
			const math::vec2 closest =
					closestOnSegment(iRing[iVertex.index], iRing[(iVertex.index + 1) % iRing.size()], *point);
			const float distance = (toWorld(world, closest) - toWorld(world, *point)).norm();
			const float radius = m_isometric ? 0.2f : getLightMarkerRadius(toWorld(world, closest));
			if (distance <= radius && distance < best) {
				best = distance;
				edge = iVertex;
				onEdge = closest;
			}
		});
		if (!edge)
			return false;
		const auto result =
				target.execute("walk.insert_vertex", {{"entity", entityArg(selected)},
													  {"polygon", static_cast<int64_t>(edge->polygon)},
													  {"ring", static_cast<int64_t>(edge->ring)},
													  {"index", static_cast<int64_t>(edge->index + 1)},
													  {"position", math::vec3{onEdge.x(), onEdge.y(), 0.f}}});
		if (result)
			selection.select(selected.getUUID(),
							 {.polygon = edge->polygon, .ring = edge->ring, .index = edge->index + 1});
		return true;
	}
	if (input::Input::isKeyPressed(input::key::LeftAlt)) {
		// Inside a floor: a hole there; outside every floor: a new floor.
		std::optional<size_t> inside;
		for (size_t polygon = 0; polygon < area.polygons.size() && !inside; ++polygon) {
			scene::nav::WalkMesh single;
			single.build(std::span{&area.polygons[polygon], 1});
			if (single.contains(*point))
				inside = polygon;
		}
		const math::vec3 position{point->x(), point->y(), 0.f};
		if (inside)
			std::ignore = target.execute("walk.add_hole", {{"entity", entityArg(selected)},
														   {"polygon", static_cast<int64_t>(*inside)},
														   {"position", position}});
		else
			std::ignore = target.execute("walk.add_polygon", {{"entity", entityArg(selected)}, {"position", position}});
		selection.clear();
		return true;
	}
	if (selection.entity == selected.getUUID() && !selection.vertices.empty()) {
		selection.clear();
		return true;
	}
	return false;
}

auto Viewport::renderWalkAreaGizmo() -> bool {
	if (m_parent == nullptr || mp_document == nullptr)
		return false;
	const auto& scene = mp_document->getActiveScene();
	const scene::Entity selected = m_parent->getSelectedEntity();
	auto& selection = gui::component::getWalkAreaSelection();
	if (!scene || !selected || !selected.hasComponent<WalkArea>() || selection.entity != selected.getUUID() ||
		mp_document->state() != SceneDocument::State::Edit)
		return false;
	auto& area = selected.getComponent<WalkArea>();
	selection.clamp(selected.getUUID(), area);
	if (selection.vertices.empty())
		return false;
	const math::mat4 world = scene->getWorldTransform(selected)();
	const auto centreOf = [&selection](const WalkArea& iArea) -> math::vec2 {
		math::vec2 sum{0.f, 0.f};
		for (const auto& vertex: selection.vertices)
			sum = sum + (*iArea.getRing(vertex.polygon, vertex.ring))[vertex.index];
		return sum * (1.f / static_cast<float>(selection.vertices.size()));
	};
	gui::Guizmo::initialize(m_lower, m_upper - m_lower,
							m_isometric || m_editorCamera.getProjectionMode() ==
												   renderer::CameraEditor::Projection::Orthographic);
	const auto& camera = getActiveCamera();
	math::mat4 cameraProjection = camera.getProjection();
	if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan)
		cameraProjection(1, 1) *= -1.f;
	math::mat4 gizmo = math::translate(math::identity<float, 4>(), toWorld(world, centreOf(area)));
	const bool snap = input::Input::isKeyPressed(input::key::LeftControl) || m_parent->getSettings().snapEnabled;
	gui::Guizmo::manipulate(camera.getView(), cameraProjection, gui::Guizmo::Type::Translation, gizmo,
							snap ? std::max(0.0001f, m_parent->getSettings().snapStep) : 0.f,
							gui::Guizmo::Space::World);
	const bool isUsing = gui::Guizmo::isUsing();
	if (isUsing && !m_walkWasUsing)
		m_walkBefore = area.polygons;
	if (isUsing && !m_walkBefore.empty()) {
		const math::vec4 moved = math::inverse(world) * math::vec4{gizmo(0, 3), gizmo(1, 3), gizmo(2, 3), 1.f};
		WalkArea before;
		before.polygons = m_walkBefore;
		const math::vec2 offset = math::vec2{moved.x(), moved.y()} - centreOf(before);
		area.polygons = m_walkBefore;
		for (const auto& vertex: selection.vertices) {
			auto& point = (*area.getRing(vertex.polygon, vertex.ring))[vertex.index];
			point = point + offset;
		}
	}
	if (!isUsing && m_walkWasUsing && !m_walkBefore.empty()) {
		const auto after = area.polygons;
		area.polygons = m_walkBefore;
		const auto& first = selection.vertices.front();
		const math::vec2 offset = (*area.getRing(first.polygon, first.ring))[first.index];
		WalkArea moved;
		moved.polygons = after;
		const math::vec2 delta = (*moved.getRing(first.polygon, first.ring))[first.index] - offset;
		if (delta.normSq() > 0.f) {
			const commands::CommandTarget target{.registry = &m_parent->getRegistry(),
												 .scene = scene,
												 .undo = mp_undoManager};
			if (!target.execute("walk.move_vertices", {{"entity", entityArg(selected)},
													   {"vertices", commands::formatWalkVertices(selection.vertices)},
													   {"offset", math::vec3{delta.x(), delta.y(), 0.f}}}))
				area.polygons = after;
		}
		m_walkBefore.clear();
	}
	m_walkWasUsing = isUsing;
	return true;
}

}// namespace owl::nest::panel
