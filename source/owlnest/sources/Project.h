/**
 * @file Project.h
 * @author Silmaen
 * @date 09/03/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>
#include <renderer/RenderStack.h>

namespace owl::nest {

/**
 * @brief Structure representing an Owl Nest project.
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
	/// Relative path to the project icon (PNG, relative to project directory).
	std::string icon;
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

	/// Project-level renderer stack definition. Empty → engine falls back to a single Renderer2D.
	renderer::RendererStackConfig rendererStack;

	/**
	 * @brief Load project configuration from a YAML file.
	 * @param[in] iFile The file to load.
	 */
	void loadFromFile(const std::filesystem::path& iFile);

	/**
	 * @brief Save project configuration to a YAML file.
	 * @param[in] iFile The file to save.
	 */
	void saveToFile(const std::filesystem::path& iFile) const;

	/**
	 * @brief Check if a project is currently loaded.
	 * @return True if a project directory is set.
	 */
	[[nodiscard]] auto isLoaded() const -> bool { return !projectDirectory.empty(); }
};

}// namespace owl::nest
