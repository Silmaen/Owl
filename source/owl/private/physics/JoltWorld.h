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
#include "scene/SceneJoint.h"

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
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
 *  Position and rotation of a body in space.
 */
struct SpacePose {
	/// Position, in world units.
	math::vec3f position{0.f, 0.f, 0.f};
	/// Rotation, a unit quaternion (x, y, z, w).
	math::vec4f rotation{0.f, 0.f, 0.f, 1.f};
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
	/// 2D (XY plane, `pose`) or 3D (`spacePose`).
	scene::SceneBody::Dimension dimension = scene::SceneBody::Dimension::Plane;
	/// Initial pose of a 2D body.
	BodyPose pose;
	/// Initial pose of a 3D body.
	SpacePose spacePose;
	/// Collider of a 2D body: one box or more (a tilemap gets one per collidable cell); empty for `shape`.
	std::vector<BodyBox> boxes;
	/// Collider of a 3D body, and of a 2D one without `boxes`.
	scene::SceneBody::Shape shape = scene::SceneBody::Shape::Box;
	/// Full size of the `shape` collider (see `scene::SceneBody::Shape`), the entity scale included.
	math::vec3f size{1.f, 1.f, 1.f};
	/// Mass per unit of area in 2D, of volume in 3D.
	float density = 1.f;
	/// Friction coefficient.
	float friction = 0.5f;
	/// Restitution coefficient.
	float restitution = 0.f;
	/// The body detects the others without stopping them.
	bool sensor = false;
	/// Collision layer, 0 to `scene::SceneBody::layerCount - 1`.
	uint8_t layer = 0;
	/// Layers the body collides with, one bit per layer.
	uint8_t mask = scene::SceneBody::allLayers;
	/// Vertices of a `ConvexHull` or `Mesh` collider, in the body frame, the entity scale included.
	std::vector<math::vec3f> meshVertices{};
	/// Triangles of a `Mesh` collider, three indices into `meshVertices` each.
	std::vector<uint32_t> meshIndices{};
};

/**
 * @brief
 *  Two bodies whose colliders started or stopped touching during a step.
 */
struct BodyContact {
	/// First body.
	uint32_t bodyA = 0;
	/// Sub shape of the first body.
	uint32_t shapeA = 0;
	/// Second body.
	uint32_t bodyB = 0;
	/// Sub shape of the second body.
	uint32_t shapeB = 0;

	/**
	 * @brief
	 *  Order of the contacts: by bodies, then by shapes.
	 * @return The order.
	 */
	auto operator<=>(const BodyContact&) const = default;
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
 *  The closest body met by a ray or a moving volume.
 */
struct QueryHit {
	/// The body.
	uint32_t body = 0;
	/// Contact point, in the world.
	math::vec3f point{0.f, 0.f, 0.f};
	/// Surface normal at the point, pointing out of the body.
	math::vec3f normal{0.f, 0.f, 0.f};
	/// Distance travelled to the hit.
	float distance = 0.f;
};

/**
 * @brief
 *  The volume of a shape cast or of an overlap query.
 */
struct QueryVolume {
	/// A sphere of radius `halfExtents.x`, or a box of `halfExtents`.
	bool sphere = true;
	/// Radius of the sphere, half size of the box.
	math::vec3f halfExtents{0.5f, 0.5f, 0.5f};
	/// Rotation of the box, a unit quaternion (x, y, z, w).
	math::vec4f rotation{0.f, 0.f, 0.f, 1.f};
};

/**
 * @brief
 *  A joint between two bodies, or a body and the world, in world space.
 */
struct JointDescription {
	/// What the joint lets the bodies do.
	scene::SceneJoint::Type type = scene::SceneJoint::Type::Hinge;
	/// First body.
	uint32_t body = 0;
	/// Second body, none for the world.
	std::optional<uint32_t> other;
	/// Joint point (the anchor on the first body for a distance joint).
	math::vec3f point{0.f, 0.f, 0.f};
	/// Distance joint: the anchor on the second body.
	math::vec3f otherPoint{0.f, 0.f, 0.f};
	/// Unit hinge axis or slide direction.
	math::vec3f axis{0.f, 0.f, 1.f};
	/// Limits of the motion (see `scene::SceneJoint`).
	bool limited = false;
	/// Smallest angle, position or length.
	float lower = 0.f;
	/// Largest angle, position or length.
	float upper = 0.f;
	/// Whether the motor runs.
	bool motor = false;
	/// Motor target speed.
	float motorSpeed = 0.f;
	/// Largest motor torque or force.
	float motorForce = 100.f;
	/// Spring frequency of a distance joint, 0 for rigid.
	float springFrequency = 0.f;
	/// Spring damping ratio of a distance joint.
	float springDamping = 0.f;
};

/**
 * @brief
 *  A Jolt physics system holding the bodies of one scene: 2D ones locked to the XY plane, 3D ones free.
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
	 *  Bytes of the whole simulation state: bodies, sleep, contact cache. For this process only (no format version).
	 * @return The state.
	 */
	[[nodiscard]] auto saveState() const -> std::vector<uint8_t>;

	/**
	 * @brief
	 *  Put back a state of `saveState()`; the world must hold the same bodies as when it was saved.
	 * @param[in] iState The state.
	 * @return False if the bytes do not match the bodies of the world (nothing usable was restored then).
	 */
	[[nodiscard]] auto restoreState(std::span<const uint8_t> iState) -> bool;

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
	 *  Pose of a 3D body.
	 * @param[in] iBody The body id.
	 * @return Its pose.
	 */
	[[nodiscard]] auto getSpacePose(uint32_t iBody) const -> SpacePose;

	/**
	 * @brief
	 *  Move a 3D body, waking it.
	 * @param[in] iBody The body id.
	 * @param[in] iPose The new pose.
	 */
	void setSpacePose(uint32_t iBody, const SpacePose& iPose);

	/**
	 * @brief
	 *  Linear velocity of a body in space.
	 * @param[in] iBody The body id.
	 * @return The velocity.
	 */
	[[nodiscard]] auto getSpaceVelocity(uint32_t iBody) const -> math::vec3f;

	/**
	 * @brief
	 *  Set the linear velocity of a body in space, waking it when the velocity is not zero.
	 * @param[in] iBody The body id.
	 * @param[in] iVelocity The velocity.
	 */
	void setSpaceVelocity(uint32_t iBody, const math::vec3f& iVelocity);

	/**
	 * @brief
	 *  Apply an impulse in space at the centre of mass, waking the body.
	 * @param[in] iBody The body id.
	 * @param[in] iImpulse The impulse.
	 */
	void addSpaceImpulse(uint32_t iBody, const math::vec3f& iImpulse);

	/**
	 * @brief
	 *  Angular velocity of a body in space.
	 * @param[in] iBody The body id.
	 * @return The velocity, in radians per second around each axis.
	 */
	[[nodiscard]] auto getSpaceAngularVelocity(uint32_t iBody) const -> math::vec3f;

	/**
	 * @brief
	 *  Set the angular velocity of a body in space.
	 * @param[in] iBody The body id.
	 * @param[in] iVelocity The velocity, in radians per second around each axis.
	 */
	void setSpaceAngularVelocity(uint32_t iBody, const math::vec3f& iVelocity);

	/**
	 * @brief
	 *  The closest body a ray meets; sensors and the bodies whose layer is not in the mask are ignored.
	 * @param[in] iOrigin Start of the ray.
	 * @param[in] iDirection Unit direction.
	 * @param[in] iDistance Length of the ray.
	 * @param[in] iMask Layers looked at, one bit per layer.
	 * @return The hit, none when the ray meets nothing.
	 */
	[[nodiscard]] auto castRay(const math::vec3f& iOrigin, const math::vec3f& iDirection, float iDistance,
							   uint8_t iMask) const -> std::optional<QueryHit>;

	/**
	 * @brief
	 *  The closest body a volume meets when it moves; sensors and the bodies outside the mask are ignored.
	 * @param[in] iVolume The volume.
	 * @param[in] iOrigin Start position of the volume centre.
	 * @param[in] iDirection Unit direction.
	 * @param[in] iDistance Length of the move.
	 * @param[in] iMask Layers looked at, one bit per layer.
	 * @return The hit, none when the volume meets nothing.
	 */
	[[nodiscard]] auto castVolume(const QueryVolume& iVolume, const math::vec3f& iOrigin, const math::vec3f& iDirection,
								  float iDistance, uint8_t iMask) const -> std::optional<QueryHit>;

	/**
	 * @brief
	 *  The bodies overlapping a volume; sensors and the bodies outside the mask are ignored.
	 * @param[in] iVolume The volume.
	 * @param[in] iCentre Position of the volume centre.
	 * @param[in] iMask Layers looked at, one bit per layer.
	 * @return The bodies, sorted, each once.
	 */
	[[nodiscard]] auto overlap(const QueryVolume& iVolume, const math::vec3f& iCentre, uint8_t iMask) const
			-> std::vector<uint32_t>;

	/**
	 * @brief
	 *  Create a joint and add it to the world.
	 * @param[in] iDescription The joint.
	 * @return The joint id, none when a body is missing.
	 */
	[[nodiscard]] auto createJoint(const JointDescription& iDescription) -> std::optional<uint32_t>;

	/**
	 * @brief
	 *  Remove a joint from the world.
	 * @param[in] iJoint The joint id.
	 */
	void destroyJoint(uint32_t iJoint);

	/**
	 * @brief
	 *  Run or stop the motor of a hinge or a slider, waking its bodies.
	 * @param[in] iJoint The joint id.
	 * @param[in] iEnabled Whether the motor runs.
	 * @param[in] iSpeed Target speed, radians or units per second.
	 */
	void setJointMotor(uint32_t iJoint, bool iEnabled, float iSpeed);

	/**
	 * @brief
	 *  Angle of a hinge from its rest pose, or position of a slider along its axis.
	 * @param[in] iJoint The joint id.
	 * @return The angle in radians or the position; 0 for the other joints.
	 */
	[[nodiscard]] auto getJointPosition(uint32_t iJoint) const -> float;

	/**
	 * @brief
	 *  Rotation of Euler angles applied X, then Y, then Z, as `math::Transform` composes them.
	 * @param[in] iEuler The angles, in radians.
	 * @return The unit quaternion.
	 */
	[[nodiscard]] static auto toRotation(const math::vec3f& iEuler) -> math::vec4f;

	/**
	 * @brief
	 *  Euler angles of a rotation, the inverse of `toRotation`.
	 * @param[in] iRotation The unit quaternion.
	 * @return The angles, in radians.
	 */
	[[nodiscard]] static auto toEuler(const math::vec4f& iRotation) -> math::vec3f;

	/**
	 * @brief
	 *  Euler angles of a rotation seen from a parent.
	 * @param[in] iParentEuler The parent world rotation, Euler angles.
	 * @param[in] iRotation The world rotation, a unit quaternion.
	 * @return The local rotation, Euler angles.
	 */
	[[nodiscard]] static auto toLocalEuler(const math::vec3f& iParentEuler, const math::vec4f& iRotation)
			-> math::vec3f;

	/**
	 * @brief
	 *  Pose between two poses of a 3D body.
	 * @param[in] iFrom The pose at 0.
	 * @param[in] iTo The pose at 1.
	 * @param[in] iAlpha The blend factor, in [0, 1].
	 * @return The blended pose.
	 */
	[[nodiscard]] static auto blend(const SpacePose& iFrom, const SpacePose& iTo, float iAlpha) -> SpacePose;

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
