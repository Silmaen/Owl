/**
 * @file HeadlessSession.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "EditorSession.h"

#include <input/ActionMap.h>
#include <renderer/RenderStack.h>
#include <renderer/gpu/Framebuffer.h>
#include <scene/SimulationClock.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace owl::nest::session {

/**
 * @brief
 *  Find the project a scene belongs to: the nearest parent directory holding an `owl_project.yml`.
 * @param[in] iScene The scene file.
 * @return The project directory, empty when none was found.
 */
[[nodiscard]] auto findProjectDirectory(const std::filesystem::path& iScene) -> std::filesystem::path;

/**
 * @brief
 *  An editing session without window nor ImGui, for the tests and the tools.
 *
 * Runs in a started `Application` (a dummy one on the Null renderer for the tests): it owns the scene, its undo
 * history and a command registry, plays synchronously at a fixed step and renders into an offscreen framebuffer of
 * its own. In an application without window it switches the input to the Null backend, which takes the held keys.
 * Scene transitions (teleports, saves and loads requested by the game) are not followed. On the Null renderer a
 * capture is a blank (black) image of the session size.
 */
class HeadlessSession final : public EditorSession {
public:
	/// Default size of the offscreen frame.
	static constexpr math::vec2ui defaultSize{640, 360};

	HeadlessSession(const HeadlessSession&) = delete;

	HeadlessSession(HeadlessSession&&) = delete;

	auto operator=(const HeadlessSession&) -> HeadlessSession& = delete;

	auto operator=(HeadlessSession&&) -> HeadlessSession& = delete;

	/**
	 * @brief
	 *  Create a session without scene.
	 * @param[in] iSize Size of the rendered frames.
	 */
	explicit HeadlessSession(math::vec2ui iSize = defaultSize);

	/**
	 * @brief
	 *  Destructor: stops the scene, releases the frame and the project asset directory.
	 */
	~HeadlessSession() override;

	/**
	 * @brief
	 *  Start an empty, never saved scene.
	 */
	void newScene();

	/**
	 * @brief
	 *  Load a scene file; its project (asset directory, renderer stack, simulation settings) is used too.
	 * @param[in] iPath The scene file.
	 * @return Nothing on success, `LoadFailed` otherwise (the previous scene is kept).
	 */
	auto loadScene(const std::filesystem::path& iPath) -> expected<void, SessionFailure>;

	/**
	 * @brief
	 *  Get the project of the loaded scene.
	 * @return The project directory, empty when the scene has none.
	 */
	[[nodiscard]] auto getProjectDirectory() const -> const std::filesystem::path& { return m_project; }

	/**
	 * @brief
	 *  Get the size of the rendered frames.
	 * @return The size in pixels.
	 */
	[[nodiscard]] auto getSize() const -> math::vec2ui { return m_size; }

	// --- EditorSession -------------------------------------------------------

	/**
	 * @brief
	 *  Get the scene shown: the edited scene, or its running copy while it plays.
	 * @return The scene, null when none is open.
	 */
	[[nodiscard]] auto getSessionScene() const -> shared<scene::Scene> override { return m_activeScene; }

	/**
	 * @brief
	 *  Get the file of the scene.
	 * @return The path, empty when the scene was never saved.
	 */
	[[nodiscard]] auto getSessionScenePath() const -> std::filesystem::path override { return m_scenePath; }

	/**
	 * @brief
	 *  Get the project of the loaded scene.
	 * @return The directory, empty without a project.
	 */
	[[nodiscard]] auto getSessionProjectDirectory() const -> std::filesystem::path override { return m_project; }

	/**
	 * @brief
	 *  Get the undo history of the scene.
	 * @return The undo manager, null when no scene is open.
	 */
	[[nodiscard]] auto getSessionUndo() -> SceneUndoManager* override;

	/**
	 * @brief
	 *  Get the command registry.
	 * @return The registry.
	 */
	[[nodiscard]] auto getRegistry() const -> const commands::CommandRegistry& override { return m_registry; }

	/**
	 * @brief
	 *  Build the context a command runs on.
	 * @return The context.
	 */
	[[nodiscard]] auto makeCommandContext() -> commands::CommandContext override;

	/**
	 * @brief
	 *  Check whether the scene plays.
	 * @return True outside Edit.
	 */
	[[nodiscard]] auto isScenePlaying() const -> bool override { return m_playing; }

	/**
	 * @brief
	 *  Play the whole request before returning.
	 * @param[in] iRequest The request.
	 * @return Nothing on success, or why the play did not start.
	 */
	auto startPlay(const PlayRequest& iRequest) -> expected<void, SessionFailure> override;

	/**
	 * @brief
	 *  Check whether a bounded play is running.
	 * @return Always false: plays are synchronous.
	 */
	[[nodiscard]] auto isPlayRunning() const -> bool override { return m_driver.isActive(); }

	/**
	 * @brief
	 *  Take the report of the last bounded play.
	 * @return The report, once.
	 */
	auto takePlayReport() -> std::optional<PlayReport> override;

	/**
	 * @brief
	 *  Stop the scene and go back to the edited scene.
	 * @return Nothing on success, `NotPlaying` in Edit.
	 */
	auto stopPlay() -> expected<void, SessionFailure> override;

	/**
	 * @brief
	 *  Render the scene through its primary camera (no simulation) and read the frame back.
	 * @return The image, or why it cannot be read.
	 */
	auto captureFrame() -> expected<FrameCapture, SessionFailure> override;

	/**
	 * @brief
	 *  Get the frame measurements.
	 * @return The statistics.
	 */
	[[nodiscard]] auto getStatistics() const -> SessionStatistics override;

private:
	/**
	 * @brief
	 *  Create the offscreen frame when needed.
	 */
	void ensureTarget();

	/**
	 * @brief
	 *  Play one frame of the running scene into the offscreen frame.
	 * @param[in] iStep The frame duration.
	 */
	void playFrame(const core::Timestep& iStep);

	/**
	 * @brief
	 *  Read the offscreen frame back, made opaque.
	 * @return The image, nothing when the renderer cannot read it back.
	 */
	[[nodiscard]] auto readTarget() const -> std::optional<FrameCapture>;

	/**
	 * @brief
	 *  Install the renderer stack of the project for the scene.
	 */
	void installRenderStack() const;

	/**
	 * @brief
	 *  Read the project settings the run depends on (renderer stack, simulation).
	 */
	void loadProjectSettings();

	/// Command registry (built-in commands).
	commands::CommandRegistry m_registry;
	/// The edited scene.
	shared<scene::Scene> m_editScene;
	/// The scene shown: the edited one, or its running copy.
	shared<scene::Scene> m_activeScene;
	/// File of the scene.
	std::filesystem::path m_scenePath;
	/// Project of the scene (its asset directory is registered while the session lives).
	std::filesystem::path m_project;
	/// Renderer stack configuration of the project (empty: the default stack).
	renderer::RendererStackConfig m_rendererStack;
	/// Undo history of the edited scene.
	SceneUndoManager m_undo;
	/// Offscreen frame the scene renders into.
	shared<renderer::gpu::Framebuffer> m_target;
	/// Size of the frames.
	math::vec2ui m_size;
	/// Bounded play in progress.
	PlayDriver m_driver;
	/// Report of the last bounded play, until taken.
	std::optional<PlayReport> m_report;
	/// Frame measurements.
	SessionStatistics m_statistics;
	/// Simulation settings of the engine before the session, restored by the destructor.
	scene::SimulationSettings m_previousSimulation;
	/// Action map of the application before the session, put back at the end.
	shared<const input::ActionMap> m_previousActions;
	/// True while the running copy plays.
	bool m_playing{false};
};

}// namespace owl::nest::session
