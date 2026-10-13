/**
 * @file EngineContext.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "scene/SimulationClock.h"

namespace owl::renderer {
class VoxelMeshCache;
}// namespace owl::renderer

namespace owl::scene {
class ScreenTransition;
class SettingsManager;
}// namespace owl::scene

namespace owl::app {

/**
 * @brief
 *  Engine state shared by the scenes of one application, owned by `Application` instead of static storage.
 *
 * Holds the screen transition, the game settings, the project's simulation tick rate and the voxel mesh cache. The
 * application creates it and releases it before shutting the renderer down; every new scene points at the
 * application's one (`Scene::getEngineContext`). A test that needs one creates its own and gives it to its scenes
 * (`Scene::setEngineContext`), so nothing leaks from a test to the next.
 */
class OWL_API EngineContext final {
public:
	/**
	 * @brief
	 *  Create the context with a fresh state.
	 */
	EngineContext();

	/**
	 * @brief
	 *  Destroy the context (its voxel meshes are GPU resources: destroy it while the renderer runs).
	 */
	~EngineContext();

	EngineContext(const EngineContext&) = delete;

	EngineContext(EngineContext&&) = delete;

	auto operator=(const EngineContext&) -> EngineContext& = delete;

	auto operator=(EngineContext&&) -> EngineContext& = delete;

	/**
	 * @brief
	 *  Full-screen transition overlay and level-load orchestrator.
	 * @return The screen transition.
	 */
	[[nodiscard]] auto getScreenTransition() -> scene::ScreenTransition& { return *mp_screenTransition; }

	/**
	 * @brief
	 *  Game settings (defaults and user overrides).
	 * @return The settings.
	 */
	[[nodiscard]] auto getSettings() -> scene::SettingsManager& { return *mp_settings; }

	/**
	 * @brief
	 *  GPU meshes of the voxel worlds.
	 * @return The voxel mesh cache.
	 */
	[[nodiscard]] auto getVoxelMeshCache() -> renderer::VoxelMeshCache& { return *mp_voxelMeshCache; }

	/**
	 * @brief
	 *  Fixed simulation tick of the game, given to every scene's clock at `Scene::onStartRuntime()`.
	 * @return The simulation settings (60 Hz unless the project sets another rate).
	 */
	[[nodiscard]] auto getSimulationSettings() const -> const scene::SimulationSettings& { return m_simulation; }

	/**
	 * @brief
	 *  Set the fixed simulation tick of the game (the project's `simulation:` block); scenes started from now on use it.
	 * @param[in] iSettings The settings, clamped into their accepted range.
	 */
	void setSimulationSettings(const scene::SimulationSettings& iSettings) { m_simulation = iSettings.clamped(); }

private:
	/// Screen transition state.
	uniq<scene::ScreenTransition> mp_screenTransition;
	/// Game settings.
	uniq<scene::SettingsManager> mp_settings;
	/// Voxel mesh cache.
	uniq<renderer::VoxelMeshCache> mp_voxelMeshCache;
	/// Fixed simulation tick of the game.
	scene::SimulationSettings m_simulation;
};

}// namespace owl::app
