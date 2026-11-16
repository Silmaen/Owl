/**
 * @file LagCompensation.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>

namespace owl::network {

/**
 * @brief
 *  What a rewound ray hit.
 */
struct OWL_API RewindHit {
	/// UUID of the entity hit.
	uint64_t entity = 0;
	/// Distance along the ray to the hit sphere.
	float distance = 0.f;
};

/**
 * @brief
 *  Recent positions of the hit spheres, so that the server judges a shot against the world the shooter saw.
 */
class OWL_API HitHistory final {
public:
	/**
	 * @brief
	 *  Keep where a hit sphere is at a time.
	 * @param[in] iTime Time of the server, in seconds.
	 * @param[in] iEntity UUID of the entity.
	 * @param[in] iCentre World position of the sphere.
	 * @param[in] iRadius Radius of the sphere.
	 */
	void record(double iTime, uint64_t iEntity, const math::vec3& iCentre, float iRadius);

	/**
	 * @brief
	 *  Forget the positions older than a time, and the entities gone since.
	 * @param[in] iOldest The oldest time to keep, in seconds.
	 */
	void prune(double iOldest);

	/**
	 * @brief
	 *  Forget an entity (destroyed).
	 * @param[in] iEntity UUID of the entity.
	 */
	void forget(uint64_t iEntity) { m_entities.erase(iEntity); }

	/**
	 * @brief
	 *  Forget every entity.
	 */
	void clear() { m_entities.clear(); }

	/**
	 * @brief
	 *  Where an entity's sphere was at a time, interpolated between the kept positions.
	 * @param[in] iEntity UUID of the entity.
	 * @param[in] iTime Time of the server, in seconds.
	 * @return The position (the oldest or newest one outside the history), none for an unknown entity.
	 */
	[[nodiscard]] auto getPositionAt(uint64_t iEntity, double iTime) const -> std::optional<math::vec3>;

	/**
	 * @brief
	 *  The nearest hit sphere a ray crosses, every sphere where it was at a time.
	 * @param[in] iTime Time of the server the ray is judged at, in seconds.
	 * @param[in] iOrigin Origin of the ray.
	 * @param[in] iDirection Direction of the ray (normalized here).
	 * @param[in] iMaxDistance Length of the ray.
	 * @param[in] iIgnored UUID of an entity the ray ignores (the shooter), 0 for none.
	 * @return The hit, none when the ray crosses no sphere.
	 */
	[[nodiscard]] auto raycast(double iTime, const math::vec3& iOrigin, const math::vec3& iDirection,
							   float iMaxDistance, uint64_t iIgnored = 0) const -> std::optional<RewindHit>;

	/**
	 * @brief
	 *  Number of entities with a history.
	 * @return The count.
	 */
	[[nodiscard]] auto getEntityCount() const -> size_t { return m_entities.size(); }

private:
	/// A kept position.
	struct Sample {
		/// Time of the server, in seconds.
		double time = 0.0;
		/// World position of the sphere.
		math::vec3 centre{0.f, 0.f, 0.f};
	};
	/// The history of one entity.
	struct Track {
		/// Radius of its sphere.
		float radius = 0.f;
		/// Positions, oldest first.
		std::deque<Sample> samples;
	};
	/// Histories by UUID.
	std::unordered_map<uint64_t, Track> m_entities;
};

}// namespace owl::network
