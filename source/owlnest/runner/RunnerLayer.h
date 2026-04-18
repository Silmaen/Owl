/**
 * @file RunnerLayer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <io/pack/PackReader.h>
#include <owl.h>

namespace owl::nest::runner {

/// Configuration loaded from runner.yml.
struct RunnerConfig {
	/// Relative path to the first scene.
	std::string firstScene;
	/// Optional path to an asset pack file.
	std::string packFile;
	/// Game display name (for window title and save directory).
	std::string gameName;
	/// Game version string.
	std::string version;
	/// Author name.
	std::string author;
	/// Relative path to the game icon.
	std::string icon;
	/// Window width in pixels.
	uint32_t windowWidth{1280};
	/// Window height in pixels.
	uint32_t windowHeight{720};
	/// Whether to start in fullscreen mode.
	bool fullscreen{false};
	/// Whether the window is resizable.
	bool resizable{true};

	/// Load configuration from a YAML file.
	void loadYaml(const std::filesystem::path& iPath);
	/// Save configuration to a YAML file.
	void saveYaml(const std::filesystem::path& iPath) const;
};

/**
 * @brief Class RunnerLayer
 */
class RunnerLayer final : public core::layer::Layer {
public:
	RunnerLayer(const RunnerLayer&) = delete;
	RunnerLayer(RunnerLayer&&) = delete;
	auto operator=(const RunnerLayer&) -> RunnerLayer& = delete;
	auto operator=(RunnerLayer&&) -> RunnerLayer& = delete;
	/**
	 * @brief Default constructor.
	 */
	RunnerLayer();
	/**
	 * @brief Destructor.
	 */
	~RunnerLayer() override = default;

	void onAttach() override;
	void onDetach() override;
	void onUpdate(const core::Timestep& iTimeStep) override;
	void onEvent(event::Event& ioEvent) override;
	void onImGuiRender(const core::Timestep& iTimeStep) override;

	[[nodiscard]] auto getActiveScene() const -> const shared<scene::Scene>& { return m_activeScene; }

private:
	static auto onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool;
	static auto onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent) -> bool;

	/// State of an in-flight cross-level teleport loaded asynchronously.
	struct PendingTransition {
		/// Bytes read from file or pack (filled on worker thread).
		shared<std::vector<uint8_t>> data;
		/// Resolved source name (for logging).
		std::string sourceName;
		/// GameState snapshot to copy into the new scene.
		scene::GameState previousGameState;
		/// Velocity to apply after the new scene's physics init.
		math::vec2f velocity = {0.f, 0.f};
		/// Name of the target entity for rotation/position.
		std::string targetName;
		/// True when the worker thread is done loading.
		std::atomic<bool> ready{false};
		/// True when the worker found no scene data (error state).
		std::atomic<bool> failed{false};
	};

	void handleTeleportRequest();
	/// Finish a pending async transition: deserialize on main thread and swap scene.
	void finishTransition();

	shared<scene::Scene> m_activeScene;
	math::vec2ui m_viewportSize = {0, 0};
	RunnerConfig m_config;
	/// If true, need to apply velocity after physics init on next frame.
	bool m_pendingTeleportVelocity = false;
	/// Stored velocity to apply after cross-level teleport physics init.
	math::vec2f m_teleportVelocity = {0.f, 0.f};
	/// Stored target name for cross-level teleport (to apply rotation after loading).
	std::string m_teleportTargetName;
	/// In-flight async scene transition (nullptr when idle).
	shared<PendingTransition> m_transition;
};
}// namespace owl::nest::runner
