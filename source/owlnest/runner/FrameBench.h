/**
 * @file FrameBench.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <core/expected.h>
#include <owl.h>
#include <scene/Scene.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::nest::runner {

/**
 * @brief
 *  Command-line options of `OwlRunner --frame-bench`.
 */
struct FrameBenchOptions {
	/// Scene file to load (absolute).
	std::filesystem::path scene;
	/// Project directory (holds `owl_project.yml`); empty when none was found.
	std::filesystem::path project;
	/// JSON output file; empty for the text summary only.
	std::filesystem::path out;
	/// PNG of the last measured frame, rendered offscreen; empty to render to the window only.
	std::filesystem::path capture;
	/// Measured frames.
	uint32_t frames{1000};
	/// Warm-up frames run before the measure.
	uint32_t warmup{120};
	/// Render backend.
	renderer::gpu::RenderAPI::Type backend{renderer::gpu::RenderAPI::Type::Vulkan};
	/// Window size.
	math::vec2ui size{1280, 720};
	/// Fixed simulation time step, in milliseconds.
	double timeStepMs{1000.0 / 60.0};
	/// Keep vertical synchronisation (off by default).
	bool vSync{false};
	/// Enable the Vulkan validation layers.
	bool validation{false};
	/// When the runner started (set by `createApplication`), origin of the start-up times; epoch when unknown.
	core::Timestep::time_point processStart;
};

/**
 * @brief
 *  Find the project a scene belongs to: the nearest parent directory holding an `owl_project.yml`.
 * @param[in] iScene The scene file.
 * @return The project directory, empty when none was found.
 */
[[nodiscard]] auto findProject(const std::filesystem::path& iScene) -> std::filesystem::path;

/**
 * @brief
 *  Check whether the command line asks for the frame bench.
 * @param[in] iArgc Number of arguments.
 * @param[in] iArgv The arguments.
 * @return True when `--frame-bench` is present.
 */
[[nodiscard]] auto hasFrameBenchFlag(int iArgc, char** iArgv) -> bool;

/**
 * @brief
 *  Parse the frame bench options.
 * @param[in] iArgc Number of arguments.
 * @param[in] iArgv The arguments.
 * @param[in] iCallerDir Directory relative paths are resolved against (the caller's working directory).
 * @return The options, or an error message.
 */
[[nodiscard]] auto parseFrameBenchOptions(int iArgc, char** iArgv, const std::filesystem::path& iCallerDir)
		-> expected<FrameBenchOptions, std::string>;

/**
 * @brief
 *  One measured frame.
 */
struct FrameSample {
	/// Backend frame number, used to match the GPU timing.
	uint64_t gpuFrameId{0};
	/// Wall time between two consecutive frame starts.
	double cpuTotalMs{0.0};
	/// Main-loop phase timings of the frame.
	app::FrameTimings app;
	/// Scene phase timings of the frame.
	scene::RuntimeTimings scene;
	/// Draw calls of the frame.
	uint64_t drawCalls{0};
	/// Queue submissions of the frame.
	uint64_t submits{0};
	/// `vkQueueWaitIdle` calls of the frame.
	uint64_t queueWaitIdles{0};
	/// `vkDeviceWaitIdle` calls of the frame.
	uint64_t deviceWaitIdles{0};
	/// Blocking fence waits of the frame besides the frame pacing (one-shot submissions, read-backs).
	uint64_t fenceWaits{0};
	/// GPU timing of the frame, when read back.
	std::optional<renderer::gpu::GpuFrameTiming> gpu;
};

/**
 * @brief
 *  Drives a frame bench: fixed time step, warm-up, measured frames, read-back of the late GPU timings, report.
 *
 * `onFrameStart` runs at the top of every `RunnerLayer::onUpdate`: it closes the previous frame (wall time, main-loop
 * phases, counter deltas) so a sample covers one full main-loop iteration, `onSceneUpdated` stores the scene phases.
 */
class FrameBench final {
public:
	FrameBench(const FrameBench&) = delete;

	FrameBench(FrameBench&&) = delete;

	auto operator=(const FrameBench&) -> FrameBench& = delete;

	auto operator=(FrameBench&&) -> FrameBench& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iOptions The bench options.
	 */
	explicit FrameBench(FrameBenchOptions iOptions);

	/**
	 * @brief
	 *  Destructor.
	 */
	~FrameBench() = default;

	/**
	 * @brief
	 *  Get the options.
	 * @return The bench options.
	 */
	[[nodiscard]] auto getOptions() const -> const FrameBenchOptions& { return m_options; }

	/**
	 * @brief
	 *  Record the start-up time to an engine ready to load a scene (window, renderer, shaders); call first thing in
	 *  the layer's attach.
	 */
	void onEngineReady();

	/**
	 * @brief
	 *  Turn on the main-loop timings and the GPU timestamps; call once the scene is loaded.
	 */
	void start();

	/**
	 * @brief
	 *  Close the previous frame and open the next one.
	 */
	void onFrameStart();

	/**
	 * @brief
	 *  Store the scene phase timings of the current frame.
	 * @param[in] iTimings The timings from `Scene::getLastRuntimeTimings`.
	 */
	void onSceneUpdated(const scene::RuntimeTimings& iTimings);

	/**
	 * @brief
	 *  Get the fixed time step fed to the scene.
	 * @return The time step of the current frame.
	 */
	[[nodiscard]] auto getTimeStep() const -> const core::Timestep& { return m_step; }

	/**
	 * @brief
	 *  Check whether every measured frame and its GPU timing are in.
	 * @return True when the bench can stop.
	 */
	[[nodiscard]] auto isDone() const -> bool { return m_done; }

	/**
	 * @brief
	 *  Check whether the report was written.
	 * @return True after `finish`.
	 */
	[[nodiscard]] auto isFinished() const -> bool { return m_finished; }

	/**
	 * @brief
	 *  Stop the timings, print the summary and write the JSON report.
	 * @param[in] iInterrupted True when the run stopped before the end (scene quit, window closed).
	 * @return The process exit code (0 success, 3 interrupted, 5 report not written).
	 */
	auto finish(bool iInterrupted) -> int;

	/**
	 * @brief
	 *  Render the report as JSON.
	 * @return The JSON document.
	 */
	[[nodiscard]] auto toJson() const -> std::string;

	/**
	 * @brief
	 *  Render the summary as a text table.
	 * @return The summary.
	 */
	[[nodiscard]] auto toText() const -> std::string;

private:
	/**
	 * @brief
	 *  Pop the GPU timings the backend read back and attach them to their frames.
	 */
	void collectGpuTimings();

	/// Bench options.
	FrameBenchOptions m_options;
	/// Fixed time step fed to the scene.
	core::Timestep m_step;
	/// Fixed time step duration.
	core::Timestep::duration m_stepDuration{};
	/// Number of `onFrameStart` calls.
	uint64_t m_calls{0};
	/// Start of the current frame.
	core::Timestep::time_point m_frameStart;
	/// Counters at the start of the current frame.
	renderer::gpu::RenderCounters m_counters;
	/// Measured frames.
	std::vector<FrameSample> m_samples;
	/// Index in `m_samples` of the frame being recorded.
	std::optional<size_t> m_current;
	/// GPU timings not matched to a frame yet, by backend frame number.
	std::unordered_map<uint64_t, renderer::gpu::GpuFrameTiming> m_gpuPending;
	/// Frames run after the last measured one, waiting for the late GPU timings.
	uint32_t m_cooldown{0};
	/// True when the backend times the GPU.
	bool m_gpuTimed{false};
	/// True when the bench can stop.
	bool m_done{false};
	/// True once the report is written.
	bool m_finished{false};
	/// True when the run stopped early.
	bool m_interrupted{false};
	/// Backend device name, read at start.
	std::string m_device;
	/// Present mode, read at start.
	std::string m_presentMode;
	/// Window size at start.
	math::vec2ui m_windowSize{0, 0};
	/// Milliseconds from the runner start to `onEngineReady()`: window, renderer and shaders.
	std::optional<double> m_startupEngineMs;
	/// Milliseconds from the runner start to the first frame.
	std::optional<double> m_startupFirstFrameMs;
};

}// namespace owl::nest::runner
