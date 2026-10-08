/**
 * @file SystemSchedule.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Timestep.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace owl::scene {

class Scene;

/**
 * @brief
 *  Phases of one runtime frame, run in this order by `Scene::onUpdateRuntime()`; the render follows them.
 */
enum struct SystemPhase : uint8_t {
	Scripts,///< Native and Lua scripts (`on_update`).
	PrePhysics,///< Gameplay controllers before the physics step: cameras, players, raycast doors, inputs.
	Physics,///< Physics step and collision callbacks.
	PostPhysics,///< Entity links and triggers: the last phase allowed to move or create entities.
	Late,///< Reads the settled world: sound positions, sprite animation.
	Ended,///< Runs instead of all the others once the game is won or lost (`Scene::Status`).
};

/**
 * @brief
 *  What a system receives each frame besides the scene.
 */
struct OWL_API SystemContext {
	/// Duration of the frame.
	core::Timestep timeStep;
	/// Whether the frame is rendered (false in headless updates).
	bool render = true;
};

/// Body of a system: one call per frame in its phase.
using SystemFunction = std::function<void(Scene& ioScene, const SystemContext& iContext)>;

/**
 * @brief
 *  One named system of a schedule.
 */
struct OWL_API SceneSystem {
	/// Unique name in the schedule (e.g. `owl.physics`), used to replace or remove it.
	std::string name;
	/// Phase the system runs in.
	SystemPhase phase = SystemPhase::PrePhysics;
	/// The function run every frame.
	SystemFunction update;
};

/**
 * @brief
 *  Ordered list of the systems a scene runs every runtime frame.
 *
 * Systems run phase by phase (`SystemPhase` order), and within a phase in insertion order. Every new scene
 * copies `getDefault()`, which holds the engine systems (`owl.*`): a game adds, replaces or removes systems
 * there once, before loading its scenes, or on one scene through `Scene::getSystems()`.
 */
class OWL_API SystemSchedule final {
public:
	/**
	 * @brief
	 *  Append a system at the end of its phase.
	 * @param[in] iSystem The system.
	 * @return False (and nothing added) when the name is empty, already used, or the function is empty.
	 */
	auto add(SceneSystem iSystem) -> bool;

	/**
	 * @brief
	 *  Insert a system just before another one, in the phase of that other one.
	 * @param[in] iBefore Name of the system to insert before.
	 * @param[in] iSystem The system (its phase is set to the one of `iBefore`).
	 * @return False (and nothing added) when `iBefore` is unknown or `iSystem` is invalid.
	 */
	auto insertBefore(const std::string& iBefore, SceneSystem iSystem) -> bool;

	/**
	 * @brief
	 *  Replace the function of a system, keeping its name, phase and position.
	 * @param[in] iName The system name.
	 * @param[in] iUpdate The new function.
	 * @return False when no system has that name or the function is empty.
	 */
	auto replace(const std::string& iName, SystemFunction iUpdate) -> bool;

	/**
	 * @brief
	 *  Remove a system.
	 * @param[in] iName The system name.
	 * @return False when no system has that name.
	 */
	auto remove(const std::string& iName) -> bool;

	/**
	 * @brief
	 *  Check whether a system is scheduled.
	 * @param[in] iName The system name.
	 * @return True if a system has that name.
	 */
	[[nodiscard]] auto has(const std::string& iName) const -> bool;

	/**
	 * @brief
	 *  Names of the systems of a phase, in run order.
	 * @param[in] iPhase The phase.
	 * @return The names.
	 */
	[[nodiscard]] auto getNames(SystemPhase iPhase) const -> std::vector<std::string>;

	/**
	 * @brief
	 *  Run the systems of one phase.
	 * @param[in] iPhase The phase.
	 * @param[in,out] ioScene The scene they update.
	 * @param[in] iContext The frame context.
	 */
	void run(SystemPhase iPhase, Scene& ioScene, const SystemContext& iContext) const;

	/**
	 * @brief
	 *  Schedule copied by every new scene, holding the engine systems until a game changes it.
	 * @return The default schedule.
	 */
	[[nodiscard]] static auto getDefault() -> SystemSchedule&;

	/**
	 * @brief
	 *  Schedule holding only the engine systems, whatever the changes made to `getDefault()`.
	 * @return A new schedule with the engine systems.
	 */
	[[nodiscard]] static auto makeEngineDefault() -> SystemSchedule;

private:
	/// The systems, grouped by phase in run order.
	std::vector<SceneSystem> m_systems;
};

}// namespace owl::scene
