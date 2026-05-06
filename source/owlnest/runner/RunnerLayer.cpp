/**
 * @file RunnerLayer.cpp
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "RunnerLayer.h"

#include <input/Input.h>
#include <input/MouseCode.h>
#include <physic/PhysicCommand.h>
#include <scene/SaveManager.h>
#include <scene/ScreenTransition.h>
#include <scene/SettingsManager.h>
#include <scene/UIInputSystem.h>
#include <scene/component/components.h>

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
	packFile.clear();
	rendererStack = renderer::RendererStackConfig{};
	if (const auto appConfig = data["RunnerConfig"]; appConfig) {
		get(appConfig, "PackFile", packFile);
		get(appConfig, "GameName", gameName);
		get(appConfig, "Version", version);
		get(appConfig, "Author", author);
		get(appConfig, "Icon", icon);
		get(appConfig, "WindowWidth", windowWidth);
		get(appConfig, "WindowHeight", windowHeight);
		get(appConfig, "Fullscreen", fullscreen);
		get(appConfig, "Resizable", resizable);
		std::string sceneName;
		get(appConfig, "FirstScene", sceneName);
		OWL_CORE_INFO("FirstScene: {}", sceneName)
		// Try to resolve to a filesystem path.
		for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
			OWL_CORE_INFO("Checking: {}", (assetsPath / sceneName).string())
			if (exists(assetsPath / sceneName)) {
				firstScene = (assetsPath / sceneName).string();
				break;
			}
		}
		// If not found on disk (e.g. pack-only), keep the raw name for pack lookup.
		if (firstScene.empty())
			firstScene = sceneName;
		// Mirror the project's `RendererStack:` block — runner consumes this on
		// startup to install the same render stack the editor uses.
		if (const auto stack = appConfig["RendererStack"]; stack)
			rendererStack = renderer::RendererStackConfig::fromYaml(stack);
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
	if (!packFile.empty())
		out << YAML::Key << "PackFile" << YAML::Value << packFile;
	if (!gameName.empty())
		out << YAML::Key << "GameName" << YAML::Value << gameName;
	if (!version.empty())
		out << YAML::Key << "Version" << YAML::Value << version;
	if (!author.empty())
		out << YAML::Key << "Author" << YAML::Value << author;
	if (!icon.empty())
		out << YAML::Key << "Icon" << YAML::Value << icon;
	out << YAML::Key << "WindowWidth" << YAML::Value << windowWidth;
	out << YAML::Key << "WindowHeight" << YAML::Value << windowHeight;
	out << YAML::Key << "Fullscreen" << YAML::Value << fullscreen;
	out << YAML::Key << "Resizable" << YAML::Value << resizable;
	if (!rendererStack.isEmpty())
		out << YAML::Key << "RendererStack" << YAML::Value << rendererStack.toYaml();
	out << YAML::EndMap;
	out << YAML::EndMap;
	std::ofstream fileOut(iPath);
	fileOut << out.c_str();
	fileOut.close();
}

RunnerLayer::RunnerLayer() : Layer("RunnerLayer") {}

void RunnerLayer::onAttach() {
	OWL_PROFILE_FUNCTION()

	// Load the config file
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

	// Initialize settings system.
	if (!m_config.gameName.empty()) {
		scene::SettingsManager::setGameName(m_config.gameName);
		scene::SaveManager::setGameName(m_config.gameName);
	}
	// Populate defaults from runner config.
	scene::SettingsManager::setDefault(scene::SettingsManager::KeyResolutionWidth,
									   static_cast<int64_t>(m_config.windowWidth));
	scene::SettingsManager::setDefault(scene::SettingsManager::KeyResolutionHeight,
									   static_cast<int64_t>(m_config.windowHeight));
	scene::SettingsManager::setDefault(scene::SettingsManager::KeyFullscreen, m_config.fullscreen);
	scene::SettingsManager::setDefault(scene::SettingsManager::KeyResizable, m_config.resizable);
	scene::SettingsManager::setDefault(scene::SettingsManager::KeyVolumeMaster, 1.0f);
	scene::SettingsManager::setDefault(scene::SettingsManager::KeyVolumeMusic, 1.0f);
	scene::SettingsManager::setDefault(scene::SettingsManager::KeyVolumeSfx, 1.0f);
	// Load game defaults from assets (game_settings.yml).
	{
		bool loaded = false;
		for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
			if (const auto gameSettingsPath = assetsPath / "game_settings.yml"; exists(gameSettingsPath)) {
				scene::SettingsManager::loadDefaults(gameSettingsPath);
				loaded = true;
				break;
			}
		}
		// Fallback: try loading from pack.
		if (!loaded && app.hasOpenPack()) {
			if (auto data = app.loadFromPack("game_settings.yml"); data) {
				const std::string content(data->begin(), data->end());
				scene::SettingsManager::loadDefaultsFromString(content);
			}
		}
	}
	// Load user overrides.
	scene::SettingsManager::loadUserSettings();

	// Apply window settings (from settings, with runner config as fallback).
	auto& window = app.getWindow();
	if (!m_config.gameName.empty())
		window.setTitle(m_config.gameName);
	scene::SettingsManager::applyBuiltins();

	m_viewportSize = window.getSize();
	m_activeScene = mkShared<scene::Scene>();

	bool loaded = false;
	// Try loading first scene from pack, then from filesystem.
	if (app.hasOpenPack()) {
		// Resolve first scene name for pack lookup.
		std::string sceneName = m_config.firstScene;
		// If firstScene was resolved to an absolute path, try to find it in pack by filename.
		if (std::filesystem::path(sceneName).is_absolute()) {
			for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
				if (sceneName.starts_with(assetsPath.string())) {
					sceneName = relative(std::filesystem::path(sceneName), assetsPath).string();
					break;
				}
			}
		}
		if (auto data = app.loadFromPack(sceneName); data) {
			if (const scene::SceneSerializer sc(m_activeScene); !sc.deserializeFromBuffer(*data, sceneName)) {
				OWL_CORE_ERROR("Failed to load first scene from pack")
				app.close();
				return;
			}
			loaded = true;
		}
	}

	// Fallback: load from filesystem if pack didn't have the scene.
	if (!loaded) {
		if (!std::filesystem::exists(m_config.firstScene)) {
			OWL_CORE_ERROR("Runner first scene {} not found", m_config.firstScene)
			app.close();
			return;
		}
		if (const scene::SceneSerializer sc(m_activeScene); !sc.deserialize(m_config.firstScene)) {
			OWL_CORE_ERROR("Failed to load first scene")
			app.close();
			return;
		}
	}

	// Install the project's renderer stack filtered by the freshly-loaded
	// scene's `EnabledRenderers`. Without this, packaged games would fall
	// back to a single implicit `Renderer2D` and any scene relying on a
	// custom stack (raycaster, screen-overlay UI, …) would render wrong.
	installRenderStack();
}

void RunnerLayer::installRenderStack() {
	if (!m_activeScene)
		return;
	const auto& stackCfg = m_config.rendererStack.isEmpty() ? renderer::RendererStackConfig::makeDefault()
															: m_config.rendererStack;
	auto stack = renderer::RenderStack::buildFromConfig(stackCfg, m_activeScene->getEnabledRenderers());
	renderer::Renderer::setRenderStack(std::move(stack));
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
			renderer::gpu::RenderCommand::setClearColor({0.1f, 0.1f, 0.1f, 1});
			renderer::gpu::RenderCommand::clear();
			m_activeScene->onViewportResize(m_viewportSize);
			if (m_activeScene->status == scene::Scene::Status::Editing) {
				m_activeScene->onStartRuntime();
				if (m_pendingTeleportVelocity) {
					// Apply stored velocity and position after physics init on new scene.
					m_pendingTeleportVelocity = false;
					if (const scene::Entity player = m_activeScene->getPrimaryPlayer()) {
						// Find target entity and position player there.
						for (const auto view = m_activeScene->registry
													   .view<scene::component::Tag, scene::component::Transform>();
							 const auto ent: view) {
							if (view.get<scene::component::Tag>(ent).tag == m_teleportTargetName) {
								const auto& targetTransform =
										view.get<scene::component::Transform>(ent).transform;
								const float targetRotation = targetTransform.rotation().z();
								// Rotate the stored velocity by the target rotation.
								const float cosR = std::cos(targetRotation);
								const float sinR = std::sin(targetRotation);
								const math::vec2f finalVelocity = {
										m_teleportVelocity.x() * cosR - m_teleportVelocity.y() * sinR,
										m_teleportVelocity.x() * sinR + m_teleportVelocity.y() * cosR};
								physic::PhysicCommand::setTransform(player,
																	{targetTransform.translation().x(),
																	 targetTransform.translation().y()},
																	targetRotation);
								physic::PhysicCommand::setVelocity(player, finalVelocity);
								auto& playerTransform =
										player.getComponent<scene::component::Transform>().transform;
								playerTransform.translation().x() = targetTransform.translation().x();
								playerTransform.translation().y() = targetTransform.translation().y();
								playerTransform.rotation().z() = targetRotation;
								break;
							}
						}
					}
				}
			} else {
				// While an async transition is loading, skip input + runtime updates.
				// The old scene has already ended; just render the last frame and wait.
				if (m_transition) {
					// The old scene's `onUpdateRuntime` no longer ticks — pump the
					// transition orchestrator manually so the loading screen
					// animates while the worker thread reads the new scene file.
					scene::ScreenTransition::update(iTimeStep.getSeconds());
					scene::ScreenTransition::render(static_cast<float>(m_viewportSize.x()),
													static_cast<float>(m_viewportSize.y()));
					handleTeleportRequest();
				} else {
					// UIRect uses Y=0 at bottom; window mouse Y=0 at top → flip Y.
					const math::vec2 mousePos = {
							input::Input::getMousePos().x(),
							static_cast<float>(m_viewportSize.y()) - input::Input::getMousePos().y()};
					scene::UIInputSystem::update(m_activeScene.get(), m_viewportSize, mousePos,
												 input::Input::isMouseButtonPressed(input::mouse::ButtonLeft));
					m_activeScene->onUpdateRuntime(iTimeStep);
					// Handle quit request from Lua (scene.quit()).
					if (m_activeScene->quitRequested) {
						m_activeScene->onEndRuntime();
						core::Application::get().close();
						return;
					}
					// Pump scene-load orchestrator: when `ScreenTransition` reports a
					// pending path (out-anim done), inject it as a teleport request
					// so the existing async loader picks it up.
					if (auto pending = scene::ScreenTransition::pendingLoadPath(); pending) {
						m_activeScene->teleportRequest.pending = true;
						m_activeScene->teleportRequest.levelName = *pending;
						m_activeScene->teleportRequest.targetName.clear();
						m_activeScene->teleportRequest.initialVelocity = {0.f, 0.f};
						m_activeScene->teleportRequest.rotationDelta = 0.f;
					}
					handleTeleportRequest();
					if (m_activeScene && m_activeScene->saveLoadRequest.pending) {
						const auto slr = m_activeScene->saveLoadRequest;
						m_activeScene->saveLoadRequest.pending = false;
						if (slr.isLoad) {
							auto newScene = owl::mkShared<scene::Scene>();
							if (auto loadResult = scene::SaveManager::load(slr.slot, newScene);
								loadResult.success) {
								m_activeScene->onEndRuntime();
								m_activeScene = newScene;
								m_activeScene->onViewportResize(m_viewportSize);
								m_activeScene->onStartRuntime();
								for (const auto& [uuid, snap]: loadResult.physicsSnapshots)
									if (auto entity = m_activeScene->findEntityByUUID(core::UUID{uuid}); entity)
										physic::PhysicCommand::applySnapshot(entity, snap);
							}
						} else {
							std::ignore = scene::SaveManager::save(slr.slot, m_activeScene, "");
						}
					}
				}
			}
		}
	} else {
		// check for finish.
		core::Application::get().close();
	}
}

void RunnerLayer::handleTeleportRequest() {
	// If a transition is already in flight, wait for it to complete.
	if (m_transition) {
		if (m_transition->ready.load())
			finishTransition();
		return;
	}

	if (!m_activeScene->teleportRequest.pending)
		return;
	// Copy request and reset.
	const auto request = m_activeScene->teleportRequest;
	m_activeScene->teleportRequest.pending = false;

	// Ensure the level name has an .owl extension.
	std::string resolvedName = request.levelName;
	if (std::filesystem::path(resolvedName).extension() != ".owl")
		resolvedName += ".owl";

	const auto& app = core::Application::get();

	// Build the transition state now, capturing the current GameState and velocity.
	auto transition = mkShared<PendingTransition>();
	transition->previousGameState = m_activeScene->getGameState();
	transition->velocity = request.initialVelocity;
	transition->targetName = request.targetName;
	transition->data = mkShared<std::vector<uint8_t>>();
	transition->sourceName = resolvedName;

	// End the old runtime now — we stop simulating the old scene while loading.
	m_activeScene->onEndRuntime();

	// Capture variables needed by the worker.
	const bool hasPack = app.hasOpenPack();
	std::vector<std::pair<std::string, std::filesystem::path>> searchRoots;
	for (const auto& [title, assetsPath]: app.getAssetDirectories())
		searchRoots.emplace_back(title, assetsPath);

	m_transition = transition;

	core::Application::get().getTaskScheduler().pushTask(core::task::Task(
			// Worker: read scene bytes from pack or filesystem.
			[transition, hasPack, searchRoots, resolvedName, &app]() -> void {
				if (hasPack) {
					for (const auto& tryName: {resolvedName, "scenes/" + resolvedName}) {
						if (auto data = app.loadFromPack(tryName); data) {
							*transition->data = std::move(*data);
							transition->sourceName = tryName;
							return;
						}
					}
				}
				for (const auto& [title, assetsPath]: searchRoots) {
					std::filesystem::path levelPath;
					if (exists(assetsPath / resolvedName))
						levelPath = assetsPath / resolvedName;
					else if (exists(assetsPath / "scenes" / resolvedName))
						levelPath = assetsPath / "scenes" / resolvedName;
					if (levelPath.empty())
						continue;
					std::ifstream file(levelPath, std::ios::binary | std::ios::ate);
					if (!file.is_open())
						continue;
					const auto size = static_cast<size_t>(file.tellg());
					file.seekg(0);
					transition->data->resize(size);
					file.read(reinterpret_cast<char*>(transition->data->data()),
							  static_cast<std::streamsize>(size));
					transition->sourceName = levelPath.string();
					return;
				}
				transition->failed.store(true);
			},
			// Termination (main thread): mark ready so the next frame can deserialize + swap.
			[transition]() -> void { transition->ready.store(true); }));
}

void RunnerLayer::finishTransition() {
	if (!m_transition)
		return;
	const auto transition = m_transition;
	m_transition.reset();

	if (transition->failed.load()) {
		OWL_CORE_ERROR("Teleport: level '{}' not found", transition->sourceName)
		return;
	}

	auto newScene = mkShared<scene::Scene>();
	if (const scene::SceneSerializer sc(newScene);
		!sc.deserializeFromBuffer(*transition->data, transition->sourceName)) {
		OWL_CORE_ERROR("Teleport: failed to deserialize level '{}'", transition->sourceName)
		return;
	}
	newScene->getGameState() = transition->previousGameState;
	newScene->onViewportResize(m_viewportSize);

	m_pendingTeleportVelocity = true;
	m_teleportVelocity = transition->velocity;
	m_teleportTargetName = transition->targetName;

	m_activeScene = newScene;

	// Rebuild the renderer stack against the new scene's `EnabledRenderers` —
	// without this the runner inherits the previous scene's stack and any
	// per-scene override (raycaster enabled on raycast_demo, world disabled,
	// …) silently stops applying.
	installRenderStack();

	// Diagnostic: how many textures still need to finish decoding after the swap?
	// With async texture loading, the placeholder is on screen immediately and real pixels arrive
	// over the next frames as scheduler tasks complete. A high number on a small scene may indicate
	// either a slow disk/pack or that async tasks are not draining.
	{
		size_t pending = 0;
		const auto countOne = [&pending](const auto& iTex) -> void {
			if (iTex && iTex->getLoadState() == renderer::gpu::LoadState::Pending)
				++pending;
		};
		m_activeScene->registry.view<scene::component::SpriteRenderer>().each(
				[&](const auto&, const scene::component::SpriteRenderer& iSr) -> void { countOne(iSr.texture); });
		m_activeScene->registry.view<scene::component::AnimatedSpriteRenderer>().each(
				[&](const auto&, const scene::component::AnimatedSpriteRenderer& iAr) -> void { countOne(iAr.texture); });
		m_activeScene->registry.view<scene::component::BackgroundTexture>().each(
				[&](const auto&, const scene::component::BackgroundTexture& iBt) -> void { countOne(iBt.texture); });
		m_activeScene->registry.view<scene::component::UIImage>().each(
				[&](const auto&, const scene::component::UIImage& iUi) -> void { countOne(iUi.texture); });
		OWL_CORE_TRACE("Teleport finished: {} texture(s) still decoding on workers", pending)
	}
}

void RunnerLayer::onEvent(event::Event& ioEvent) {
	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::KeyPressedEvent>(
			[]<typename T0>(T0&& ioPh1) -> bool { return onKeyPressed(std::forward<T0>(ioPh1)); });
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[]<typename T0>(T0&& ioPh1) -> bool { return onMouseButtonPressed(std::forward<T0>(ioPh1)); });
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
