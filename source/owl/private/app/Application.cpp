/**
 * @file Application.cpp
 * @author Silmaen
 * @date 04/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "app/Application.h"

#include "core/Environment.h"
#include "core/external/yaml.h"
#include "core/utils/StringUtils.h"
#include "data/assets/pack/PackExtractor.h"
#include "gui/UiLayer.h"
#include "input/Input.h"
#include "renderer/Renderer.h"
#include "sound/SoundSystem.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
OWL_DIAG_POP

#include <atomic>
#include <csignal>
#include <cstdint>

namespace owl::app {

Application* Application::s_instance = nullptr;

namespace {
std::atomic_bool g_StopRequested{false};

void onStopSignal(const int iSignal) {
	if (g_StopRequested.exchange(true)) {
		// Second signal: the main loop is stuck, fall back to the default (terminating) handler.
		std::signal(iSignal, SIG_DFL);
		std::raise(iSignal);
	}
}

auto requestedWindowPlatform(const window::Platform iConfigured) -> window::Platform {
	const auto envValue = core::getEnv(std::string{window::g_PlatformEnvVar});
	if (envValue.empty() && core::getEnv("OWL_FORCE_X11") == "1") {
		OWL_CORE_WARN("Application: OWL_FORCE_X11 is deprecated, use {}=x11.", window::g_PlatformEnvVar)
		return window::Platform::X11;
	}
	return window::resolvePlatform(iConfigured,
								   envValue.empty() ? std::nullopt : std::optional<std::string_view>{envValue});
}
}// namespace

Application::Application(AppParams iAppParams)// NOLINT(readability-function-cognitive-complexity)
	: m_initParams{std::move(iAppParams)} {

	OWL_PROFILE_FUNCTION()

	OWL_CORE_ASSERT(!s_instance, "Application already exists!")
	s_instance = this;
	g_StopRequested.store(false);
	if (!m_initParams.isDummy) {
		std::signal(SIGINT, onStopSignal);
		std::signal(SIGTERM, onStopSignal);
	}

	// Look for things on the storages
	{
		m_workingDirectory = absolute(std::filesystem::current_path());

		OWL_CORE_INFO("Working directory: {}.", m_workingDirectory.string())

		// load config file if any
		if (!m_initParams.isDummy && m_initParams.useConfigFile) {
			OWL_SCOPE_UNTRACK
			const auto configPath = m_workingDirectory / "config.yml";
			if (exists(configPath))
				m_initParams.loadFromFile(configPath);
			// save config
			m_initParams.saveToFile(configPath);
		}

		if (m_initParams.useDebugging) {
			core::appendEnv("VK_ADD_LAYER_PATH", m_workingDirectory.string());
#ifdef OWL_VULKAN_LAYER_PATH

			core::appendEnv("VK_ADD_LAYER_PATH", OWL_VULKAN_LAYER_PATH);
#endif
		}

		core::Log::setFrameFrequency(m_initParams.frameLogFrequency);
	}
	if (!m_initParams.packFile.empty()) {
		auto packPath = std::filesystem::path(m_initParams.packFile);
		if (packPath.is_relative())
			packPath = m_workingDirectory / packPath;
		if (openPack(packPath)) {
			const auto assetsDir = m_workingDirectory / "assets";
			if (const auto extracted = data::assets::pack::extractPack(m_packReader, assetsDir); extracted) {
				OWL_CORE_INFO("Pack: extracted {} entries into '{}' ({} up to date).", extracted->written,
							  assetsDir.string(), extracted->skipped)
			} else {
				OWL_CORE_ERROR("Pack: extraction into '{}' failed ({}).", assetsDir.string(),
							   magic_enum::enum_name(extracted.error()))
			}
		} else {
			OWL_CORE_ERROR("Failed to open asset pack: {}.", packPath.string())
		}
	}
	// Lowest priority first: a development tree finds the engine and app assets above the working directory.
	{
		if (const auto engineAssets = searchAssets("engine_assets"); engineAssets.has_value())
			m_assetDirectories.push_front({"Engine assets", engineAssets.value()});
		if (exists(m_workingDirectory / "assets"))
			m_assetDirectories.push_front({"working dir assets", m_workingDirectory / "assets"});
		if (!m_initParams.assetsPattern.empty()) {
			if (const auto appAssets = searchAssets(m_initParams.assetsPattern); appAssets.has_value())
				m_assetDirectories.push_front({"App assets", appAssets.value()});
		}
		if (m_assetDirectories.empty() && !hasOpenPack())
			OWL_CORE_ERROR("Application: No asset directory found from '{}'.", m_workingDirectory.string())
	}

	// Create the renderer
	{

		renderer::gpu::RenderCommand::create(m_initParams.renderer);
		// check renderer creation
		if (renderer::gpu::RenderCommand::getState() != renderer::gpu::RenderAPI::State::Created) {
			OWL_CORE_ERROR("ERROR while Creating Renderer.")
			m_state = State::Error;
			return;
		}
		// wait for all asynchronous tasks
		m_scheduler.waitEmptyQueue();
	}

	// create main window
	{
		mp_appWindow = window::Window::create({
				.winType = m_initParams.isDummy ? window::Type::Null : window::Type::Glfw,
				.title = m_initParams.name,
				.iconPath =
						m_initParams.icon.empty()
								? ""
								: renderer::Renderer::getTextureLibrary().find(m_initParams.icon).value_or("").string(),
				.width = m_initParams.width,
				.height = m_initParams.height,
				.platform = requestedWindowPlatform(m_initParams.windowPlatform),
				.appId = m_initParams.appId,
				.installDesktopEntry = m_initParams.installDesktopEntry,
		});

		input::Input::init();
		if (!m_initParams.vSync)
			mp_appWindow->setVSync(false);

		OWL_CORE_INFO("Window Created.")
	}

	// initialize the renderer context (no shader compilation yet)
	{
		renderer::Renderer::initContext();
		// check renderer initialization
		if (renderer::gpu::RenderCommand::getState() != renderer::gpu::RenderAPI::State::Ready) {
			OWL_CORE_ERROR("ERROR while Initializing Renderer.")
			m_state = State::Error;
			return;
		}
		// wait for all asynchronous tasks
		m_scheduler.waitEmptyQueue();

		OWL_CORE_TRACE("Renderer context initiated.")
	}

	// set up the callbacks
	mp_appWindow->setEventCallback([this]<typename T0>(T0&& ioPh1) -> auto { onEvent(std::forward<T0>(ioPh1)); });

	// create the GUI layer
	if (m_initParams.hasGui) {
		mp_imGuiLayer = mkShared<gui::UiLayer>();

		pushOverlay(mp_imGuiLayer);

		// applying the theme.
		if (const auto defaultTheme = m_workingDirectory / "theme.yml"; exists(defaultTheme)) {
			gui::Theme theme;
			theme.loadFromFile(defaultTheme);

			gui::UiLayer::setTheme(theme);
		}
		// wait for all asynchronous tasks
		m_scheduler.waitEmptyQueue();

		OWL_CORE_TRACE("GUI Layer created.")
	}

	{
		const auto requestedSound = m_initParams.isDummy ? sound::SoundAPI::Type::Null : m_initParams.sound;

		sound::SoundCommand::create(requestedSound);
		bool soundReady = sound::SoundCommand::getState() == sound::SoundAPI::State::Created;
		if (soundReady) {
			sound::SoundSystem::init();
			soundReady = sound::SoundCommand::getState() == sound::SoundAPI::State::Ready;
		}
		if (!soundReady && requestedSound != sound::SoundAPI::Type::Null) {
			OWL_CORE_WARN("Sound backend unavailable -- falling back to Null sound (audio disabled).")
			sound::SoundSystem::shutdown();

			sound::SoundCommand::create(sound::SoundAPI::Type::Null);

			sound::SoundSystem::init();
			soundReady = sound::SoundCommand::getState() == sound::SoundAPI::State::Ready;
		}
		if (!soundReady) {
			OWL_CORE_ERROR("ERROR while Initializing Sound system.")
			m_state = State::Error;
			return;
		}
		// wait for all asynchronous tasks
		m_scheduler.waitEmptyQueue();

		OWL_CORE_INFO("Sound system initiated.")
	}

	// Compile renderer shaders with an ImGui loading screen.
	{
		renderer::Renderer::initShaders([this](const uint32_t iCurrent, const uint32_t iTotal,
											   const std::string& iName) -> void {
			OWL_CORE_INFO("Compiling shaders {}/{}: {}...", iCurrent + 1, iTotal, iName)
			// Render a loading frame if ImGui is available.
			if (mp_imGuiLayer && mp_appWindow && !m_minimized) {
				renderer::gpu::RenderCommand::beginFrame();
				if (renderer::gpu::RenderCommand::getState() == renderer::gpu::RenderAPI::State::Ready) {
					mp_imGuiLayer->begin();
					const auto* viewport = ImGui::GetMainViewport();
					ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
					ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Always);
					if (ImGui::Begin("##Loading", nullptr,
									 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
											 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
						ImGui::Text("Loading engine...");
						const auto progress = static_cast<float>(iCurrent) / static_cast<float>(iTotal);
						ImGui::ProgressBar(progress, ImVec2(-1, 0));
						ImGui::Text("Compiling shader: %s", iName.c_str());
					}
					ImGui::End();
					mp_imGuiLayer->end();
				}
				renderer::gpu::RenderCommand::endFrame();
				mp_appWindow->onUpdate();
			}
		});
		m_scheduler.waitEmptyQueue();

		OWL_CORE_INFO("Renderer initiated.")
	}

	// update the state here. (required for font initialization)
	m_state = State::Running;
	m_fontLibrary.init();
	// wait for all asynchronous tasks
	m_scheduler.waitEmptyQueue();

	OWL_CORE_TRACE("Application creation done.")
}

void Application::enableDocking() const {
	if (mp_imGuiLayer)
		mp_imGuiLayer->enableDocking();
}

void Application::disableDocking() const {
	if (mp_imGuiLayer)
		mp_imGuiLayer->disableDocking();
}

Application::~Application() {
	OWL_PROFILE_FUNCTION()

	m_fontLibrary.destroy();
	if (renderer::gpu::RenderCommand::getState() != renderer::gpu::RenderAPI::State::Error) {
		// Ensure the GPU is idle before tearing anything down.
		if (mp_appWindow && mp_appWindow->getGraphContext() != nullptr)
			mp_appWindow->getGraphContext()->waitIdle();
		// 1. Release layers first — they may own GPU resources (e.g. ImGui Vulkan backend).
		m_layerStack.clear();

		input::Input::invalidate();
		// 2. Release Renderer2D / BackgroundRenderer resources (only if shaders were initialized).
		if (renderer::Renderer::getState() == renderer::Renderer::State::Running) {
			renderer::Renderer::shutdown();
		} else {
			renderer::Renderer::reset();
		}

		renderer::gpu::RenderCommand::invalidate();

		OWL_CORE_TRACE("Renderer shut down and invalidated.")
		// 4. Finally tear down the window (destroys GLFW window / terminates GLFW).
		if (mp_appWindow) {
			mp_appWindow->shutdown();
			OWL_CORE_TRACE("Application window shut down.")
		}
		mp_appWindow.reset();
	}
	if (sound::SoundCommand::getState() != sound::SoundAPI::State::Error) {
		sound::SoundSystem::shutdown();

		sound::SoundCommand::invalidate();

		OWL_CORE_TRACE("Sound system shut down and invalidated.")
	}
	if (!m_initParams.isDummy) {
		std::signal(SIGINT, SIG_DFL);
		std::signal(SIGTERM, SIG_DFL);
	}
	invalidate();
}

void Application::addAssetDirectory(const AssetDirectory& iDir) { m_assetDirectories.push_front(iDir); }

void Application::removeAssetDirectory(const std::filesystem::path& iPath) {
	m_assetDirectories.remove_if([&iPath](const AssetDirectory& iDir) -> bool { return iDir.assetsPath == iPath; });
}

void Application::setWindowTitle(const std::string& iTitle) {
	if (mp_appWindow)
		mp_appWindow->setTitle(iTitle);
}

void Application::close() { m_state = State::Stopped; }

void Application::invalidate() { s_instance = nullptr; }

void Application::run() {
	OWL_PROFILE_FUNCTION()

	OWL_PROFILE_THREAD_NAME("Main")

#if OWL_TRACKER_VERBOSITY >= 3
	uint64_t frameCount = 0;
#endif
	using clock = std::chrono::steady_clock;
	const auto elapsedMs = [](const clock::time_point& iFrom, const clock::time_point& iTo) -> double {
		return std::chrono::duration<double, std::milli>(iTo - iFrom).count();
	};
	while (m_state == State::Running) {
		OWL_PROFILE_SCOPE("RunLoop")
		if (g_StopRequested.load()) {
			OWL_CORE_INFO("Application: Stop signal received, closing.")
			close();
			break;
		}
		OWL_CORE_FRAME_ADVANCE
		const bool timed = m_frameTimingsEnabled;
		const auto now = [timed]() -> clock::time_point { return timed ? clock::now() : clock::time_point{}; };
		const auto tStart = now();
		auto tBegin = tStart;
		auto tLayers = tStart;
		auto tGui = tStart;
		auto tEnd = tStart;
		m_stepper.update();

		// Graphics part.
		if (!m_minimized) {
			renderer::gpu::RenderCommand::beginFrame();
			tBegin = now();
			if (renderer::gpu::RenderCommand::getState() != renderer::gpu::RenderAPI::State::Ready) {
				m_state = State::Error;
				continue;
			}
			{

				OWL_PROFILE_SCOPE("LayerStack onUpdate")
				for (const auto& layer: m_layerStack) layer->onUpdate(m_stepper);
			}
			tLayers = now();
			if (mp_imGuiLayer) {
				OWL_PROFILE_SCOPE("LayerStack onImUpdate")
				mp_imGuiLayer->begin();
				for (const auto& layer: m_layerStack) layer->onImGuiRender(m_stepper);
				mp_imGuiLayer->end();
			}
			tGui = now();

			renderer::gpu::RenderCommand::endFrame();
			tEnd = now();
		}

		// sound part
		{

			sound::SoundCommand::frame(m_stepper);
			if (sound::SoundCommand::getState() != sound::SoundAPI::State::Ready) {
				m_state = State::Error;
				continue;
			}
		}
		const auto tSound = now();

		mp_appWindow->onUpdate();
		const auto tPresent = now();
		m_scheduler.frame(m_stepper);
		if (timed) {
			const auto tDone = clock::now();
			m_lastFrameTimings = {.beginFrameMs = elapsedMs(tStart, tBegin),
								  .layersMs = elapsedMs(tBegin, tLayers),
								  .guiMs = elapsedMs(tLayers, tGui),
								  .endFrameMs = elapsedMs(tGui, tEnd),
								  .soundMs = elapsedMs(tEnd, tSound),
								  .presentMs = elapsedMs(tSound, tPresent),
								  .schedulerMs = elapsedMs(tPresent, tDone),
								  .totalMs = elapsedMs(tStart, tDone)};
		}
		OWL_PROFILE_FRAME_MARK()
#if OWL_TRACKER_VERBOSITY >= 3
		{
			if (const auto& memState = debug::TrackerAPI::checkState();
				memState.allocationCalls > memState.deallocationCalls && frameCount > 0) {
				OWL_CORE_TRACE("----------------------------------")
				OWL_CORE_TRACE("Frame Leak Detected.")
				OWL_CORE_TRACE("-----------------------------------")
				OWL_CORE_TRACE("")
				OWL_CORE_TRACE(" LEAK Amount: {} in {} Unallocated chunks.",
							   owl::core::utils::sizeToString(memState.allocatedMemory), memState.allocs.size())
				for (const auto& chunk: memState.allocs) { OWL_CORE_TRACE(" ** {}", chunk.toStr()) }

				OWL_CORE_TRACE("----------------------------------")
				OWL_CORE_TRACE("")
			}
		}
		++frameCount;
#endif
	}
}

void Application::onEvent(event::Event& ioEvent) {
	OWL_PROFILE_FUNCTION()

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::WindowCloseEvent>(
			[this]<typename T0>(T0&& ioPh1) -> bool { return onWindowClosed(std::forward<T0>(ioPh1)); });
	dispatcher.dispatch<event::WindowResizeEvent>(
			[this]<typename T0>(T0&& ioPh1) -> bool { return onWindowResized(std::forward<T0>(ioPh1)); });

#if !defined(__clang__) or __clang_major__ > 15
	for (const auto& it: std::ranges::reverse_view(m_layerStack)) {
#else
	for (auto it2 = m_layerStack.rbegin(); it2 != m_layerStack.rend(); ++it2) {
		auto it = (*it2);
#endif
		if (ioEvent.handled)
			break;
		it->onEvent(ioEvent);
	}
}

auto Application::onWindowClosed(const event::WindowCloseEvent&) -> bool {
	OWL_PROFILE_FUNCTION()

	close();
	return true;
}

auto Application::onWindowResized(const event::WindowResizeEvent& iEvent) -> bool {
	OWL_PROFILE_FUNCTION()

	if (iEvent.getWidth() == 0 || iEvent.getHeight() == 0) {
		m_minimized = true;
		return false;
	}
	m_minimized = false;
	renderer::Renderer::onWindowResized(iEvent.getWidth(), iEvent.getHeight());
	return false;
}

void Application::pushLayer(shared<app::layer::Layer>&& iLayer) {
	OWL_PROFILE_FUNCTION()

	if (renderer::gpu::RenderCommand::getState() == renderer::gpu::RenderAPI::State::Error)
		return;
	m_layerStack.pushLayer(std::move(iLayer));
}

void Application::pushOverlay(shared<app::layer::Layer>&& iOverlay) {
	OWL_PROFILE_FUNCTION()

	if (renderer::gpu::RenderCommand::getState() == renderer::gpu::RenderAPI::State::Error)
		return;
	m_layerStack.pushOverlay(std::move(iOverlay));
}

auto Application::searchAssets(const std::string& iPattern) const -> std::optional<std::filesystem::path> {
	OWL_PROFILE_FUNCTION()

	OWL_SCOPE_UNTRACK
	std::filesystem::path parent = m_workingDirectory;
	std::filesystem::path assets = parent / iPattern;
	while (parent != parent.root_path()) {
		if (exists(assets)) {
			return assets;
		}
		parent = parent.parent_path();
		assets = parent / iPattern;
	}
	return std::nullopt;
}

void AppParams::loadFromFile(const std::filesystem::path& iFile) {
	YAML::Node data = YAML::LoadFile(iFile.string());
	if (const auto appConfig = data["AppConfig"]; appConfig) {
		get(appConfig, "width", width);
		get(appConfig, "height", height);
		std::string rendererStr;
		get(appConfig, "renderer", rendererStr);
		if (rendererStr == "null") {
			renderer = renderer::gpu::RenderAPI::Type::Null;
		} else {
			if (const auto dRenderer = magic_enum::enum_cast<renderer::gpu::RenderAPI::Type>(rendererStr);
				dRenderer.has_value())
				renderer = dRenderer.value();
		}
		get(appConfig, "sound", rendererStr);
		if (rendererStr == "null") {
			sound = sound::SoundAPI::Type::Null;
		} else {
			if (const auto dSound = magic_enum::enum_cast<sound::SoundAPI::Type>(rendererStr); dSound.has_value())
				sound = dSound.value();
		}
		get(appConfig, "hasGui", hasGui);
		get(appConfig, "useDebugging", useDebugging);
		get(appConfig, "frameLogFrequency", frameLogFrequency);
		std::string platformStr;
		get(appConfig, "windowPlatform", platformStr);
		if (const auto platform = window::parsePlatform(platformStr); platform.has_value())
			windowPlatform = platform.value();
		else if (!platformStr.empty())
			OWL_CORE_WARN("AppParams: Unknown windowPlatform '{}', keeping {}.", platformStr,
						  window::platformName(windowPlatform))
		get(appConfig, "installDesktopEntry", installDesktopEntry);
	}
}

void AppParams::saveToFile(const std::filesystem::path& iFile) const {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "AppConfig" << YAML::Value << YAML::BeginMap;

	out << YAML::Key << "width" << YAML::Value << width;
	out << YAML::Key << "height" << YAML::Value << height;
	out << YAML::Key << "renderer" << YAML::Value << std::string(magic_enum::enum_name(renderer));
	out << YAML::Key << "sound" << YAML::Value << std::string(magic_enum::enum_name(sound));
	out << YAML::Key << "hasGui" << YAML::Value << hasGui;
	out << YAML::Key << "useDebugging" << YAML::Value << useDebugging;
	out << YAML::Key << "frameLogFrequency" << YAML::Value << frameLogFrequency;
	out << YAML::Key << "windowPlatform" << YAML::Value << std::string(window::platformName(windowPlatform));
	out << YAML::Key << "installDesktopEntry" << YAML::Value << installDesktopEntry;

	out << YAML::EndMap;
	out << YAML::EndMap;
	std::ofstream fileOut(iFile);
	fileOut << out.c_str();
	fileOut.close();
}

auto Application::openPack(const std::filesystem::path& iPackFile) -> bool {
	closePack();
	if (const auto opened = m_packReader.tryOpen(iPackFile); !opened) {
		OWL_CORE_ERROR("Failed to open asset pack: {} ({}).", iPackFile.string(), magic_enum::enum_name(opened.error()))
		return false;
	}
	OWL_CORE_INFO("Opened asset pack: {} ({} entries).", iPackFile.string(), m_packReader.getHeader().entryCount)
	return true;
}

void Application::closePack() {
	if (m_packReader.isOpen())
		m_packReader.close();
}

auto Application::hasOpenPack() const -> bool { return m_packReader.isOpen(); }

auto Application::loadFromPack(const std::string& iPath) const -> std::optional<std::vector<uint8_t>> {
	if (!m_packReader.isOpen())
		return std::nullopt;
	return m_packReader.readEntry(iPath);
}

auto Application::packContains(const std::string& iPath) const -> bool {
	if (!m_packReader.isOpen())
		return false;
	return m_packReader.contains(iPath);
}

}// namespace owl::app
