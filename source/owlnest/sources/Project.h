/**
 * @file Project.h
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <core/FormatVersion.h>
#include <data/assets/pack/GameExporter.h>
#include <owlgui.h>
#include <renderer/RenderStack.h>
#include <scene/SimulationClock.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace owl::nest {
/**
 * @brief
 *  Structure representing an Owl Nest project.
 */
struct Project {
	/// Project display name.
	std::string name;
	/// Relative path to the first scene.
	std::string firstScene;
	/// Project version string (freeform, e.g., "1.0.0").
	std::string version;
	/// Author or studio name.
	std::string author;
	/// Short project description.
	std::string description;
	/// HUD asset (`.owlhud`, relative to an asset folder) of the scenes that name none; empty for none.
	std::string defaultHud;
	/// Relative path to the project icon (PNG, relative to project directory).
	std::string icon;
	/// Optional gamepad mapping file (SDL_GameControllerDB format), relative to the project directory.
	std::string gamepadMappings;
	/// Absolute path to the project root directory.
	std::filesystem::path projectDirectory;

	/// Window configuration settings.
	struct WindowSettings {
		/// Default window width in pixels.
		uint32_t width{1280};
		/// Default window height in pixels.
		uint32_t height{720};
		/// Whether to start in fullscreen mode.
		bool fullscreen{false};
		/// Whether the window is resizable.
		bool resizable{true};
	};
	/// Window settings for the game.
	WindowSettings window;

	/// Fixed simulation tick of the game (`simulation:`): rate of `on_tick` and of the physics steps.
	scene::SimulationSettings simulation;

	/// Project-level renderer stack definition. Empty → engine falls back to a single Renderer2D.
	renderer::RendererStackConfig rendererStack;

	/**
	 * @brief
	 *  Format descriptor of `owl_project.yml`, with its migration chain.
	 * @return The project format.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  Load project configuration from a YAML file.
	 *
	 * An older format version is migrated, a newer one is refused.
	 * @param[in] iFile The file to load.
	 * @return True on success; on failure the project is left as it was.
	 */
	[[nodiscard]] auto loadFromFile(const std::filesystem::path& iFile) -> bool;

	/**
	 * @brief
	 *  Save project configuration to a YAML file, atomically: a failed save keeps the previous file.
	 * @param[in] iFile The file to save.
	 * @return True on success.
	 */
	[[nodiscard]] auto saveToFile(const std::filesystem::path& iFile) const -> bool;

	/**
	 * @brief
	 *  Check if a project is currently loaded.
	 * @return True if a project directory is set.
	 */
	[[nodiscard]] auto isLoaded() const -> bool { return !projectDirectory.empty(); }

	/**
	 * @brief
	 *  Add the mappings of the project gamepad mapping file to the input layer (GLFW keeps them until exit).
	 * @return True if the project has no mapping file or if its mappings were added.
	 */
	[[nodiscard]] auto loadGamepadMappings() const -> bool;

	/**
	 * @brief
	 *  Build the game export settings of this project.
	 * @param[in] iOutputDir Parent folder of the exported game.
	 * @param[in] iRunnerDir Folder holding the OwlRunner executable and the shared libraries.
	 * @return The export settings (default pack flags).
	 */
	[[nodiscard]] auto makeExportSettings(const std::filesystem::path& iOutputDir,
										  const std::filesystem::path& iRunnerDir) const
			-> data::assets::pack::ExportSettings;
};

}// namespace owl::nest
