/**
 * @file PhysicCommand.h
 * @author Silmaen
 * @date 12/27/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "physics/PhysicsSettings.h"
#include "scene/PhysicsSnapshot.h"
#include "scene/Scene.h"
#include "scene/SceneBody.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

/**
 * @brief
 *  Namespace for phyisics management.
 */
namespace owl::physics {
/**
 * @brief
 *  Physics of the scenes: each scene owns its Jolt world (2D bodies locked to the XY plane), created by `init()` and reached through the
 *  scene (scene-level calls) or the entity (body calls).
 */
class OWL_API PhysicCommand final {
public:
	PhysicCommand() = delete;

	~PhysicCommand() = delete;

	PhysicCommand(const PhysicCommand&) = delete;

	PhysicCommand(PhysicCommand&&) = delete;

	auto operator=(const PhysicCommand&) -> PhysicCommand& = delete;

	auto operator=(PhysicCommand&&) -> PhysicCommand& = delete;

	/**
	 * @brief
	 *  Create the physical world of a scene, replacing the one it already has.
	 *
	 * The world belongs to the scene: several scenes simulate side by side, and the scene destructor
	 * destroys it. From then on the bodies follow their components: a `PhysicBody` added while running gets
	 * its body at the next `step()`, and removing a `PhysicBody`, `Tilemap`, raycast door or pushwall
	 * component (or its entity) destroys the matching body.
	 * @param[in,out] ioScene The scene whose world is created.
	 */
	static void init(scene::Scene& ioScene);

	/**
	 * @brief
	 *  Destroy the world of a scene. Does nothing when the scene has none.
	 * @param[in,out] ioScene The scene whose world is destroyed.
	 */
	static void destroy(scene::Scene& ioScene);

	/**
	 * @brief
	 *  Check if a scene has a physical world.
	 * @param[in] iScene The scene to check.
	 * @return True if its world exists.
	 */
	[[nodiscard]] static auto isInitialized(const scene::Scene& iScene) -> bool;

	/**
	 * @brief
	 *  Run one fixed step of a scene's world, of the tick duration of its simulation clock.
	 *
	 * Bodies of the `PhysicBody` components added since the last step are created first. The begun contacts are
	 * gathered for `takeCollisionEvents()`; the entity transforms are left as they are (`writeFrameTransforms()`,
	 * `syncSimulatedTransforms()`). The `owl.physics` system calls it once per tick of `Scene::onUpdateRuntime()`.
	 * @param[in,out] ioScene The scene whose world is stepped.
	 */
	static void step(scene::Scene& ioScene);

	/**
	 * @brief
	 *  Write the bodies' poses into the entity transforms for drawing.
	 *
	 * With interpolation on, the poses are blended between the last two steps by the alpha of the scene's simulation
	 * clock, so a frame without a step still moves them; without it, the poses of the last step are written when a
	 * step ran since the previous call. Also sets `getLastFrameStepCount()` and `getInterpolationAlpha()`.
	 * @param[in,out] ioScene The scene whose transforms are written.
	 */
	static void writeFrameTransforms(scene::Scene& ioScene);

	/**
	 * @brief
	 *  Advance the world of a scene by one rendered frame, outside of the scene's systems.
	 *
	 * The frame duration is given to the scene's simulation clock (`Scene::getSimulationClock()`), which runs as
	 * many fixed steps as it holds, at most `maxTicksPerFrame` (the excess time is dropped); then
	 * `writeFrameTransforms()`. The contact events of all the steps are gathered, a pair being reported at most once
	 * per frame. `Scene::onUpdateRuntime()` does not use it: it advances the clock itself and steps per tick.
	 * @param[in,out] ioScene The scene whose world is stepped.
	 * @param[in] iTimestep The duration of the rendered frame.
	 */
	static void frame(scene::Scene& ioScene, const core::Timestep& iTimestep);

	/**
	 * @brief
	 *  Settings of the running world of a scene.
	 * @param[in] iScene The scene.
	 * @return The clamped settings read from the scene at `init()`, or the defaults when it has no world.
	 */
	[[nodiscard]] static auto getSettings(const scene::Scene& iScene) -> PhysicsSettings;

	/**
	 * @brief
	 *  Number of threads running the physics solver of a scene.
	 * @param[in] iScene The scene.
	 * @return 1 for the single-threaded solver, the task pool size when multi-threaded, 0 without a world.
	 */
	[[nodiscard]] static auto getWorkerCount(const scene::Scene& iScene) -> uint32_t;

	/**
	 * @brief
	 *  Number of fixed steps run before the last `writeFrameTransforms()` (or `frame()`) call on a scene.
	 * @param[in] iScene The scene.
	 * @return The step count, 0 without a world.
	 */
	[[nodiscard]] static auto getLastFrameStepCount(const scene::Scene& iScene) -> uint32_t;

	/**
	 * @brief
	 *  Blend factor used for the transforms written by the last `writeFrameTransforms()` call on a scene.
	 * @param[in] iScene The scene.
	 * @return The alpha of the scene's simulation clock, in [0, 1); 1 when interpolation is off.
	 */
	[[nodiscard]] static auto getInterpolationAlpha(const scene::Scene& iScene) -> float;

	/**
	 * @brief
	 *  Write the state of the last fixed step, not the interpolated one, into the entity transforms.
	 *
	 * Used before serialising a running scene (save game) so the saved positions match the saved
	 * velocities. The next `writeFrameTransforms()` writes interpolated transforms again.
	 * @param[in,out] ioScene The scene whose transforms are written.
	 */
	static void syncSimulatedTransforms(scene::Scene& ioScene);

	/**
	 * @brief
	 *  Move every body of the scene's world back by a shift of the world origin.
	 *
	 * Velocities, contacts and sleep are kept, and the two poses the interpolation blends move too, so the next frame
	 * draws the bodies where they were. `Scene::shiftWorldOrigin()` calls it.
	 * @param[in,out] ioScene The scene whose bodies move.
	 * @param[in] iShift Displacement of the world origin, in world units.
	 */
	static void shiftOrigin(scene::Scene& ioScene, const math::vec2f& iShift);

	/**
	 * @brief
	 *  Two entities whose bodies started or stopped touching during a physics step.
	 */
	struct CollisionEvent {
		/// UUID of the first entity of the pair.
		core::UUID entityA;
		/// UUID of the second entity of the pair.
		core::UUID entityB;
		/// True when the bodies stopped touching (`on_collision_exit`, `on_trigger_exit`).
		bool ended = false;
		/// True when one of the bodies is a sensor (`on_trigger_enter`, `on_trigger_exit`).
		bool sensor = false;
	};

	/**
	 * @brief
	 *  Hand over the collisions that began or ended during the steps since the last call on a scene, and clear them.
	 *
	 * Built from the contacts Jolt adds and removes. A pair begins once when its first contact begins and ends when
	 * its last one ends: contacts between further shapes of the same two entities (tilemap cells) report nothing more.
	 * A pair that ends and begins again within one frame reports nothing; a body destroyed while touching ends its
	 * pairs.
	 * @param[in,out] ioScene The scene.
	 * @return The events in the order they happened, sorted by body within a step: the same order on every run and
	 *  worker count.
	 */
	[[nodiscard]] static auto takeCollisionEvents(scene::Scene& ioScene) -> std::vector<CollisionEvent>;

	/**
	 * @brief
	 *  What a ray or a moving shape met first.
	 */
	struct QueryHit {
		/// UUID of the entity of the body met.
		core::UUID entity;
		/// Contact point, in the world.
		math::vec3f point{0.f, 0.f, 0.f};
		/// Surface normal at the point, out of the body met.
		math::vec3f normal{0.f, 0.f, 0.f};
		/// Distance travelled to the hit.
		float distance = 0.f;
	};

	/**
	 * @brief
	 *  A sphere (a disc in the XY plane) or a box, for the shape casts and overlap queries.
	 */
	struct QueryShape {
		/// A sphere of radius `halfExtents.x`, or a box of `halfExtents` (a 2D box is one unit deep).
		bool sphere = true;
		/// Radius of the sphere, half size of the box.
		math::vec3f halfExtents{0.5f, 0.5f, 0.5f};
		/// Euler angles of the box, in radians (only Z for a 2D query).
		math::vec3f rotation{0.f, 0.f, 0.f};
	};

	/**
	 * @brief
	 *  The first body a ray meets in a scene's world, sensors ignored.
	 *
	 * A 2D query is a 3D one in the plane z = 0: it meets the 2D bodies and the 3D ones crossing the plane.
	 * @param[in] iScene The scene.
	 * @param[in] iOrigin Start of the ray.
	 * @param[in] iDirection Direction of the ray (normalized here).
	 * @param[in] iDistance Length of the ray.
	 * @param[in] iMask Collision layers looked at, one bit per layer.
	 * @return The hit, none when nothing is met or the scene has no world.
	 */
	[[nodiscard]] static auto raycast(const scene::Scene& iScene, const math::vec3f& iOrigin,
									  const math::vec3f& iDirection, float iDistance,
									  uint8_t iMask = scene::SceneBody::allLayers) -> std::optional<QueryHit>;

	/**
	 * @brief
	 *  The first body a shape meets when it moves through a scene's world, sensors ignored.
	 * @param[in] iScene The scene.
	 * @param[in] iShape The shape.
	 * @param[in] iOrigin Start position of the shape centre.
	 * @param[in] iDirection Direction of the move (normalized here).
	 * @param[in] iDistance Length of the move.
	 * @param[in] iMask Collision layers looked at, one bit per layer.
	 * @return The hit, none when nothing is met or the scene has no world.
	 */
	[[nodiscard]] static auto shapeCast(const scene::Scene& iScene, const QueryShape& iShape,
										const math::vec3f& iOrigin, const math::vec3f& iDirection, float iDistance,
										uint8_t iMask = scene::SceneBody::allLayers) -> std::optional<QueryHit>;

	/**
	 * @brief
	 *  The entities whose bodies overlap a shape in a scene's world, sensors ignored.
	 * @param[in] iScene The scene.
	 * @param[in] iShape The shape.
	 * @param[in] iCentre Position of the shape centre.
	 * @param[in] iMask Collision layers looked at, one bit per layer.
	 * @return The entities, sorted by body, each once.
	 */
	[[nodiscard]] static auto overlap(const scene::Scene& iScene, const QueryShape& iShape, const math::vec3f& iCentre,
									  uint8_t iMask = scene::SceneBody::allLayers) -> std::vector<core::UUID>;

	/**
	 * @brief
	 *  Run or stop the motor of the hinge or slider joint of an entity (`component::PhysicJoint`).
	 * @param[in] iEntity The entity with the joint.
	 * @param[in] iEnabled Whether the motor runs.
	 * @param[in] iSpeed Target speed, radians or units per second.
	 */
	static void setJointMotor(const scene::Entity& iEntity, bool iEnabled, float iSpeed);

	/**
	 * @brief
	 *  Angle of the hinge joint of an entity from its rest pose, or position of its slider.
	 * @param[in] iEntity The entity with the joint.
	 * @return The angle in radians or the position; 0 for another joint or without one.
	 */
	[[nodiscard]] static auto getJointPosition(const scene::Entity& iEntity) -> float;

	/**
	 * @brief
	 *  A joint broken by its load (`SceneJoint::breakForce`, `breakTorque`).
	 */
	struct JointBreak {
		/// UUID of the entity carrying the joint.
		core::UUID entity;
		/// UUID of the entity it joined, 0 for the world.
		core::UUID connected;
		/// Force the joint held when it broke, in newtons.
		float force = 0.f;
		/// Torque the joint held when it broke, in newton metres.
		float torque = 0.f;
	};

	/**
	 * @brief
	 *  Hand over and clear the joints broken during the steps (`on_joint_break`).
	 * @param[in,out] ioScene The scene.
	 * @return The broken joints, in the order they broke.
	 */
	[[nodiscard]] static auto takeJointBreaks(scene::Scene& ioScene) -> std::vector<JointBreak>;

	/**
	 * @brief
	 *  Push the body of an entity with a force over the next step: at its centre of mass, or at the entity for a part
	 *  of an assembly (a thruster turns the assembly it pushes).
	 * @param[in] iEntity The entity; a static body is left alone.
	 * @param[in] iForce The force, in newtons (a 2D body keeps X and Y).
	 */
	static void addForce(const scene::Entity& iEntity, const math::vec3f& iForce);

	/**
	 * @brief
	 *  Push the body of an entity with a force applied at a world point over the next step: off the centre of mass, it
	 *  also turns the body.
	 * @param[in] iEntity The entity; a static body is left alone.
	 * @param[in] iForce The force, in newtons (a 2D body keeps X and Y).
	 * @param[in] iPoint The world point where it applies.
	 */
	static void addForceAtPoint(const scene::Entity& iEntity, const math::vec3f& iForce, const math::vec3f& iPoint);

	/**
	 * @brief
	 *  Turn the body of an entity with a torque over the next step.
	 * @param[in] iEntity The entity; a static body is left alone.
	 * @param[in] iTorque The torque, in newton metres (a 2D body keeps Z).
	 */
	static void addTorque(const scene::Entity& iEntity, const math::vec3f& iTorque);

	/**
	 * @brief
	 *  Merge the collider of an entity into the body of its nearest ancestor with a `PhysicBody` (`SceneBody::part`):
	 *  its own body goes, the assembly's mass, centre of mass and inertia are recomputed, its joints move to the
	 *  assembly.
	 * @param[in] iEntity The entity with a `PhysicBody`.
	 * @return False without an ancestor body of the same dimension (the entity keeps its own body).
	 */
	static auto attach(const scene::Entity& iEntity) -> bool;

	/**
	 * @brief
	 *  Take a part out of its assembly: it gets a body of its own, moving at the speed its point had, and the assembly
	 *  is rebuilt without it.
	 * @param[in] iEntity The part.
	 */
	static void detach(const scene::Entity& iEntity);

	/**
	 * @brief
	 *  Get the entity whose body carries a part.
	 * @param[in] iEntity The entity.
	 * @return The root of its assembly, 0 when the entity is not a merged part.
	 */
	[[nodiscard]] static auto getAssemblyRoot(const scene::Entity& iEntity) -> core::UUID;

	/**
	 * @brief
	 *  Get the mass of the body an entity moves (the whole assembly for a part).
	 * @param[in] iEntity The entity.
	 * @return The mass, 0 for a static or kinematic body or without a world.
	 */
	[[nodiscard]] static auto getMass(const scene::Entity& iEntity) -> float;

	/**
	 * @brief
	 *  Get the centre of mass of the body an entity moves (the whole assembly for a part).
	 * @param[in] iEntity The entity.
	 * @return The centre of mass in the world, the origin without a body.
	 */
	[[nodiscard]] static auto getCenterOfMass(const scene::Entity& iEntity) -> math::vec3f;


	/**
	 * @brief
	 *  Snapshot of the simulation of a scene's world, for rewind and rollback.
	 *
	 * Holds every body (pose, velocities, sleep), the contact cache and the touching pairs of the collision events: a
	 * `restoreState()` followed by the same steps gives the same world, bit for bit, and the same events. The bytes are
	 * for this process only: not a save format.
	 * @param[in] iScene The scene.
	 * @return The state, empty when the scene has no world.
	 */
	[[nodiscard]] static auto saveState(const scene::Scene& iScene) -> std::vector<uint8_t>;

	/**
	 * @brief
	 *  Put back a state of `saveState()` into a scene's world, which must hold the same bodies as when it was saved.
	 *
	 * The entity transforms follow at the next `writeFrameTransforms()`; the collisions not yet taken are dropped.
	 * @param[in,out] ioScene The scene.
	 * @param[in] iState The state.
	 * @return False without a world, or when the bodies changed since the save (the world is then left as it was
	 * partly restored: restore another state or call `init()`).
	 */
	[[nodiscard]] static auto restoreState(scene::Scene& ioScene, std::span<const uint8_t> iState) -> bool;

	/**
	 * @brief
	 *  Remove the physics bodies owned by an entity (PhysicBody, tilemap, raycast door or pushwall body).
	 *
	 * Component removal already does it; this drops the bodies of an entity that stays in the scene. No-op
	 * when the entity's scene has no world or the entity owns no body.
	 * @param[in] iEntity The entity whose bodies are removed.
	 */
	static void destroyBody(const scene::Entity& iEntity);

	/**
	 * @brief
	 *  Apply an impulsion to the given entity (if Entity supports it).
	 * @param iEntity The Entity where to apply impulse.
	 * @param iImpulse The impulse force and direction.
	 */
	static void impulse(const scene::Entity& iEntity, const math::vec2f& iImpulse);

	/**
	 * @brief
	 *  Get the velocity of the given entity.
	 * @param iEntity The entity to check.
	 * @return The entity velocity or null.
	 */
	static auto getVelocity(const scene::Entity& iEntity) -> math::vec2f;

	/**
	 * @brief
	 *  Set the transform (position and rotation) of the given entity's physics body.
	 * @param iEntity The entity to modify.
	 * @param iPosition The new position.
	 * @param iRotation The new rotation angle (radians).
	 */
	static void setTransform(const scene::Entity& iEntity, const math::vec2f& iPosition, float iRotation);

	/**
	 * @brief
	 *  Set the linear velocity of the given entity's physics body.
	 * @param iEntity The entity to modify.
	 * @param iVelocity The new velocity.
	 */
	static void setVelocity(const scene::Entity& iEntity, const math::vec2f& iVelocity);

	/**
	 * @brief
	 *  Apply an impulse in space to the given entity's body; a 2D body takes its X and Y only.
	 * @param[in] iEntity The entity.
	 * @param[in] iImpulse The impulse.
	 */
	static void impulse3D(const scene::Entity& iEntity, const math::vec3f& iImpulse);

	/**
	 * @brief
	 *  Linear velocity in space of the given entity's body.
	 * @param[in] iEntity The entity.
	 * @return The velocity (Z is 0 for a 2D body), zero without a moving body.
	 */
	[[nodiscard]] static auto getVelocity3D(const scene::Entity& iEntity) -> math::vec3f;

	/**
	 * @brief
	 *  Set the linear velocity in space of the given entity's body; a 2D body takes its X and Y only.
	 * @param[in] iEntity The entity.
	 * @param[in] iVelocity The velocity.
	 */
	static void setVelocity3D(const scene::Entity& iEntity, const math::vec3f& iVelocity);

	/**
	 * @brief
	 *  Move the given entity's body in space; a 2D body takes X, Y and the rotation around Z only.
	 * @param[in] iEntity The entity.
	 * @param[in] iPosition The world position.
	 * @param[in] iRotation The world rotation, Euler angles in radians (as `math::Transform`).
	 */
	static void setTransform3D(const scene::Entity& iEntity, const math::vec3f& iPosition,
							   const math::vec3f& iRotation);

	/**
	 * @brief
	 *  Set the gravity scale of the given dynamic body. A value of 0
	 * cancels gravity for that body without changing its mass; useful for
	 * top-down characters that share a 2D world with platformer scenes.
	 * @param iEntity The entity to modify (must have a dynamic PhysicBody).
	 * @param iScale The new gravity scale (1 = full world gravity, 0 = none).
	 */
	static void setGravityScale(const scene::Entity& iEntity, float iScale);

	/**
	 * @brief
	 *  Gravity at a point of a scene: the world gravity plus the fields of the enabled `GravitySource`s.
	 *
	 * Read from the components, with or without a running world.
	 * @param[in] iScene The scene.
	 * @param[in] iPoint The point, in world units.
	 * @return The acceleration a body of gravity scale 1 feels there, in world units per second squared.
	 */
	[[nodiscard]] static auto getGravityAt(const scene::Scene& iScene, const math::vec3f& iPoint) -> math::vec3f;

	/**
	 * @brief
	 *  Future positions of the body of an entity under the gravity, one per fixed step, collisions ignored.
	 *
	 * Integrated as the solver does, from the body's position and velocity in a running world, from the entity's
	 * position at rest otherwise; the sources are taken where they stand.
	 * @param[in] iEntity The entity with a `PhysicBody`.
	 * @param[in] iSteps Number of fixed steps, at most `maxPredictionSteps`.
	 * @return The positions, empty without a dynamic body.
	 */
	[[nodiscard]] static auto predictTrajectory(const scene::Entity& iEntity, uint32_t iSteps)
			-> std::vector<math::vec3f>;

	/// Highest accepted step count of `predictTrajectory()`.
	static constexpr uint32_t maxPredictionSteps = 100000;

	/**
	 * @brief
	 *  Whether the last step of a scene's world ran on rails: a time warp beyond `PhysicsSettings::railsTimeScale`.
	 *
	 * On rails the solver stops: every awake dynamic body follows its conic arc around the strongest field (or its
	 * parabola under the uniform pull) for the warped duration of the step, its rotation held; a sleeping body stays
	 * where it rests. No contact begins or ends.
	 * @param[in] iScene The scene.
	 * @return True when the last step was on rails, false without a world.
	 */
	[[nodiscard]] static auto isOnRails(const scene::Scene& iScene) -> bool;

	/**
	 * @brief
	 *  Hold the body of an entity out of the simulation, or give it back (the simulation level of detail).
	 *
	 * A held body sleeps and is put back to sleep when something wakes it; the fields and the rails leave it alone. Given
	 * back, it wakes with the velocity passed.
	 * @param[in] iEntity The entity with a dynamic `PhysicBody`.
	 * @param[in] iSimulated False holds the body, true gives it back.
	 * @param[in] iVelocity Velocity the body wakes with when given back.
	 */
	static void setSimulated(const scene::Entity& iEntity, bool iSimulated,
							 const math::vec3f& iVelocity = {0.f, 0.f, 0.f});

	/**
	 * @brief
	 *  Whether the body of an entity is simulated, not held by `setSimulated()`.
	 * @param[in] iEntity The entity.
	 * @return False while held; true otherwise, without a body too.
	 */
	[[nodiscard]] static auto isSimulated(const scene::Entity& iEntity) -> bool;

	/**
	 * @brief
	 *  Carry a held body along its arc around the strongest field, as the rails of a time warp do.
	 * @param[in] iEntity The entity whose body is held.
	 * @param[in] iVelocity Velocity of the body, kept while it is held.
	 * @param[in] iSeconds Duration.
	 * @return The velocity at the end of the arc; `iVelocity` when the body is not held.
	 */
	[[nodiscard]] static auto followRails(const scene::Entity& iEntity, const math::vec3f& iVelocity, float iSeconds)
			-> math::vec3f;

	/// Snapshot of a physics body's runtime state (for save/load), defined with the scene that saves it.
	using PhysicsSnapshot = scene::PhysicsSnapshot;

	/**
	 * @brief
	 *  Get a snapshot of a physics entity's runtime state.
	 * @param[in] iEntity The entity with a PhysicBody.
	 * @return The snapshot (zero values if entity has no body or physics not initialized).
	 */
	[[nodiscard]] static auto getSnapshot(const scene::Entity& iEntity) -> PhysicsSnapshot;

	/**
	 * @brief
	 *  Apply a snapshot to a physics entity (restore velocity/wake state).
	 * @param[in] iEntity The entity with a PhysicBody.
	 * @param[in] iSnapshot The snapshot to apply.
	 */
	static void applySnapshot(const scene::Entity& iEntity, const PhysicsSnapshot& iSnapshot);
};

}// namespace owl::physics
