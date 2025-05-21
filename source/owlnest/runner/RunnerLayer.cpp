/**
 * @file RunnerLayer.cpp
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "RunnerLayer.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP


namespace owl::nest::runner {
namespace {

template<class T>
void get(const YAML::Node& iNode, const std::string& iKey, T& oValue) {
	if (const auto val = iNode[iKey]; val)
		oValue = val.as<T>();
}

auto isSubdir(const std::filesystem::path& iFile, const std::filesystem::path& iDir)
		-> std::optional<std::filesystem::path> {
	if (!is_directory(iDir))
		return std::nullopt;
	if (iFile.string().find(iDir.string()) != std::string::npos) {
		return relative(iFile, iDir);
	}
	return std::nullopt;
}
}// namespace

void RunnerConfig::loadYaml(const std::filesystem::path& iPath) {
	YAML::Node data = YAML::LoadFile(iPath.string());
	const auto& app = core::Application::get();
	firstScene.clear();
	if (const auto appConfig = data["RunnerConfig"]; appConfig) {
		std::string bob;
		get(appConfig, "FirstScene", bob);
		OWL_CORE_INFO("FirstScene: {}", bob)
		for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
			OWL_CORE_INFO("Checking: {}", (assetsPath / bob).string())
			if (exists(assetsPath / bob)) {
				firstScene = (assetsPath / bob).string();
				break;
			}
		}
	}
}

void RunnerConfig::saveYaml(const std::filesystem::path& iPath) const {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "RunnerConfig" << YAML::Value << YAML::BeginMap;

	const auto& app = core::Application::get();
	for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
		if (const auto dir = isSubdir(firstScene, assetsPath); dir.has_value()) {
			out << YAML::Key << "FirstScene" << YAML::Value << dir.value().string();
			break;
		}
	}
	out << YAML::EndMap;
	out << YAML::EndMap;
	std::ofstream fileOut(iPath);
	fileOut << out.c_str();
	fileOut.close();
}

RunnerLayer::RunnerLayer() : Layer("RunnerLayer") {}

void RunnerLayer::onAttach() {
	OWL_PROFILE_FUNCTION()

	// Lod the config file
	auto& app = core::Application::get();
	{
		const auto config = app.getWorkingDirectory() / "runner.yml";
		if (!exists(config)) {
			OWL_CORE_ERROR("Runner config not found")
			app.close();
			return;
		}
		m_config.loadYaml(config);
	}
	m_viewportSize = app.getWindow().getSize();
	m_activeScene = mkShared<scene::Scene>();
	if (!std::filesystem::exists(m_config.firstScene)) {
		OWL_CORE_ERROR("Runner first scene {} not found", m_config.firstScene)
		app.close();
		return;
	}
	if (const scene::SceneSerializer sc(m_activeScene); !sc.deserialize(m_config.firstScene)) {
		OWL_CORE_ERROR("Failed to load first scene")
		app.close();
	}
}

void RunnerLayer::onDetach() {
	OWL_PROFILE_FUNCTION()

	// unload the active scene.
	m_activeScene.reset();

	OWL_TRACE("RunnerLayer: deleted activeScene.")
}

void RunnerLayer::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()
	// resize
	if (m_activeScene != nullptr) {
		{
			OWL_PROFILE_SCOPE("Render Preparation")
			renderer::RenderCommand::setClearColor({0.1f, 0.1f, 0.1f, 1});
			renderer::RenderCommand::clear();
			m_activeScene->onViewportResize(m_viewportSize);
			if (m_activeScene->status == scene::Scene::Status::Editing)
				m_activeScene->onStartRuntime();
			else
				m_activeScene->onUpdateRuntime(iTimeStep);
		}
	} else {
		// check for finish.
		core::Application::get().close();
	}
}

void RunnerLayer::onEvent(event::Event& ioEvent) {
	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::KeyPressedEvent>(
			[this]<typename T0>(T0&& ioPh1) { return onKeyPressed(std::forward<T0>(ioPh1)); });
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[this]<typename T0>(T0&& ioPh1) { return onMouseButtonPressed(std::forward<T0>(ioPh1)); });
}

void RunnerLayer::onImGuiRender(const core::Timestep&) {
	OWL_PROFILE_FUNCTION()
	//=============================================================
	m_viewportSize = core::Application::get().getWindow().getSize();
	//=============================================================
}

auto RunnerLayer::onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool {
	// Shortcuts
	if (static_cast<int>(ioEvent.getRepeatCount()) > 0)
		return false;
	return false;
}

auto RunnerLayer::onMouseButtonPressed([[maybe_unused]] const event::MouseButtonPressedEvent& ioEvent) -> bool {
	// noting yet
	return false;
}

}// namespace owl::nest::runner
