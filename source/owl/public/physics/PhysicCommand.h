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
 *  Class for physics management.
 */
class OWL_API PhysicCommand final {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	PhysicCommand();

	/**
	 * @brief
	 *  Default destructor.
	 */
	~PhysicCommand() = default;

	PhysicCommand(const PhysicCommand&) = delete;

	PhysicCommand(PhysicCommand&&) = delete;

	auto operator=(const PhysicCommand&) -> PhysicCommand& = delete;

	auto operator=(PhysicCommand&&) -> PhysicCommand& = delete;

	/**
	 * @brief
	 *  Initialize the physical world based on the given scene.
	 * From then on the bodies follow their components: a `PhysicBody` added while running gets its body at
	 * the next `frame()`, and removing a `PhysicBody`, `Tilemap`, raycast door or pushwall component (or its
	 * entity) destroys the matching body.
	 * @param iScene The Scene onto apply physics.
	 */
	static void init(scene::Scene* iScene);

	/**
	 * @brief
	 *  Destroy the world and unlink scene. Does nothing when physics is not initialized.
	 */
	static void destroy();

	/**
	 * @brief
	 *  Destroy the world only if it is bound to the given scene.
	 *
	 * Called by the scene destructor so the static state never outlives the scene it points to.
	 * @param[in] iScene The scene being released.
	 */
	static void releaseScene(const scene::Scene* iScene);

	/**
	 * @brief
	 *  Check if physic is initiated and link to the scene.
	 * @return True if initiated.
	 */
	static auto isInitialized() -> bool;

	/**
	 * @brief
	 *  Advance the world by one rendered frame, at the fixed rate of the scene's `PhysicsSettings`.
	 *
	 * The frame duration is added to an accumulator; as many fixed steps as it holds are run, at most
	 * `maxStepsPerFrame` (the excess time is dropped). The contact events of all the steps are gathered,
	 * a pair being reported at most once per frame. The entity transforms are then written, blended
	 * between the last two steps when interpolation is on, so a frame without a step still moves them.
	 * @param[in] iTimestep The duration of the rendered frame.
	 */
	static void frame(const core::Timestep& iTimestep);

	/**
	 * @brief
	 *  Settings of the running world.
	 * @return The clamped settings read from the scene at `init()`, or the defaults when not initialised.
	 */
	[[nodiscard]] static auto getSettings() -> PhysicsSettings;

	/**
	 * @brief
	 *  Number of threads running the Box2D solver.
	 * @return 1 for the single-threaded solver, the task pool size when multi-threaded, 0 when not initialised.
	 */
	[[nodiscard]] static auto getWorkerCount() -> uint32_t;

	/**
	 * @brief
	 *  Number of fixed steps run by the last `frame()` call.
	 * @return The step count, 0 when not initialised.
	 */
	[[nodiscard]] static auto getLastFrameStepCount() -> uint32_t;

	/**
	 * @brief
	 *  Blend factor used for the transforms written by the last `frame()` call.
	 * @return The leftover accumulated time over the step duration, in [0, 1); 1 when interpolation is off.
	 */
	[[nodiscard]] static auto getInterpolationAlpha() -> float;

	/**
	 * @brief
	 *  Write the state of the last fixed step, not the interpolated one, into the entity transforms.
	 *
	 * Used before serialising a running scene (save game) so the saved positions match the saved
	 * velocities. The next `frame()` writes interpolated transforms again.
	 */
	static void syncSimulatedTransforms();

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
	 *  Hand over the collisions that began during the last `frame()` calls, and clear them.
	 *
	 * Built from Box2D begin-touch contact events. A pair is reported once when its first contact
	 * begins; contacts between further shapes of the same two entities (tilemap cells) are not
	 * reported again until all of them have ended.
	 * @return The begun collisions, in Box2D event order.
	 */
	[[nodiscard]] static auto takeCollisionEvents() -> std::vector<CollisionEvent>;

	/**
	 * @brief
	 *  Remove the Box2D bodies owned by an entity (PhysicBody, tilemap, raycast door or pushwall body).
	 *
	 * Component removal already does it; this drops the bodies of an entity that stays in the scene. No-op
	 * when physics is not initialised, the entity belongs to another scene or owns no body.
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

	/**
	 * @brief
	 *  Snapshot of a physics body's runtime state (for save/load).
	 */
	struct PhysicsSnapshot {
		/// Linear velocity.
		math::vec2f linearVelocity{0.f, 0.f};
		/// Angular velocity.
		float angularVelocity = 0.f;
		/// Whether the body is awake.
		bool awake = true;
	};

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

private:
	/// Implementation class.
	class Impl;
	/// Pointer to the implementation.
	static shared<Impl> m_impl;
	/// pointer to the active scene.
	static scene::Scene* m_scene;
};

}// namespace owl::physics
