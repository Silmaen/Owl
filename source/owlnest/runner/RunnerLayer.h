/**
 * @file RunnerLayer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "FrameBench.h"

#include <data/assets/pack/PackReader.h>
#include <owl.h>
#include <scene/SceneSerializer.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

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
	/// Renderer-stack definition forwarded from the project's `RendererStack:`
	/// block. The runner consumes this on startup (and after every cross-level
	/// teleport) to call `Renderer::setRenderStack` — without it, packaged
	/// games fall back to a single implicit `Renderer2D` and any scene that
	/// relies on a custom stack (raycaster, future voxel, screen-overlay UI)
	/// renders incorrectly.
	renderer::RendererStackConfig rendererStack;

	/**
	 * @brief
	 *  Load configuration from a YAML file.
	 */
	void loadYaml(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Save configuration to a YAML file.
	 */
	void saveYaml(const std::filesystem::path& iPath) const;
};

/**
 * @brief
 *  Smoke-test mode: play every packed scene for a few frames, then quit with a non-zero exit code on any error log.
 */
struct SmokeTest {
	/// Frames played per scene (0 disables the smoke test).
	uint32_t frames{0};
};

/**
 * @brief
 *  Class RunnerLayer
 */
class RunnerLayer final : public app::layer::Layer {
public:
	RunnerLayer(const RunnerLayer&) = delete;

	RunnerLayer(RunnerLayer&&) = delete;

	auto operator=(const RunnerLayer&) -> RunnerLayer& = delete;

	auto operator=(RunnerLayer&&) -> RunnerLayer& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iSmokeTest Smoke-test options (inactive by default).
	 */
	explicit RunnerLayer(const SmokeTest& iSmokeTest = {});

	/**
	 * @brief
	 *  Constructor for a frame bench run: the scene comes from the options instead of `runner.yml`.
	 * @param[in] iBench The frame bench options.
	 */
	explicit RunnerLayer(const FrameBenchOptions& iBench);

	/**
	 * @brief
	 *  Destructor.
	 */
	~RunnerLayer() override = default;

	/**
	 * @brief
	 *  Handle the attach event.
	 */
	void onAttach() override;

	/**
	 * @brief
	 *  Handle the detach event.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Handle the update event.
	 * @param[in] iTimeStep Frame time delta.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Handle the event event.
	 * @param[in,out] ioEvent The incoming event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Handle the im gui render event.
	 * @param[in] iTimeStep Frame time delta.
	 */
	void onImGuiRender(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Get the active scene.
	 * @return The active scene.
	 */
	[[nodiscard]] auto getActiveScene() const -> const shared<scene::Scene>& { return m_activeScene; }

private:
	/**
	 * @brief
	 *  Handle the key pressed event.
	 * @param[in,out] ioEvent The incoming event.
	 * @return True when the event was consumed.
	 */
	static auto onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool;

	/**
	 * @brief
	 *  Handle the mouse button pressed event.
	 * @param[in,out] ioEvent The incoming event.
	 * @return True when the event was consumed.
	 */
	static auto onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent) -> bool;

	/// State of an in-flight cross-level teleport loaded asynchronously.
	struct PendingTransition {
		/// Parsed YAML root (filled on worker thread). The worker reads the
		/// scene bytes and runs `SceneSerializer::parseBuffer` so the main
		/// thread only pays the entity-creation + GPU upload cost.
		shared<scene::ParsedScene> parsed;
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

	/**
	 * @brief
	 *  Handle teleport request.
	 */
	void handleTeleportRequest();
	/**
	 * @brief
	 *  Place the player at the teleport target of a freshly started scene, with the stored velocity rotated.
	 */
	void applyPendingTeleport();

	/**
	 * @brief
	 *  Finish a pending async transition: deserialize on main thread and swap scene.
	 */
	void finishTransition();
	/// Build the engine's `RenderStack` from `m_config.rendererStack` filtered
	/// by the active scene's `EnabledRenderers`, and install it via
	/// `Renderer::setRenderStack`. Called at startup and after every scene
	/// swap so per-scene overrides apply post-teleport.
	void installRenderStack();

	/**
	 * @brief
	 *  Frame bench start-up: project assets and renderer stack, scene, null input, timings.
	 */
	void attachFrameBench();

	/**
	 * @brief
	 *  Write the frame bench report, set the exit code and stop the application.
	 * @param[in] iInterrupted True when the run stopped before the end.
	 */
	void finishFrameBench(bool iInterrupted);

	/**
	 * @brief
	 *  Frame bench step at the top of `onUpdate`: close the previous frame, stop when the bench is done.
	 * @return False when the bench is over and the frame must be skipped.
	 */
	auto stepFrameBench() -> bool;

	/**
	 * @brief
	 *  Read the offscreen capture target back and write it as the `--capture` PNG.
	 * @return True on success.
	 */
	[[nodiscard]] auto writeCapture() const -> bool;

	/**
	 * @brief
	 *  Run the scene update (cursor capture outside a frame bench, phase timings inside).
	 * @param[in] iTimeStep Time step fed to the scene.
	 */
	void updateSceneRuntime(const core::Timestep& iTimeStep);

	/**
	 * @brief
	 *  Load a scene from the open pack, or from the asset directories, into a fresh active scene.
	 * @param[in] iSceneName Pack path or file path of the scene.
	 * @return True when the scene was loaded.
	 */
	auto loadScene(const std::string& iSceneName) -> bool;

	/**
	 * @brief
	 *  Collect the scenes the smoke test plays: the first scene, then every other scene in the pack.
	 */
	void initSmokeTest();

	/**
	 * @brief
	 *  Count a smoke-test frame and switch to the next scene (or quit) once the scene played enough frames.
	 * @return True when the application is closing.
	 */
	auto stepSmokeTest() -> bool;

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
	/// Frame bench driver (nullptr in a normal run).
	uniq<FrameBench> m_frameBench;
	/// Offscreen target the scene renders into when the frame bench writes a capture.
	shared<renderer::gpu::Framebuffer> m_captureTarget;
	/// Smoke-test options.
	SmokeTest m_smokeTest;
	/// Scenes left to play in smoke-test mode, in order.
	std::vector<std::string> m_smokeScenes;
	/// Index of the scene being played in smoke-test mode.
	size_t m_smokeIndex = 0;
	/// Frames played on the current smoke-test scene.
	uint32_t m_smokeFrame = 0;
	/// Scenes that failed to load in smoke-test mode.
	uint32_t m_smokeFailures = 0;
};
}// namespace owl::nest::runner
