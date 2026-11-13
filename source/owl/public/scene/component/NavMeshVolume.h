/**
 * @file NavMeshVolume.h
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "core/UUID.h"
#include "math/vectors.h"
#include "scene/nav/NavMesh.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace owl::scene::component {

/**
 * @brief
 *  A box of a 3D scene whose floors are cut into a navigation mesh, walked by the `NavAgent`s.
 *
 * The box is centred on the entity and aligned on the world axes (the rotation of the entity is ignored). A bake
 * (the **Bake** button of Owl Nest, the `nav.bake` command, Lua `nav.bake`) reads the static geometry inside it: the
 * colliders of the static bodies and the meshes of the `MeshRenderer`s without a body, except what moves (agents,
 * obstacles, spline followers, bodies that are not static, and their children). With `WalkAreas` it reads the
 * floors of the `WalkArea`s whose entity stands inside the box instead: the mesh of a 2D room, in the plane (x, y) of
 * the areas, stored with the plane height as Z (see `toMeshSpace`). The baked triangles are saved with the scene, so a game loads the same mesh on every target; the
 * `NavLink`s are attached to it when it is read.
 */
struct OWL_API NavMeshVolume {
	/// The geometry a bake reads.
	enum struct Sources : uint8_t {
		CollidersAndMeshes,///< Static colliders, and meshes of the entities without a body.
		Colliders,///< Static colliders only.
		Meshes,///< Meshes only.
		WalkAreas,///< The floors of the `WalkArea`s inside the box (a 2D room, plane (x, y)).
	};

	/// Full size of the box, centred on the entity.
	math::vec3 size{20.f, 6.f, 20.f};
	/// How the floors are cut.
	nav::NavMeshSettings settings;
	/// The geometry a bake reads.
	Sources sources = Sources::CollidersAndMeshes;
	/// Baked corners, in world space (in the space of `toMeshSpace` for `WalkAreas`).
	std::vector<math::vec3> vertices;
	/// Baked triangles, three indices in `vertices` each.
	std::vector<uint32_t> indices;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Nav Mesh Volume"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "NavMeshVolume"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);

	/**
	 * @brief
	 *  Get the navigation mesh of the baked triangles, read again when they changed.
	 * @return The mesh.
	 */
	[[nodiscard]] auto getMesh() const -> const nav::NavMesh&;

	/**
	 * @brief
	 *  Attach links to the mesh when they differ from the attached ones.
	 * @param[in] iLinks The links, in world space.
	 */
	void setLinks(const std::vector<nav::NavMeshLink>& iLinks) const;

	/**
	 * @brief
	 *  Check if the volume holds a baked mesh.
	 * @return True when triangles were baked.
	 */
	[[nodiscard]] auto isBaked() const -> bool { return indices.size() >= 3; }

	/**
	 * @brief
	 *  Check if the mesh lies in the plane (x, y) of walk areas.
	 * @return True for the `WalkAreas` sources.
	 */
	[[nodiscard]] auto isWalkPlane() const -> bool { return sources == Sources::WalkAreas; }

	/**
	 * @brief
	 *  A world point in the space of the mesh: unchanged, or (x, 0, y) for a mesh of walk areas, whose floor is the
	 *  plane (x, z) of a navigation mesh.
	 * @param[in] iPoint The world point.
	 * @return The point the mesh is searched with.
	 */
	[[nodiscard]] auto toMeshSpace(const math::vec3& iPoint) const -> math::vec3 {
		return isWalkPlane() ? math::vec3{iPoint.x(), 0.f, iPoint.y()} : iPoint;
	}

	/**
	 * @brief
	 *  A point of the mesh in world space: unchanged, or (x, z, 0) for a mesh of walk areas.
	 * @param[in] iPoint The point of the mesh.
	 * @return The world point.
	 */
	[[nodiscard]] auto toWorldSpace(const math::vec3& iPoint) const -> math::vec3 {
		return isWalkPlane() ? math::vec3{iPoint.x(), iPoint.z(), 0.f} : iPoint;
	}

	/// Runtime: the mesh read from `vertices` and `indices`.
	mutable nav::NavMesh m_mesh;
	/// Runtime: the corners the mesh was read from.
	mutable std::vector<math::vec3> m_meshVertices;
	/// Runtime: the triangles the mesh was read from.
	mutable std::vector<uint32_t> m_meshIndices;
	/// Runtime: whether the mesh was read once.
	mutable bool m_meshBuilt = false;
	/// Runtime: the links attached to the mesh.
	mutable std::vector<nav::NavMeshLink> m_links;
};

/**
 * @brief
 *  An actor walking on a `NavMeshVolume`: a goal, a path around the walls and over the links, the others avoided.
 *
 * `nav.go_to` (Lua) gives it a goal; the `owl.nav_agents` system finds the path, steers along it on the fixed tick at
 * up to `speed`, keeps clear of the other agents and of the `NavObstacle`s (`avoidance`), crosses the links in a
 * straight line, keeps the entity on the surface and turns it the way it walks (`turn`). The entity origin is the
 * feet of the actor. Its speed goes to the `speedParameter` of its `Animator` (or of its first child holding one).
 */
struct OWL_API NavAgent {
	/// Entity holding the `NavMeshVolume` (0: the first volume holding the agent).
	core::UUID volume{0};
	/// Highest speed, in world units per second.
	float speed = 3.5f;
	/// Largest change of velocity per second.
	float acceleration = 10.f;
	/// Radius of the actor, for the avoidance.
	float radius = 0.4f;
	/// Distance from the goal at which the agent stops.
	float stoppingDistance = 0.1f;
	/// Whether the agent steers clear of the other agents and of the obstacles.
	bool avoidance = true;
	/// Whether the entity turns around Y the way it walks.
	bool turn = true;
	/// Float parameter of the `Animator` that receives the speed (empty: none).
	std::string speedParameter{"speed"};

	/// Runtime: the corners left to walk.
	std::vector<nav::NavPathPoint> m_path;
	/// Runtime: index of the next corner in `m_path`.
	size_t m_next = 0;
	/// Runtime: the goal asked for, waiting for a path.
	math::vec3 m_target;
	/// Runtime: whether a path to `m_target` is to be found.
	bool m_pendingTarget = false;
	/// Runtime: velocity on the plane (x, z).
	math::vec2 m_velocity;
	/// Runtime: the triangle under the agent, -1 when unknown.
	int32_t m_triangle = -1;
	/// Runtime: distance crossed on the current link.
	float m_linkProgress = 0.f;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Nav Agent"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "NavAgent"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);

	/**
	 * @brief
	 *  Ask for a goal: the path is found at the next tick.
	 * @param[in] iTarget The goal, in world space.
	 */
	void goTo(const math::vec3& iTarget) {
		m_target = iTarget;
		m_pendingTarget = true;
	}

	/**
	 * @brief
	 *  Check if the agent is walking or about to.
	 * @return True while corners are left or a goal waits for its path.
	 */
	[[nodiscard]] auto isMoving() const -> bool { return m_pendingTarget || m_next < m_path.size(); }

	/**
	 * @brief
	 *  Stop where the agent stands.
	 */
	void stop() {
		m_path.clear();
		m_next = 0;
		m_pendingTarget = false;
		m_linkProgress = 0.f;
	}
};

/**
 * @brief
 *  A moving obstacle the `NavAgent`s steer around: a crate pushed around, a cart, a monster without a path.
 *
 * A vertical cylinder at the entity origin. Its velocity is read from its moves between two ticks; the agents take
 * the whole avoidance against it. It does not cut the navigation mesh: a wall that stays belongs to the bake.
 */
struct OWL_API NavObstacle {
	/// Radius of the cylinder.
	float radius = 0.5f;
	/// Height of the cylinder, drawn by the overlay.
	float height = 1.f;

	/// Runtime: world position at the last tick.
	math::vec3 m_lastPosition;
	/// Runtime: velocity on the plane (x, z).
	math::vec2 m_velocity;
	/// Runtime: whether `m_lastPosition` was read.
	bool m_started = false;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Nav Obstacle"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "NavObstacle"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

/**
 * @brief
 *  A jump, a ladder or a gap: a link of the navigation meshes from the entity origin to an end point.
 *
 * The agents cross it in a straight line at their speed. It is attached to every `NavMeshVolume` whose mesh is
 * within reach of both ends, without a new bake.
 */
struct OWL_API NavLink {
	/// End point, relative to the entity origin (in world axes).
	math::vec3 end{0.f, 0.f, 2.f};
	/// Whether the link is crossed both ways.
	bool bidirectional = true;
	/// Cost of the crossing per unit of length, at least 1 (walking costs 1).
	float costScale = 1.f;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Nav Link"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "NavLink"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
