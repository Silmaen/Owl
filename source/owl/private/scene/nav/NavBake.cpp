/**
 * @file NavBake.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "app/EngineContext.h"
#include "renderer/MeshCache.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"
#include "scene/nav/NavBake.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace owl::scene::nav {

namespace {

// The entities holding every component given, in the order of their UUID.
template<typename... Components>
auto sortedEntities(const Scene& iScene) -> std::vector<Entity> {
	std::vector<uint64_t> uuids;
	for (const auto handle: iScene.registry.view<component::ID, Components...>())
		uuids.push_back(static_cast<uint64_t>(iScene.registry.get<component::ID>(handle).id));
	std::ranges::sort(uuids);
	std::vector<Entity> entities;
	entities.reserve(uuids.size());
	for (const auto uuid: uuids) entities.push_back(iScene.findEntityByUUID(core::UUID{uuid}));
	return entities;
}

auto movesItself(const Entity& iEntity) -> bool {
	if (iEntity.hasComponent<component::NavAgent>() || iEntity.hasComponent<component::NavObstacle>() ||
		iEntity.hasComponent<component::SplineFollower>())
		return true;
	return iEntity.hasComponent<component::PhysicBody>() &&
		   iEntity.getComponent<component::PhysicBody>().body.type != SceneBody::BodyType::Static;
}

// An entity that moves, or whose parent or ancestor moves, is no static geometry.
auto isMoving(const Scene& iScene, const Entity& iEntity) -> bool {
	Entity current = iEntity;
	for (size_t depth = 0; current && depth < 256; ++depth) {
		if (movesItself(current))
			return true;
		const auto parentId = current.getComponent<component::Hierarchy>().parentId;
		if (parentId == core::UUID{0})
			return false;
		current = iScene.findEntityByUUID(parentId);
	}
	return false;
}

auto findMesh(const Scene& iScene, const component::MeshRenderer& iRenderer) -> shared<renderer::MeshResource> {
	if (iRenderer.mesh != nullptr)
		return iRenderer.mesh;
	if (iScene.getEngineContext() == nullptr || iRenderer.meshPath.empty())
		return nullptr;
	return iScene.getEngineContext()->getMeshCache().get(iRenderer.meshPath);
}

void addMesh(const Scene& iScene, const Entity& iEntity, const math::mat4& iWorld, NavGeometry& ioGeometry) {
	if (!iEntity.hasComponent<component::MeshRenderer>())
		return;
	const auto mesh = findMesh(iScene, iEntity.getComponent<component::MeshRenderer>());
	if (mesh == nullptr || mesh->getGeometry().isEmpty()) {
		OWL_CORE_WARN("NavMesh: Entity '{}' has no mesh to read, skipped by the bake.", iEntity.getName())
		return;
	}
	const auto& geometry = mesh->getGeometry();
	std::vector<math::vec3> positions;
	positions.reserve(geometry.vertices.size());
	for (const auto& vertex: geometry.vertices) positions.push_back(vertex.position);
	ioGeometry.addTriangles(positions, geometry.indices, iWorld);
}

void addCollider(const Scene& iScene, const Entity& iEntity, const SceneBody& iBody, const math::mat4& iWorld,
				 NavGeometry& ioGeometry) {
	const auto& size = iBody.colliderSize;
	switch (iBody.shape) {
		case SceneBody::Shape::Box:
			ioGeometry.addBox(size, iWorld);
			break;
		case SceneBody::Shape::Sphere:
			ioGeometry.addCylinder(size.x(), size.x(), iWorld);
			break;
		case SceneBody::Shape::Capsule:
		case SceneBody::Shape::Cylinder:
			ioGeometry.addCylinder(size.x(), size.y(), iWorld);
			break;
		case SceneBody::Shape::ConvexHull:
		case SceneBody::Shape::Mesh:
			addMesh(iScene, iEntity, iWorld, ioGeometry);
			break;
	}
}

}// namespace

void getNavVolumeBounds(const Scene& iScene, const Entity& iVolume, math::vec3& oMin, math::vec3& oMax) {
	const auto& volume = iVolume.getComponent<component::NavMeshVolume>();
	const math::mat4 world = iScene.getWorldTransform(iVolume)();
	const math::vec3 centre{world(0, 3), world(1, 3), world(2, 3)};
	const math::vec3 half = volume.size * 0.5f;
	oMin = centre - half;
	oMax = centre + half;
}

auto collectNavGeometry(const Scene& iScene, const Entity& iVolume) -> NavGeometry {
	OWL_PROFILE_FUNCTION()

	NavGeometry geometry;
	if (!iVolume || !iVolume.hasComponent<component::NavMeshVolume>())
		return geometry;
	const auto sources = iVolume.getComponent<component::NavMeshVolume>().sources;
	const bool colliders = sources != component::NavMeshVolume::Sources::Meshes;
	const bool meshes = sources != component::NavMeshVolume::Sources::Colliders;
	for (const auto& entity: sortedEntities<component::Transform>(iScene)) {
		if (entity == iVolume || isMoving(iScene, entity) || !iScene.isEffectivelyVisible(entity, false))
			continue;
		const math::mat4 world = iScene.getWorldTransform(entity)();
		if (entity.hasComponent<component::PhysicBody>()) {
			const auto& body = entity.getComponent<component::PhysicBody>().body;
			if (colliders && body.type == SceneBody::BodyType::Static &&
				body.dimension == SceneBody::Dimension::Space && !body.sensor)
				addCollider(iScene, entity, body, world, geometry);
			continue;
		}
		if (meshes)
			addMesh(iScene, entity, world, geometry);
	}
	return geometry;
}

auto collectWalkTriangles(const Scene& iScene, const Entity& iVolume, std::vector<math::vec3>& oVertices,
						  std::vector<uint32_t>& oIndices) -> bool {
	oVertices.clear();
	oIndices.clear();
	if (!iVolume || !iVolume.hasComponent<component::NavMeshVolume>())
		return false;
	math::vec3 boundsMin;
	math::vec3 boundsMax;
	getNavVolumeBounds(iScene, iVolume, boundsMin, boundsMax);
	for (const auto& entity: sortedEntities<component::WalkArea>(iScene)) {
		const math::mat4 world = iScene.getWorldTransform(entity)();
		const math::vec2 origin{world(0, 3), world(1, 3)};
		if (origin.x() < boundsMin.x() || origin.x() > boundsMax.x() || origin.y() < boundsMin.y() ||
			origin.y() > boundsMax.y())
			continue;
		const auto& mesh = entity.getComponent<component::WalkArea>().getMesh();
		const auto first = static_cast<uint32_t>(oVertices.size());
		for (const auto& vertex: mesh.getVertices())
			oVertices.emplace_back(vertex.x() + origin.x(), 0.f, vertex.y() + origin.y());
		for (const auto& triangle: mesh.getTriangles()) {
			auto corners = triangle.vertices;
			// Counter-clockwise in the plane (x, z) seen from above, as the voxel bake writes them.
			const auto& a = oVertices[first + corners[0]];
			const auto& b = oVertices[first + corners[1]];
			const auto& c = oVertices[first + corners[2]];
			if ((b.z() - a.z()) * (c.x() - a.x()) - (b.x() - a.x()) * (c.z() - a.z()) > 0.f)
				std::swap(corners[1], corners[2]);
			for (const auto corner: corners) oIndices.push_back(first + corner);
		}
	}
	return !oIndices.empty();
}

auto bakeNavMesh(Scene& ioScene, const Entity& iVolume) -> owl::expected<void, NavBuildError> {
	OWL_PROFILE_FUNCTION()

	if (!iVolume || !iVolume.hasComponent<component::NavMeshVolume>()) {
		OWL_CORE_WARN("NavMesh: Bake asked on an entity without a NavMeshVolume.")
		return owl::unexpected{NavBuildError::NoGeometry};
	}
	math::vec3 boundsMin;
	math::vec3 boundsMax;
	getNavVolumeBounds(ioScene, iVolume, boundsMin, boundsMax);
	auto& volume = iVolume.getComponent<component::NavMeshVolume>();
	if (volume.isWalkPlane()) {
		std::vector<math::vec3> vertices;
		std::vector<uint32_t> indices;
		if (!collectWalkTriangles(ioScene, iVolume, vertices, indices))
			return owl::unexpected{NavBuildError::NoGeometry};
		volume.vertices = std::move(vertices);
		volume.indices = std::move(indices);
		OWL_CORE_INFO("NavMesh: '{}' baked from its walk areas, {} triangles.", iVolume.getName(),
					  volume.indices.size() / 3)
		return {};
	}
	NavMesh mesh;
	if (auto built = mesh.build(collectNavGeometry(ioScene, iVolume), volume.settings, boundsMin, boundsMax); !built)
		return owl::unexpected{built.error()};
	volume.vertices = mesh.getVertices();
	volume.indices = mesh.getIndices();
	OWL_CORE_INFO("NavMesh: '{}' baked, {} triangles.", iVolume.getName(), volume.indices.size() / 3)
	return {};
}

auto collectNavLinks(const Scene& iScene) -> std::vector<NavMeshLink> {
	std::vector<NavMeshLink> links;
	for (const auto& entity: sortedEntities<component::NavLink>(iScene)) {
		const auto& link = entity.getComponent<component::NavLink>();
		const math::mat4 world = iScene.getWorldTransform(entity)();
		const math::vec3 start{world(0, 3), world(1, 3), world(2, 3)};
		links.push_back({.start = start,
						 .end = start + link.end,
						 .bidirectional = link.bidirectional,
						 .costScale = link.costScale});
	}
	return links;
}

auto findNavVolume(const Scene& iScene, const math::vec3& iPoint) -> Entity {
	for (const auto& entity: sortedEntities<component::NavMeshVolume>(iScene)) {
		math::vec3 boundsMin;
		math::vec3 boundsMax;
		getNavVolumeBounds(iScene, entity, boundsMin, boundsMax);
		if (iPoint.x() >= boundsMin.x() && iPoint.x() <= boundsMax.x() && iPoint.y() >= boundsMin.y() &&
			iPoint.y() <= boundsMax.y() && iPoint.z() >= boundsMin.z() && iPoint.z() <= boundsMax.z())
			return entity;
	}
	return {};
}

}// namespace owl::scene::nav
