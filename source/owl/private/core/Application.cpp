/**
 * @file Application.cpp
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/Application.h"

#include "core/Environment.h"
#include "core/utils/StringUtils.h"
#include "external/yaml.h"
#include "input/Input.h"
#include "renderer/Renderer.h"
#include "sound/SoundSystem.h"

namespace owl::core {


Application* Application::s_instance = nullptr;

Application::Application(AppParams iAppParams) : m_initParams{std::move(iAppParams)} {
	OWL_PROFILE_FUNCTION()

	OWL_CORE_ASSERT(!s_instance, "Application already exists!")
	s_instance = this;

	// Look for things on the storages
	{
		// Set up a working directory
		// Assuming present of a folder 'res' containing the data
		m_workingDirectory = absolute(std::filesystem::current_path());
		OWL_CORE_INFO("Working directory: {}", m_workingDirectory.string())

		// load config file if any
		if (!m_initParams.isDummy) {
			OWL_SCOPE_UNTRACK
			const auto configPath = m_workingDirectory / "config.yml";
			if (exists(configPath))
				m_initParams.loadFromFile(configPath);
			// save config
			m_initParams.saveToFile(configPath);
		}

		if (m_initParams.useDebugging) {
			appendEnv("VK_ADD_LAYER_PATH", m_workingDirectory.string());
#ifdef OWL_VULKAN_LAYER_PATH
			appendEnv("VK_ADD_LAYER_PATH", OWL_VULKAN_LAYER_PATH);
#endif
		}

		Log::setFrameFrequency(m_initParams.frameLogFrequency);
	}
	// Looking for asset Directories
	{
#if defined(OWL_DEVELOPMENT) || defined(OWL_PACKAGE_ENGINE)
		// first (lowest priority) - Engine assets.
		if (const auto engineAsset = searchAssets("engine_assets"); engineAsset.has_value()) {
			m_assetDirectories.push_front({"Engine assets", engineAsset.value()});
		} else {
			OWL_CORE_ERROR("Unable to find engine assets")
		}
#endif
		// second working dir asset directory
		if (exists(m_workingDirectory / "assets")) {
			m_assetDirectories.push_front({"working dir assets", m_workingDirectory / "assets"});
		} else {
			OWL_CORE_WARN("Unable to find working dir assets")
		}
#ifdef OWL_DEVELOPMENT
		// third app asset if any.
		if (!m_initParams.assetsPattern.empty()) {
			if (const auto engineAsset = searchAssets(m_initParams.assetsPattern); engineAsset.has_value()) {
				m_assetDirectories.push_front({"App assets", engineAsset.value()});
			} else {
				OWL_CORE_ERROR("Unable to find app assets")
			}
		}
#endif
	}

	// Create the renderer
	{
		renderer::RenderCommand::create(m_initParams.renderer);
		// check renderer creation
		if (renderer::RenderCommand::getState() != renderer::RenderAPI::State::Created) {
			OWL_CORE_ERROR("ERROR while Creating Renderer")
			m_state = State::Error;
			return;
		}
		// wait for all asynchron tasks
		m_scheduler.waitEmptyQueue();
	}

	// create main window
	{
		mp_appWindow = input::Window::create({
				.winType = m_initParams.isDummy ? input::Type::Null : input::Type::Glfw,
				.title = m_initParams.name,
				.iconPath =
						m_initParams.icon.empty()
								? ""
								: renderer::Renderer::getTextureLibrary().find(m_initParams.icon).value_or("").string(),
				.width = m_initParams.width,
				.height = m_initParams.height,
		});
		input::Input::init();
		OWL_CORE_INFO("Window Created.")
	}

	// initialize the renderer
	{
		renderer::Renderer::init();
		// check renderer initialization
		if (renderer::RenderCommand::getState() != renderer::RenderAPI::State::Ready) {
			OWL_CORE_ERROR("ERROR while Initializing Renderer")
			m_state = State::Error;
			return;
		}
		// wait for all asynchron tasks
		m_scheduler.waitEmptyQueue();
		OWL_CORE_INFO("Renderer initiated.")
	}

	// set up the callbacks
	mp_appWindow->setEventCallback([this]<typename T0>(T0&& ioPh1) { onEvent(std::forward<T0>(ioPh1)); });

	// create the GUI layer
	if (m_initParams.hasGui) {
		mp_guiLayer = mkShared<gui::UiLayer>();
		pushOverlay(mp_guiLayer);

		// applying the theme.
		if (const auto defaultTheme = m_workingDirectory / "theme.yml"; exists(defaultTheme)) {
			gui::Theme theme;
			theme.loadFromFile(defaultTheme);
			gui::UiLayer::setTheme(theme);
		}
		// wait for all asynchron tasks
		m_scheduler.waitEmptyQueue();

		OWL_CORE_TRACE("GUI Layer created.")
	}

	// Create the sound system
	{
		sound::SoundCommand::create(m_initParams.isDummy ? sound::SoundAPI::Type::Null : m_initParams.sound);
		// check sound system creation
		if (sound::SoundCommand::getState() != sound::SoundAPI::State::Created) {
			OWL_CORE_ERROR("ERROR while Creating Sound system")
			m_state = State::Error;
			return;
		}
		sound::SoundSystem::init();
		// check sound system initialization
		if (sound::SoundCommand::getState() != sound::SoundAPI::State::Ready) {
			OWL_CORE_ERROR("ERROR while Initializing Sound system")
			m_state = State::Error;
			return;
		}
		// wait for all asynchron tasks
		m_scheduler.waitEmptyQueue();
		OWL_CORE_INFO("Sound system initiated.")
	}

	// update the state here. (required for font initialization)
	m_state = State::Running;
	m_fontLibrary.init();
	// wait for all asynchron tasks
	m_scheduler.waitEmptyQueue();

	OWL_CORE_TRACE("Application creation done.")
}

void Application::enableDocking() const {
	if (mp_guiLayer)
		mp_guiLayer->enableDocking();
}

void Application::disableDocking() const {
	if (mp_guiLayer)
		mp_guiLayer->disableDocking();
}

Application::~Application() {
	OWL_PROFILE_FUNCTION()

	m_fontLibrary.destroy();
	if (renderer::RenderCommand::getState() != renderer::RenderAPI::State::Error) {
		m_layerStack.clear();
		input::Input::invalidate();
		mp_appWindow->shutdown();
		OWL_CORE_TRACE("Application window shut down.")
		renderer::Renderer::shutdown();
		renderer::RenderCommand::invalidate();
		OWL_CORE_TRACE("Renderer shut down and invalidated.")
		mp_appWindow.reset();
	}
	if (sound::SoundCommand::getState() != sound::SoundAPI::State::Error) {
		sound::SoundSystem::shutdown();
		sound::SoundCommand::invalidate();
		OWL_CORE_TRACE("Sound system shut down and invalidated.")
	}
	invalidate();
}

void Application::close() { m_state = State::Stopped; }

void Application::invalidate() { s_instance = nullptr; }

void Application::run() {
	OWL_PROFILE_FUNCTION()

#if OWL_TRACKER_VERBOSITY >= 3
	uint64_t frameCount = 0;
#endif
	while (m_state == State::Running) {
		OWL_PROFILE_SCOPE("RunLoop")
		OWL_CORE_FRAME_ADVANCE

		m_stepper.update();

		// Graphics part.
		if (!m_minimized) {
			renderer::RenderCommand::beginFrame();
			if (renderer::RenderCommand::getState() != renderer::RenderAPI::State::Ready) {
				m_state = State::Error;
				continue;
			}
			{
				OWL_PROFILE_SCOPE("LayerStack onUpdate")

				for (const auto& layer: m_layerStack) layer->onUpdate(m_stepper);
			}
			if (mp_guiLayer) {
				OWL_PROFILE_SCOPE("LayerStack onImUpdate")
				mp_guiLayer->begin();
				for (const auto& layer: m_layerStack) layer->onImGuiRender(m_stepper);
				mp_guiLayer->end();
			}
			renderer::RenderCommand::endFrame();
		}

		// sound part
		{
			sound::SoundCommand::frame(m_stepper);
			if (sound::SoundCommand::getState() != sound::SoundAPI::State::Ready) {
				m_state = State::Error;
				continue;
			}
		}

		mp_appWindow->onUpdate();

		m_scheduler.frame(m_stepper);

#if OWL_TRACKER_VERBOSITY >= 3
		{
			if (const auto& memState = debug::TrackerAPI::checkState();
				memState.allocationCalls > memState.deallocationCalls && frameCount > 0) {
				OWL_CORE_TRACE("----------------------------------")
				OWL_CORE_TRACE("Frame Leak Detected")
				OWL_CORE_TRACE("-----------------------------------")
				OWL_CORE_TRACE("")
				OWL_CORE_TRACE(" LEAK Amount: {} in {} Unallocated chunks",
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
			[this]<typename T0>(T0&& ioPh1) { return onWindowClosed(std::forward<T0>(ioPh1)); });
	dispatcher.dispatch<event::WindowResizeEvent>(
			[this]<typename T0>(T0&& ioPh1) { return onWindowResized(std::forward<T0>(ioPh1)); });

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

auto Application::onWindowClosed(event::WindowCloseEvent&) -> bool {
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

void Application::pushLayer(shared<layer::Layer>&& iLayer) {
	OWL_PROFILE_FUNCTION()
	if (renderer::RenderCommand::getState() == renderer::RenderAPI::State::Error)
		return;
	m_layerStack.pushLayer(std::move(iLayer));
}

void Application::pushOverlay(shared<layer::Layer>&& iOverlay) {
	OWL_PROFILE_FUNCTION()
	if (renderer::RenderCommand::getState() == renderer::RenderAPI::State::Error)
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
		if (const auto dRenderer = magic_enum::enum_cast<renderer::RenderAPI::Type>(rendererStr); dRenderer.has_value())
			renderer = dRenderer.value();
		get(appConfig, "sound", rendererStr);
		if (const auto dSound = magic_enum::enum_cast<sound::SoundAPI::Type>(rendererStr); dSound.has_value())
			sound = dSound.value();
		get(appConfig, "hasGui", hasGui);
		get(appConfig, "useDebugging", useDebugging);
		get(appConfig, "frameLogFrequency", frameLogFrequency);
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

	out << YAML::EndMap;
	out << YAML::EndMap;
	std::ofstream fileOut(iFile);
	fileOut << out.c_str();
	fileOut.close();
}


}// namespace owl::core
