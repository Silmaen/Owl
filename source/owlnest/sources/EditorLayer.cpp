/**
 * @file EditorLayer.cpp
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"

#include "commands/EntityCommands.h"
#include "commands/PrefabCommands.h"

#include <gui/IconBank.h>
#include <gui/utils.h>
#include <io/pack/AssetScanner.h>
#include <io/pack/PackWriter.h>
#include <physic/PhysicCommand.h>
#include <scene/PrefabSerializer.h>
#include <scene/component/components.h>
#include <sound/SoundCommand.h>
#include <sound/SoundSystem.h>

#include <chrono>
#include <cstdlib>


namespace owl::nest {
namespace {

void buildIconBank() {
	auto& iconBank = gui::IconBank::instance();

	// Resolve icon file paths: try SVG in assets_sources first, then PNG in assets.
	const auto& assetDirs = core::Application::get().getAssetDirectories();
	const auto resolve = [&](const std::string& iName) -> std::filesystem::path {
		for (const auto& dir: assetDirs) {
			// SVG sources in assets_sources/ (parallel to the assets/ directory).
			const auto svgDir = dir.assetsPath.parent_path() / "assets_sources";
			if (const auto p = svgDir / (iName + ".svg"); exists(p))
				return p;
			// PNG fallback in assets/.
			if (const auto p = dir.assetsPath / (iName + ".png"); exists(p))
				return p;
		}
		return {};
	};

	// clang-format off
	std::vector<std::pair<std::string, std::filesystem::path>> icons = {
		// Toolbar icons (playback + gizmo controls)
		{"ctrl_rotation",     resolve("icons/toolbar/ctrl_rotation")},
		{"ctrl_scale",        resolve("icons/toolbar/ctrl_scale")},
		{"ctrl_translation",  resolve("icons/toolbar/ctrl_translation")},
		{"PlayButton",        resolve("icons/toolbar/play")},
		{"PauseButton",       resolve("icons/toolbar/pause")},
		{"StopButton",        resolve("icons/toolbar/stop")},
		{"StepButton",        resolve("icons/toolbar/step")},
		// Visibility icons
		{"camera_on",         resolve("icons/visibility/camera_on")},
		{"camera_off",        resolve("icons/visibility/camera_off")},
		{"eye_open",          resolve("icons/visibility/eye_open")},
		{"eye_closed",        resolve("icons/visibility/eye_closed")},
		// Trigger icons
		{"trigger_victory",   resolve("icons/triggers/victory")},
		{"trigger_death",     resolve("icons/triggers/death")},
		{"trigger_target",    resolve("icons/triggers/target")},
		{"trigger_teleport",  resolve("icons/triggers/teleport")},
		{"trigger_timer",     resolve("icons/triggers/timer")},
		{"trigger_interact",  resolve("icons/triggers/interaction")},
		{"trigger_lua",       resolve("icons/triggers/lua_callback")},
		// File browser icons
		{"folder_icon",       resolve("icons/browser/folder")},
		{"glsl_icon",         resolve("icons/browser/glsl")},
		{"jpg_icon",          resolve("icons/browser/jpg")},
		{"json_icon",         resolve("icons/browser/json")},
		{"owl_icon",          resolve("icons/browser/owl")},
		{"png_icon",          resolve("icons/browser/png")},
		{"svg_icon",          resolve("icons/browser/svg_file")},
		{"text_icon",         resolve("icons/browser/text")},
		{"ttf_icon",          resolve("icons/browser/ttf")},
		{"yml_icon",          resolve("icons/browser/yml")},
		{"lua_icon",          resolve("icons/browser/lua")},
		{"prefab_icon",       resolve("icons/browser/prefab")},
		// Action icons (context menus, toolbar, etc.)
		{"delete",            resolve("icons/actions/delete")},
		{"rename",            resolve("icons/actions/rename")},
		{"new_folder",        resolve("icons/actions/new_folder")},
		{"import_file",       resolve("icons/actions/import_file")},
		{"import_folder",     resolve("icons/actions/import_folder")},
		{"add_entity",        resolve("icons/actions/add_entity")},
		{"add_child_entity",  resolve("icons/actions/add_child_entity")},
		{"add_component",     resolve("icons/actions/add_component")},
		{"delete_entity",     resolve("icons/actions/delete_entity")},
		{"delete_cascade",    resolve("icons/actions/delete_cascade")},
		{"unparent",          resolve("icons/actions/unparent")},
		{"save",              resolve("icons/actions/save")},
		{"open",              resolve("icons/actions/open")},
		{"new_scene",         resolve("icons/actions/new_scene")},
		{"duplicate",         resolve("icons/actions/duplicate")},
		{"undo",              resolve("icons/actions/undo")},
		{"redo",              resolve("icons/actions/redo")},
		{"settings",          resolve("icons/actions/settings")},
		{"search",            resolve("icons/actions/search")},
		{"pack",              resolve("icons/actions/pack")},
		// UI / navigation icons
		{"back",              resolve("icons/actions/back")},
		{"close",             resolve("icons/actions/close")},
		{"project",           resolve("icons/actions/project")},
		{"exit",              resolve("icons/actions/exit")},
		// Panel icons
		{"scene_hierarchy",   resolve("icons/panels/scene_hierarchy")},
		{"content_browser",   resolve("icons/panels/content_browser")},
		{"stats",             resolve("icons/panels/stats")},
		{"properties",        resolve("icons/panels/properties")},
		{"log",               resolve("icons/panels/log")},
		{"viewport",          resolve("icons/panels/viewport")},
		// Component icons
		{"comp_transform",    resolve("icons/components/transform")},
		{"comp_camera",       resolve("icons/components/camera")},
		{"comp_sprite",       resolve("icons/components/sprite")},
		{"comp_animated_sprite", resolve("icons/components/animated_sprite")},
		{"comp_circle",       resolve("icons/components/circle")},
		{"comp_text",         resolve("icons/components/text")},
		{"comp_physics",      resolve("icons/components/physics")},
		{"comp_script",       resolve("icons/components/script")},
		{"comp_lua_script",   resolve("icons/components/lua_script")},
		{"comp_sound",        resolve("icons/components/sound")},
		{"comp_trigger",      resolve("icons/components/trigger")},
		{"comp_player",       resolve("icons/components/player")},
		{"comp_link",         resolve("icons/components/link")},
		{"comp_background",   resolve("icons/components/background")},
		{"comp_visibility",   resolve("icons/components/visibility")},
		{"comp_canvas",       resolve("icons/components/canvas")},
		{"comp_ui_rect",      resolve("icons/components/ui_rect")},
		{"comp_ui_text",      resolve("icons/components/ui_text")},
		{"comp_ui_image",     resolve("icons/components/ui_image")},
		{"comp_ui_panel",     resolve("icons/components/ui_panel")},
		{"comp_ui_button",    resolve("icons/components/ui_button")},
		{"comp_ui_slider",    resolve("icons/components/ui_slider")},
		{"comp_ui_progress",  resolve("icons/components/ui_progress")},
	};
	// clang-format on

	// Remove entries with empty paths
	std::erase_if(icons, [](const auto& iEntry) { return iEntry.second.empty(); });

	// Extract theme colors for icon rendering from the active ImGui style.
	const auto& style = ImGui::GetStyle();
	const gui::IconThemeColors themeColors{
			.primary = {style.Colors[ImGuiCol_Text].x, style.Colors[ImGuiCol_Text].y, style.Colors[ImGuiCol_Text].z,
						style.Colors[ImGuiCol_Text].w},
			.secondary = {style.Colors[ImGuiCol_ButtonActive].x, style.Colors[ImGuiCol_ButtonActive].y,
						  style.Colors[ImGuiCol_ButtonActive].z, style.Colors[ImGuiCol_ButtonActive].w},
	};
	iconBank.build(icons, 64, themeColors);
}
void loadTriggerTextures() {
	// Trigger icons are also used for Renderer2D viewport drawing (not just ImGui),
	// so they need to remain in the texture library as individual textures.
	auto& textureLibrary = renderer::Renderer::getTextureLibrary();
	textureLibrary.load("icons/triggers/victory");
	textureLibrary.load("icons/triggers/death");
	textureLibrary.load("icons/triggers/target");
	textureLibrary.load("icons/triggers/teleport");
	textureLibrary.load("icons/triggers/timer");
	textureLibrary.load("icons/triggers/interaction");
	textureLibrary.load("icons/triggers/lua_callback");
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
	m_viewport.setUndoManager(&m_undoManager);
	m_sceneHierarchy.setUndoManager(&m_undoManager);

	buildIconBank();
	loadTriggerTextures();
	loadSounds();

	// Register all editor actions with default keybindings
	// clang-format off
	m_actionRegistry.registerAction("scene.new", "New Scene",
		{input::key::N, Modifiers::Ctrl},
		[this] { if (m_state == State::Edit) newScene(); });
	m_actionRegistry.registerAction("scene.open", "Open Scene",
		{input::key::O, Modifiers::Ctrl},
		[this] { if (m_state == State::Edit) openScene(); });
	m_actionRegistry.registerAction("scene.save", "Save Scene",
		{input::key::S, Modifiers::Ctrl},
		[this] { if (m_state == State::Edit && !m_currentScenePath.empty()) saveCurrentScene(); });
	m_actionRegistry.registerAction("scene.saveAs", "Save Scene As",
		{input::key::S, Modifiers::Ctrl | Modifiers::Shift},
		[this] { if (m_state == State::Edit) saveSceneAs(); });
	m_actionRegistry.registerAction("entity.duplicate", "Duplicate Entity",
		{input::key::D, Modifiers::Ctrl},
		[this] { if (m_state == State::Edit) onDuplicateEntity(); });
	m_actionRegistry.registerAction("guizmo.none", "Guizmo: None",
		{input::key::Q, Modifiers::None},
		[this] {
			if (m_state == State::Edit && (m_viewport.isFocused() || m_viewport.isHovered())
				&& !gui::Guizmo::isUsing())
				m_viewport.setGuizmoType(gui::Guizmo::Type::None);
		});
	m_actionRegistry.registerAction("guizmo.translate", "Guizmo: Translate",
		{input::key::W, Modifiers::None},
		[this] {
			if (m_state == State::Edit && (m_viewport.isFocused() || m_viewport.isHovered())
				&& !gui::Guizmo::isUsing())
				m_viewport.setGuizmoType(gui::Guizmo::Type::Translation);
		});
	m_actionRegistry.registerAction("guizmo.rotate", "Guizmo: Rotate",
		{input::key::E, Modifiers::None},
		[this] {
			if (m_state == State::Edit && (m_viewport.isFocused() || m_viewport.isHovered())
				&& !gui::Guizmo::isUsing())
				m_viewport.setGuizmoType(gui::Guizmo::Type::Rotation);
		});
	m_actionRegistry.registerAction("guizmo.scale", "Guizmo: Scale",
		{input::key::R, Modifiers::None},
		[this] {
			if (m_state == State::Edit && (m_viewport.isFocused() || m_viewport.isHovered())
				&& !gui::Guizmo::isUsing())
				m_viewport.setGuizmoType(gui::Guizmo::Type::Scale);
		});
	m_actionRegistry.registerAction("guizmo.all", "Guizmo: All",
		{input::key::T, Modifiers::None},
		[this] {
			if (m_state == State::Edit && (m_viewport.isFocused() || m_viewport.isHovered())
				&& !gui::Guizmo::isUsing())
				m_viewport.setGuizmoType(gui::Guizmo::Type::All);
		});
	// Playback actions
	m_actionRegistry.registerAction("scene.play", "Play/Resume",
		{input::key::F5, Modifiers::None},
		[this] {
			if (m_state == State::Edit)
				onScenePlay();
			else if (m_state == State::Pause)
				onSceneResume();
		});
	m_actionRegistry.registerAction("scene.pause", "Pause",
		{input::key::F6, Modifiers::None},
		[this] { if (m_state == State::Play) onScenePause(); });
	m_actionRegistry.registerAction("scene.stop", "Stop",
		{input::key::F7, Modifiers::None},
		[this] { if (m_state == State::Play || m_state == State::Pause) onSceneStop(); });
	m_actionRegistry.registerAction("scene.step", "Step Frame",
		{input::key::F8, Modifiers::None},
		[this] { if (m_state == State::Pause) onSceneStep(); });
	// Entity actions
	m_actionRegistry.registerAction("entity.delete", "Delete Entity",
		{input::key::Delete, Modifiers::None},
		[this] {
			if (m_state != State::Edit) return;
			if (auto ent = getSelectedEntity(); ent) {
				m_undoManager.push(mkUniq<commands::DeleteEntityCommand>(ent));
				m_activeScene->destroyEntity(ent);
				setSelectedEntity({});
			}
		});
	// Undo/Redo
	m_actionRegistry.registerAction("edit.undo", "Undo",
		{input::key::Z, Modifiers::Ctrl},
		[this] { performUndo(); });
	m_actionRegistry.registerAction("edit.redo", "Redo",
		{input::key::Y, Modifiers::Ctrl},
		[this] { performRedo(); });
	// clang-format on

	// Apply saved keybinding overrides
	m_actionRegistry.loadOverrides(m_settings.keybindingOverrides);

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
							std::format("Translation ({})", m_actionRegistry.getShortcutString("guizmo.translate"))});
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
							std::format("Rotation ({})", m_actionRegistry.getShortcutString("guizmo.rotate"))});
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
							std::format("Scale ({})", m_actionRegistry.getShortcutString("guizmo.scale"))});

	m_contentBrowser.attach();
	m_contentBrowser.setSceneOpenCallback([this](const std::filesystem::path& iPath) { openScene(iPath); });
	newScene();
}

void EditorLayer::onDetach() {
	OWL_PROFILE_FUNCTION()

	// Sync keybinding overrides before saving
	m_settings.keybindingOverrides = m_actionRegistry.getOverrides();
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
	if ((m_state == State::Play || m_state == State::Pause) && m_activeScene->status == scene::Scene::Status::Editing) {
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
						physic::PhysicCommand::setTransform(
								player, {targetTransform.translation().x(), targetTransform.translation().y()},
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

	// Handle deferred quit request from Lua (scene.quit()).
	if (m_stopRequested) {
		m_stopRequested = false;
		if (m_state == State::Play || m_state == State::Pause)
			onSceneStop();
		return;
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
	m_settingsPanel.onImGuiRender(m_settings, m_actionRegistry);
	m_asyncProgress.onImGuiRender();
	renderWelcomeScreen();
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

void EditorLayer::renderWelcomeScreen() {
	if (m_project.isLoaded() || !m_showWelcomeScreen)
		return;

	const auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(520, 400), ImGuiCond_Appearing);

	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	bool open = true;
	if (ImGui::Begin("Welcome to Owl Nest", &open, flags)) {
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Welcome to Owl Nest");
		ImGui::TextWrapped("Get started by creating a new project or opening an existing one.");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("New Project...", ImVec2(150, 0)))
			newProject();
		ImGui::SameLine();
		if (ImGui::Button("Open Project...", ImVec2(150, 0)))
			openProject();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Recent Projects");
		ImGui::Spacing();

		if (m_settings.recentProjects.empty()) {
			ImGui::TextDisabled("No recent projects.");
		} else {
			std::filesystem::path toOpen;
			std::filesystem::path toRemove;
			ImGui::BeginChild("##recents", ImVec2(0, 200), ImGuiChildFlags_Borders);
			for (const auto& recent: m_settings.recentProjects) {
				const std::filesystem::path path(recent);
				const auto name = path.filename().string();
				ImGui::PushID(recent.c_str());
				if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						toOpen = path;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", recent.c_str());
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
				if (ImGui::SmallButton("x"))
					toRemove = path;
				ImGui::PopID();
			}
			ImGui::EndChild();
			if (!toOpen.empty())
				openProject(toOpen);
			if (!toRemove.empty())
				m_settings.removeRecentProject(toRemove);
		}
	}
	ImGui::End();
	if (!open)
		m_showWelcomeScreen = false;
}

void EditorLayer::renderStats(const core::Timestep& iTimeStep) {
	if (!m_settings.showStats)
		return;
	ImGui::Begin("Stats");
	ImGui::Text("%s", std::format("FPS: {:.2f}", iTimeStep.getFps()).c_str());
	ImGui::Separator();
	ImGui::Text("%s", std::format("Current used memory: {}",
								  core::utils::sizeToString(debug::TrackerAPI::globals().allocatedMemory))
							  .c_str());
	ImGui::Text("%s",
				std::format("Max used memory: {}", core::utils::sizeToString(debug::TrackerAPI::globals().memoryPeek))
						.c_str());
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

void EditorLayer::renderMenu() {// NOLINT(readability-function-cognitive-complexity)
	const auto& iconBank = gui::IconBank::instance();

	if (ImGui::BeginMenuBar()) {
		// === File menu: everything about projects ===
		if (ImGui::BeginMenu("File")) {
			if (iconBank.menuItem("new_folder", "New Project"))
				newProject();
			if (iconBank.menuItem("open", "Open Project"))
				openProject();
			// Open Recent submenu.
			if (ImGui::BeginMenu("Open Recent", !m_settings.recentProjects.empty())) {
				std::filesystem::path toOpen;
				for (const auto& recent: m_settings.recentProjects) {
					const std::filesystem::path path(recent);
					const auto label = path.filename().string() + "  (" + path.parent_path().string() + ")";
					if (ImGui::MenuItem(label.c_str()))
						toOpen = path;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Clear Recent Projects"))
					m_settings.recentProjects.clear();
				ImGui::EndMenu();
				if (!toOpen.empty())
					openProject(toOpen);
			}
			if (iconBank.menuItem("save", "Save Project", nullptr, m_project.isLoaded()))
				saveProject();
			if (iconBank.menuItem("close", "Close Project", nullptr, m_project.isLoaded()))
				closeProject();
			ImGui::Separator();
			if (iconBank.menuItem("pack", "Pack Game", nullptr, m_project.isLoaded()))
				packGame();
			ImGui::Separator();
			if (!m_project.isLoaded() && ImGui::MenuItem("Welcome Screen"))
				m_showWelcomeScreen = true;
			if (iconBank.menuItem("exit", "Exit"))
				core::Application::get().close();
			ImGui::EndMenu();
		}
		// === Edit menu: undo/redo + settings ===
		if (ImGui::BeginMenu("Edit")) {
			const bool canUndo = m_undoManager.canUndo() && m_state == State::Edit;
			const bool canRedo = m_undoManager.canRedo() && m_state == State::Edit;
			const auto undoLabel =
					canUndo ? std::format("Undo {}", m_undoManager.undoDescription()) : std::string("Undo");
			const auto redoLabel =
					canRedo ? std::format("Redo {}", m_undoManager.redoDescription()) : std::string("Redo");
			if (iconBank.menuItem("undo", undoLabel.c_str(), m_actionRegistry.getShortcutString("edit.undo").c_str(),
								  canUndo))
				performUndo();
			if (iconBank.menuItem("redo", redoLabel.c_str(), m_actionRegistry.getShortcutString("edit.redo").c_str(),
								  canRedo))
				performRedo();
			ImGui::Separator();
			if (iconBank.menuItem("settings", "Engine Settings"))
				m_parameters.open();
			if (iconBank.menuItem("settings", "Editor Settings"))
				m_settingsPanel.open();
			if (iconBank.menuItem("settings", "Project Settings", nullptr, m_project.isLoaded()))
				m_projectSettings.open(m_project);
			ImGui::EndMenu();
		}
		// === Current menu: everything about the currently open scene ===
		if (ImGui::BeginMenu("Current", m_project.isLoaded())) {
			if (iconBank.menuItem("new_scene", "New Scene", m_actionRegistry.getShortcutString("scene.new").c_str()))
				newScene();
			if (iconBank.menuItem("open", "Open Scene", m_actionRegistry.getShortcutString("scene.open").c_str()))
				openScene();
			ImGui::Separator();
			if (iconBank.menuItem("save", "Save Scene", m_actionRegistry.getShortcutString("scene.save").c_str(),
								  !m_currentScenePath.empty()))
				saveCurrentScene();
			if (iconBank.menuItem("save", "Save Scene as..",
								  m_actionRegistry.getShortcutString("scene.saveAs").c_str()))
				saveSceneAs();
			ImGui::Separator();
			if (iconBank.menuItem("import_file", "Import Scene"))
				importScene();
			if (iconBank.menuItem("pack", "Pack Scene", nullptr, !m_currentScenePath.empty()))
				packScene();
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

	const auto& iconBank = gui::IconBank::instance();

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
	m_undoManager.clear();
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

	// Read file on background thread, deserialize on main thread in callback.
	auto state = mkShared<AsyncProgressState>();
	state->setMessage("Loading scene...");
	m_asyncProgress.open("Loading Scene...", state, false);

	auto fileData = mkShared<std::vector<uint8_t>>();

	core::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, scenePath = iScenePath, fileData]() {
				state->progress.store(0.2f);
				std::ifstream file(scenePath, std::ios::binary | std::ios::ate);
				if (!file.is_open()) {
					state->setError("Failed to open scene: " + scenePath.string());
					return;
				}
				const auto size = static_cast<size_t>(file.tellg());
				file.seekg(0);
				fileData->resize(size);
				file.read(reinterpret_cast<char*>(fileData->data()), static_cast<std::streamsize>(size));
				state->progress.store(0.8f);
				state->setMessage("Deserializing...");
			},
			// Termination: runs on main thread — safe to modify scene.
			[this, state, scenePath = iScenePath, fileData]() {
				if (state->hasError.load()) {
					state->completed.store(true);
					return;
				}
				const auto newScene = mkShared<scene::Scene>();
				if (const scene::SceneSerializer serializer(newScene);
					serializer.deserializeFromBuffer(*fileData, scenePath.string())) {
					m_editorScene = newScene;
					m_editorScene->onViewportResize(m_viewport.getSize());
					m_sceneHierarchy.setContext(m_editorScene);
					m_activeScene = m_editorScene;
					m_currentScenePath = scenePath;
					m_undoManager.clear();
				} else {
					state->setError("Failed to deserialize scene.");
				}
				state->completed.store(true);
			}));
}

void EditorLayer::saveSceneAs() {
	if (const auto filepath = core::utils::FileDialog::saveFile("Owl Scene (*.owl)|owl\n"); !filepath.empty())
		saveSceneAs(filepath);
}

void EditorLayer::saveSceneAs(const std::filesystem::path& iScenePath) {
	// Serialize to string on main thread (read-only, safe), write file on background thread.
	const scene::SceneSerializer serializer(m_activeScene);
	auto yamlData = mkShared<std::string>(serializer.serializeToString());
	m_currentScenePath = iScenePath;

	auto state = mkShared<AsyncProgressState>();
	state->setMessage("Saving scene...");
	state->progress.store(0.5f);
	m_asyncProgress.open("Saving...", state, false);

	core::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, yamlData, path = iScenePath]() {
				std::ofstream fileOut(path);
				if (!fileOut.is_open()) {
					state->setError("Failed to open file for writing: " + path.string());
					return;
				}
				fileOut << *yamlData;
				fileOut.close();
				state->progress.store(1.0f);
				state->setMessage("Done!");
				OWL_CORE_INFO("Scene saved to {}", path.string())
			},
			[state]() { state->completed.store(true); }));
}

void EditorLayer::saveCurrentScene() {
	if (m_currentScenePath.empty())
		saveSceneAs();
	else
		saveSceneAs(m_currentScenePath);
	m_undoManager.markSaved();
}

auto EditorLayer::onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool {
	return m_actionRegistry.dispatch(ioEvent);
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

	// Preserve game state across scene transition.
	const auto previousGameState = m_activeScene->getGameState();

	m_activeScene->onEndRuntime();

	const auto newScene = mkShared<scene::Scene>();
	if (const scene::SceneSerializer sc(newScene); !sc.deserialize(levelPath)) {
		OWL_CORE_ERROR("Teleport: failed to load level '{}'", request.levelName)
		return;
	}
	newScene->getGameState() = previousGameState;
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

void EditorLayer::handleSaveLoadRequest() {
	if (!m_activeScene || !m_activeScene->saveLoadRequest.pending)
		return;
	const auto slr = m_activeScene->saveLoadRequest;
	m_activeScene->saveLoadRequest.pending = false;
	if (slr.isLoad) {
		auto newScene = mkShared<scene::Scene>();
		if (auto loadResult = scene::SaveManager::load(slr.slot, newScene); loadResult.success) {
			m_activeScene->onEndRuntime();
			m_activeScene = newScene;
			m_activeScene->onViewportResize(m_viewport.getSize());
			m_activeScene->onStartRuntime();
			// Apply physics snapshots after physics initialization.
			for (const auto& [uuid, snap]: loadResult.physicsSnapshots)
				if (auto entity = m_activeScene->findEntityByUUID(core::UUID{uuid}); entity)
					physic::PhysicCommand::applySnapshot(entity, snap);
			m_sceneHierarchy.setContext(m_activeScene);
		}
	} else {
		std::ignore = scene::SaveManager::save(slr.slot, m_activeScene, m_currentScenePath.string());
	}
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
		m_settings.removeRecentProject(iDir);
		return;
	}
	if (m_project.isLoaded())
		closeProject();

	m_project.loadFromFile(configFile);
	core::Application::get().addAssetDirectory(
			{std::format("Project: {}", m_project.name), m_project.projectDirectory});
	m_contentBrowser.attach();
	updateWindowTitle();

	// Add to recent projects list.
	m_settings.pushRecentProject(iDir);

	if (!m_project.firstScene.empty()) {
		const auto scenePath = m_project.projectDirectory / m_project.firstScene;
		if (exists(scenePath))
			openScene(scenePath);
	}
}

void EditorLayer::saveProject() {
	if (!m_project.isLoaded())
		return;
	if (m_state == State::Edit)
		saveCurrentScene();
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
	const auto* const dirty = m_undoManager.isDirty() ? " *" : "";
	if (m_project.isLoaded())
		app.setWindowTitle(std::format("{} - {}{}", app.getInitParams().name, m_project.name, dirty));
	else
		app.setWindowTitle(std::format("{}{}", app.getInitParams().name, dirty));
}

void EditorLayer::onDuplicateEntity() {
	if (m_state != State::Edit)
		return;

	if (const scene::Entity selectedEntity = m_sceneHierarchy.getSelectedEntity(); selectedEntity) {
		auto dup = m_editorScene->duplicateEntity(selectedEntity);
		m_undoManager.push(mkUniq<commands::DuplicateEntityCommand>(selectedEntity, dup));
	}
}

void EditorLayer::performUndo() {
	if (m_state != State::Edit || !m_activeScene)
		return;
	m_undoManager.undo(*m_activeScene);
	if (const auto hint = m_undoManager.lastSelectionHint(); hint != core::UUID{0}) {
		if (auto entity = m_activeScene->findEntityByUUID(hint); entity)
			setSelectedEntity(entity);
	}
}

void EditorLayer::performRedo() {
	if (m_state != State::Edit || !m_activeScene)
		return;
	m_undoManager.redo(*m_activeScene);
	if (const auto hint = m_undoManager.lastSelectionHint(); hint != core::UUID{0}) {
		if (auto entity = m_activeScene->findEntityByUUID(hint); entity)
			setSelectedEntity(entity);
	}
}

auto EditorLayer::getSelectedEntity() const -> scene::Entity { return m_sceneHierarchy.getSelectedEntity(); }

void EditorLayer::setSelectedEntity(const scene::Entity iEntity) { m_sceneHierarchy.setSelectedEntity(iEntity); }

void EditorLayer::packScene() {
	if (m_currentScenePath.empty() || m_asyncProgress.isActive())
		return;

	const auto destPath = core::utils::FileDialog::saveFile("Owl Scene Pack (*.owlpack)|owlpack\n");
	if (destPath.empty())
		return;

	auto outputPath = destPath;
	if (outputPath.extension() != ".owlpack")
		outputPath.replace_extension(".owlpack");

	// Snapshot for thread safety.
	const auto scenePath = m_currentScenePath;
	const auto sceneFilename = m_currentScenePath.filename().string();

	auto state = mkShared<AsyncProgressState>();
	m_asyncProgress.open("Packing Scene...", state, true);

	core::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, scenePath, sceneFilename, outputPath]() {
				state->setMessage("Scanning assets...");
				const auto assets = io::pack::AssetScanner::scanScene(scenePath);
				if (assets.empty()) {
					state->setError("No assets found to pack for scene " + sceneFilename);
					return;
				}
				if (state->cancelRequested.load())
					return;
				state->progress.store(0.2f);
				state->setMessage("Writing pack (" + std::to_string(assets.size()) + " assets)...");

				io::pack::PackWriter writer;
				for (const auto& ref: assets) writer.addFile(ref.diskPath, ref.packPath, ref.assetType);

				const bool writeOk = writer.write(
						outputPath, io::pack::PackFlags::Default,
						[&state](const uint32_t iCurrent, const uint32_t iTotal) {
							state->progress.store(0.2f + 0.75f * static_cast<float>(iCurrent) /
																   static_cast<float>(iTotal));
						},
						[&state]() -> bool { return state->cancelRequested.load(); });
				if (!writeOk) {
					if (state->cancelRequested.load())
						state->setError("Packing cancelled.");
					else
						state->setError("Failed to write scene pack.");
					return;
				}
				state->progress.store(1.0f);
				state->setMessage("Done!");
				OWL_CORE_INFO("Scene packed: {} ({} assets) -> {}", sceneFilename, assets.size(),
							  outputPath.string())
			},
			[state]() { state->completed.store(true); }));
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

/// Sanitize a string for use as a filename (replace unsafe characters with _).
auto sanitizeFilename(const std::string& iName) -> std::string {
	std::string result;
	result.reserve(iName.size());
	for (const auto ch: iName) {
		if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' ||
			ch == '|' || ch == ' ')
			result += '_';
		else
			result += ch;
	}
	return result;
}

#ifdef OWL_PLATFORM_LINUX
/// Write a Linux launcher shell script that sets LD_LIBRARY_PATH.
void writeLinuxLauncher(const std::filesystem::path& iGameDir, const std::string& iExeName) {
	std::ofstream script(iGameDir / "launch.sh");
	script << "#!/bin/sh\n";
	script << "# Launcher for " << iExeName << "\n";
	script << "SCRIPT_DIR=\"$(cd \"$(dirname \"$0\")\" && pwd)\"\n";
	script << "export LD_LIBRARY_PATH=\"${SCRIPT_DIR}:${LD_LIBRARY_PATH}\"\n";
	script << "exec \"${SCRIPT_DIR}/" << iExeName << "\" \"$@\"\n";
	script.close();
	std::filesystem::permissions(iGameDir / "launch.sh",
								 std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
										 std::filesystem::perms::others_exec,
								 std::filesystem::perm_options::add);
}
#endif

/// Write a game_info.yml metadata file.
void writeMetadata(const std::filesystem::path& iGameDir, const std::string& iGameName, const std::string& iVersion,
				   const std::string& iAuthor, const std::string& iDescription) {
	const auto now = std::chrono::system_clock::now();
	const auto days = std::chrono::floor<std::chrono::days>(now);
	const std::chrono::year_month_day ymd{days};
	const auto packDate = std::format("{:%Y-%m-%d}", ymd);
#ifdef OWL_PLATFORM_LINUX
	constexpr auto platform = "linux-x64";
#elif defined(OWL_PLATFORM_WINDOWS)
	constexpr auto platform = "windows-x64";
#else
	constexpr auto platform = "unknown";
#endif
	std::ofstream meta(iGameDir / "game_info.yml");
	meta << "GameInfo:\n";
	meta << "  Name: " << iGameName << "\n";
	if (!iVersion.empty())
		meta << "  Version: " << iVersion << "\n";
	if (!iAuthor.empty())
		meta << "  Author: " << iAuthor << "\n";
	if (!iDescription.empty())
		meta << "  Description: " << iDescription << "\n";
	meta << "  EngineVersion: " << owl::getVersionString() << "\n";
	meta << "  PackDate: " << packDate << "\n";
	meta << "  Platform: " << platform << "\n";
}

#ifdef OWL_PLATFORM_WINDOWS
/// Create a .zip archive from a directory using PowerShell.
auto createZipArchive(const std::filesystem::path& iSourceDir, const std::filesystem::path& iOutputZip) -> bool {
	const auto cmd = std::format(
			"powershell -NoProfile -Command \"Compress-Archive -Path '{}\\*' -DestinationPath '{}' -Force\"",
			iSourceDir.string(), iOutputZip.string());
	return std::system(cmd.c_str()) == 0;// NOLINT(concurrency-mt-unsafe)
}
#endif

}// namespace

void EditorLayer::packGame() {
	if (!m_project.isLoaded() || m_asyncProgress.isActive())
		return;

	// Ask for destination folder (synchronous — OS dialog).
	const auto destDir = core::utils::FileDialog::pickFolder();
	if (destDir.empty())
		return;

	// Snapshot project data (copy by value for thread safety).
	const auto gameName = sanitizeFilename(m_project.name);
	const auto projectDir = m_project.projectDirectory;
	const auto firstScene = m_project.firstScene;
	const auto projectName = m_project.name;
	const auto projectVersion = m_project.version;
	const auto projectAuthor = m_project.author;
	const auto projectDesc = m_project.description;
	const auto projectIcon = m_project.icon;
	const auto windowCfg = m_project.window;
	const auto runnerSrcDir = core::Application::get().getWorkingDirectory();

	const auto gameDir = destDir / gameName;
	std::filesystem::create_directories(gameDir);

	// Create shared progress state and open the modal.
	auto state = mkShared<AsyncProgressState>();
	m_asyncProgress.open("Packing Game...", state, true);

	// Push async task to the scheduler.
	core::Application::get().getTaskScheduler().pushTask(core::task::Task(
			// --- Worker function (runs on thread pool) ---
			[state, gameName, gameDir, projectDir, firstScene, projectName, projectVersion, projectAuthor, projectDesc,
			 projectIcon, windowCfg, runnerSrcDir]() {
				// Phase 1: Scan assets (0% – 20%).
				state->setMessage("Scanning assets...");
				const auto assets = io::pack::AssetScanner::scanProject(projectDir, firstScene);
				if (assets.empty()) {
					state->setError("No assets found to pack.");
					return;
				}
				if (state->cancelRequested.load())
					return;
				state->progress.store(0.15f);
				state->setMessage("Building pack (" + std::to_string(assets.size()) + " assets)...");

				// Phase 2: Build and write pack file (20% – 80%).
				io::pack::PackWriter writer;
				for (const auto& ref: assets) writer.addFile(ref.diskPath, ref.packPath, ref.assetType);
				if (const auto gsPath = projectDir / "game_settings.yml"; exists(gsPath))
					writer.addFile(gsPath, "game_settings.yml", io::pack::AssetType::Other);

				const auto packFilename = gameName + ".owlpack";
				const auto packPath = gameDir / packFilename;
				const bool writeOk = writer.write(
						packPath, io::pack::PackFlags::Default,
						[&state](const uint32_t iCurrent, const uint32_t iTotal) {
							state->progress.store(0.2f + 0.6f * static_cast<float>(iCurrent) /
																  static_cast<float>(iTotal));
						},
						[&state]() -> bool { return state->cancelRequested.load(); });
				if (!writeOk) {
					if (state->cancelRequested.load())
						state->setError("Packaging cancelled.");
					else
						state->setError("Failed to write game pack.");
					return;
				}
				state->progress.store(0.82f);

				// Phase 3: Generate config files (80% – 90%).
				state->setMessage("Writing configuration...");
				{
					std::string resolvedFirstScene = firstScene;
					if (std::filesystem::path(resolvedFirstScene).extension() != ".owl")
						resolvedFirstScene += ".owl";
					for (const auto& ref: assets) {
						if (ref.assetType == io::pack::AssetType::Scene &&
							ref.packPath.ends_with(resolvedFirstScene)) {
							resolvedFirstScene = ref.packPath;
							break;
						}
					}
					std::ofstream configOut(gameDir / "runner.yml");
					configOut << "RunnerConfig:\n";
					configOut << "  FirstScene: " << resolvedFirstScene << "\n";
					configOut << "  PackFile: " << packFilename << "\n";
					configOut << "  GameName: " << projectName << "\n";
					if (!projectVersion.empty())
						configOut << "  Version: " << projectVersion << "\n";
					if (!projectAuthor.empty())
						configOut << "  Author: " << projectAuthor << "\n";
					if (!projectIcon.empty())
						configOut << "  Icon: " << projectIcon << "\n";
					configOut << "  WindowWidth: " << windowCfg.width << "\n";
					configOut << "  WindowHeight: " << windowCfg.height << "\n";
					configOut << "  Fullscreen: " << (windowCfg.fullscreen ? "true" : "false") << "\n";
					configOut << "  Resizable: " << (windowCfg.resizable ? "true" : "false") << "\n";
				}
				if (state->cancelRequested.load())
					return;

				// Copy icon.
				if (!projectIcon.empty()) {
					const auto iconSrc = projectDir / projectIcon;
					if (exists(iconSrc)) {
						const auto iconDst = gameDir / projectIcon;
						std::filesystem::create_directories(iconDst.parent_path());
						std::error_code ec;
						std::filesystem::copy(iconSrc, iconDst,
											  std::filesystem::copy_options::overwrite_existing, ec);
					}
				}
				state->progress.store(0.88f);

				// Phase 4: Copy runner + shared libs (90% – 100%).
				state->setMessage("Copying runner...");
				std::filesystem::path runnerExe;
				for (const auto& candidate: {"OwlRunner", "OwlRunner.exe"}) {
					if (const auto p = runnerSrcDir / candidate; exists(p)) {
						runnerExe = p;
						break;
					}
				}
				if (runnerExe.empty()) {
					state->setError("OwlRunner executable not found.");
					return;
				}
#ifdef OWL_PLATFORM_WINDOWS
				const auto exeFilename = gameName + ".exe";
#else
				const auto& exeFilename = gameName;
#endif
				const auto destExe = gameDir / exeFilename;
				std::filesystem::copy_file(runnerExe, destExe,
										   std::filesystem::copy_options::overwrite_existing);
#ifdef OWL_PLATFORM_LINUX
				std::filesystem::permissions(destExe,
											 std::filesystem::perms::owner_exec |
													 std::filesystem::perms::group_exec |
													 std::filesystem::perms::others_exec,
											 std::filesystem::perm_options::add);
#endif
				copySharedLibs(runnerSrcDir, gameDir);
#ifdef OWL_PLATFORM_LINUX
				writeLinuxLauncher(gameDir, exeFilename);
#endif
				writeMetadata(gameDir, projectName, projectVersion, projectAuthor, projectDesc);
#ifdef OWL_PLATFORM_WINDOWS
				{
					const auto zipPath = gameDir.parent_path() / (gameName + ".zip");
					createZipArchive(gameDir, zipPath);
				}
#endif
				state->progress.store(1.0f);
				state->setMessage("Done!");
				OWL_CORE_INFO("Game exported: {} ({} assets) -> {}", projectName, assets.size(),
							  gameDir.string())
			},
			// --- Termination callback (runs on main thread) ---
			[state]() { state->completed.store(true); }));
}

void EditorLayer::instantiatePrefab(const std::filesystem::path& iPrefabPath, const std::string& iAssetRelativePath) {
	if (m_state != State::Edit || !m_activeScene)
		return;
	auto root = scene::PrefabSerializer::instantiate(iPrefabPath, m_activeScene, iAssetRelativePath);
	if (!root) {
		OWL_WARN("Failed to instantiate prefab: {}", iPrefabPath.string())
		return;
	}
	const auto info = scene::PrefabSerializer::readInfo(iPrefabPath);
	const auto name = info.has_value() ? info->name : iPrefabPath.stem().string();
	m_undoManager.push(mkUniq<commands::InstantiatePrefabCommand>(root, *m_activeScene, name));
	setSelectedEntity(root);
}

}// namespace owl::nest
