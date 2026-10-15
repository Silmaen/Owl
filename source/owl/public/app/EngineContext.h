/**
 * @file EngineContext.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "input/ActionMap.h"
#include "scene/SimulationClock.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace owl::renderer {
class VoxelMeshCache;
}// namespace owl::renderer

namespace owl::scene {
class HudLibrary;
class ScreenTransition;
class SettingsManager;
}// namespace owl::scene

namespace owl::app {

/**
 * @brief
 *  Engine state shared by the scenes of one application, owned by `Application` instead of static storage.
 *
 * Holds the screen transition, the game settings, the project's simulation tick rate, its action map and default HUD,
 * the HUD library and the voxel mesh cache. The
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

	/**
	 * @brief
	 *  Action map of the game, read by the scenes when they sample the input.
	 * @return The map: the built-in actions until a project map is set.
	 */
	[[nodiscard]] auto getActionMap() const -> const shared<const input::ActionMap>& { return m_actionMap; }

	/**
	 * @brief
	 *  Set the action map of the game (the project's `input_actions.yml`); scenes read it from their next sample.
	 * @param[in] iMap The project map, completed with the built-in actions it does not define.
	 */
	void setActionMap(const input::ActionMap& iMap) {
		m_actionMap = mkShared<const input::ActionMap>(iMap.withDefaults());
	}

	/**
	 * @brief
	 *  Read the `input_actions.yml` of a project folder and make it the action map of the game.
	 *
	 * A project without the file, or with an unreadable one (logged), plays with the built-in actions.
	 * @param[in] iProjectDirectory The project folder; empty for none.
	 * @return True if the project's own map was loaded.
	 */
	auto loadActionMap(const std::filesystem::path& iProjectDirectory) -> bool;

	/**
	 * @brief
	 *  Make the action map of the game from the text of an `input_actions.yml` (a packed game).
	 * @param[in] iText The YAML text.
	 * @return True if the text was a valid map; the built-in actions are used otherwise.
	 */
	auto loadActionMapFromText(std::string_view iText) -> bool;

	/**
	 * @brief
	 *  The `.owlhud` files loaded so far, shared by the scenes and kept across scene transitions.
	 * @return The HUD library.
	 */
	[[nodiscard]] auto getHudLibrary() -> scene::HudLibrary& { return *mp_hudLibrary; }

	/**
	 * @brief
	 *  HUD asset of the scenes that name none (the project's `DefaultHud`).
	 * @return The asset path, empty for no default HUD.
	 */
	[[nodiscard]] auto getDefaultHud() const -> const std::string& { return m_defaultHud; }

	/**
	 * @brief
	 *  Set the HUD asset of the scenes that name none.
	 * @param[in] iPath The asset path, empty for none.
	 */
	void setDefaultHud(const std::string& iPath) { m_defaultHud = iPath; }

private:
	/// Screen transition state.
	uniq<scene::ScreenTransition> mp_screenTransition;
	/// Game settings.
	uniq<scene::SettingsManager> mp_settings;
	/// Voxel mesh cache.
	uniq<renderer::VoxelMeshCache> mp_voxelMeshCache;
	/// Fixed simulation tick of the game.
	scene::SimulationSettings m_simulation;
	/// Action map of the game.
	shared<const input::ActionMap> m_actionMap = input::ActionMap::getDefault();
	/// HUD asset of the scenes that name none.
	std::string m_defaultHud;
	/// Loaded HUD scenes (declared last: they hold GPU resources and are destroyed first).
	uniq<scene::HudLibrary> mp_hudLibrary;
};

}// namespace owl::app
