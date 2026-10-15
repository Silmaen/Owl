/**
 * @file HeadlessSession.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "HeadlessSession.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <input/ActionMap.h>
#include <input/Input.h>
#include <renderer/Renderer.h>
#include <renderer/gpu/RenderCommand.h>
#include <scene/SceneSerializer.h>
#include <window/Window.h>

#include <yaml-cpp/yaml.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest::session {

namespace {

auto failure(const SessionError iError, std::string iMessage) -> unexpected<SessionFailure> {
	OWL_WARN("HeadlessSession: {}.", iMessage)
	return unexpected<SessionFailure>{SessionFailure{.error = iError, .message = std::move(iMessage)}};
}

}// namespace

auto findProjectDirectory(const std::filesystem::path& iScene) -> std::filesystem::path {
	for (auto dir = iScene.parent_path(); !dir.empty(); dir = dir.parent_path()) {
		if (exists(dir / "owl_project.yml"))
			return dir;
		if (dir == dir.root_path())
			break;
	}
	return {};
}

HeadlessSession::HeadlessSession(const math::vec2ui iSize)
	: m_size{iSize.surface() > 0 ? iSize : defaultSize},
	  m_previousSimulation{app::Application::instanced()
								   ? app::Application::get().getEngineContext().getSimulationSettings()
								   : scene::SimulationSettings{}},
	  m_previousActions{app::Application::instanced() ? app::Application::get().getEngineContext().getActionMap()
													  : input::ActionMap::getDefault()} {
	m_registry.setJournal(&getJournal());
	// Without a window the GLFW input has nothing to poll: the Null input also takes the injected keys.
	if (app::Application::instanced() && app::Application::get().getWindow().getType() == window::Type::Null)
		input::Input::init(window::Type::Null);
}

HeadlessSession::~HeadlessSession() {
	if (m_playing && m_activeScene)
		m_activeScene->onEndRuntime();
	input::Input::resetInjection();
	m_activeScene.reset();
	m_editScene.reset();
	m_target.reset();
	if (!app::Application::instanced())
		return;
	renderer::Renderer::setRenderStack(renderer::RenderStack{});
	auto& app = app::Application::get();
	if (!m_project.empty())
		app.removeAssetDirectory(m_project);
	app.getEngineContext().setSimulationSettings(m_previousSimulation);
	app.getEngineContext().setActionMap(*m_previousActions);
}

void HeadlessSession::newScene() {
	if (m_playing)
		std::ignore = stopPlay();
	m_editScene = mkShared<scene::Scene>();
	m_activeScene = m_editScene;
	m_scenePath.clear();
	m_undo.clear();
	m_undo.markSaved();
	installRenderStack();
}

auto HeadlessSession::loadScene(const std::filesystem::path& iPath) -> expected<void, SessionFailure> {
	const auto path = std::filesystem::weakly_canonical(std::filesystem::absolute(iPath));
	if (!exists(path))
		return failure(SessionError::LoadFailed, std::format("scene {} not found", path.string()));
	if (const auto project = findProjectDirectory(path); project != m_project) {
		auto& app = app::Application::get();
		if (!m_project.empty())
			app.removeAssetDirectory(m_project);
		m_project = project;
		if (!m_project.empty())
			app.addAssetDirectory({.title = "Session project", .assetsPath = m_project});
	}
	const auto loaded = mkShared<scene::Scene>();
	if (const scene::SceneSerializer serializer{loaded}; !serializer.deserialize(path))
		return failure(SessionError::LoadFailed, std::format("cannot load scene {}", path.string()));
	if (m_playing)
		std::ignore = stopPlay();
	m_editScene = loaded;
	m_activeScene = loaded;
	m_scenePath = path;
	m_undo.clear();
	m_undo.markSaved();
	loadProjectSettings();
	installRenderStack();
	return {};
}

void HeadlessSession::loadProjectSettings() {
	m_rendererStack = renderer::RendererStackConfig{};
	scene::SimulationSettings simulation = m_previousSimulation;
	if (!m_project.empty()) {
		try {
			if (const auto project = YAML::LoadFile((m_project / "owl_project.yml").string())["OwlProject"]; project) {
				if (const auto stack = project["RendererStack"]; stack)
					m_rendererStack = renderer::RendererStackConfig::fromYaml(YAML::Dump(stack));
				if (const auto settings = project["simulation"]; settings) {
					if (const auto rate = settings["tickRate"]; rate)
						simulation.tickRate = rate.as<float>();
					if (const auto ticks = settings["maxTicksPerFrame"]; ticks)
						simulation.maxTicksPerFrame = ticks.as<uint32_t>();
				}
			}
		} catch (const std::exception& iEx) {
			OWL_WARN("HeadlessSession: Cannot read {}/owl_project.yml ({}).", m_project.string(), iEx.what())
		}
	}
	app::Application::get().getEngineContext().setSimulationSettings(simulation);
	std::ignore = app::Application::get().getEngineContext().loadActionMap(m_project);
}

void HeadlessSession::installRenderStack() const {
	if (!m_activeScene)
		return;
	const auto& config = m_rendererStack.isEmpty() ? renderer::RendererStackConfig::makeDefault() : m_rendererStack;
	renderer::Renderer::setRenderStack(
			renderer::RenderStack::buildFromConfig(config, m_activeScene->getEnabledRenderers()));
}

auto HeadlessSession::getSessionUndo() -> SceneUndoManager* { return m_editScene ? &m_undo : nullptr; }

auto HeadlessSession::makeCommandContext() -> commands::CommandContext {
	return {.scene = m_activeScene,
			.undo = getSessionUndo(),
			.playing = m_playing,
			.scenePath = m_scenePath,
			.onSaved = [this](const std::filesystem::path& iPath) -> void { m_scenePath = iPath; }};
}

void HeadlessSession::ensureTarget() {
	if (m_target)
		return;
	using Format = renderer::gpu::AttachmentSpecification::Format;
	using Tiling = renderer::gpu::AttachmentSpecification::Tiling;
	m_target = renderer::gpu::Framebuffer::create({.size = m_size,
												   .attachments = {{Format::Surface, Tiling::Optimal},
																   {Format::RedInteger, Tiling::Optimal},
																   {Format::Depth24Stencil8, Tiling::Optimal}},
												   .samples = 1,
												   .swapChainTarget = false,
												   .debugName = "headless session"});
}

void HeadlessSession::playFrame(const core::Timestep& iStep) {
	// Frames count, not seconds: the async work (texture decodes) ends before each frame.
	app::Application::get().getTaskScheduler().waitEmptyQueue();
	ensureTarget();
	const auto drawsBefore = renderer::gpu::RenderCommand::getRenderCounters().drawCalls;
	const auto start = std::chrono::steady_clock::now();
	m_target->bind();
	renderer::gpu::RenderCommand::setClearColor({0.1f, 0.1f, 0.1f, 1.f});
	renderer::gpu::RenderCommand::clear();
	m_activeScene->onViewportResize(m_size);
	m_activeScene->onUpdateRuntime(iStep);
	m_target->unbind();
	const double frameMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	m_statistics.addFrame(frameMs, renderer::gpu::RenderCommand::getRenderCounters().drawCalls - drawsBefore);
}

auto HeadlessSession::startPlay(const PlayRequest& iRequest) -> expected<void, SessionFailure> {
	if (!m_editScene)
		return failure(SessionError::NoScene, "no scene to play");
	if (m_driver.isActive())
		return failure(SessionError::AlreadyPlaying, "a bounded play is running");
	if (const auto invalid = PlayDriver::validate(iRequest); invalid)
		return failure(invalid->error, invalid->message);
	if (!m_playing) {
		m_activeScene = scene::Scene::copy(m_editScene);
		m_activeScene->onStartRuntime();
		m_playing = true;
	}
	m_activeScene->setRuntimeTimingsEnabled(true);
	m_driver.start(iRequest, m_activeScene->getSimulationClock().getStepTimestep(),
				   m_activeScene->getSimulationClock().getTotalTickCount());
	bool done = false;
	bool interrupted = false;
	while (!done) {
		m_driver.beginFrame();
		playFrame(m_driver.getTimestep());
		done = m_driver.endFrame(m_statistics.lastFrameMs, m_statistics.lastDrawCalls, *m_activeScene);
		const bool ended = m_activeScene->status == scene::Scene::Status::Victory ||
						   m_activeScene->status == scene::Scene::Status::Death;
		if (!done && (m_activeScene->quitRequested || ended)) {
			interrupted = true;
			done = true;
		}
	}
	auto report = m_driver.finish(m_activeScene.get(), interrupted);
	if (iRequest.capture)
		report.capture = readTarget();
	if (iRequest.stopAtEnd || interrupted)
		std::ignore = stopPlay();
	m_report = std::move(report);
	return {};
}

auto HeadlessSession::takePlayReport() -> std::optional<PlayReport> { return std::exchange(m_report, std::nullopt); }

auto HeadlessSession::stopPlay() -> expected<void, SessionFailure> {
	if (!m_playing)
		return failure(SessionError::NotPlaying, "the scene does not play");
	if (m_driver.isActive())
		m_report = m_driver.finish(m_activeScene.get(), /*iInterrupted=*/true);
	m_activeScene->onEndRuntime();
	m_activeScene = m_editScene;
	m_playing = false;
	return {};
}

auto HeadlessSession::captureFrame() -> expected<FrameCapture, SessionFailure> {
	if (!m_activeScene)
		return failure(SessionError::NoScene, "no scene to capture");
	ensureTarget();
	m_target->bind();
	renderer::gpu::RenderCommand::setClearColor({0.1f, 0.1f, 0.1f, 1.f});
	renderer::gpu::RenderCommand::clear();
	m_activeScene->onViewportResize(m_size);
	m_activeScene->onRenderRuntime();
	m_target->unbind();
	auto frame = readTarget();
	if (!frame)
		return failure(SessionError::CaptureUnavailable, "the renderer cannot read the frame back");
	return std::move(*frame);
}

auto HeadlessSession::readTarget() const -> std::optional<FrameCapture> {
	auto pixels = m_target->readColorAttachment(0);
	if (pixels.empty()) {
		OWL_WARN("HeadlessSession: The renderer cannot read the frame back.")
		return std::nullopt;
	}
	// Captures read the same in any viewer: opaque.
	for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
	return FrameCapture{.size = m_target->getSpecification().size, .rgba = std::move(pixels)};
}

auto HeadlessSession::getStatistics() const -> SessionStatistics {
	SessionStatistics statistics = m_statistics;
	if (m_activeScene) {
		statistics.entityCount = m_activeScene->getAllEntities().size();
		statistics.lastPhases = m_activeScene->getLastRuntimeTimings();
		statistics.tickCount = m_playing ? m_activeScene->getSimulationClock().getTotalTickCount() : 0;
	}
	return statistics;
}

}// namespace owl::nest::session
