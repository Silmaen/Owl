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

#include <cstdint>
#include <vector>

/**
 * @brief
 *  Namespace for phyisics management.
 */
namespace owl::physics {
/**
 * @brief
 *  Physics of the scenes: each scene owns its Box2D world, created by `init()` and reached through the
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
	 * its body at the next `frame()`, and removing a `PhysicBody`, `Tilemap`, raycast door or pushwall
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
	 *  Advance the world of a scene by one rendered frame, at the fixed rate of its `PhysicsSettings`.
	 *
	 * The frame duration is added to an accumulator; as many fixed steps as it holds are run, at most
	 * `maxStepsPerFrame` (the excess time is dropped). The contact events of all the steps are gathered,
	 * a pair being reported at most once per frame. The entity transforms are then written, blended
	 * between the last two steps when interpolation is on, so a frame without a step still moves them.
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
	 *  Number of threads running the Box2D solver of a scene.
	 * @param[in] iScene The scene.
	 * @return 1 for the single-threaded solver, the task pool size when multi-threaded, 0 without a world.
	 */
	[[nodiscard]] static auto getWorkerCount(const scene::Scene& iScene) -> uint32_t;

	/**
	 * @brief
	 *  Number of fixed steps run by the last `frame()` call on a scene.
	 * @param[in] iScene The scene.
	 * @return The step count, 0 without a world.
	 */
	[[nodiscard]] static auto getLastFrameStepCount(const scene::Scene& iScene) -> uint32_t;

	/**
	 * @brief
	 *  Blend factor used for the transforms written by the last `frame()` call on a scene.
	 * @param[in] iScene The scene.
	 * @return The leftover accumulated time over the step duration, in [0, 1); 1 when interpolation is off.
	 */
	[[nodiscard]] static auto getInterpolationAlpha(const scene::Scene& iScene) -> float;

	/**
	 * @brief
	 *  Write the state of the last fixed step, not the interpolated one, into the entity transforms.
	 *
	 * Used before serialising a running scene (save game) so the saved positions match the saved
	 * velocities. The next `frame()` writes interpolated transforms again.
	 * @param[in,out] ioScene The scene whose transforms are written.
	 */
	static void syncSimulatedTransforms(scene::Scene& ioScene);

	/**
	 * @brief
	 *  Two entities whose bodies started touching during a physics step.
	 */
	struct CollisionEvent {
		/// UUID of the first entity of the pair.
		core::UUID entityA;
		/// UUID of the second entity of the pair.
		core::UUID entityB;
	};

	/**
	 * @brief
	 *  Hand over the collisions that began during the last `frame()` calls on a scene, and clear them.
	 *
	 * Built from Box2D begin-touch contact events. A pair is reported once when its first contact
	 * begins; contacts between further shapes of the same two entities (tilemap cells) are not
	 * reported again until all of them have ended.
	 * @param[in,out] ioScene The scene.
	 * @return The begun collisions, in Box2D event order.
	 */
	[[nodiscard]] static auto takeCollisionEvents(scene::Scene& ioScene) -> std::vector<CollisionEvent>;

	/**
	 * @brief
	 *  Remove the Box2D bodies owned by an entity (PhysicBody, tilemap, raycast door or pushwall body).
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
	 *  Set the gravity scale of the given dynamic body. A value of 0
	 * cancels gravity for that body without changing its mass; useful for
	 * top-down characters that share a 2D world with platformer scenes.
	 * @param iEntity The entity to modify (must have a dynamic PhysicBody).
	 * @param iScale The new gravity scale (1 = full world gravity, 0 = none).
	 */
	static void setGravityScale(const scene::Entity& iEntity, float iScale);

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
