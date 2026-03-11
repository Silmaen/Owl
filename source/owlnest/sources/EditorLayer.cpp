/**
 * @file EditorLayer.cpp
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"

#include <gui/IconBank.h>
#include <gui/utils.h>
#include <io/pack/AssetScanner.h>
#include <io/pack/PackWriter.h>
#include <physic/PhysicCommand.h>
#include <scene/component/components.h>
#include <sound/SoundCommand.h>
#include <sound/SoundSystem.h>


namespace owl::nest {
namespace {

void buildIconBank() {
	auto& iconBank = gui::IconBank::instance();

	// Resolve icon file paths from asset directories
	auto& textureLibrary = renderer::Renderer::getTextureLibrary();
	const auto resolve = [&](const std::string& iName) -> std::filesystem::path {
		if (const auto path = textureLibrary.find(iName); path.has_value())
			return path.value();
		return {};
	};

	// clang-format off
	std::vector<std::pair<std::string, std::filesystem::path>> icons = {
		// Control icons
		{"ctrl_rotation",     resolve("ctrl_rotation")},
		{"ctrl_scale",        resolve("ctrl_scale")},
		{"ctrl_translation",  resolve("ctrl_translation")},
		// Playback icons
		{"PlayButton",        resolve("icons/PlayButton")},
		{"PauseButton",       resolve("icons/PauseButton")},
		{"StopButton",        resolve("icons/StopButton")},
		{"StepButton",        resolve("icons/StepButton")},
		// Visibility icons
		{"camera_on",         resolve("icons/visibility/camera_on")},
		{"camera_off",        resolve("icons/visibility/camera_off")},
		{"eye_open",          resolve("icons/visibility/eye_open")},
		{"eye_closed",        resolve("icons/visibility/eye_closed")},
		// Trigger icons
		{"trigger_victory",   resolve("icons/triggers/trigger_victory")},
		{"trigger_death",     resolve("icons/triggers/trigger_death")},
		{"trigger_target",    resolve("icons/triggers/trigger_target")},
		{"trigger_teleport",  resolve("icons/triggers/trigger_teleport")},
		// File browser icons
		{"folder_icon",       resolve("icons/files/folder_icon")},
		{"glsl_icon",         resolve("icons/files/glsl_icon")},
		{"jpg_icon",          resolve("icons/files/jpg_icon")},
		{"json_icon",         resolve("icons/files/json_icon")},
		{"owl_icon",          resolve("icons/files/owl_icon")},
		{"png_icon",          resolve("icons/files/png_icon")},
		{"svg_icon",          resolve("icons/files/svg_icon")},
		{"text_icon",         resolve("icons/files/text_icon")},
		{"ttf_icon",          resolve("icons/files/ttf_icon")},
		{"yml_icon",          resolve("icons/files/yml_icon")},
		// Action icons (context menus, toolbar, etc.)
		{"delete",            resolve("icons/actions/delete")},
		{"rename",            resolve("icons/actions/rename")},
		{"new_folder",        resolve("icons/actions/new_folder")},
		{"import_file",       resolve("icons/actions/import_file")},
		{"import_folder",     resolve("icons/actions/import_folder")},
		{"add_entity",        resolve("icons/actions/add_entity")},
		{"delete_entity",     resolve("icons/actions/delete_entity")},
		{"save",              resolve("icons/actions/save")},
		{"open",              resolve("icons/actions/open")},
		{"new_scene",         resolve("icons/actions/new_scene")},
		{"duplicate",         resolve("icons/actions/duplicate")},
		{"undo",              resolve("icons/actions/undo")},
		{"redo",              resolve("icons/actions/redo")},
		{"component_camera",  resolve("icons/actions/component_camera")},
		{"component_sprite",  resolve("icons/actions/component_sprite")},
		{"component_physics", resolve("icons/actions/component_physics")},
		{"component_script",  resolve("icons/actions/component_script")},
		{"component_sound",   resolve("icons/actions/component_sound")},
		{"settings",          resolve("icons/actions/settings")},
		{"search",            resolve("icons/actions/search")},
		{"pack",              resolve("icons/actions/pack")},
		// UI / navigation icons
		{"back",              resolve("icons/actions/back")},
		{"scene_hierarchy",   resolve("icons/actions/scene_hierarchy")},
		{"content_browser",   resolve("icons/actions/content_browser")},
		{"stats",             resolve("icons/actions/stats")},
		{"properties",        resolve("icons/actions/properties")},
		{"log",               resolve("icons/actions/log")},
		{"viewport",          resolve("icons/actions/viewport")},
		{"close",             resolve("icons/actions/close")},
		{"project",           resolve("icons/actions/project")},
		{"exit",              resolve("icons/actions/exit")},
	};
	// clang-format on

	// Remove entries with empty paths
	std::erase_if(icons, [](const auto& iEntry) { return iEntry.second.empty(); });

	iconBank.build(icons, 128);
}
void loadTriggerTextures() {
	// Trigger icons are also used for Renderer2D viewport drawing (not just ImGui),
	// so they need to remain in the texture library as individual textures.
	auto& textureLibrary = renderer::Renderer::getTextureLibrary();
	textureLibrary.load("icons/triggers/trigger_victory");
	textureLibrary.load("icons/triggers/trigger_death");
	textureLibrary.load("icons/triggers/trigger_target");
	textureLibrary.load("icons/triggers/trigger_teleport");
}
void loadSounds() {
	auto& soundLibrary = sound::SoundSystem::getSoundLibrary();
	soundLibrary.load("clic.wav");
}

}// namespace
EditorLayer::EditorLayer() : Layer("EditorLayer"), m_cameraController{1280.0f / 720.0f} {}

void EditorLayer::onAttach() {
	OWL_PROFILE_FUNCTION()

	core::Application::get().enableDocking();

	if (const auto f = core::Application::get().getWorkingDirectory() / "OwlNest_settings.yml"; exists(f))
		m_settings.loadFromFile(f);

	// Apply theme from saved settings
	if (m_settings.themePreset != "Custom") {
		for (const auto& [preset, name]: gui::Theme::getPresetNames()) {
			if (name == m_settings.themePreset) {
				gui::UiLayer::setTheme(gui::Theme::fromPreset(preset));
				break;
			}
		}
	}

	m_viewport.attach();
	m_viewport.attachParent(this);

	buildIconBank();
	loadTriggerTextures();
	loadSounds();

	m_controlBar.init(gui::widgets::ButtonBarData{{.id = "##controlBar", .visible = true}, false, false, true});
	m_controlBar.addButton({{.id = "##ctrlTranslation", .visible = true},
							"ctrl_translation",
							"T",
							[this] { return m_viewport.getGuizmoType() == gui::Guizmo::Type::Translation; },
							[this] {
								if (m_viewport.getGuizmoType() == gui::Guizmo::Type::Translation)
									m_viewport.setGuizmoType(gui::Guizmo::Type::None);
								else
									m_viewport.setGuizmoType(gui::Guizmo::Type::Translation);
							},
							{32, 32},
							"Translation (W)"});
	m_controlBar.addButton({{.id = "##ctrlRotation", .visible = true},
							"ctrl_rotation",
							"T",
							[this] { return m_viewport.getGuizmoType() == gui::Guizmo::Type::Rotation; },
							[this] {
								if (m_viewport.getGuizmoType() == gui::Guizmo::Type::Rotation)
									m_viewport.setGuizmoType(gui::Guizmo::Type::None);
								else
									m_viewport.setGuizmoType(gui::Guizmo::Type::Rotation);
							},
							{32, 32},
							"Rotation (E)"});
	m_controlBar.addButton({{.id = "##ctrlScale", .visible = true},
							"ctrl_scale",
							"T",
							[this] { return m_viewport.getGuizmoType() == gui::Guizmo::Type::Scale; },
							[this] {
								if (m_viewport.getGuizmoType() == gui::Guizmo::Type::Scale)
									m_viewport.setGuizmoType(gui::Guizmo::Type::None);
								else
									m_viewport.setGuizmoType(gui::Guizmo::Type::Scale);
							},
							{32, 32},
							"Scale (R)"});

	m_contentBrowser.attach();
	newScene();
}

void EditorLayer::onDetach() {
	OWL_PROFILE_FUNCTION()

	m_settings.saveToFile(core::Application::get().getWorkingDirectory() / "OwlNest_settings.yml");

	m_viewport.detach();
	OWL_TRACE("EditorLayer: viewport freed.")
	m_contentBrowser.detach();
	OWL_TRACE("EditorLayer: deleted editor FrameBuffer.")

	m_controlBar.clearButtons();

	m_activeScene.reset();
	OWL_TRACE("EditorLayer: deleted activeScene.")
}

void EditorLayer::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	// resize
	m_cameraController.onResize(m_viewport.getSize());
	m_activeScene->onViewportResize(m_viewport.getSize());

	// Update scene
	if (m_state == State::Edit) {
		if (m_viewport.isFocused())
			m_cameraController.onUpdate(iTimeStep);
	}

	// After a cross-level teleport, the new scene is in Editing state.
	// Start its runtime and apply the pending velocity.
	if ((m_state == State::Play || m_state == State::Pause) &&
		m_activeScene->status == scene::Scene::Status::Editing) {
		m_activeScene->onStartRuntime();
		if (m_pendingTeleportVelocity) {
			m_pendingTeleportVelocity = false;
			if (const scene::Entity player = m_activeScene->getPrimaryPlayer()) {
				for (const auto view =
							 m_activeScene->registry.view<scene::component::Tag, scene::component::Transform>();
					 const auto ent: view) {
					if (view.get<scene::component::Tag>(ent).tag == m_teleportTargetName) {
						const auto& targetTransform = view.get<scene::component::Transform>(ent).transform;
						const float targetRotation = targetTransform.rotation().z();
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
						auto& playerTransform = player.getComponent<scene::component::Transform>().transform;
						playerTransform.translation().x() = targetTransform.translation().x();
						playerTransform.translation().y() = targetTransform.translation().y();
						playerTransform.rotation().z() = targetRotation;
						break;
					}
				}
			}
		}
	}

	// update the viewport
	m_viewport.onUpdate(iTimeStep);
}

void EditorLayer::onEvent(event::Event& ioEvent) {
	m_cameraController.onEvent(ioEvent);
	m_viewport.onEvent(ioEvent);

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::KeyPressedEvent>(
			[this]<typename T0>(T0&& ioPh1) { return onKeyPressed(std::forward<T0>(ioPh1)); });
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[]<typename T0>(T0&& ioPh1) { return onMouseButtonPressed(std::forward<T0>(ioPh1)); });
	dispatcher.dispatch<event::FileDropEvent>([this](const event::FileDropEvent& iEvent) {
		m_contentBrowser.handleFileDrop(iEvent.getPaths());
		return true;
	});
}

void EditorLayer::onImGuiRender(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	// ==================================================================
	renderStats(iTimeStep);
	//=============================================================
	renderMenu();
	//=============================================================
	m_sceneHierarchy.onImGuiRender();
	m_contentBrowser.onImGuiRender();
	m_viewport.onRender();
	m_parameters.onImGuiRender();
	m_projectSettings.onImGuiRender();
	if (m_projectSettings.hasResult()) {
		m_project = m_projectSettings.consumeResult();
		saveProject();
		updateWindowTitle();
	}
	m_logPanel.onImGuiRender();
	//=============================================================
	{
		const auto& lower = m_viewport.getLowerBound();
		const auto& upper = m_viewport.getUpperBound();
		const float centerX = (lower.x() + upper.x()) * 0.5f;
		ImGui::SetNextWindowPos({centerX, lower.y() + 8.0f}, ImGuiCond_Always, {0.5f, 0.0f});
	}
	renderToolbar();
	if (m_state == State::Edit) {
		const auto& upper = m_viewport.getUpperBound();
		const auto& lower = m_viewport.getLowerBound();
		ImGui::SetNextWindowPos({upper.x() - 8.0f, lower.y() + 8.0f}, ImGuiCond_Always, {1.0f, 0.0f});
		m_controlBar.onRender();
	}
}

void EditorLayer::renderStats(const core::Timestep& iTimeStep) {
	if (!m_settings.showStats)
		return;
	ImGui::Begin("Stats");
	ImGui::Text("%s", std::format("FPS: {:.2f}", iTimeStep.getFps()).c_str());
	ImGui::Separator();
	ImGui::Text("%s",
				std::format("Current used memory: {}", core::utils::sizeToString(debug::TrackerAPI::globals().allocatedMemory)).c_str());
	ImGui::Text("%s", std::format("Max used memory: {}", core::utils::sizeToString(debug::TrackerAPI::globals().memoryPeek)).c_str());
	ImGui::Text("%s", std::format("Allocation calls: {}", debug::TrackerAPI::globals().allocationCalls).c_str());
	ImGui::Text("%s", std::format("Deallocation calls: {}", debug::TrackerAPI::globals().deallocationCalls).c_str());
	ImGui::Text("%s",
				std::format("Frame allocation: {}", debug::TrackerAPI::globals().allocationCalls - m_lastAllocCalls)
						.c_str());
	ImGui::Text("%s", std::format("Frame deallocation: {}",
								  debug::TrackerAPI::globals().deallocationCalls - m_lastDeallocCalls)
							  .c_str());
	m_lastAllocCalls = debug::TrackerAPI::globals().allocationCalls;
	m_lastDeallocCalls = debug::TrackerAPI::globals().deallocationCalls;
	ImGui::Separator();
	std::string name = "None";
	if (const auto ent = m_viewport.getHoveredEntity()) {
		if (ent.hasComponent<scene::component::Tag>())
			name = ent.getComponent<scene::component::Tag>().tag;
	}
	ImGui::Text("Hovered Entity: %s", name.c_str());
	ImGui::Separator();
	const auto stats = renderer::Renderer2D::getStats();
	ImGui::Text("Renderer2D Stats:");
	ImGui::Text("Draw Calls: %ud", stats.drawCalls);
	ImGui::Text("Quads: %ud", stats.quadCount);
	ImGui::Text("Vertices: %ud", stats.getTotalVertexCount());
	ImGui::Text("Indices: %ud", stats.getTotalIndexCount());
	ImGui::Text("Viewport size: %f %f", static_cast<double>(m_viewport.getSize().x()),
				static_cast<double>(m_viewport.getSize().y()));
	ImGui::Text("Aspect ratio: %f", static_cast<double>(m_viewport.getSize().ratio()));
	ImGui::End();
}

void EditorLayer::renderThemeMenu() {
	if (!ImGui::BeginMenu("Theme"))
		return;
	for (const auto& [preset, name]: gui::Theme::getPresetNames()) {
		const bool selected = m_settings.themePreset == name;
		if (ImGui::MenuItem(name.c_str(), nullptr, selected)) {
			m_settings.themePreset = name;
			gui::UiLayer::setTheme(gui::Theme::fromPreset(preset));
		}
	}
	ImGui::Separator();
	const bool selected = m_settings.themePreset == "Custom";
	if (ImGui::MenuItem("Custom (theme.yml)", nullptr, selected)) {
		m_settings.themePreset = "Custom";
		if (const auto themePath = core::Application::get().getWorkingDirectory() / "theme.yml"; exists(themePath)) {
			gui::Theme theme;
			theme.loadFromFile(themePath);
			gui::UiLayer::setTheme(theme);
		}
	}
	ImGui::EndMenu();
}

void EditorLayer::renderMenu() {
	const auto& iconBank = gui::IconBank::instance();

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (iconBank.menuItem("new_folder", "New Project"))
				newProject();
			if (iconBank.menuItem("open", "Open Project"))
				openProject();
			if (iconBank.menuItem("save", "Save Project", nullptr, m_project.isLoaded()))
				saveProject();
			if (iconBank.menuItem("close", "Close Project", nullptr, m_project.isLoaded()))
				closeProject();
			ImGui::Separator();
			if (iconBank.menuItem("new_scene", "New", "Ctrl+N"))
				newScene();
			ImGui::Separator();
			if (iconBank.menuItem("open", "Open Scene", "Ctrl+O"))
				openScene();
			if (iconBank.menuItem("save", "Save Scene", "Ctrl+S"))
				if (!m_currentScenePath.empty())
					saveCurrentScene();
			if (iconBank.menuItem("save", "Save Scene as..", "Ctrl+Shift+S"))
				saveSceneAs();
			ImGui::Separator();
			if (iconBank.menuItem("exit", "Exit"))
				core::Application::get().close();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Project", m_project.isLoaded())) {
			if (iconBank.menuItem("import_file", "Import Scene"))
				importScene();
			ImGui::Separator();
			if (iconBank.menuItem("pack", "Pack Scene"))
				if (!m_currentScenePath.empty())
					packScene();
			if (iconBank.menuItem("pack", "Pack Game"))
				packGame();
			ImGui::Separator();
			if (iconBank.menuItem("settings", "Project Settings"))
				m_projectSettings.open(m_project);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Settings")) {
			if (iconBank.menuItem("stats", "Show Stats")) {
				m_settings.showStats = !m_settings.showStats;
			}
			if (iconBank.menuItem("settings", "Parameters"))
				m_parameters.open();
			ImGui::Separator();
			renderThemeMenu();
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void EditorLayer::renderToolbar() {
	constexpr float buttonImageSize = 32.0f;
	int buttonCount = 2;
	if (m_state == State::Edit)
		buttonCount = 1;
	else if (m_state == State::Pause)
		buttonCount = 3;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, gui::vec(math::vec2{0.f, 2.f}));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, gui::vec(math::vec2{0.f, 0.f}));

	// Compute toolbar window size to exactly fit the buttons
	const auto& style = ImGui::GetStyle();
	const float buttonWidgetWidth = buttonImageSize + style.FramePadding.x * 2.0f;
	const float buttonWidgetHeight = buttonImageSize + style.FramePadding.y * 2.0f;
	const float toolbarWidth = static_cast<float>(buttonCount) * buttonWidgetWidth +
							   static_cast<float>(buttonCount - 1) * style.ItemSpacing.x;
	const float toolbarHeight = buttonWidgetHeight + 4.0f;// 4 = 2 * windowPaddingY
	ImGui::SetNextWindowSize({toolbarWidth, toolbarHeight});

	constexpr math::vec4 transparent{0.f, 0.f, 0.f, 0.f};
	ImGui::PushStyleColor(ImGuiCol_Button, gui::vec(transparent));
	const auto& colors = style.Colors;
	const auto& buttonHovered = colors[ImGuiCol_ButtonHovered];
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
						  gui::vec(math::vec4{buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f}));
	const auto& buttonActive = colors[ImGuiCol_ButtonActive];
	ImGui::PushStyleColor(ImGuiCol_ButtonActive,
						  gui::vec(math::vec4{buttonActive.x, buttonActive.y, buttonActive.z, 0.5f}));

	ImGui::Begin("##toolbar", nullptr,
				 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	auto& iconBank = gui::IconBank::instance();

	// Helper to render an icon button with fallback
	const auto iconButton = [&](const char* iId, const char* iIconName, const char* iFallback,
								const float iSize) -> bool {
		const auto sizeVec = gui::vec(math::vec2{iSize, iSize});
		if (const auto info = iconBank.getIcon(iIconName); info.has_value())
			return ImGui::ImageButton(iId, static_cast<ImTextureID>(info->textureId), sizeVec, gui::vec(info->uv0),
									  gui::vec(info->uv1));
		return ImGui::Button(iFallback, sizeVec);
	};

	if (m_state == State::Edit) {
		// Edit mode: single Play button centered
		if (iconButton("btn_play", "PlayButton", "play", buttonImageSize))
			onScenePlay();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Play");
	} else {
		// Play or Pause mode: two buttons (Pause/Resume + Stop)
		const auto* const pauseResumeIcon = m_state == State::Play ? "PauseButton" : "PlayButton";
		if (iconButton("btn_pause_resume", pauseResumeIcon, m_state == State::Play ? "pause" : "resume",
					   buttonImageSize)) {
			if (m_state == State::Play)
				onScenePause();
			else
				onSceneResume();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(m_state == State::Play ? "Pause" : "Resume");

		ImGui::SameLine();

		if (iconButton("btn_stop", "StopButton", "stop", buttonImageSize))
			onSceneStop();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Stop");

		if (m_state == State::Pause) {
			ImGui::SameLine();
			if (iconButton("btn_step", "StepButton", "step", buttonImageSize))
				onSceneStep();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Step");
		}
	}
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
	ImGui::End();
}
OWL_DIAG_POP

void EditorLayer::newScene() {
	m_activeScene = mkShared<scene::Scene>();
	m_activeScene->onViewportResize(m_viewport.getSize());
	m_sceneHierarchy.setContext(m_activeScene);
}

void EditorLayer::openScene() {
	if (const auto filepath = core::utils::FileDialog::openFile("Owl Scene (*.owl)|owl\n"); !filepath.empty())
		openScene(filepath);
}

void EditorLayer::openScene(const std::filesystem::path& iScenePath) {
	if (iScenePath.extension().string() != ".owl") {
		OWL_CORE_WARN("Cannot Open file {}: not a scene", iScenePath.string())
		return;
	}
	if (m_state != State::Edit)
		onSceneStop();
	const auto newScene = mkShared<scene::Scene>();

	if (const scene::SceneSerializer serializer(newScene); serializer.deserialize(iScenePath)) {
		m_editorScene = newScene;
		m_editorScene->onViewportResize(m_viewport.getSize());
		m_sceneHierarchy.setContext(m_editorScene);
		m_activeScene = m_editorScene;
		m_currentScenePath = iScenePath;
	}
}

void EditorLayer::saveSceneAs() {
	if (const auto filepath = core::utils::FileDialog::saveFile("Owl Scene (*.owl)|owl\n"); !filepath.empty())
		saveSceneAs(filepath);
}

void EditorLayer::saveSceneAs(const std::filesystem::path& iScenePath) {
	const scene::SceneSerializer serializer(m_activeScene);
	serializer.serialize(iScenePath);
	m_currentScenePath = iScenePath;
}

void EditorLayer::saveCurrentScene() {
	if (m_currentScenePath.empty())
		saveSceneAs();
	else
		saveSceneAs(m_currentScenePath);
}

auto EditorLayer::onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool {
	// Shortcuts
	if (static_cast<int>(ioEvent.getRepeatCount()) > 0)
		return false;
	if (m_state == State::Edit) {
		const bool control = input::Input::isKeyPressed(input::key::LeftControl) ||
							 input::Input::isKeyPressed(input::key::RightControl);
		const bool shift =
				input::Input::isKeyPressed(input::key::LeftShift) || input::Input::isKeyPressed(input::key::RightShift);
		switch (ioEvent.getKeyCode()) {
			case input::key::N:
				{
					if (control) {
						newScene();
						return true;
					}
					break;
				}
			case input::key::O:
				{
					if (control) {
						openScene();
						return true;
					}
					break;
				}
			case input::key::S:
				{
					if (control) {
						if (shift) {
							saveSceneAs();
							return true;
						}
						saveCurrentScene();
						return true;
					}
					break;
				}
			// Scene Commands
			case input::key::D:
				{
					if (control) {
						onDuplicateEntity();
						return true;
					}
					break;
				}
			default:
				break;
		}
	}

	return false;
}

auto EditorLayer::onMouseButtonPressed([[maybe_unused]] const event::MouseButtonPressedEvent& ioEvent) -> bool {
	// noting yet
	return false;
}

void EditorLayer::onScenePlay() {
	if (m_editorScene) {
		auto& soundLibrary = sound::SoundSystem::getSoundLibrary();
		sound::SoundCommand::playSound(soundLibrary.get("clic.wav"));

		m_state = State::Play;
		m_activeScene = scene::Scene::copy(m_editorScene);
		m_activeScene->onStartRuntime();

		m_sceneHierarchy.setContext(m_activeScene);
	}
}

void EditorLayer::onScenePause() { m_state = State::Pause; }

void EditorLayer::onSceneResume() { m_state = State::Play; }

void EditorLayer::onSceneStop() {
	m_state = State::Edit;
	m_pendingTeleportVelocity = false;

	m_activeScene->onEndRuntime();
	m_activeScene = m_editorScene;

	m_sceneHierarchy.setContext(m_activeScene);
}

void EditorLayer::onSceneStep() { m_stepRequested = true; }

auto EditorLayer::consumeStepRequest() -> bool {
	if (m_stepRequested) {
		m_stepRequested = false;
		return true;
	}
	return false;
}

void EditorLayer::handleTeleportRequest() {
	if (!m_activeScene->teleportRequest.pending)
		return;
	const auto request = m_activeScene->teleportRequest;
	m_activeScene->teleportRequest.pending = false;

	// Ensure the level name has an .owl extension.
	std::string resolvedName = request.levelName;
	if (std::filesystem::path(resolvedName).extension() != ".owl")
		resolvedName += ".owl";

	const auto& app = core::Application::get();
	std::filesystem::path levelPath;
	for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
		if (exists(assetsPath / resolvedName)) {
			levelPath = assetsPath / resolvedName;
			break;
		}
		if (exists(assetsPath / "scenes" / resolvedName)) {
			levelPath = assetsPath / "scenes" / resolvedName;
			break;
		}
	}
	if (levelPath.empty()) {
		OWL_CORE_ERROR("Teleport: level '{}' not found", resolvedName)
		return;
	}

	m_activeScene->onEndRuntime();

	auto newScene = mkShared<scene::Scene>();
	if (const scene::SceneSerializer sc(newScene); !sc.deserialize(levelPath)) {
		OWL_CORE_ERROR("Teleport: failed to load level '{}'", request.levelName)
		return;
	}
	newScene->onViewportResize(m_viewport.getSize());

	m_pendingTeleportVelocity = true;
	m_teleportVelocity = request.initialVelocity;
	m_teleportTargetName = request.targetName;

	m_activeScene = newScene;
	m_sceneHierarchy.setContext(m_activeScene);
}

void EditorLayer::newProject() {
	const auto dir = core::utils::FileDialog::pickFolder();
	if (dir.empty())
		return;
	if (!exists(dir))
		create_directories(dir);
	create_directories(dir / "scenes");

	Project project;
	project.name = dir.filename().string();
	project.projectDirectory = dir;
	project.saveToFile(dir / "owl_project.yml");

	openProject(dir);
}

void EditorLayer::openProject() {
	const auto dir = core::utils::FileDialog::pickFolder();
	if (!dir.empty())
		openProject(dir);
}

void EditorLayer::openProject(const std::filesystem::path& iDir) {
	const auto configFile = iDir / "owl_project.yml";
	if (!exists(configFile)) {
		OWL_CORE_WARN("No owl_project.yml found in {}", iDir.string())
		return;
	}
	if (m_project.isLoaded())
		closeProject();

	m_project.loadFromFile(configFile);
	core::Application::get().addAssetDirectory(
			{std::format("Project: {}", m_project.name), m_project.projectDirectory});
	m_contentBrowser.attach();
	updateWindowTitle();

	if (!m_project.firstScene.empty()) {
		const auto scenePath = m_project.projectDirectory / m_project.firstScene;
		if (exists(scenePath))
			openScene(scenePath);
	}
}

void EditorLayer::saveProject() {
	if (!m_project.isLoaded())
		return;
	m_project.saveToFile(m_project.projectDirectory / "owl_project.yml");
}

void EditorLayer::closeProject() {
	if (!m_project.isLoaded())
		return;
	core::Application::get().removeAssetDirectory(m_project.projectDirectory);
	m_contentBrowser.attach();
	m_project = {};
	updateWindowTitle();
}

void EditorLayer::importScene() {
	if (!m_project.isLoaded())
		return;
	const auto filepath = core::utils::FileDialog::openFile("Owl Scene (*.owl)|owl\n");
	if (filepath.empty())
		return;
	const auto scenesDir = m_project.projectDirectory / "scenes";
	if (!exists(scenesDir))
		create_directories(scenesDir);
	const auto dest = scenesDir / filepath.filename();
	if (exists(dest)) {
		OWL_CORE_WARN("Scene '{}' already exists in project", filepath.filename().string())
		return;
	}
	std::filesystem::copy_file(filepath, dest);
	OWL_CORE_INFO("Imported scene '{}' into project", filepath.filename().string())
}

void EditorLayer::updateWindowTitle() {
	auto& app = core::Application::get();
	if (m_project.isLoaded())
		app.setWindowTitle(std::format("{} - {}", app.getInitParams().name, m_project.name));
	else
		app.setWindowTitle(app.getInitParams().name);
}

void EditorLayer::onDuplicateEntity() const {
	if (m_state != State::Edit)
		return;

	if (const scene::Entity selectedEntity = m_sceneHierarchy.getSelectedEntity(); selectedEntity)
		m_editorScene->duplicateEntity(selectedEntity);
}

auto EditorLayer::getSelectedEntity() const -> scene::Entity { return m_sceneHierarchy.getSelectedEntity(); }

void EditorLayer::setSelectedEntity(const scene::Entity iEntity) { m_sceneHierarchy.setSelectedEntity(iEntity); }

void EditorLayer::packScene() {
	if (m_currentScenePath.empty())
		return;

	const auto destPath = core::utils::FileDialog::saveFile("Owl Scene Pack (*.owlpack)|owlpack\n");
	if (destPath.empty())
		return;

	// Scan the current scene for assets.
	const auto assets = io::pack::AssetScanner::scanScene(m_currentScenePath);
	if (assets.empty()) {
		OWL_CORE_WARN("No assets found to pack for scene {}", m_currentScenePath.string())
		return;
	}

	// Build the scene pack.
	io::pack::PackWriter writer;
	for (const auto& ref: assets) {
		writer.addFile(ref.diskPath, ref.packPath, ref.assetType);
	}

	auto outputPath = destPath;
	if (outputPath.extension() != ".owlpack")
		outputPath.replace_extension(".owlpack");
	if (!writer.write(outputPath)) {
		OWL_CORE_ERROR("Failed to write scene pack to {}", outputPath.string())
		return;
	}

	OWL_CORE_INFO("Scene packed: {} ({} assets) -> {}", m_currentScenePath.filename().string(), assets.size(),
				  outputPath.string())
}

namespace {

void copySharedLibs(const std::filesystem::path& iSrcDir, const std::filesystem::path& iDestDir) {
	for (const auto& entry: std::filesystem::directory_iterator(iSrcDir)) {
		if (!entry.is_regular_file() && !entry.is_symlink())
			continue;
		const auto ext = entry.path().extension().string();
		const auto filename = entry.path().filename().string();
		// Copy .so files (Linux) and .dll files (Windows).
		if (ext == ".so" || filename.find(".so.") != std::string::npos || ext == ".dll") {
			const auto dest = iDestDir / entry.path().filename();
			if (!exists(dest))
				std::filesystem::copy(entry.path(), dest, std::filesystem::copy_options::copy_symlinks);
		}
	}
}

}// namespace

void EditorLayer::packGame() {
	if (!m_project.isLoaded())
		return;

	// Ask for destination folder.
	const auto destDir = core::utils::FileDialog::pickFolder();
	if (destDir.empty())
		return;

	// Create output directory structure.
	const auto gameDir = destDir / m_project.name;
	std::filesystem::create_directories(gameDir);

	// Scan all assets reachable from the first scene.
	const auto assets =
			io::pack::AssetScanner::scanProject(m_project.projectDirectory, m_project.firstScene);
	if (assets.empty()) {
		OWL_CORE_WARN("No assets found to pack")
		return;
	}

	// Build the asset pack.
	io::pack::PackWriter writer;
	for (const auto& ref: assets) {
		writer.addFile(ref.diskPath, ref.packPath, ref.assetType);
	}

	const auto packFilename = m_project.name + ".owlpack";
	const auto packPath = gameDir / packFilename;
	if (!writer.write(packPath)) {
		OWL_CORE_ERROR("Failed to write game pack to {}", packPath.string())
		return;
	}

	// Generate runner.yml with pack reference.
	{
		std::string firstScene = m_project.firstScene;
		if (std::filesystem::path(firstScene).extension() != ".owl")
			firstScene += ".owl";
		// Find the matching pack path for the first scene.
		for (const auto& ref: assets) {
			if (ref.assetType == io::pack::AssetType::Scene && ref.packPath.ends_with(firstScene)) {
				firstScene = ref.packPath;
				break;
			}
		}
		std::ofstream configOut(gameDir / "runner.yml");
		configOut << "RunnerConfig:\n";
		configOut << "  FirstScene: " << firstScene << "\n";
		configOut << "  PackFile: " << packFilename << "\n";
	}

	// Copy the runner executable.
	const auto& app = core::Application::get();
	const auto runnerSrcDir = app.getWorkingDirectory();
	// Find the runner executable next to the current OwlNest binary.
	std::filesystem::path runnerExe;
	for (const auto& candidate: {"OwlRunner", "OwlRunner.exe"}) {
		if (const auto p = runnerSrcDir / candidate; exists(p)) {
			runnerExe = p;
			break;
		}
	}
	if (runnerExe.empty()) {
		OWL_CORE_ERROR("OwlRunner executable not found in {}", runnerSrcDir.string())
		return;
	}
	std::filesystem::copy_file(runnerExe, gameDir / runnerExe.filename(),
							   std::filesystem::copy_options::overwrite_existing);
	// Set executable permission on Linux.
#ifdef OWL_PLATFORM_LINUX
	std::filesystem::permissions(gameDir / runnerExe.filename(),
								std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
										std::filesystem::perms::others_exec,
								std::filesystem::perm_options::add);
#endif

	// Copy shared libraries.
	copySharedLibs(runnerSrcDir, gameDir);

	OWL_CORE_INFO("Game exported: {} ({} assets) -> {}", m_project.name, assets.size(), gameDir.string())
}

}// namespace owl::nest
