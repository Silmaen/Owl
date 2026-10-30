/**
 * @file Spline.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Macros.h"
#include "core/Serializer.h"
#include "core/UUID.h"
#include "math/SplineCurve.h"
#include "math/vectors.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace owl::scene::component {

/**
 * @brief
 *  A path through control points: a road, a river, a rail, the route of a patrol or of a camera.
 *
 * The points live in the space of the entity (its `Transform` moves, turns and scales the whole path). The curve is
 * sampled by distance (`sample`); its lanes are paths beside it, at a lateral offset (to the right when looking along
 * the curve with `up` above) and a height along `up`. A `SplineFollower` moves an entity along it. In Owl Nest each
 * point is a handle in the viewport.
 */
struct OWL_API Spline {
	/**
	 * @brief
	 *  A path beside the curve: a lane of a road, a track of a rail, a line of a formation.
	 */
	struct OWL_API Lane {
		/// Name the followers and the scripts use.
		std::string name;
		/// Lateral offset, in the units of the entity: positive to the right when looking along the curve.
		float offset{0.f};
		/// Offset along `up`, in the units of the entity.
		float height{0.f};

		OWL_DIAG_PUSH
		OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
		/**
		 * @brief
		 *  Compare two lanes field by field.
		 * @param[in] iOther The other lane.
		 * @return True when every field is equal.
		 */
		auto operator==(const Lane& iOther) const -> bool = default;
		OWL_DIAG_POP
	};

	/// The curve: its points, interpolation, closure and sampling resolution.
	math::SplineCurve curve;
	/// Lanes beside the curve.
	std::vector<Lane> lanes;
	/// Up direction of the path, in the space of the entity: (0, 1, 0) in 3D, (0, 0, 1) for a 2D path.
	math::vec3 up{0.f, 1.f, 0.f};

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Spline"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Spline"; }

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
	 *  Find a lane by name.
	 * @param[in] iName The lane name.
	 * @return The lane, or null when the spline has none of that name.
	 */
	[[nodiscard]] auto findLane(std::string_view iName) const -> const Lane*;

	/**
	 * @brief
	 *  Unit lateral direction (to the right) for a direction of the curve.
	 * @param[in] iTangent Unit direction of the curve.
	 * @return The lateral direction, perpendicular to the tangent and to `up` (null when they are parallel).
	 */
	[[nodiscard]] auto getLateral(const math::vec3& iTangent) const -> math::vec3;

	/**
	 * @brief
	 *  Position and direction at a distance along a lane, in the space of the entity.
	 * @param[in] iDistance Distance along the curve: clamped on an open curve, wrapped on a closed one.
	 * @param[in] iLane Lane name; empty or unknown for the curve itself.
	 * @param[in] iOffset Extra lateral offset added to the lane's.
	 * @return The sample (its distance is measured on the curve, not on the lane).
	 */
	[[nodiscard]] auto sample(float iDistance, std::string_view iLane = {}, float iOffset = 0.f) const
			-> math::SplineSample;
};

/**
 * @brief
 *  Moves an entity along a `Spline`: a train on its rail, a car in its lane, a camera on its track.
 *
 * The `owl.spline_followers` system advances the distance at `speed` world units per second each frame and places the
 * entity there, in the lane named `lane` plus `offset`. At an end of an open spline it stops (`Once`), goes back to the
 * start (`Loop`) or turns round (`PingPong`); a closed spline loops in every mode but `Once`, which stops after a
 * lap. With `orient`, the entity faces the direction of the curve: its -Z axis in 3D, its +X axis on a 2D path
 * (`up` along Z).
 */
struct OWL_API SplineFollower {
	/// What the entity does at the end of the spline.
	enum struct Mode : uint8_t {
		Once,///< Stops at the end.
		Loop,///< Starts again from the other end.
		PingPong,///< Turns round.
	};

	/// Whether the entity moves; off leaves it where it is.
	bool enabled = true;
	/// Entity holding the `Spline` (0: none).
	core::UUID spline{0};
	/// Lane followed; empty for the curve itself.
	std::string lane;
	/// Extra lateral offset, in the units of the spline entity.
	float offset = 0.f;
	/// Speed along the curve, in units of the spline per second; negative goes backwards.
	float speed = 2.f;
	/// Distance along the curve where the entity starts.
	float startDistance = 0.f;
	/// What the entity does at the end of the spline.
	Mode mode = Mode::Loop;
	/// Whether the entity faces the direction of the curve.
	bool orient = true;

	/// Runtime: the start distance was read.
	bool m_started = false;
	/// Runtime: distance along the curve.
	float m_distance = 0.f;
	/// Runtime: +1 forwards, -1 backwards (ping-pong).
	float m_direction = 1.f;
	/// Runtime: distance travelled since the start (ends a lap of `Once` on a closed spline).
	float m_travelled = 0.f;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Spline Follower"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "SplineFollower"; }

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
