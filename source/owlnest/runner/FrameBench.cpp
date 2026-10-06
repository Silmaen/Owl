/**
 * @file FrameBench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "FrameBench.h"

#include "FrameBenchStats.h"

#include <renderer/RendererVoxel.h>
#include <renderer/gpu/RenderCommand.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <functional>
#include <print>
#include <span>
#include <string_view>

namespace owl::nest::runner {

namespace {

// Extra frames allowed after the measure for the late GPU timings to come back.
constexpr uint32_t g_maxCooldown = 16;

auto parseUnsigned(const std::string_view iText, uint32_t& oValue) -> bool {
	const std::string text{iText};
	const char* const end = text.c_str() + text.size();
	const auto [ptr, ec] = std::from_chars(text.c_str(), end, oValue);
	return ec == std::errc{} && ptr == end;
}

auto parseDouble(const std::string_view iText, double& oValue) -> bool {
	const std::string text{iText};
	const char* const end = text.c_str() + text.size();
	const auto [ptr, ec] = std::from_chars(text.c_str(), end, oValue);
	return ec == std::errc{} && ptr == end;
}

auto parseSize(const std::string_view iText, math::vec2ui& oSize) -> bool {
	const auto sep = iText.find('x');
	if (sep == std::string_view::npos)
		return false;
	uint32_t width = 0;
	uint32_t height = 0;
	if (!parseUnsigned(iText.substr(0, sep), width) || !parseUnsigned(iText.substr(sep + 1), height) || width == 0 ||
		height == 0)
		return false;
	oSize = {width, height};
	return true;
}

auto findProject(const std::filesystem::path& iScene) -> std::filesystem::path {
	for (auto dir = iScene.parent_path(); !dir.empty(); dir = dir.parent_path()) {
		if (exists(dir / "owl_project.yml"))
			return dir;
		if (dir == dir.root_path())
			break;
	}
	return {};
}

auto backendName(const renderer::gpu::RenderAPI::Type iType) -> std::string_view {
	switch (iType) {
		case renderer::gpu::RenderAPI::Type::Null:
			return "null";
		case renderer::gpu::RenderAPI::Type::OpenGL:
			return "opengl";
		case renderer::gpu::RenderAPI::Type::Vulkan:
			return "vulkan";
	}
	return "unknown";
}

auto escapeJson(const std::string_view iText) -> std::string {
	std::string out;
	out.reserve(iText.size());
	for (const char c: iText) {
		if (c == '"' || c == '\\') {
			out.push_back('\\');
			out.push_back(c);
		} else if (static_cast<unsigned char>(c) < 0x20) {
			out += std::format("\\u{:04x}", static_cast<unsigned>(c));
		} else {
			out.push_back(c);
		}
	}
	return out;
}

// One reported series: name and per-sample accessor (nothing when the sample has no value).
struct Series {
	std::string_view name;
	std::function<std::optional<double>(const FrameSample&)> value;
};

auto makeSeries() -> std::vector<Series> {
	using S = FrameSample;
	const auto counter = [](uint64_t S::* iField) -> std::function<std::optional<double>(const S&)> {
		return [iField](const S& iSample) -> std::optional<double> { return static_cast<double>(iSample.*iField); };
	};
	return {
			{"cpu_total_ms", [](const S& iS) -> std::optional<double> { return iS.cpuTotalMs; }},
			{"cpu_begin_frame_ms", [](const S& iS) -> std::optional<double> { return iS.app.beginFrameMs; }},
			{"cpu_scene_update_ms",
			 [](const S& iS) -> std::optional<double> {
				 return std::max(0.0, iS.scene.totalMs - iS.scene.scriptsMs - iS.scene.physicsMs - iS.scene.renderMs);
			 }},
			{"cpu_scripts_ms", [](const S& iS) -> std::optional<double> { return iS.scene.scriptsMs; }},
			{"cpu_physics_ms", [](const S& iS) -> std::optional<double> { return iS.scene.physicsMs; }},
			{"cpu_render_prep_ms", [](const S& iS) -> std::optional<double> { return iS.scene.renderMs; }},
			{"cpu_runner_other_ms",
			 [](const S& iS) -> std::optional<double> { return std::max(0.0, iS.app.layersMs - iS.scene.totalMs); }},
			{"cpu_gui_ms", [](const S& iS) -> std::optional<double> { return iS.app.guiMs; }},
			{"cpu_submit_ms", [](const S& iS) -> std::optional<double> { return iS.app.endFrameMs; }},
			{"cpu_present_ms", [](const S& iS) -> std::optional<double> { return iS.app.presentMs; }},
			{"cpu_other_ms", [](const S& iS) -> std::optional<double> { return iS.app.soundMs + iS.app.schedulerMs; }},
			{"gpu_busy_ms",
			 [](const S& iS) -> std::optional<double> {
				 if (!iS.gpu.has_value() || iS.gpu->intervalCount == 0)
					 return std::nullopt;
				 return iS.gpu->busyMs;
			 }},
			{"gpu_span_ms",
			 [](const S& iS) -> std::optional<double> {
				 if (!iS.gpu.has_value() || iS.gpu->intervalCount == 0)
					 return std::nullopt;
				 return iS.gpu->spanMs;
			 }},
			{"draw_calls", counter(&S::drawCalls)},
			{"submits", counter(&S::submits)},
			{"queue_wait_idle", counter(&S::queueWaitIdles)},
			{"device_wait_idle", counter(&S::deviceWaitIdles)},
	};
}

auto collect(const std::vector<FrameSample>& iSamples, const Series& iSeries) -> std::vector<double> {
	std::vector<double> values;
	values.reserve(iSamples.size());
	for (const auto& sample: iSamples)
		if (const auto value = iSeries.value(sample); value.has_value())
			values.push_back(*value);
	return values;
}

auto jsonNumber(const std::optional<double>& iValue) -> std::string {
	if (!iValue.has_value())
		return "null";
	return std::format("{:.4f}", *iValue);
}

}// namespace

auto hasFrameBenchFlag(const int iArgc, char** iArgv) -> bool {
	const std::span args(iArgv, static_cast<size_t>(std::max(iArgc, 0)));
	return std::ranges::any_of(args, [](const char* iArg) -> bool {
		return iArg != nullptr && std::string_view{iArg} == "--frame-bench";
	});
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto parseFrameBenchOptions(const int iArgc, char** iArgv, const std::filesystem::path& iCallerDir)
		-> expected<FrameBenchOptions, std::string> {
	FrameBenchOptions options;
	const std::span args(iArgv, static_cast<size_t>(std::max(iArgc, 0)));
	const auto resolve = [&iCallerDir](const std::string_view iPath) -> std::filesystem::path {
		const std::filesystem::path path{iPath};
		return path.is_absolute() ? path : (iCallerDir / path).lexically_normal();
	};
	for (size_t i = 1; i < args.size(); ++i) {
		const std::string_view arg{args[i]};
		const bool hasValue = i + 1 < args.size();
		const std::string_view value = hasValue ? std::string_view{args[i + 1]} : std::string_view{};
		const auto badValue = [&arg, &value]() -> unexpected<std::string> {
			return unexpected<std::string>{std::format("invalid value '{}' for {}", value, arg)};
		};
		if (arg == "--vsync") {
			options.vSync = true;
			continue;
		}
		if (arg == "--validation") {
			options.validation = true;
			continue;
		}
		if (!hasValue)
			return unexpected<std::string>{std::format("missing value for {}", arg)};
		++i;
		if (arg == "--frame-bench") {
			options.scene = resolve(value);
		} else if (arg == "--project") {
			options.project = resolve(value);
		} else if (arg == "--out") {
			options.out = resolve(value);
		} else if (arg == "--capture") {
			options.capture = resolve(value);
		} else if (arg == "--frames") {
			if (!parseUnsigned(value, options.frames) || options.frames == 0)
				return badValue();
		} else if (arg == "--warmup") {
			if (!parseUnsigned(value, options.warmup))
				return badValue();
		} else if (arg == "--size") {
			if (!parseSize(value, options.size))
				return badValue();
		} else if (arg == "--timestep-ms") {
			if (!parseDouble(value, options.timeStepMs) || options.timeStepMs <= 0.0)
				return badValue();
		} else if (arg == "--backend") {
			if (value == "vulkan")
				options.backend = renderer::gpu::RenderAPI::Type::Vulkan;
			else if (value == "opengl")
				options.backend = renderer::gpu::RenderAPI::Type::OpenGL;
			else if (value == "null")
				options.backend = renderer::gpu::RenderAPI::Type::Null;
			else
				return badValue();
		} else {
			return unexpected<std::string>{std::format("unknown option {}", arg)};
		}
	}
	if (options.scene.empty())
		return unexpected<std::string>{"--frame-bench needs a scene file"};
	if (!exists(options.scene))
		return unexpected<std::string>{std::format("scene file {} not found", options.scene.string())};
	if (options.project.empty())
		options.project = findProject(options.scene);
	return options;
}

FrameBench::FrameBench(FrameBenchOptions iOptions)
	: m_options{std::move(iOptions)}, m_stepDuration{std::chrono::duration_cast<core::Timestep::duration>(
											  std::chrono::duration<double, std::milli>{m_options.timeStepMs})} {
	m_samples.reserve(m_options.frames);
}

void FrameBench::start() {
	auto& app = app::Application::get();
	app.setFrameTimingsEnabled(true);
	renderer::gpu::RenderCommand::setGpuTimestampsEnabled(true);
	m_gpuTimed = renderer::gpu::RenderCommand::hasGpuTimestamps();
	m_device = renderer::gpu::RenderCommand::getDeviceName();
	m_presentMode = renderer::gpu::RenderCommand::getPresentMode();
	m_windowSize = app.getWindow().getSize();
	// A capture is compared to a reference image: mesh every voxel chunk in its frame, as a fixed frame count needs.
	if (!m_options.capture.empty()) {
		auto meshing = renderer::RendererVoxel::getMeshingConfig();
		meshing.async = false;
		renderer::RendererVoxel::setMeshingConfig(meshing);
	}
	OWL_INFO("FrameBench: {} on {} ({}, present {}), {} warm-up + {} measured frames.",
			 m_options.scene.filename().string(), backendName(m_options.backend), m_device, m_presentMode,
			 m_options.warmup, m_options.frames)
}

void FrameBench::onFrameStart() {
	const auto now = core::Timestep::clock::now();
	const auto counters = renderer::gpu::RenderCommand::getRenderCounters();
	if (m_current.has_value()) {
		auto& sample = m_samples[*m_current];
		sample.cpuTotalMs = std::chrono::duration<double, std::milli>(now - m_frameStart).count();
		sample.app = app::Application::get().getLastFrameTimings();
		sample.drawCalls = counters.drawCalls - m_counters.drawCalls;
		sample.submits = counters.submits - m_counters.submits;
		sample.queueWaitIdles = counters.queueWaitIdles - m_counters.queueWaitIdles;
		sample.deviceWaitIdles = counters.deviceWaitIdles - m_counters.deviceWaitIdles;
		m_current.reset();
	}
	m_frameStart = now;
	m_counters = counters;
	collectGpuTimings();
	m_step.forceUpdate(m_stepDuration);

	const uint64_t index = m_calls++;
	if (index >= m_options.warmup && index < static_cast<uint64_t>(m_options.warmup) + m_options.frames) {
		FrameSample sample;
		sample.gpuFrameId = renderer::gpu::RenderCommand::getGpuFrameId();
		m_samples.push_back(sample);
		m_current = m_samples.size() - 1;
		return;
	}
	if (index < m_options.warmup)
		return;
	const bool gpuMissing = m_gpuTimed && std::ranges::any_of(m_samples, [](const FrameSample& iSample) -> bool {
								return !iSample.gpu.has_value();
							});
	if (!gpuMissing || ++m_cooldown > g_maxCooldown)
		m_done = true;
}

void FrameBench::onSceneUpdated(const scene::RuntimeTimings& iTimings) {
	if (m_current.has_value())
		m_samples[*m_current].scene = iTimings;
}

void FrameBench::collectGpuTimings() {
	for (const auto& timing: renderer::gpu::RenderCommand::popGpuFrameTimings()) m_gpuPending[timing.frameId] = timing;
	if (m_gpuPending.empty())
		return;
	for (auto& sample: m_samples) {
		if (sample.gpu.has_value())
			continue;
		if (const auto it = m_gpuPending.find(sample.gpuFrameId); it != m_gpuPending.end()) {
			sample.gpu = it->second;
			m_gpuPending.erase(it);
		}
	}
	m_gpuPending.clear();
}

auto FrameBench::finish(const bool iInterrupted) -> int {
	if (m_finished)
		return iInterrupted ? 3 : 0;
	m_finished = true;
	m_interrupted = iInterrupted;
	collectGpuTimings();
	if (m_current.has_value()) {
		m_samples.pop_back();
		m_current.reset();
	}
	app::Application::get().setFrameTimingsEnabled(false);
	renderer::gpu::RenderCommand::setGpuTimestampsEnabled(false);
	std::println("{}", toText());
	int code = iInterrupted ? 3 : 0;
	if (!m_options.out.empty()) {
		std::error_code ec;
		if (m_options.out.has_parent_path())
			std::filesystem::create_directories(m_options.out.parent_path(), ec);
		std::ofstream file(m_options.out);
		if (file.good()) {
			file << toJson();
			OWL_INFO("FrameBench: Report written to {}.", m_options.out.string())
		} else {
			OWL_ERROR("FrameBench: Cannot write the report {}.", m_options.out.string())
			code = 5;
		}
	}
	return code;
}

auto FrameBench::toJson() const -> std::string {
	std::string json = "{\n";
	json += std::format("  \"tool\": \"OwlRunner --frame-bench\",\n");
#if defined(OWL_MAJOR) && defined(OWL_MINOR) && defined(OWL_PATCH)
	json += std::format("  \"engine_version\": \"{}.{}.{}\",\n", OWL_MAJOR, OWL_MINOR, OWL_PATCH);
#endif
#if defined(NDEBUG)
	json += "  \"build\": \"release\",\n";
#else
	json += "  \"build\": \"debug\",\n";
#endif
	json += std::format("  \"scene\": \"{}\",\n", escapeJson(m_options.scene.generic_string()));
	json += std::format("  \"backend\": \"{}\",\n", backendName(m_options.backend));
	json += std::format("  \"device\": \"{}\",\n", escapeJson(m_device));
	json += std::format("  \"present_mode\": \"{}\",\n", escapeJson(m_presentMode));
	json += std::format("  \"vsync_requested\": {},\n", m_options.vSync);
	json += std::format("  \"gpu_timestamps\": {},\n", m_gpuTimed);
	json += std::format("  \"window\": [{}, {}],\n", m_windowSize.x(), m_windowSize.y());
	json += std::format("  \"timestep_ms\": {:.4f},\n", m_options.timeStepMs);
	json += std::format("  \"warmup\": {},\n", m_options.warmup);
	json += std::format("  \"frames_requested\": {},\n", m_options.frames);
	json += std::format("  \"frames_measured\": {},\n", m_samples.size());
	json += std::format("  \"interrupted\": {},\n", m_interrupted);
	const auto series = makeSeries();
	json += "  \"summary\": {\n";
	for (size_t s = 0; s < series.size(); ++s) {
		const auto stat = summarize(collect(m_samples, series[s]));
		json += std::format("    \"{}\": {{\"count\": {}, \"median\": {:.4f}, \"p95\": {:.4f}, \"p99\": {:.4f}, "
							"\"iqr\": {:.4f}, \"mean\": {:.4f}, \"min\": {:.4f}, \"max\": {:.4f}}}{}\n",
							series[s].name, stat.count, stat.median, stat.p95, stat.p99, stat.iqr, stat.mean, stat.min,
							stat.max, s + 1 < series.size() ? "," : "");
	}
	json += "  },\n";
	json += "  \"samples\": [\n";
	for (size_t i = 0; i < m_samples.size(); ++i) {
		json += "    {";
		for (size_t s = 0; s < series.size(); ++s)
			json += std::format("\"{}\": {}{}", series[s].name, jsonNumber(series[s].value(m_samples[i])),
								s + 1 < series.size() ? ", " : "");
		json += i + 1 < m_samples.size() ? "},\n" : "}\n";
	}
	json += "  ]\n}\n";
	return json;
}

auto FrameBench::toText() const -> std::string {
	std::string text = std::format("Frame bench: {} | backend {} | device {} | present {} | {} frames{}\n",
								   m_options.scene.filename().string(), backendName(m_options.backend), m_device,
								   m_presentMode, m_samples.size(), m_interrupted ? " (interrupted)" : "");
	text += std::format("{:<22} {:>6} {:>10} {:>10} {:>10} {:>10} {:>10}\n", "series", "count", "median", "p95", "p99",
						"iqr", "max");
	for (const auto& series: makeSeries()) {
		const auto stat = summarize(collect(m_samples, series));
		text += std::format("{:<22} {:>6} {:>10.4f} {:>10.4f} {:>10.4f} {:>10.4f} {:>10.4f}\n", series.name, stat.count,
							stat.median, stat.p95, stat.p99, stat.iqr, stat.max);
	}
	return text;
}

}// namespace owl::nest::runner
