/**
 * @file WalkArea.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "core/UUID.h"
#include "math/vectors.h"
#include "scene/nav/WalkMesh.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace owl::scene::component {

/**
 * @brief
 *  A 2D floor where actors walk freely: polygons with holes on the plane (x, y) of the entity.
 *
 * The polygons live in the space of the entity; `getMesh` cuts them into triangles the first time it is asked after a
 * change. Walking up the plane (a greater y) moves an actor away from the viewer: with `depthScale` it shrinks from
 * `nearScale` at `nearY` to `farScale` at `farY` (and beyond, linearly); with `depthSort` its Z follows its y, so that
 * the sprites and meshes in front of it are drawn over it and the `WalkBehind` masks hide it once it walks behind
 * their baseline. `WalkAgent` entities walk on it; Lua queries it through `walk.*`. In Owl Nest every vertex is a
 * handle in the viewport.
 */
struct OWL_API WalkArea {
	/// Floors of the area, in the space of the entity.
	std::vector<nav::WalkPolygon> polygons;
	/// Whether the agents are scaled by their height on the plane.
	bool depthScale = false;
	/// Height on the plane (y, in the space of the entity) where an agent has `nearScale`.
	float nearY = 0.f;
	/// Scale of an agent at `nearY`.
	float nearScale = 1.f;
	/// Height on the plane where an agent has `farScale`.
	float farY = 10.f;
	/// Scale of an agent at `farY`.
	float farScale = 0.5f;
	/// Whether the Z of the agents and of the masks follows their height on the plane.
	bool depthSort = false;
	/// World Z gained for each unit walked down the plane (towards the viewer), with `depthSort`.
	float depthPerUnit = 0.1f;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Walk Area"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "WalkArea"; }

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
	 *  Get the triangles of the polygons, cut again when the polygons changed.
	 * @return The mesh.
	 */
	[[nodiscard]] auto getMesh() const -> const nav::WalkMesh&;

	/**
	 * @brief
	 *  Get the scale of an agent at a height of the plane.
	 * @param[in] iY Height, in the space of the entity.
	 * @return The scale (1 without `depthScale`, never below 0.01).
	 */
	[[nodiscard]] auto getScaleAt(float iY) const -> float;

	/**
	 * @brief
	 *  Get the Z offset of an agent or a mask at a height of the plane.
	 * @param[in] iY Height, in the space of the entity.
	 * @return The offset from the Z of the entity: 0 at `nearY`, positive below it (0 without `depthSort`).
	 */
	[[nodiscard]] auto getDepthAt(float iY) const -> float;

	/**
	 * @brief
	 *  Get the vertices of a ring of a polygon.
	 * @param[in] iPolygon Index of the polygon.
	 * @param[in] iRing 0 for the outline, `1 + n` for the hole `n`.
	 * @return The vertices, null when the ring does not exist.
	 */
	[[nodiscard]] auto getRing(size_t iPolygon, size_t iRing) -> std::vector<math::vec2>*;

	/**
	 * @brief
	 *  Get the vertices of a ring of a polygon.
	 * @param[in] iPolygon Index of the polygon.
	 * @param[in] iRing 0 for the outline, `1 + n` for the hole `n`.
	 * @return The vertices, null when the ring does not exist.
	 */
	[[nodiscard]] auto getRing(size_t iPolygon, size_t iRing) const -> const std::vector<math::vec2>*;

	/// Runtime: the triangles, cut from `m_meshSource`.
	mutable nav::WalkMesh m_mesh;
	/// Runtime: the polygons the triangles were cut from.
	mutable std::vector<nav::WalkPolygon> m_meshSource;
	/// Runtime: whether the triangles were cut once.
	mutable bool m_meshBuilt = false;
};

/**
 * @brief
 *  An actor walking on a `WalkArea`: a click on the floor, a path around the holes, a walk at `speed`.
 *
 * `walk.go_to` (Lua) gives it a goal; the `owl.walk_agents` system finds the path, moves the entity along it, turns it
 * the way it walks (`facing`), scales it by its height on the plane and sets its Z (when the area does), and gives the
 * speed to the `speedParameter` of its `Animator` (or of the first child that has one). The entity origin is the feet
 * of the actor. Its scale at the start of Play is its scale at a factor 1.
 */
struct OWL_API WalkAgent {
	/// How the entity turns the way it walks.
	enum struct Facing : uint8_t {
		None,///< Never turned.
		MirrorX,///< A sprite: the X scale is negative while walking left.
		TurnY,///< A mesh seen from the front: turned around Y, facing +Z towards the viewer when walking down.
	};

	/// Entity holding the `WalkArea` (0: none).
	core::UUID area{0};
	/// Walking speed, in world units per second (at a scale factor of 1).
	float speed = 2.f;
	/// Whether the speed follows the scale by depth (slower far away).
	bool scaleSpeed = true;
	/// How the entity turns the way it walks.
	Facing facing = Facing::None;
	/// Float parameter of the `Animator` that receives the speed (empty: none).
	std::string speedParameter{"speed"};

	/// Runtime: the corners left to walk, in world space.
	std::vector<math::vec2> m_path;
	/// Runtime: index of the next corner in `m_path`.
	size_t m_next = 0;
	/// Runtime: the scale of the entity at a factor 1.
	math::vec3 m_baseScale{1.f, 1.f, 1.f};
	/// Runtime: whether `m_baseScale` was read.
	bool m_started = false;
	/// Runtime: speed of the last frame.
	float m_currentSpeed = 0.f;
	/// Runtime: last horizontal direction (-1 left, 1 right).
	float m_sideways = 1.f;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Walk Agent"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "WalkAgent"; }

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
	 *  Check if the agent is walking.
	 * @return True while corners are left.
	 */
	[[nodiscard]] auto isMoving() const -> bool { return m_next < m_path.size(); }

	/**
	 * @brief
	 *  Stop where the agent stands.
	 */
	void stop() {
		m_path.clear();
		m_next = 0;
	}
};

/**
 * @brief
 *  A walk-behind mask: a part of the backdrop (a pillar, a hedge, a table) drawn over the actors walking behind it.
 *
 * Put on a sprite cut from the backdrop. The `owl.walk_agents` system sets its Z to the depth its `WalkArea` gives to
 * the `baseline` height: an actor whose feet are above the line (further away) is behind it, one below the line in
 * front of it. Needs `depthSort` on the area; a sprite on a depth-tested 2D layer drawn after a mesh layer hides the
 * meshes behind it the same way.
 */
struct OWL_API WalkBehind {
	/// Entity holding the `WalkArea` (0: none).
	core::UUID area{0};
	/// Height of the line on the plane, in the space of the area.
	float baseline = 0.f;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Walk Behind"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "WalkBehind"; }

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
