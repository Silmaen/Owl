/**
 * @file EditorSession.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../UndoManager.h"
#include "../commands/CommandJournal.h"
#include "../commands/CommandRegistry.h"
#include "../commands/Macro.h"
#include "HeldInput.h"

#include <core/Timestep.h>
#include <core/expected.h>
#include <math/vectors.h>
#include <scene/Scene.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::session {

/**
 * @brief
 *  Why a session request did not run.
 */
enum struct SessionError : uint8_t {
	NoScene,///< No scene is open.
	AlreadyPlaying,///< A bounded play is already running.
	NotPlaying,///< The scene does not play.
	InvalidRequest,///< The request is out of range (no frame nor tick, bad time step...).
	CaptureUnavailable,///< The renderer cannot read the frame back.
	LoadFailed///< The scene file cannot be read.
};

/**
 * @brief
 *  A session failure and its explanation.
 */
struct SessionFailure {
	/// Failure reason.
	SessionError error{SessionError::InvalidRequest};
	/// Human-readable explanation.
	std::string message;
};

/**
 * @brief
 *  A bounded play: run the game for a number of frames or simulation ticks at a fixed step, then stop.
 */
struct PlayRequest {
	/// Largest number of frames or ticks a request may ask for.
	static constexpr uint32_t maxCount = 100'000;
	/// Frames to play (0: bounded by `ticks` only).
	uint32_t frames{0};
	/// Simulation ticks to run (0: bounded by `frames` only); the play ends at the first bound reached.
	uint32_t ticks{0};
	/// Duration of every frame in milliseconds; 0 plays one simulation tick per frame.
	double timestepMs{0.0};
	/// Keys held during the whole play (through the input injection).
	std::vector<uint16_t> keys{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Mouse buttons held during the whole play.
	std::vector<uint8_t> mouseButtons{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Gamepad held during the whole play (the slot then reads as connected).
	std::optional<HeldGamepad> gamepad{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// True to stop the scene at the end (back to the edited scene); false leaves it paused on the last frame.
	bool stopAtEnd{true};
	/// True to capture the last frame into the report.
	bool capture{false};
};

/**
 * @brief
 *  An image read back from the renderer.
 */
struct FrameCapture {
	/// Image size in pixels.
	math::vec2ui size{0, 0};
	/// `width * height * 4` bytes, RGBA, top row first, opaque.
	std::vector<uint8_t> rgba;

	/**
	 * @brief
	 *  Check that the pixels match the size.
	 * @return True when the image holds `width * height` pixels.
	 */
	[[nodiscard]] auto isValid() const -> bool {
		return size.surface() > 0 && rgba.size() == static_cast<size_t>(size.surface()) * 4;
	}

	/**
	 * @brief
	 *  Encode the image as a PNG file content.
	 * @return The PNG bytes, empty when the image is not valid.
	 */
	[[nodiscard]] auto encodePng() const -> std::vector<uint8_t>;
};

/**
 * @brief
 *  What a bounded play did.
 */
struct PlayReport {
	/// Frames played.
	uint32_t frames{0};
	/// Simulation ticks run.
	uint64_t ticks{0};
	/// Duration of every frame, in milliseconds.
	double timestepMs{0.0};
	/// CPU time of all the frames, in milliseconds.
	double totalMs{0.0};
	/// Longest frame, in milliseconds.
	double maxFrameMs{0.0};
	/// Draw calls issued by all the frames.
	uint64_t drawCalls{0};
	/// Phase timings of the last frame.
	scene::RuntimeTimings lastPhases{};
	/// Entities of the scene at the end.
	size_t entityCount{0};
	/// True when the play stopped before its bound (user stop, scene quit, game over in the headless session).
	bool interrupted{false};
	/// The last frame, when the request asked for it and the renderer could read it back.
	std::optional<FrameCapture> capture{};// NOLINT(readability-redundant-member-init) callers may omit it

	/**
	 * @brief
	 *  Mean CPU time of a frame.
	 * @return The mean, in milliseconds (0 without frame).
	 */
	[[nodiscard]] auto getMeanFrameMs() const -> double {
		return frames > 0 ? totalMs / static_cast<double>(frames) : 0.0;
	}
};

/**
 * @brief
 *  Measurements of the frames a session rendered.
 */
struct SessionStatistics {
	/// Frames rendered since the session started.
	uint64_t frameCount{0};
	/// CPU time of the last frame, in milliseconds.
	double lastFrameMs{0.0};
	/// Mean CPU time of a frame, in milliseconds.
	double meanFrameMs{0.0};
	/// Draw calls of the last frame.
	uint64_t lastDrawCalls{0};
	/// Entities of the current scene.
	size_t entityCount{0};
	/// Simulation ticks run since the scene started playing (0 in Edit).
	uint64_t tickCount{0};
	/// Phase timings of the last runtime update (zero while they are not measured).
	scene::RuntimeTimings lastPhases{};

	/**
	 * @brief
	 *  Account for a rendered frame.
	 * @param[in] iFrameMs CPU time of the frame, in milliseconds.
	 * @param[in] iDrawCalls Draw calls of the frame.
	 */
	void addFrame(double iFrameMs, uint64_t iDrawCalls);
};

/**
 * @brief
 *  Drives a bounded play frame by frame: time step, held keys, bounds, measurements.
 *
 * Shared by the editor (one frame of the main loop at a time) and the headless session (a loop).
 */
class PlayDriver final {
public:
	PlayDriver() = default;

	PlayDriver(const PlayDriver&) = delete;

	PlayDriver(PlayDriver&&) = default;

	auto operator=(const PlayDriver&) -> PlayDriver& = delete;

	auto operator=(PlayDriver&&) -> PlayDriver& = default;

	~PlayDriver() = default;

	/**
	 * @brief
	 *  Check a request.
	 * @param[in] iRequest The request.
	 * @return The failure, or nothing when the request can run.
	 */
	[[nodiscard]] static auto validate(const PlayRequest& iRequest) -> std::optional<SessionFailure>;

	/**
	 * @brief
	 *  Start a bounded play (the request is valid).
	 * @param[in] iRequest The request.
	 * @param[in] iTickStep Duration of one simulation tick of the scene, used when the request gives no step.
	 * @param[in] iTicksBefore Ticks the scene clock already counts, so only the ticks of this play are counted.
	 */
	void start(const PlayRequest& iRequest, const core::Timestep& iTickStep, uint64_t iTicksBefore);

	/**
	 * @brief
	 *  Check whether a bounded play runs.
	 * @return True between `start` and `finish`.
	 */
	[[nodiscard]] auto isActive() const -> bool { return m_active; }

	/**
	 * @brief
	 *  Get the request being played.
	 * @return The request.
	 */
	[[nodiscard]] auto getRequest() const -> const PlayRequest& { return m_request; }

	/**
	 * @brief
	 *  Get the duration of every frame of the play.
	 * @return The fixed time step.
	 */
	[[nodiscard]] auto getTimestep() const -> const core::Timestep& { return m_step; }

	/**
	 * @brief
	 *  Set the inputs of the next frame (the held keys, buttons and gamepad).
	 */
	void beginFrame() const;

	/**
	 * @brief
	 *  Account for a played frame.
	 * @param[in] iFrameMs CPU time of the frame, in milliseconds.
	 * @param[in] iDrawCalls Draw calls of the frame.
	 * @param[in] iScene The playing scene.
	 * @return True when a bound is reached (`PlayRequest::maxCount` frames at most, whatever the request).
	 */
	auto endFrame(double iFrameMs, uint64_t iDrawCalls, const scene::Scene& iScene) -> bool;

	/**
	 * @brief
	 *  End the play: release the held keys and build the report.
	 * @param[in] iScene The playing scene (null when it is gone: no end state in the report).
	 * @param[in] iInterrupted True when the play stops before its bound.
	 * @return The report (without capture).
	 */
	auto finish(const scene::Scene* iScene, bool iInterrupted) -> PlayReport;

private:
	/// The request being played.
	PlayRequest m_request;
	/// Duration of every frame.
	core::Timestep m_step;
	/// Scene clock ticks at the last frame accounted for.
	uint64_t m_lastTickTotal{0};
	/// Report under construction.
	PlayReport m_report;
	/// True between `start` and `finish`.
	bool m_active{false};
};

/**
 * @brief
 *  An editing session: the scene, its undo history, the command registry, bounded plays, captures and statistics.
 *
 * Implemented by the editor (`EditorLayer`, the active scene document) and by the `HeadlessSession` (no window, no
 * ImGui). Clients (tests, the MCP dispatcher, the console) only see this interface.
 */
class EditorSession {
public:
	EditorSession() = default;

	EditorSession(const EditorSession&) = delete;

	EditorSession(EditorSession&&) = delete;

	auto operator=(const EditorSession&) -> EditorSession& = delete;

	auto operator=(EditorSession&&) -> EditorSession& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~EditorSession();

	/**
	 * @brief
	 *  Get the scene shown: the edited scene, or its running copy while it plays.
	 * @return The scene, null when none is open.
	 */
	[[nodiscard]] virtual auto getSessionScene() const -> shared<scene::Scene> = 0;

	/**
	 * @brief
	 *  Get the file of the scene.
	 * @return The path, empty when the scene was never saved.
	 */
	[[nodiscard]] virtual auto getSessionScenePath() const -> std::filesystem::path = 0;

	/**
	 * @brief
	 *  Get the directory of the open project, the only place remote callers may name files in.
	 * @return The directory, empty without a project.
	 */
	[[nodiscard]] virtual auto getSessionProjectDirectory() const -> std::filesystem::path = 0;

	/**
	 * @brief
	 *  Get the undo history of the scene.
	 * @return The undo manager, null when no scene is open.
	 */
	[[nodiscard]] virtual auto getSessionUndo() -> SceneUndoManager* = 0;

	/**
	 * @brief
	 *  Get the command registry.
	 * @return The registry.
	 */
	[[nodiscard]] virtual auto getRegistry() const -> const commands::CommandRegistry& = 0;

	/**
	 * @brief
	 *  Build the context a command runs on: scene, undo history, play state, scene file, save callback.
	 * @return The context (null scene when none is open: commands then fail cleanly).
	 */
	[[nodiscard]] virtual auto makeCommandContext() -> commands::CommandContext = 0;

	/**
	 * @brief
	 *  Check whether the scene plays (or is paused in Play).
	 * @return True outside Edit.
	 */
	[[nodiscard]] virtual auto isScenePlaying() const -> bool = 0;

	/**
	 * @brief
	 *  Start a bounded play; a scene in Edit enters Play, a playing scene goes on from where it is.
	 *
	 * The headless session plays the whole request before returning; the editor plays it over the next frames of
	 * its main loop. The report is then available from `takePlayReport`.
	 * @param[in] iRequest The request.
	 * @return Nothing on success, or why the play did not start.
	 */
	virtual auto startPlay(const PlayRequest& iRequest) -> expected<void, SessionFailure> = 0;

	/**
	 * @brief
	 *  Check whether a bounded play is running.
	 * @return True until its bound is reached.
	 */
	[[nodiscard]] virtual auto isPlayRunning() const -> bool = 0;

	/**
	 * @brief
	 *  Take the report of the last bounded play.
	 * @return The report, once (nothing while it runs or when no play ended since the last call).
	 */
	virtual auto takePlayReport() -> std::optional<PlayReport> = 0;

	/**
	 * @brief
	 *  Stop the scene (an unfinished bounded play reports as interrupted) and go back to the edited scene.
	 * @return Nothing on success, `NotPlaying` when the scene is in Edit.
	 */
	virtual auto stopPlay() -> expected<void, SessionFailure> = 0;

	/**
	 * @brief
	 *  Read the last rendered frame back.
	 * @return The image, or why it cannot be read.
	 */
	virtual auto captureFrame() -> expected<FrameCapture, SessionFailure> = 0;

	/**
	 * @brief
	 *  Get the frame measurements.
	 * @return The statistics.
	 */
	[[nodiscard]] virtual auto getStatistics() const -> SessionStatistics = 0;

	/**
	 * @brief
	 *  Run a named command on the session's context (see CommandRegistry::execute).
	 * @param[in] iName Command name.
	 * @param[in] iArgs Command arguments.
	 * @param[in] iSource Who asks, recorded by the journal (by default an external tool, the MCP server).
	 * @return What the command did, or why it did not run.
	 */
	auto execute(std::string_view iName, const commands::CommandArgs& iArgs,
				 commands::CommandSource iSource = commands::CommandSource::Mcp)
			-> expected<commands::CommandResult, commands::CommandFailure>;

	/**
	 * @brief
	 *  Replay a macro on the session's scene, as one undo step (refused while the scene plays).
	 * @param[in] iMacro The macro.
	 * @param[in] iDescription Description of the undo step.
	 * @return What the replay did, or the failing line and why.
	 */
	auto replay(const commands::Macro& iMacro, const std::string& iDescription)
			-> expected<commands::MacroReport, commands::MacroFailure>;

	/**
	 * @brief
	 *  Get the journal of the commands executed in the session, whatever their source.
	 * @return The journal.
	 */
	[[nodiscard]] auto getJournal() -> commands::CommandJournal& { return m_journal; }

	/**
	 * @brief
	 *  Get the journal of the commands executed in the session, whatever their source.
	 * @return The journal.
	 */
	[[nodiscard]] auto getJournal() const -> const commands::CommandJournal& { return m_journal; }

private:
	/// Commands executed in the session; the implementation binds its registry to it.
	commands::CommandJournal m_journal;
};

}// namespace owl::nest::session
