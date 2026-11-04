/**
 * @file JoltWorld.h
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "scene/SceneBody.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace owl::physics {

class SolverTaskPool;

/**
 * @brief
 *  Position and rotation of a body in the XY plane.
 */
struct BodyPose {
	/// Position, in world units.
	math::vec2f position{0.f, 0.f};
	/// Rotation around Z, in radians, in [-pi, pi].
	float angle = 0.f;
};

/**
 * @brief
 *  One box of a body's collider, in the body frame.
 */
struct BodyBox {
	/// Half size along X and Y.
	math::vec2f halfExtents{0.5f, 0.5f};
	/// Centre of the box in the body frame.
	math::vec2f offset{0.f, 0.f};
};

/**
 * @brief
 *  What a body is made of when it is created.
 */
struct BodyDescription {
	/// Static, dynamic or kinematic.
	scene::SceneBody::BodyType type = scene::SceneBody::BodyType::Static;
	/// The body does not rotate.
	bool fixedRotation = false;
	/// Initial pose.
	BodyPose pose;
	/// Collider: one box or more (a tilemap gets one per collidable cell).
	std::vector<BodyBox> boxes;
	/// Mass per unit of area.
	float density = 1.f;
	/// Friction coefficient.
	float friction = 0.5f;
	/// Restitution coefficient.
	float restitution = 0.f;
};

/**
 * @brief
 *  Two bodies whose colliders started or stopped touching during a step.
 */
struct BodyContact {
	/// First body.
	uint32_t bodyA = 0;
	/// Second body.
	uint32_t bodyB = 0;
};

/**
 * @brief
 *  The contacts that began and ended during a step.
 */
struct BodyContacts {
	/// Begun contacts, one per pair of touching boxes.
	std::vector<BodyContact> begins;
	/// Ended contacts, one per pair of boxes that stopped touching.
	std::vector<BodyContact> ends;
};

/**
 * @brief
 *  A Jolt physics system holding the 2D bodies of one scene, locked to the XY plane.
 *
 * The only file that includes Jolt (it is compiled with Jolt's instruction set); everything here is in engine types.
 * Bodies are named by their Jolt body id. Results are bit-exact on every target and for every worker count.
 */
class JoltWorld final {
public:
	JoltWorld(const JoltWorld&) = delete;

	JoltWorld(JoltWorld&&) = delete;

	auto operator=(const JoltWorld&) -> JoltWorld& = delete;

	auto operator=(JoltWorld&&) -> JoltWorld& = delete;

	/**
	 * @brief
	 *  Create the physics system.
	 * @param[in] ipPool The worker threads of the steps, or nullptr to step on the calling thread.
	 */
	explicit JoltWorld(SolverTaskPool* ipPool);

	/**
	 * @brief
	 *  Destroy the bodies and the physics system.
	 */
	~JoltWorld();

	/**
	 * @brief
	 *  Create a body and add it to the world.
	 * @param[in] iDescription The body.
	 * @return The body id, or nullopt when the world is full or the collider invalid.
	 */
	[[nodiscard]] auto createBody(const BodyDescription& iDescription) -> std::optional<uint32_t>;

	/**
	 * @brief
	 *  Remove a body from the world and destroy it.
	 * @param[in] iBody The body id.
	 */
	void destroyBody(uint32_t iBody);

	/**
	 * @brief
	 *  Run one step.
	 * @param[in] iSeconds Duration of the step.
	 * @param[in] iCollisionSteps Collision and integration passes inside the step, at least 1.
	 */
	void step(float iSeconds, uint32_t iCollisionSteps);

	/**
	 * @brief
	 *  Hand over the contacts of the steps since the last call, sorted by body and box so the order is the same on
	 *  every run and for every worker count.
	 * @return The begun and ended contacts.
	 */
	[[nodiscard]] auto takeContacts() -> BodyContacts;

	/**
	 * @brief
	 *  Pose of a body.
	 * @param[in] iBody The body id.
	 * @return Its pose.
	 */
	[[nodiscard]] auto getPose(uint32_t iBody) const -> BodyPose;

	/**
	 * @brief
	 *  Move a body, waking it.
	 * @param[in] iBody The body id.
	 * @param[in] iPose The new pose.
	 */
	void setPose(uint32_t iBody, const BodyPose& iPose);

	/**
	 * @brief
	 *  Move a body without waking it nor changing its velocity.
	 * @param[in] iBody The body id.
	 * @param[in] iPosition The new position.
	 */
	void setPositionQuietly(uint32_t iBody, const math::vec2f& iPosition);

	/**
	 * @brief
	 *  Linear velocity of a body.
	 * @param[in] iBody The body id.
	 * @return The velocity.
	 */
	[[nodiscard]] auto getLinearVelocity(uint32_t iBody) const -> math::vec2f;

	/**
	 * @brief
	 *  Set the linear velocity of a body, waking it when the velocity is not zero.
	 * @param[in] iBody The body id.
	 * @param[in] iVelocity The velocity.
	 */
	void setLinearVelocity(uint32_t iBody, const math::vec2f& iVelocity);

	/**
	 * @brief
	 *  Angular velocity of a body around Z.
	 * @param[in] iBody The body id.
	 * @return The velocity, in radians per second.
	 */
	[[nodiscard]] auto getAngularVelocity(uint32_t iBody) const -> float;

	/**
	 * @brief
	 *  Set the angular velocity of a body around Z.
	 * @param[in] iBody The body id.
	 * @param[in] iVelocity The velocity, in radians per second.
	 */
	void setAngularVelocity(uint32_t iBody, float iVelocity);

	/**
	 * @brief
	 *  Apply an impulse at the centre of mass, waking the body.
	 * @param[in] iBody The body id.
	 * @param[in] iImpulse The impulse.
	 */
	void addImpulse(uint32_t iBody, const math::vec2f& iImpulse);

	/**
	 * @brief
	 *  Set how much of the gravity applies to a body, waking it.
	 * @param[in] iBody The body id.
	 * @param[in] iFactor The gravity factor.
	 */
	void setGravityFactor(uint32_t iBody, float iFactor);

	/**
	 * @brief
	 *  Check if a body is simulated (not sleeping).
	 * @param[in] iBody The body id.
	 * @return True if awake.
	 */
	[[nodiscard]] auto isAwake(uint32_t iBody) const -> bool;

	/**
	 * @brief
	 *  Wake a body.
	 * @param[in] iBody The body id.
	 */
	void wake(uint32_t iBody);

	/**
	 * @brief
	 *  Pose that keeps, around a teleported body, the motion it had between two steps.
	 * @param[in] iPrevious The pose before the last step.
	 * @param[in] iCurrent The pose after the last step.
	 * @param[in] iMoved The pose it was moved to.
	 * @return The previous pose carried along with the move.
	 */
	[[nodiscard]] static auto carryPrevious(const BodyPose& iPrevious, const BodyPose& iCurrent, const BodyPose& iMoved)
			-> BodyPose;

private:
	struct Impl;
	/// The Jolt objects.
	uniq<Impl> mp_impl;
};

}// namespace owl::physics
