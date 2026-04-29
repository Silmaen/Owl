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
#include "document/AnimationDocument.h"
#include "document/CodeEditorDocument.h"
#include "document/NodeGraphDocument.h"
#include "document/SceneFlowDocument.h"

#include <gui/FontPreviewCache.h>
#include <gui/IconBank.h>
#include <gui/utils.h>
#include <io/pack/AssetScanner.h>
#include <io/pack/PackWriter.h>
#include <physic/PhysicCommand.h>
#include <scene/PrefabSerializer.h>
#include <scene/component/components.h>
#include <sound/SoundCommand.h>
#include <sound/SoundSystem.h>

#include <imgui_stdlib.h>

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
		{"owlanim_icon",      resolve("icons/browser/owlanim")},
		{"wav_icon",          resolve("icons/browser/wav")},
		{"mp3_icon",          resolve("icons/browser/mp3")},
		{"ogg_icon",          resolve("icons/browser/ogg")},
		{"flac_icon",         resolve("icons/browser/flac")},
		{"obj_icon",          resolve("icons/browser/obj")},
		{"gltf_icon",         resolve("icons/browser/gltf")},
		{"glb_icon",          resolve("icons/browser/glb")},
		{"fbx_icon",          resolve("icons/browser/fbx")},
		{"py_icon",           resolve("icons/browser/py")},
		{"cpp_icon",          resolve("icons/browser/cpp")},
		{"h_icon",            resolve("icons/browser/h")},
		{"c_icon",            resolve("icons/browser/c")},
		{"md_icon",           resolve("icons/browser/md")},
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
		{"info",              resolve("icons/panels/info")},
		{"preview",           resolve("icons/panels/preview")},
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
	std::erase_if(icons, [](const auto& iEntry) -> auto { return iEntry.second.empty(); });

	// Extract theme colors for icon rendering. Primary follows the text color; secondary is a fixed
	// amber/gold matching the Owl Nest brand (used as accent for highlights inside icons).
	const auto& style = ImGui::GetStyle();
	const gui::IconThemeColors themeColors{
			.primary = {style.Colors[ImGuiCol_Text].x, style.Colors[ImGuiCol_Text].y, style.Colors[ImGuiCol_Text].z,
						style.Colors[ImGuiCol_Text].w},
			.secondary = {1.0f, 0.78f, 0.15f, 1.0f},
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

auto EditorLayer::activeSceneDocument() const -> SceneDocument* {
	auto* doc = m_documents.getActive();
	if (doc == nullptr || doc->type() != DocumentType::Scene)
		return nullptr;
	return static_cast<SceneDocument*>(doc);
}

auto EditorLayer::activeViewport() const -> panel::Viewport* {
	if (auto* doc = activeSceneDocument(); doc != nullptr)
		return &doc->getViewport();
	return nullptr;
}

auto EditorLayer::activeViewportSize() const -> math::vec2ui {
	if (const auto* v = activeViewport(); v != nullptr && v->getSize().surface() > 0)
		return v->getSize();
	return {1280u, 720u};
}

auto EditorLayer::ensureActiveSceneDocument() -> SceneDocument& {
	if (auto* existing = activeSceneDocument(); existing != nullptr)
		return *existing;
	auto doc = mkUniq<SceneDocument>();
	doc->onAttach(this);
	auto* raw = m_documents.add(std::move(doc));
	// m_documents.add() sets it active.
	return *static_cast<SceneDocument*>(raw);
}

auto EditorLayer::getState() const -> State {
	if (const auto* doc = activeSceneDocument(); doc != nullptr)
		return doc->state();
	return State::Edit;
}

auto EditorLayer::getActiveScene() const -> const shared<scene::Scene>& {
	if (const auto* doc = activeSceneDocument(); doc != nullptr)
		return doc->getActiveScene();
	static const shared<scene::Scene> s_empty;
	return s_empty;
}

auto EditorLayer::activeUndoManager() -> SceneUndoManager* {
	if (auto* doc = activeSceneDocument(); doc != nullptr)
		return &doc->undoManager();
	return nullptr;
}

void EditorLayer::requestStop() {
	if (auto* doc = activeSceneDocument(); doc != nullptr)
		doc->requestStop();
}

auto EditorLayer::findSceneDocumentByPath(const std::filesystem::path& iPath) const -> SceneDocument* {
	const auto canonical = std::filesystem::weakly_canonical(iPath);
	for (const auto& docPtr: m_documents.list()) {
		if (docPtr && docPtr->type() == DocumentType::Scene) {
			auto* scene = static_cast<SceneDocument*>(docPtr.get());
			if (!scene->filePath().empty() &&
				std::filesystem::weakly_canonical(scene->filePath()) == canonical)
				return scene;
		}
	}
	return nullptr;
}

auto EditorLayer::findPlayingSceneDocument() const -> SceneDocument* {
	for (const auto& docPtr: m_documents.list()) {
		if (docPtr && docPtr->type() == DocumentType::Scene) {
			auto* scene = static_cast<SceneDocument*>(docPtr.get());
			if (scene->state() != SceneDocument::State::Edit)
				return scene;
		}
	}
	return nullptr;
}

void EditorLayer::syncActiveDocumentPanels() {
	auto* doc = activeSceneDocument();
	if (doc == nullptr) {
		static const shared<scene::Scene> s_empty;
		m_sceneHierarchy.setContext(s_empty);
		m_sceneHierarchy.setUndoManager(nullptr);
	} else {
		m_sceneHierarchy.setContext(doc->getActiveScene());
		m_sceneHierarchy.setUndoManager(&doc->undoManager());
		// The per-document viewport owns its own undo pointer already (set in SceneDocument::onAttach).
	}
	// Whichever document is active drives the override on the global panels — when it overrides,
	// SceneHierarchy delegates its `Scene Hierarchy` / `Properties` content to the document.
	m_sceneHierarchy.setActiveDocument(m_documents.getActive());
	updateWindowTitle();
	refreshRibbonForActiveDoc();
}

void EditorLayer::handleContentBrowserDrop(const std::filesystem::path& iRelativePath) {
	const auto ext = iRelativePath.extension().string();
	static const std::vector<std::string> codeExts = {
			".lua", ".py",   ".c",    ".cpp",  ".cc",        ".cxx", ".h",  ".hpp",
			".hxx", ".yml",  ".yaml", ".json", ".md",        ".markdown", ".svg", ".xml"};

	const auto relString = iRelativePath.generic_string();
	if (ext == ".owlprefab") {
		if (const auto full = renderer::Renderer::getTextureLibrary().find(relString); full.has_value())
			instantiatePrefab(full.value(), relString);
	} else if (ext == ".owl") {
		if (const auto full = renderer::Renderer::getTextureLibrary().find(relString); full.has_value())
			openScene(full.value());
	} else if (ext == ".owlflow") {
		if (const auto full = renderer::Renderer::getTextureLibrary().find(relString); full.has_value())
			openNodeGraphFile(full.value());
	} else if (ext == ".owlanim") {
		if (const auto full = renderer::Renderer::getTextureLibrary().find(relString); full.has_value())
			openAnimationFile(full.value());
	} else if (std::ranges::find(codeExts, ext) != codeExts.end()) {
		if (const auto full = renderer::Renderer::getTextureLibrary().find(relString); full.has_value())
			openCodeFile(full.value());
		else
			OWL_CORE_WARN("Could not resolve dropped file: {}", relString)
	} else {
		OWL_CORE_WARN("Could not load {}: unsupported file type", relString)
	}
}

void EditorLayer::closeDocument(const core::UUID iId) {
	const bool wasActive = (m_documents.getActive() != nullptr && m_documents.getActive()->id() == iId);
	if (!m_documents.remove(iId))
		return;
	// Ensure there is always at least one scene document to edit.  This creates a fresh
	// SceneDocument and makes it the active one — we must `syncActiveDocumentPanels` AFTER this
	// so the hierarchy, ribbon, etc. catch up with the new active doc (otherwise they would be
	// left in the `null active` state from the removal).
	if (m_documents.empty())
		ensureActiveSceneDocument();
	if (wasActive || m_documents.getActive() != nullptr)
		syncActiveDocumentPanels();
}

void EditorLayer::requestCloseDocument(const core::UUID iId) {
	const auto* doc = m_documents.find(iId);
	if (doc == nullptr)
		return;
	if (doc->isDirty()) {
		m_pendingCloseDocId = iId;
		m_openCloseDocModal = true;
	} else {
		m_deferredCloseIds.push_back(iId);
	}
}

void EditorLayer::requestCloseActiveDocument() {
	if (const auto* doc = m_documents.getActive(); doc != nullptr)
		requestCloseDocument(doc->id());
}

void EditorLayer::renderRecentProjectsPopup() {
	if (m_openRecentProjectsPopup) {
		ImGui::OpenPopup("##RecentProjectsPopup");
		m_openRecentProjectsPopup = false;
	}
	if (ImGui::BeginPopup("##RecentProjectsPopup")) {
		if (m_settings.recentProjects.empty()) {
			ImGui::TextDisabled("No recent projects");
		} else {
			for (const auto& recent: m_settings.recentProjects) {
				const auto label = std::filesystem::path(recent).filename().string();
				const auto display = label.empty() ? recent : label;
				if (ImGui::MenuItem(display.c_str(), recent.c_str())) {
					ImGui::CloseCurrentPopup();
					openProject(std::filesystem::path{recent});
				}
			}
		}
		ImGui::EndPopup();
	}
}

void EditorLayer::renderCloseDocumentModal() {
	if (m_openCloseDocModal) {
		ImGui::OpenPopup("Close Unsaved Document?");
		m_openCloseDocModal = false;
	}
	if (ImGui::BeginPopupModal("Close Unsaved Document?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		if (const auto* doc = m_documents.find(m_pendingCloseDocId); doc != nullptr) {
			ImGui::Text("'%s' has unsaved changes.", doc->title().c_str());
			ImGui::Spacing();
			const auto& iconBank = gui::IconBank::instance();
			if (iconBank.iconButton("delete", "Discard changes", {ImGui::GetFontSize() * 9.f, 0})) {
				const auto id = m_pendingCloseDocId;
				m_pendingCloseDocId = core::UUID{0};
				ImGui::CloseCurrentPopup();
				m_deferredCloseIds.push_back(id);
			}
			ImGui::SameLine();
			if (iconBank.iconButton("close", "Cancel", {ImGui::GetFontSize() * 7.f, 0})) {
				m_pendingCloseDocId = core::UUID{0};
				ImGui::CloseCurrentPopup();
			}
		} else {
			m_pendingCloseDocId = core::UUID{0};
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

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

	// Create the initial scene document. Its onAttach() initialises its own Viewport.
	auto& initialDoc = ensureActiveSceneDocument();
	m_sceneHierarchy.setUndoManager(&initialDoc.undoManager());

	buildIconBank();
	loadTriggerTextures();
	loadSounds();

	// Register all editor actions with default keybindings
	// clang-format off
	m_actionRegistry.registerAction("scene.new", "New Scene",
		{input::key::N, Modifiers::Ctrl},
		[this] () -> void { if (getState() == State::Edit) newScene(); });
	m_actionRegistry.registerAction("scene.open", "Open Scene",
		{input::key::O, Modifiers::Ctrl},
		[this] () -> void { if (getState() == State::Edit) openScene(); });
	m_actionRegistry.registerAction("scene.save", "Save Scene",
		{input::key::S, Modifiers::Ctrl},
		[this] () -> void {
			const auto* doc = activeSceneDocument();
			if (getState() == State::Edit && doc != nullptr && !doc->filePath().empty())
				saveCurrentScene();
		});
	m_actionRegistry.registerAction("scene.saveAs", "Save Scene As",
		{input::key::S, Modifiers::Ctrl | Modifiers::Shift},
		[this] () -> void { if (getState() == State::Edit) saveSceneAs(); });
	m_actionRegistry.registerAction("entity.duplicate", "Duplicate Entity",
		{input::key::D, Modifiers::Ctrl},
		[this] () -> void { if (getState() == State::Edit) onDuplicateEntity(); });
	m_actionRegistry.registerAction("guizmo.none", "Guizmo: None",
		{input::key::Q, Modifiers::None},
		[this] () -> void {
			if (auto* vp = activeViewport(); vp != nullptr && getState() == State::Edit &&
				(vp->isFocused() || vp->isHovered()) && !gui::Guizmo::isUsing())
				vp->setGuizmoType(gui::Guizmo::Type::None);
		});
	m_actionRegistry.registerAction("guizmo.translate", "Guizmo: Translate",
		{input::key::W, Modifiers::None},
		[this] () -> void {
			if (auto* vp = activeViewport(); vp != nullptr && getState() == State::Edit &&
				(vp->isFocused() || vp->isHovered()) && !gui::Guizmo::isUsing())
				vp->setGuizmoType(gui::Guizmo::Type::Translation);
		});
	m_actionRegistry.registerAction("guizmo.rotate", "Guizmo: Rotate",
		{input::key::E, Modifiers::None},
		[this] () -> void {
			if (auto* vp = activeViewport(); vp != nullptr && getState() == State::Edit &&
				(vp->isFocused() || vp->isHovered()) && !gui::Guizmo::isUsing())
				vp->setGuizmoType(gui::Guizmo::Type::Rotation);
		});
	m_actionRegistry.registerAction("guizmo.scale", "Guizmo: Scale",
		{input::key::R, Modifiers::None},
		[this] () -> void {
			if (auto* vp = activeViewport(); vp != nullptr && getState() == State::Edit &&
				(vp->isFocused() || vp->isHovered()) && !gui::Guizmo::isUsing())
				vp->setGuizmoType(gui::Guizmo::Type::Scale);
		});
	m_actionRegistry.registerAction("guizmo.all", "Guizmo: All",
		{input::key::T, Modifiers::None},
		[this] () -> void {
			if (auto* vp = activeViewport(); vp != nullptr && getState() == State::Edit &&
				(vp->isFocused() || vp->isHovered()) && !gui::Guizmo::isUsing())
				vp->setGuizmoType(gui::Guizmo::Type::All);
		});
	// Playback actions
	m_actionRegistry.registerAction("scene.play", "Play/Resume",
		{input::key::F5, Modifiers::None},
		[this] () -> void {
			if (getState() == State::Edit)
				onScenePlay();
			else if (getState() == State::Pause)
				onSceneResume();
		});
	m_actionRegistry.registerAction("scene.pause", "Pause",
		{input::key::F6, Modifiers::None},
		[this] () -> void { if (getState() == State::Play) onScenePause(); });
	m_actionRegistry.registerAction("scene.stop", "Stop",
		{input::key::F7, Modifiers::None},
		[this] () -> void { if (getState() == State::Play || getState() == State::Pause) onSceneStop(); });
	m_actionRegistry.registerAction("scene.step", "Step Frame",
		{input::key::F8, Modifiers::None},
		[this] () -> void { if (getState() == State::Pause) onSceneStep(); });
	// Entity actions
	m_actionRegistry.registerAction("entity.delete", "Delete Entity",
		{input::key::Delete, Modifiers::None},
		[this] () -> void {
			if (getState() != State::Edit) return;
			auto* doc = activeSceneDocument();
			if (doc == nullptr) return;
			if (auto ent = getSelectedEntity(); ent) {
				doc->undoManager().push(mkUniq<commands::DeleteEntityCommand>(ent));
				doc->getActiveScene()->destroyEntity(ent);
				setSelectedEntity({});
			}
		});
	// Undo/Redo
	m_actionRegistry.registerAction("edit.undo", "Undo",
		{input::key::Z, Modifiers::Ctrl},
		[this] () -> void { performUndo(); });
	m_actionRegistry.registerAction("edit.redo", "Redo",
		{input::key::Y, Modifiers::Ctrl},
		[this] () -> void { performRedo(); });
	// Document-level shortcuts
	m_actionRegistry.registerAction("doc.close", "Close Document",
		{input::key::W, Modifiers::Ctrl},
		[this] () -> void { requestCloseActiveDocument(); });
	m_actionRegistry.registerAction("doc.next", "Next Document",
		{input::key::Tab, Modifiers::Ctrl},
		[this] () -> void {
			const auto& docs = m_documents.list();
			if (docs.size() < 2) return;
			auto* current = m_documents.getActive();
			for (size_t i = 0; i < docs.size(); ++i) {
				if (docs[i].get() == current) {
					m_documents.setActive(docs[(i + 1) % docs.size()].get());
					syncActiveDocumentPanels();
					return;
				}
			}
		});
	m_actionRegistry.registerAction("doc.prev", "Previous Document",
		{input::key::Tab, Modifiers::Ctrl | Modifiers::Shift},
		[this] () -> void {
			const auto& docs = m_documents.list();
			if (docs.size() < 2) return;
			auto* current = m_documents.getActive();
			for (size_t i = 0; i < docs.size(); ++i) {
				if (docs[i].get() == current) {
					m_documents.setActive(docs[(i + docs.size() - 1) % docs.size()].get());
					syncActiveDocumentPanels();
					return;
				}
			}
		});
	m_actionRegistry.registerAction("help.context", "Help",
		{input::key::F1, Modifiers::None},
		[this] () -> void { onContextualHelp(); });
	// clang-format on

	// Apply saved keybinding overrides
	m_actionRegistry.loadOverrides(m_settings.keybindingOverrides);

	buildRibbon();
	if (auto ui = core::Application::get().getImGuiLayer(); ui != nullptr)
		ui->setTopBarCallback([this] () -> void {
			m_ribbon.onRender();
			renderRecentProjectsPopup();
		});

	m_contentBrowser.attach();
	m_contentBrowser.setSceneOpenCallback([this](const std::filesystem::path& iPath) -> void { openScene(iPath); });
	m_contentBrowser.setCodeOpenCallback([this](const std::filesystem::path& iPath) -> void { openCodeFile(iPath); });
	m_contentBrowser.setNodeGraphOpenCallback(
			[this](const std::filesystem::path& iPath) -> void { openNodeGraphFile(iPath); });
	m_contentBrowser.setAnimationOpenCallback(
			[this](const std::filesystem::path& iPath) -> void { openAnimationFile(iPath); });
	newScene();
}

void EditorLayer::onDetach() {
	OWL_PROFILE_FUNCTION()

	// Sync keybinding overrides before saving
	m_settings.keybindingOverrides = m_actionRegistry.getOverrides();
	m_settings.saveToFile(core::Application::get().getWorkingDirectory() / "OwlNest_settings.yml");

	m_contentBrowser.detach();
	OWL_TRACE("EditorLayer: deleted editor FrameBuffer.")

	if (auto ui = core::Application::get().getImGuiLayer(); ui != nullptr)
		ui->setTopBarCallback({});
	m_ribbon.clear();

	m_documents.clear();
	OWL_TRACE("EditorLayer: closed all documents (and their viewports).")
}

void EditorLayer::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	// Render any queued font-preview thumbnails before scene/document updates so the next
	// inspector frame can sample them. Safe here: no Renderer2D scene is active yet.
	gui::FontPreviewCache::get().pumpPending();

	// Inactive documents in Play mode still advance their simulation without rendering —
	// lets a scene keep running in the background while the user edits another tab.
	auto* activeDoc = activeSceneDocument();
	for (const auto& docPtr: m_documents.list()) {
		if (!docPtr || docPtr->type() != DocumentType::Scene)
			continue;
		auto* scene = static_cast<SceneDocument*>(docPtr.get());
		if (scene == activeDoc)
			continue;
		if (scene->state() != SceneDocument::State::Play)
			continue;
		if (const auto& s = scene->getActiveScene(); s) {
			if (s->status == scene::Scene::Status::Editing)
				s->onStartRuntime();
			s->onUpdateRuntime(iTimeStep, /*iRender=*/false);
			scene->applyPendingTeleportVelocity();
		}
	}

	if (activeDoc == nullptr)
		return;

	auto& viewport = activeDoc->getViewport();
	m_cameraController.onResize(viewport.getSize());
	if (const auto& scene = activeDoc->getActiveScene())
		scene->onViewportResize(viewport.getSize());

	// Update scene
	if (activeDoc->state() == SceneDocument::State::Edit) {
		if (viewport.isFocused())
			m_cameraController.onUpdate(iTimeStep);
	}

	// After a cross-level teleport, the new scene is in Editing state.
	// Start its runtime and apply the pending velocity.
	const auto& activeScene = activeDoc->getActiveScene();
	if ((activeDoc->state() == SceneDocument::State::Play ||
		 activeDoc->state() == SceneDocument::State::Pause) &&
		activeScene && activeScene->status == scene::Scene::Status::Editing) {
		activeScene->onStartRuntime();
		activeDoc->applyPendingTeleportVelocity();
	}

	// Handle deferred quit request from Lua (scene.quit()).
	if (activeDoc->isStopRequested()) {
		activeDoc->clearStopRequest();
		if (activeDoc->state() == SceneDocument::State::Play ||
			activeDoc->state() == SceneDocument::State::Pause)
			onSceneStop();
		return;
	}

	// Drive the active document's scene update (which renders into its own framebuffer).
	activeDoc->onUpdate(iTimeStep);
}

void EditorLayer::onEvent(event::Event& ioEvent) {
	m_cameraController.onEvent(ioEvent);
	if (auto* doc = activeSceneDocument(); doc != nullptr)
		doc->onEvent(ioEvent);

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::KeyPressedEvent>(
			[this]<typename T0>(T0&& ioPh1) -> auto { return onKeyPressed(std::forward<T0>(ioPh1)); });
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[]<typename T0>(T0&& ioPh1) -> auto { return onMouseButtonPressed(std::forward<T0>(ioPh1)); });
	dispatcher.dispatch<event::FileDropEvent>([this](const event::FileDropEvent& iEvent) -> bool {
		m_contentBrowser.handleFileDrop(iEvent.getPaths());
		return true;
	});
}

void EditorLayer::onImGuiRender(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	// Drain any deferred close requests from the previous frame.  Closing happens here — after the
	// prior frame's ImGui render has been submitted — so no live ImGui draw list references the
	// freed color-attachment textures.
	if (!m_deferredCloseIds.empty()) {
		auto ids = std::move(m_deferredCloseIds);
		m_deferredCloseIds.clear();
		for (const auto id: ids)
			closeDocument(id);
	}

	// The ribbon is drawn from the UiLayer top-bar callback registered in onAttach.
	// ==================================================================
	renderStats(iTimeStep);
	//=============================================================
	// Each scene document renders its own viewport (ImGui's docking groups them as tabs when
	// docked into the same node; users can tear one off to see scenes side-by-side). Render
	// viewports first so tab-focus changes update the active document before the global panels
	// (hierarchy, content browser) reflect it.
	const auto* activeBefore = m_documents.getActive();
	// Snapshot pointers before iterating — a document may spawn a new document during its render
	// (e.g. a drag-drop on the code editor opens a new tab), which would reallocate the underlying
	// vector and invalidate range-for iterators.
	std::vector<Document*> renderList;
	renderList.reserve(m_documents.list().size());
	for (const auto& docPtr: m_documents.list()) {
		if (docPtr)
			renderList.push_back(docPtr.get());
	}
	std::vector<core::UUID> toClose;
	for (auto* doc: renderList) {
		doc->onImGuiRender();
		// Detect a click on the tab's close X — each document type exposes its own `isOpen`.
		if (doc->type() == DocumentType::Scene) {
			auto* scene = static_cast<SceneDocument*>(doc);
			if (!scene->getViewport().isOpen()) {
				toClose.push_back(scene->id());
				scene->getViewport().setOpen(true);// reset — the actual close may be cancelled
			}
		} else if (doc->type() == DocumentType::Code) {
			auto* code = static_cast<CodeEditorDocument*>(doc);
			if (!code->isOpen()) {
				toClose.push_back(code->id());
				code->setOpen(true);
			}
		} else if (doc->type() == DocumentType::NodeGraph) {
			auto* graph = static_cast<NodeGraphDocument*>(doc);
			if (!graph->isOpen()) {
				toClose.push_back(graph->id());
				graph->setOpen(true);
			}
		}
	}
	if (m_documents.getActive() != activeBefore)
		syncActiveDocumentPanels();
	for (const auto id: toClose)
		requestCloseDocument(id);
	renderCloseDocumentModal();
	//=============================================================
	m_sceneHierarchy.onImGuiRender();
	m_contentBrowser.onImGuiRender();
	m_parameters.onImGuiRender();
	m_projectSettings.onImGuiRender();
	if (m_projectSettings.hasResult()) {
		m_project = m_projectSettings.consumeResult();
		saveProject();
		updateWindowTitle();
	}
	m_logPanel.onImGuiRender();
	m_settingsPanel.onImGuiRender(m_settings, m_actionRegistry);
	m_helpPanel.onImGuiRender(iTimeStep);
	m_asyncProgress.onImGuiRender();
	renderWelcomeScreen();
	renderPackWizardModal();
	renderPackValidationModal();
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

		const auto& iconBank = gui::IconBank::instance();
		if (iconBank.iconButton("project", "New Project...", {ImGui::GetFontSize() * 8.f, 0}))
			newProject();
		ImGui::SameLine();
		if (iconBank.iconButton("open", "Open Project...", {ImGui::GetFontSize() * 8.f, 0}))
			openProject();
		ImGui::SameLine();
		if (iconBank.iconButton("info", "Getting Started", {ImGui::GetFontSize() * 8.f, 0}))
			m_helpPanel.open("getting_started");

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
	if (const auto* vp = activeViewport(); vp != nullptr) {
		if (const auto ent = vp->getHoveredEntity(); ent && ent.hasComponent<scene::component::Tag>())
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
	const auto vpSize = activeViewportSize();
	ImGui::Text("Viewport size: %u x %u", vpSize.x(), vpSize.y());
	ImGui::Text("Aspect ratio: %f", static_cast<double>(vpSize.ratio()));
	ImGui::End();
}

void EditorLayer::buildRibbon() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;

	const auto shortcut = [this](const char* iActionId) -> std::string {
		return m_actionRegistry.getShortcutString(iActionId);
	};
	const auto tipWithShortcut = [&](std::string_view iBase, const char* iActionId) -> std::string {
		const auto sc = shortcut(iActionId);
		return sc.empty() ? std::string{iBase} : std::format("{} ({})", iBase, sc);
	};

	m_ribbon.clear();

	// =============================== File tab ===============================
	const auto fileTab = m_ribbon.addTab("File");
	m_ribbon.setTabHighlighted(fileTab, true);
	const auto gProject = m_ribbon.addGroup(fileTab, "Project");
	m_ribbon.addButton(fileTab, gProject, Button{.iconName = "new_folder", .label = "New",
												 .tooltip = "Create a new project",
												 .onClick = [this] () -> void { newProject(); },
												 .size = Size::Large});
	m_ribbon.addButton(fileTab, gProject, Button{.iconName = "open", .label = "Open",
												 .tooltip = "Open an existing project",
												 .onClick = [this] () -> void { openProject(); },
												 .size = Size::Large});
	m_ribbon.addButton(fileTab, gProject, Button{.iconName = "save", .label = "Save",
												 .tooltip = "Save the current project",
												 .isEnabled = [this] () -> bool { return m_project.isLoaded(); },
												 .onClick = [this] () -> void { saveProject(); },
												 .size = Size::Small});
	m_ribbon.addButton(fileTab, gProject, Button{.iconName = "save", .label = "Save As",
												 .tooltip = "Duplicate the current project to a new folder",
												 .isEnabled = [this] () -> bool { return m_project.isLoaded(); },
												 .onClick = [this] () -> void { saveProjectAs(); },
												 .size = Size::Small});
	m_ribbon.addButton(fileTab, gProject, Button{.iconName = "close", .label = "Close",
												 .tooltip = "Close the current project",
												 .isEnabled = [this] () -> bool { return m_project.isLoaded(); },
												 .onClick = [this] () -> void { closeProject(); },
												 .size = Size::Small});
	const auto gRecent = m_ribbon.addGroup(fileTab, "Recent");
	m_ribbon.addButton(fileTab, gRecent, Button{.iconName = "open", .label = "Recent",
												.tooltip = "Open a recently-used project",
												.isEnabled = [this] () -> bool { return !m_settings.recentProjects.empty(); },
												.onClick = [this] () -> void { m_openRecentProjectsPopup = true; },
												.size = Size::Large});
	const auto gPack = m_ribbon.addGroup(fileTab, "Package");
	m_ribbon.addButton(fileTab, gPack, Button{.iconName = "pack", .label = "Pack Game",
											  .tooltip = "Package the current project as a standalone game",
											  .isEnabled = [this] () -> bool { return m_project.isLoaded(); },
											  .onClick = [this] () -> void { packGame(); },
											  .size = Size::Large});
	const auto gViews = m_ribbon.addGroup(fileTab, "Views");
	m_ribbon.addButton(fileTab, gViews, Button{.iconName = "open", .label = "Scene Flow",
											   .tooltip = "Show the project scenes as a graph (teleport links)",
											   .isEnabled = [this] () -> bool { return m_project.isLoaded(); },
											   .onClick = [this] () -> void { openSceneFlowView(); },
											   .size = Size::Large});
	const auto gAssets = m_ribbon.addGroup(fileTab, "Assets");
	m_ribbon.addButton(fileTab, gAssets, Button{.iconName = "new_scene", .label = "New Animation",
												.tooltip = "Create a new spritesheet animation clip (.owlanim)",
												.onClick = [this] () -> void { newAnimationClip(); },
												.size = Size::Large});
	const auto gHelp = m_ribbon.addGroup(fileTab, "Help");
	m_ribbon.addButton(fileTab, gHelp, Button{.iconName = "info", .label = "Help",
											  .tooltip = tipWithShortcut("Open the editor help", "help.context"),
											  .onClick = [this] () -> void { m_helpPanel.open(); },
											  .size = Size::Large});
	const auto gExit = m_ribbon.addGroup(fileTab, "Session");
	m_ribbon.addButton(fileTab, gExit, Button{.iconName = "exit", .label = "Exit",
											  .tooltip = "Quit Owl Nest",
											  .onClick = [] () -> void { core::Application::get().close(); },
											  .size = Size::Large});

	// =============================== Edit tab ===============================
	const auto editTab = m_ribbon.addTab("Edit");
	const auto gHistory = m_ribbon.addGroup(editTab, "History");
	m_ribbon.addButton(editTab, gHistory, Button{.iconName = "undo", .label = "Undo",
												 .tooltip = tipWithShortcut("Undo", "edit.undo"),
												 .isEnabled = [this] () -> bool {
													 const auto* u = activeUndoManager();
													 return u != nullptr && u->canUndo() && getState() == State::Edit;
												 },
												 .onClick = [this] () -> void { performUndo(); },
												 .size = Size::Large});
	m_ribbon.addButton(editTab, gHistory, Button{.iconName = "redo", .label = "Redo",
												 .tooltip = tipWithShortcut("Redo", "edit.redo"),
												 .isEnabled = [this] () -> bool {
													 const auto* u = activeUndoManager();
													 return u != nullptr && u->canRedo() && getState() == State::Edit;
												 },
												 .onClick = [this] () -> void { performRedo(); },
												 .size = Size::Large});
	const auto gSettings = m_ribbon.addGroup(editTab, "Settings");
	m_ribbon.addButton(editTab, gSettings, Button{.iconName = "settings", .label = "Engine",
												  .tooltip = "Open engine settings",
												  .onClick = [this] () -> void { m_parameters.open(); },
												  .size = Size::Small});
	m_ribbon.addButton(editTab, gSettings, Button{.iconName = "settings", .label = "Editor",
												  .tooltip = "Open editor settings",
												  .onClick = [this] () -> void { m_settingsPanel.open(); },
												  .size = Size::Small});
	m_ribbon.addButton(editTab, gSettings, Button{.iconName = "settings", .label = "Project",
												  .tooltip = "Open project settings",
												  .isEnabled = [this] () -> bool { return m_project.isLoaded(); },
												  .onClick = [this] () -> void { m_projectSettings.open(m_project); },
												  .size = Size::Small});

	// =============================== Contextual tab =========================
	if (const auto* active = m_documents.getActive(); active != nullptr) {
		if (active->type() == DocumentType::Code)
			buildCodeTab();
		else if (active->type() == DocumentType::NodeGraph)
			buildNodeGraphTab();
		else if (active->type() == DocumentType::Animation)
			buildAnimationTab();
		else
			buildSceneTab();
		m_lastRibbonDocType = active->type();
	} else {
		// No document open: show the scene tab anyway so new-scene actions stay reachable.
		buildSceneTab();
		m_lastRibbonDocType = DocumentType::Scene;
	}
}

void EditorLayer::buildSceneTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto tipWithShortcut = [this](std::string_view iBase, const char* iActionId) -> std::string {
		const auto sc = m_actionRegistry.getShortcutString(iActionId);
		return sc.empty() ? std::string{iBase} : std::format("{} ({})", iBase, sc);
	};
	const auto sceneTab = m_ribbon.addTab("Scene");
	const auto gScene = m_ribbon.addGroup(sceneTab, "File");
	m_ribbon.addButton(sceneTab, gScene, Button{.iconName = "new_scene", .label = "New",
												.tooltip = tipWithShortcut("New Scene", "scene.new"),
												.isEnabled = [this] () -> bool { return m_project.isLoaded(); },
												.onClick = [this] () -> void { newScene(); },
												.size = Size::Large});
	m_ribbon.addButton(sceneTab, gScene, Button{.iconName = "open", .label = "Open",
												.tooltip = tipWithShortcut("Open Scene", "scene.open"),
												.isEnabled = [this] () -> bool { return m_project.isLoaded(); },
												.onClick = [this] () -> void { openScene(); },
												.size = Size::Large});
	m_ribbon.addButton(sceneTab, gScene, Button{.iconName = "save", .label = "Save",
												.tooltip = tipWithShortcut("Save Scene", "scene.save"),
												.isEnabled = [this] () -> bool {
													const auto* d = activeSceneDocument();
													return d != nullptr && !d->filePath().empty();
												},
												.onClick = [this] () -> void { saveCurrentScene(); },
												.size = Size::Small});
	m_ribbon.addButton(sceneTab, gScene, Button{.iconName = "save", .label = "Save As",
												.tooltip = tipWithShortcut("Save Scene As…", "scene.saveAs"),
												.isEnabled = [this] () -> bool { return activeSceneDocument() != nullptr; },
												.onClick = [this] () -> void { saveSceneAs(); },
												.size = Size::Small});
	m_ribbon.addButton(sceneTab, gScene, Button{.iconName = "import_file", .label = "Import",
												.tooltip = "Import an existing scene file into this project",
												.isEnabled = [this] () -> bool { return m_project.isLoaded(); },
												.onClick = [this] () -> void { importScene(); },
												.size = Size::Small});
	m_ribbon.addButton(sceneTab, gScene, Button{.iconName = "close", .label = "Close",
												.tooltip = tipWithShortcut("Close Scene", "doc.close"),
												.isEnabled = [this] () -> bool { return activeSceneDocument() != nullptr; },
												.onClick = [this] () -> void { requestCloseActiveDocument(); },
												.size = Size::Large});

	const auto gPlay = m_ribbon.addGroup(sceneTab, "Playback");
	m_ribbon.addButton(sceneTab, gPlay, Button{.iconName = "PlayButton", .label = "Play",
											   .tooltip = tipWithShortcut("Play / Resume", "scene.play"),
											   .isEnabled = [this] () -> bool {
												   return activeSceneDocument() != nullptr &&
														  getState() != State::Play;
											   },
											   .onClick = [this] () -> void {
												   if (getState() == State::Edit)
													   onScenePlay();
												   else if (getState() == State::Pause)
													   onSceneResume();
											   },
											   .size = Size::Large});
	m_ribbon.addButton(sceneTab, gPlay, Button{.iconName = "StopButton", .label = "Stop",
											   .tooltip = tipWithShortcut("Stop", "scene.stop"),
											   .isEnabled = [this] () -> bool { return getState() != State::Edit; },
											   .onClick = [this] () -> void { onSceneStop(); },
											   .size = Size::Large});
	m_ribbon.addButton(sceneTab, gPlay, Button{.iconName = "PauseButton", .label = "Pause",
											   .tooltip = tipWithShortcut("Pause", "scene.pause"),
											   .isEnabled = [this] () -> bool { return getState() == State::Play; },
											   .onClick = [this] () -> void { onScenePause(); },
											   .size = Size::Small});
	m_ribbon.addButton(sceneTab, gPlay, Button{.iconName = "StepButton", .label = "Step",
											   .tooltip = tipWithShortcut("Step Frame", "scene.step"),
											   .isEnabled = [this] () -> bool { return getState() == State::Pause; },
											   .onClick = [this] () -> void { onSceneStep(); },
											   .size = Size::Small});

	const auto gGizmo = m_ribbon.addGroup(sceneTab, "Gizmo");
	const auto gizmoButton = [this](gui::Guizmo::Type iType, const char* iIcon, const char* iLabel,
									const char* iShortcutAction) -> Button {
		return Button{.iconName = iIcon, .label = iLabel,
					  .tooltip = std::format("{} ({})", iLabel, m_actionRegistry.getShortcutString(iShortcutAction)),
					  .isEnabled = [this] () -> bool { return activeViewport() != nullptr && getState() == State::Edit; },
					  .isChecked = [this, iType] () -> bool {
						  auto* vp = activeViewport();
						  return vp != nullptr && vp->getGuizmoType() == iType;
					  },
					  .onClick = [this, iType] () -> void {
						  if (auto* vp = activeViewport(); vp != nullptr)
							  vp->setGuizmoType(vp->getGuizmoType() == iType ? gui::Guizmo::Type::None : iType);
					  },
					  .size = Size::Large};
	};
	m_ribbon.addButton(sceneTab, gGizmo,
					   gizmoButton(gui::Guizmo::Type::Translation, "ctrl_translation", "Translate", "guizmo.translate"));
	m_ribbon.addButton(sceneTab, gGizmo,
					   gizmoButton(gui::Guizmo::Type::Rotation, "ctrl_rotation", "Rotate", "guizmo.rotate"));
	m_ribbon.addButton(sceneTab, gGizmo,
					   gizmoButton(gui::Guizmo::Type::Scale, "ctrl_scale", "Scale", "guizmo.scale"));

	const auto gScenePack = m_ribbon.addGroup(sceneTab, "Package");
	m_ribbon.addButton(sceneTab, gScenePack, Button{.iconName = "pack", .label = "Pack Scene",
													.tooltip = "Package this scene as a standalone .owlpack",
													.isEnabled = [this] () -> bool {
														const auto* d = activeSceneDocument();
														return d != nullptr && !d->filePath().empty();
													},
													.onClick = [this] () -> void { packScene(); },
													.size = Size::Large});
}

void EditorLayer::buildCodeTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto tipWithShortcut = [this](std::string_view iBase, const char* iActionId) -> std::string {
		const auto sc = m_actionRegistry.getShortcutString(iActionId);
		return sc.empty() ? std::string{iBase} : std::format("{} ({})", iBase, sc);
	};
	const auto codeTab = m_ribbon.addTab("Text");
	const auto gFile = m_ribbon.addGroup(codeTab, "File");
	m_ribbon.addButton(codeTab, gFile, Button{.iconName = "save", .label = "Save",
											  .tooltip = tipWithShortcut("Save", "scene.save"),
											  .isEnabled = [this] () -> bool {
												  const auto* d = m_documents.getActive();
												  return d != nullptr && d->type() == DocumentType::Code &&
														 !d->filePath().empty();
											  },
											  .onClick = [this] () -> void {
												  if (auto* d = m_documents.getActive();
													  d != nullptr && d->type() == DocumentType::Code)
													  std::ignore = d->save();
											  },
											  .size = Size::Large});
	m_ribbon.addButton(codeTab, gFile, Button{.iconName = "close", .label = "Close",
											  .tooltip = tipWithShortcut("Close Document", "doc.close"),
											  .isEnabled = [this] () -> bool {
												  const auto* d = m_documents.getActive();
												  return d != nullptr && d->type() == DocumentType::Code;
											  },
											  .onClick = [this] () -> void { requestCloseActiveDocument(); },
											  .size = Size::Large});
	const auto gPreview = m_ribbon.addGroup(codeTab, "Preview");
	m_ribbon.addButton(codeTab, gPreview,
					   Button{.iconName = "preview",
							  .label = "Preview",
							  .tooltip = "Toggle the live preview pane (Markdown / SVG)",
							  .isEnabled = [this] () -> bool {
								  auto* d = dynamic_cast<CodeEditorDocument*>(m_documents.getActive());
								  return d != nullptr && d->canShowPreview();
							  },
							  .onClick = [this] () -> void {
								  if (auto* d = dynamic_cast<CodeEditorDocument*>(m_documents.getActive()))
									  d->togglePreview();
							  },
							  .size = Size::Large});
}

void EditorLayer::buildNodeGraphTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto tipWithShortcut = [this](std::string_view iBase, const char* iActionId) -> std::string {
		const auto sc = m_actionRegistry.getShortcutString(iActionId);
		return sc.empty() ? std::string{iBase} : std::format("{} ({})", iBase, sc);
	};
	const auto graphTab = m_ribbon.addTab("Graph");
	const auto gFile = m_ribbon.addGroup(graphTab, "File");
	m_ribbon.addButton(graphTab, gFile,
					   Button{.iconName = "save", .label = "Save",
							  .tooltip = tipWithShortcut("Save", "scene.save"),
							  .isEnabled = [this] () -> bool {
								  const auto* d = m_documents.getActive();
								  return d != nullptr && d->type() == DocumentType::NodeGraph &&
										 !d->filePath().empty();
							  },
							  .onClick = [this] () -> void {
								  if (auto* d = m_documents.getActive();
									  d != nullptr && d->type() == DocumentType::NodeGraph)
									  std::ignore = d->save();
							  },
							  .size = Size::Large});
	m_ribbon.addButton(graphTab, gFile,
					   Button{.iconName = "close", .label = "Close",
							  .tooltip = tipWithShortcut("Close Document", "doc.close"),
							  .isEnabled = [this] () -> bool {
								  const auto* d = m_documents.getActive();
								  return d != nullptr && d->type() == DocumentType::NodeGraph;
							  },
							  .onClick = [this] () -> void { requestCloseActiveDocument(); },
							  .size = Size::Large});
}

void EditorLayer::buildAnimationTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto tipWithShortcut = [this](std::string_view iBase, const char* iActionId) -> std::string {
		const auto sc = m_actionRegistry.getShortcutString(iActionId);
		return sc.empty() ? std::string{iBase} : std::format("{} ({})", iBase, sc);
	};
	const auto activeAnim = [this] () -> AnimationDocument* {
		auto* d = m_documents.getActive();
		if (d != nullptr && d->type() == DocumentType::Animation)
			return static_cast<AnimationDocument*>(d);
		return nullptr;
	};

	const auto animTab = m_ribbon.addTab("Animation");
	const auto gPlay = m_ribbon.addGroup(animTab, "Playback");
	m_ribbon.addButton(animTab, gPlay,
					   Button{.iconName = "PlayButton", .label = "Play",
							  .tooltip = "Resume preview playback",
							  .isEnabled = [activeAnim] () -> bool {
								  auto* a = activeAnim();
								  return a != nullptr && !a->isPlaying();
							  },
							  .onClick = [activeAnim] () -> void {
								  if (auto* a = activeAnim())
									  a->play();
							  },
							  .size = Size::Large});
	m_ribbon.addButton(animTab, gPlay,
					   Button{.iconName = "PauseButton", .label = "Pause",
							  .tooltip = "Pause preview playback",
							  .isEnabled = [activeAnim] () -> bool {
								  auto* a = activeAnim();
								  return a != nullptr && a->isPlaying();
							  },
							  .onClick = [activeAnim] () -> void {
								  if (auto* a = activeAnim())
									  a->pause();
							  },
							  .size = Size::Small});
	m_ribbon.addButton(animTab, gPlay,
					   Button{.iconName = "StopButton", .label = "Stop",
							  .tooltip = "Stop and rewind to first frame",
							  .isEnabled = [activeAnim] () -> bool { return activeAnim() != nullptr; },
							  .onClick = [activeAnim] () -> void {
								  if (auto* a = activeAnim())
									  a->stop();
							  },
							  .size = Size::Small});

	const auto gFrame = m_ribbon.addGroup(animTab, "Frame");
	m_ribbon.addButton(animTab, gFrame,
					   Button{.iconName = "back", .label = "Previous",
							  .tooltip = "Step back one frame",
							  .isEnabled = [activeAnim] () -> bool { return activeAnim() != nullptr; },
							  .onClick = [activeAnim] () -> void {
								  if (auto* a = activeAnim())
									  a->stepPrevious();
							  },
							  .size = Size::Small});
	m_ribbon.addButton(animTab, gFrame,
					   Button{.iconName = "StepButton", .label = "Next",
							  .tooltip = "Step forward one frame",
							  .isEnabled = [activeAnim] () -> bool { return activeAnim() != nullptr; },
							  .onClick = [activeAnim] () -> void {
								  if (auto* a = activeAnim())
									  a->stepNext();
							  },
							  .size = Size::Small});

	const auto gFile = m_ribbon.addGroup(animTab, "File");
	m_ribbon.addButton(animTab, gFile,
					   Button{.iconName = "save", .label = "Save",
							  .tooltip = tipWithShortcut("Save", "scene.save"),
							  .isEnabled = [this] () -> bool {
								  const auto* d = m_documents.getActive();
								  return d != nullptr && d->type() == DocumentType::Animation &&
										 !d->filePath().empty();
							  },
							  .onClick = [this] () -> void {
								  if (auto* d = m_documents.getActive();
									  d != nullptr && d->type() == DocumentType::Animation)
									  std::ignore = d->save();
							  },
							  .size = Size::Large});
	m_ribbon.addButton(animTab, gFile,
					   Button{.iconName = "save", .label = "Save As",
							  .tooltip = "Save the clip to a new file",
							  .isEnabled = [this] () -> bool {
								  const auto* d = m_documents.getActive();
								  return d != nullptr && d->type() == DocumentType::Animation;
							  },
							  .onClick = [this] () -> void {
								  if (const auto path = core::utils::FileDialog::saveFile(
											  "Owl Animation (*.owlanim)|owlanim\n");
									  !path.empty()) {
									  if (auto* d = m_documents.getActive();
										  d != nullptr && d->type() == DocumentType::Animation)
										  std::ignore = d->saveAs(path);
								  }
							  },
							  .size = Size::Small});
	m_ribbon.addButton(animTab, gFile,
					   Button{.iconName = "close", .label = "Close",
							  .tooltip = tipWithShortcut("Close Document", "doc.close"),
							  .isEnabled = [this] () -> bool {
								  const auto* d = m_documents.getActive();
								  return d != nullptr && d->type() == DocumentType::Animation;
							  },
							  .onClick = [this] () -> void { requestCloseActiveDocument(); },
							  .size = Size::Large});
}

void EditorLayer::refreshRibbonForActiveDoc() {
	const auto* active = m_documents.getActive();
	const auto currentType =
			active != nullptr ? std::optional{active->type()} : std::optional<DocumentType>{DocumentType::Scene};
	if (currentType == m_lastRibbonDocType)
		return;
	buildRibbon();
}

void EditorLayer::newScene() {
	// Reuse an already-empty untitled tab when possible; otherwise create a new one.
	SceneDocument* target = nullptr;
	if (auto* current = activeSceneDocument();
		current != nullptr && current->filePath().empty() && !current->isDirty())
		target = current;
	if (target == nullptr) {
		auto doc = mkUniq<SceneDocument>();
		doc->onAttach(this);
		target = static_cast<SceneDocument*>(m_documents.add(std::move(doc)));
	}
	target->newScene(activeViewportSize());
	m_documents.setActive(target);
	syncActiveDocumentPanels();
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

	// If this scene is already open in a tab, just make it active.
	if (auto* existing = findSceneDocumentByPath(iScenePath); existing != nullptr) {
		m_documents.setActive(existing);
		syncActiveDocumentPanels();
		return;
	}

	if (getState() != State::Edit)
		onSceneStop();

	// Read file on background thread, deserialize on main thread in callback.
	auto state = mkShared<AsyncProgressState>();
	state->setMessage("Loading scene...");
	m_asyncProgress.open("Loading Scene...", state, false);

	auto fileData = mkShared<std::vector<uint8_t>>();

	core::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, scenePath = iScenePath, fileData]() -> void {
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
			[this, state, scenePath = iScenePath, fileData]() -> void {
				if (state->hasError.load()) {
					state->completed.store(true);
					return;
				}
				const auto newScene = mkShared<scene::Scene>();
				if (const scene::SceneSerializer serializer(newScene);
					serializer.deserializeFromBuffer(*fileData, scenePath.string())) {
					// Create a new SceneDocument tab for this scene; if the current active doc is
					// an empty Untitled scene (no path, no undo history) reuse it instead.
					SceneDocument* target = nullptr;
					if (auto* current = activeSceneDocument();
						current != nullptr && current->filePath().empty() && !current->isDirty())
						target = current;
					if (target == nullptr) {
						auto doc = mkUniq<SceneDocument>();
						doc->onAttach(this);
						target = static_cast<SceneDocument*>(m_documents.add(std::move(doc)));
					}
					target->applyLoadedScene(newScene, scenePath, activeViewportSize());
					m_documents.setActive(target);
					syncActiveDocumentPanels();
				} else {
					state->setError("Failed to deserialize scene.");
				}
				state->completed.store(true);
			}));
}

void EditorLayer::openCodeFile(const std::filesystem::path& iPath) {
	if (iPath.empty() || !exists(iPath))
		return;
	// If already open, re-activate the existing tab.
	for (const auto& docPtr: m_documents.list()) {
		if (docPtr && docPtr->type() == DocumentType::Code && docPtr->filePath() == iPath) {
			m_documents.setActive(docPtr.get());
			syncActiveDocumentPanels();
			return;
		}
	}
	auto doc = mkUniq<CodeEditorDocument>();
	doc->onAttach(this);
	if (!doc->loadFromFile(iPath)) {
		OWL_CORE_WARN("Failed to open code file: {}", iPath.string())
		doc->onDetach();
		return;
	}
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

void EditorLayer::openSceneFlowView() {
	if (!m_project.isLoaded())
		return;
	// Re-use an already-open Scene Flow document if present.
	for (const auto& docPtr: m_documents.list()) {
		if (auto* graph = dynamic_cast<SceneFlowDocument*>(docPtr.get())) {
			graph->refreshFromProject(m_project);
			m_documents.setActive(graph);
			syncActiveDocumentPanels();
			return;
		}
	}
	auto doc = mkUniq<SceneFlowDocument>();
	doc->onAttach(this);
	doc->refreshFromProject(m_project);
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

void EditorLayer::openNodeGraphFile(const std::filesystem::path& iPath) {
	if (iPath.empty() || !exists(iPath))
		return;
	// If already open, re-activate the existing tab.
	for (const auto& docPtr: m_documents.list()) {
		if (docPtr && docPtr->type() == DocumentType::NodeGraph && docPtr->filePath() == iPath) {
			m_documents.setActive(docPtr.get());
			syncActiveDocumentPanels();
			return;
		}
	}
	auto doc = mkUniq<NodeGraphDocument>();
	doc->onAttach(this);
	if (!doc->loadFromFile(iPath)) {
		OWL_CORE_WARN("Failed to open node-graph file: {}", iPath.string())
		doc->onDetach();
		return;
	}
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

void EditorLayer::openAnimationFile(const std::filesystem::path& iPath) {
	if (iPath.empty() || !exists(iPath))
		return;
	for (const auto& docPtr: m_documents.list()) {
		if (docPtr && docPtr->type() == DocumentType::Animation && docPtr->filePath() == iPath) {
			m_documents.setActive(docPtr.get());
			syncActiveDocumentPanels();
			return;
		}
	}
	auto doc = mkUniq<AnimationDocument>();
	doc->onAttach(this);
	if (!doc->loadFromFile(iPath)) {
		OWL_CORE_WARN("Failed to open animation file: {}", iPath.string())
		doc->onDetach();
		return;
	}
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

void EditorLayer::newAnimationClip() {
	auto doc = mkUniq<AnimationDocument>();
	doc->onAttach(this);
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

auto EditorLayer::loadOrOpenSceneDocument(const std::filesystem::path& iScenePath) -> SceneDocument* {
	if (iScenePath.empty())
		return nullptr;
	if (auto* existing = findSceneDocumentByPath(iScenePath); existing != nullptr)
		return existing;
	if (!std::filesystem::exists(iScenePath))
		return nullptr;

	// Synchronous load: small files, single user-initiated action context. Bypasses the async
	// progress modal so editor-side tooling (Scene Flow link edits) does not flash a UI dialog
	// for every Trigger create/delete.
	std::ifstream file(iScenePath, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		OWL_CORE_WARN("loadOrOpenSceneDocument: cannot open '{}'", iScenePath.string())
		return nullptr;
	}
	const auto size = static_cast<size_t>(file.tellg());
	file.seekg(0);
	std::vector<uint8_t> bytes(size);
	file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
	const auto newScene = mkShared<scene::Scene>();
	const scene::SceneSerializer serializer(newScene);
	if (!serializer.deserializeFromBuffer(bytes, iScenePath.string())) {
		OWL_CORE_WARN("loadOrOpenSceneDocument: failed to deserialize '{}'", iScenePath.string())
		return nullptr;
	}
	auto doc = mkUniq<SceneDocument>();
	doc->onAttach(this);
	auto* target = static_cast<SceneDocument*>(m_documents.add(std::move(doc)));
	target->applyLoadedScene(newScene, iScenePath, activeViewportSize());
	// Intentionally do NOT call setActive() — Scene Flow edits should leave the user's focus
	// on the document they were inspecting.
	return target;
}

void EditorLayer::saveSceneAs() {
	if (const auto filepath = core::utils::FileDialog::saveFile("Owl Scene (*.owl)|owl\n"); !filepath.empty())
		saveSceneAs(filepath);
}

void EditorLayer::saveSceneAs(const std::filesystem::path& iScenePath) {
	auto* doc = activeSceneDocument();
	if (doc == nullptr || !doc->getActiveScene())
		return;

	// Serialize to string on main thread (read-only, safe), write file on background thread.
	const scene::SceneSerializer serializer(doc->getActiveScene());
	auto yamlData = mkShared<std::string>(serializer.serializeToString());
	doc->setScenePath(iScenePath);

	auto state = mkShared<AsyncProgressState>();
	state->setMessage("Saving scene...");
	state->progress.store(0.5f);
	m_asyncProgress.open("Saving...", state, false);

	core::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, yamlData, path = iScenePath]() -> void {
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
			[state]() -> void { state->completed.store(true); }));
	updateWindowTitle();
}

void EditorLayer::saveCurrentScene() {
	auto* doc = activeSceneDocument();
	if (doc == nullptr)
		return;
	const auto path = doc->filePath();
	if (path.empty())
		saveSceneAs();
	else
		saveSceneAs(path);
	doc->undoManager().markSaved();
}

auto EditorLayer::onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool {
	return m_actionRegistry.dispatch(ioEvent);
}

auto EditorLayer::onMouseButtonPressed([[maybe_unused]] const event::MouseButtonPressedEvent& ioEvent) -> bool {
	// noting yet
	return false;
}

void EditorLayer::onScenePlay() {
	auto* doc = activeSceneDocument();
	if (doc == nullptr)
		return;
	doc->onScenePlay();
	m_sceneHierarchy.setContext(doc->getActiveScene());
}

void EditorLayer::onScenePause() {
	if (auto* doc = activeSceneDocument(); doc != nullptr)
		doc->onScenePause();
}

void EditorLayer::onSceneResume() {
	if (auto* doc = activeSceneDocument(); doc != nullptr)
		doc->onSceneResume();
}

void EditorLayer::onSceneStop() {
	auto* doc = activeSceneDocument();
	if (doc == nullptr)
		return;
	doc->onSceneStop();
	m_sceneHierarchy.setContext(doc->getActiveScene());
}

void EditorLayer::onSceneStep() {
	if (auto* doc = activeSceneDocument(); doc != nullptr)
		doc->onSceneStep();
}

auto EditorLayer::consumeStepRequest() -> bool {
	auto* doc = activeSceneDocument();
	return doc != nullptr && doc->consumeStepRequest();
}

void EditorLayer::handleTeleportRequest() {
	auto* doc = activeSceneDocument();
	if (doc == nullptr)
		return;
	doc->handleTeleportRequest(activeViewportSize());
	m_sceneHierarchy.setContext(doc->getActiveScene());
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
	auto* doc = activeSceneDocument();
	if (doc == nullptr)
		return;
	doc->handleSaveLoadRequest(activeViewportSize());
	m_sceneHierarchy.setContext(doc->getActiveScene());
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
	if (getState() == State::Edit)
		saveCurrentScene();
	m_project.saveToFile(m_project.projectDirectory / "owl_project.yml");
}

void EditorLayer::saveProjectAs() {
	if (!m_project.isLoaded())
		return;
	const auto dest = core::utils::FileDialog::pickFolder();
	if (dest.empty())
		return;
	if (std::filesystem::weakly_canonical(dest) == std::filesystem::weakly_canonical(m_project.projectDirectory)) {
		OWL_CORE_WARN("Save Project As: destination is the current project directory — ignored")
		return;
	}
	std::error_code ec;
	if (!exists(dest))
		create_directories(dest, ec);
	// Flush the current yml + active scene into the source tree before copying.
	if (getState() == State::Edit)
		saveCurrentScene();
	m_project.saveToFile(m_project.projectDirectory / "owl_project.yml");
	std::filesystem::copy(m_project.projectDirectory, dest,
						  std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
						  ec);
	if (ec) {
		OWL_CORE_ERROR("Save Project As: copy failed — {}", ec.message())
		return;
	}
	closeProject();
	openProject(dest);
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
	const auto* doc = activeSceneDocument();
	const auto* const dirty = (doc != nullptr && doc->isDirty()) ? " *" : "";
	if (m_project.isLoaded())
		app.setWindowTitle(std::format("{} - {}{}", app.getInitParams().name, m_project.name, dirty));
	else
		app.setWindowTitle(std::format("{}{}", app.getInitParams().name, dirty));
}

void EditorLayer::onDuplicateEntity() {
	auto* doc = activeSceneDocument();
	if (doc == nullptr || doc->state() != SceneDocument::State::Edit)
		return;

	if (const scene::Entity selectedEntity = m_sceneHierarchy.getSelectedEntity(); selectedEntity) {
		auto dup = doc->getEditorScene()->duplicateEntity(selectedEntity);
		doc->undoManager().push(mkUniq<commands::DuplicateEntityCommand>(selectedEntity, dup));
	}
}

void EditorLayer::performUndo() {
	auto* doc = activeSceneDocument();
	if (doc == nullptr || doc->state() != SceneDocument::State::Edit || !doc->getActiveScene())
		return;
	auto& undo = doc->undoManager();
	undo.undo(*doc->getActiveScene());
	if (const auto hint = undo.lastSelectionHint(); hint != core::UUID{0}) {
		if (auto entity = doc->getActiveScene()->findEntityByUUID(hint); entity)
			setSelectedEntity(entity);
	}
}

void EditorLayer::performRedo() {
	auto* doc = activeSceneDocument();
	if (doc == nullptr || doc->state() != SceneDocument::State::Edit || !doc->getActiveScene())
		return;
	auto& undo = doc->undoManager();
	undo.redo(*doc->getActiveScene());
	if (const auto hint = undo.lastSelectionHint(); hint != core::UUID{0}) {
		if (auto entity = doc->getActiveScene()->findEntityByUUID(hint); entity)
			setSelectedEntity(entity);
	}
}

auto EditorLayer::getSelectedEntity() const -> scene::Entity { return m_sceneHierarchy.getSelectedEntity(); }

void EditorLayer::setSelectedEntity(const scene::Entity iEntity) { m_sceneHierarchy.setSelectedEntity(iEntity); }

void EditorLayer::packScene() {
	const auto* doc = activeSceneDocument();
	if (doc == nullptr || doc->filePath().empty() || m_asyncProgress.isActive())
		return;

	const auto destPath = core::utils::FileDialog::saveFile("Owl Scene Pack (*.owlpack)|owlpack\n");
	if (destPath.empty())
		return;

	auto outputPath = destPath;
	if (outputPath.extension() != ".owlpack")
		outputPath.replace_extension(".owlpack");

	// Snapshot for thread safety.
	const auto scenePath = doc->filePath();
	const auto sceneFilename = scenePath.filename().string();

	auto state = mkShared<AsyncProgressState>();
	m_asyncProgress.open("Packing Scene...", state, true);

	core::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, scenePath, sceneFilename, outputPath]() -> void {
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
						[&state](const uint32_t iCurrent, const uint32_t iTotal) -> void {
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
				std::error_code sizeEc;
				const auto packBytes = exists(outputPath) ? std::filesystem::file_size(outputPath, sizeEc) : 0;
				const auto mib = static_cast<double>(packBytes) / (1024.0 * 1024.0);
				state->setMessage(std::format("Packed {} assets ({:.2f} MiB)\nOutput: {}", assets.size(), mib,
											  outputPath.string()));
				OWL_CORE_INFO("Scene packed: {} ({} assets, {:.2f} MiB) -> {}", sceneFilename, assets.size(),
							  mib, outputPath.string())
			},
			[state]() -> void { state->completed.store(true); }));
}

namespace {

void copySharedLibs(const std::filesystem::path& iSrcDir, const std::filesystem::path& iDestDir) {
	std::error_code ec;
	for (const auto& entry: std::filesystem::directory_iterator(iSrcDir, ec)) {
		if (!entry.is_regular_file() && !entry.is_symlink())
			continue;
		const auto ext = entry.path().extension().string();
		const auto filename = entry.path().filename().string();
		// Copy .so files (Linux) and .dll files (Windows).
		if (ext == ".so" || filename.find(".so.") != std::string::npos || ext == ".dll") {
			const auto dest = iDestDir / entry.path().filename();
			if (!exists(dest)) {
				std::error_code copyEc;
				std::filesystem::copy(entry.path(), dest, std::filesystem::copy_options::copy_symlinks, copyEc);
				if (copyEc)
					OWL_CORE_WARN("Failed to copy {}: {}", entry.path().string(), copyEc.message())
			}
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
	std::error_code ec;
	std::filesystem::permissions(iGameDir / "launch.sh",
								 std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
										 std::filesystem::perms::others_exec,
								 std::filesystem::perm_options::add, ec);
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
	if (!m_project.isLoaded() || m_asyncProgress.isActive() || m_showPackValidation || m_showPackWizard)
		return;
	// Open the packaging wizard to let the user choose destination + options.
	m_pendingPackDestDir.clear();
	m_showPackWizard = true;
}

void EditorLayer::renderPackWizardModal() {
	if (!m_showPackWizard)
		return;
	if (!ImGui::IsPopupOpen("Packaging Wizard"))
		ImGui::OpenPopup("Packaging Wizard");

	const auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Always);

	if (ImGui::BeginPopupModal("Packaging Wizard", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Pack Game");
		ImGui::TextWrapped("Project: %s  (version: %s)", m_project.name.c_str(),
						   m_project.version.empty() ? "-" : m_project.version.c_str());
		ImGui::Spacing();
		ImGui::Separator();

		// Target platform (read-only — current build platform).
		const char* platform =
#ifdef OWL_PLATFORM_WINDOWS
				"Windows (x64)";
#else
				"Linux (x64)";
#endif
		ImGui::Text("Target platform: %s", platform);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip("The pack is built for the current platform. Cross-compilation is planned for v0.3.");

		// Destination folder with Browse button.
		ImGui::Spacing();
		ImGui::Text("Output folder:");
		auto destStr = m_pendingPackDestDir.string();
		ImGui::SetNextItemWidth(-120);
		if (ImGui::InputText("##dest", &destStr))
			m_pendingPackDestDir = std::filesystem::path(destStr);
		ImGui::SameLine();
		if (gui::IconBank::instance().iconButton("open", "Browse...##packDest")) {
			const auto picked = core::utils::FileDialog::pickFolder();
			if (!picked.empty())
				m_pendingPackDestDir = picked;
		}

		// Options.
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Options");
		ImGui::Checkbox("Compress pack (zstd)", &m_packCompress);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip("Compress asset blobs with zstd. Smaller output at the cost of a slower pack.");
		ImGui::Checkbox("Obfuscate TOC (XOR)", &m_packObfuscate);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip("XOR-obfuscate the table of contents to deter casual pack inspection.");

		ImGui::Spacing();
		ImGui::Separator();
		const bool canStart = !m_pendingPackDestDir.empty();
		const auto& iconBank = gui::IconBank::instance();
		ImGui::BeginDisabled(!canStart);
		if (iconBank.iconButton("pack", "Start Packaging", {ImGui::GetFontSize() * 10.f, 0})) {
			m_showPackWizard = false;
			ImGui::CloseCurrentPopup();
			launchPackValidation();
		}
		ImGui::EndDisabled();
		if (!canStart && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip("Choose an output folder to enable packaging.");
		ImGui::SameLine();
		if (iconBank.iconButton("close", "Cancel##packWiz", {ImGui::GetFontSize() * 7.f, 0})) {
			m_showPackWizard = false;
			m_pendingPackDestDir.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void EditorLayer::launchPackValidation() {
	m_pendingPackWarnings.clear();

	// Launch async validation: scan project + collect warnings off the main thread.
	auto state = mkShared<AsyncProgressState>();
	state->setMessage("Validating project...");
	state->progress.store(0.3f);
	m_asyncProgress.open("Validating...", state, false);

	const auto projectDir = m_project.projectDirectory;
	const auto firstScene = m_project.firstScene;
	auto warningsOut = mkShared<std::vector<std::string>>();
	auto assetsOut = mkShared<std::vector<io::pack::AssetReference>>();
	const auto runnerSrcDir = core::Application::get().getWorkingDirectory();

	core::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, projectDir, firstScene, warningsOut, assetsOut, runnerSrcDir]() -> void {
				*assetsOut = io::pack::AssetScanner::scanProject(projectDir, firstScene, warningsOut.get());
				state->progress.store(0.8f);
				bool hasRunner = false;
				for (const auto& candidate: {"OwlRunner", "OwlRunner.exe"}) {
					if (exists(runnerSrcDir / candidate)) {
						hasRunner = true;
						break;
					}
				}
				if (!hasRunner)
					warningsOut->emplace_back(
							"OwlRunner executable not found — packed game will not be playable.");
				if (assetsOut->empty())
					warningsOut->emplace_back("No assets to pack — check firstScene and its references.");
				state->progress.store(1.0f);
			},
			[this, state, warningsOut, assetsOut]() -> void {
				state->completed.store(true);
				m_asyncProgress.close();
				m_pendingPackWarnings = std::move(*warningsOut);
				m_pendingPackAssets = assetsOut;
				if (m_pendingPackWarnings.empty())
					startPackGame();
				else
					m_showPackValidation = true;
			}));
}

void EditorLayer::renderPackValidationModal() {
	if (!m_showPackValidation)
		return;
	if (!ImGui::IsPopupOpen("Packaging Validation"))
		ImGui::OpenPopup("Packaging Validation");

	const auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Always);

	if (ImGui::BeginPopupModal("Packaging Validation", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%zu issue(s) found before packaging:",
						   m_pendingPackWarnings.size());
		ImGui::Separator();
		ImGui::BeginChild("##packWarnings", ImVec2(0, 200), ImGuiChildFlags_Borders);
		for (const auto& warning: m_pendingPackWarnings)
			ImGui::BulletText("%s", warning.c_str());
		ImGui::EndChild();
		ImGui::TextDisabled("The game may be missing assets or not playable. Proceed anyway?");
		ImGui::Spacing();
		const auto& iconBank = gui::IconBank::instance();
		if (iconBank.iconButton("pack", "Proceed anyway", {ImGui::GetFontSize() * 10.f, 0})) {
			m_showPackValidation = false;
			ImGui::CloseCurrentPopup();
			startPackGame();
		}
		ImGui::SameLine();
		if (iconBank.iconButton("close", "Cancel", {ImGui::GetFontSize() * 7.f, 0})) {
			m_showPackValidation = false;
			m_pendingPackWarnings.clear();
			m_pendingPackDestDir.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void EditorLayer::startPackGame() {
	const auto destDir = m_pendingPackDestDir;
	m_pendingPackDestDir.clear();
	m_pendingPackWarnings.clear();

	// Reuse the pre-scanned assets from the validation pass (avoids a double scan).
	auto preScanned = m_pendingPackAssets;
	m_pendingPackAssets.reset();

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
	if (std::error_code ec; !std::filesystem::create_directories(gameDir, ec) && ec) {
		OWL_CORE_ERROR("Pack: cannot create output directory '{}': {}", gameDir.string(), ec.message())
		// Open an error modal so the user sees the failure instead of a silent abort.
		auto errState = mkShared<AsyncProgressState>();
		errState->setError(std::format("Cannot create output directory '{}': {}.\n"
									   "Tip: make sure the parent path contains only directories, "
									   "not a file with the same name as the output folder.",
									   gameDir.string(), ec.message()));
		m_asyncProgress.open("Packaging Error", errState, false);
		return;
	}

	// Create shared progress state and open the modal.
	auto state = mkShared<AsyncProgressState>();
	m_asyncProgress.open("Packing Game...", state, true);

	// Push async task to the scheduler.
	core::Application::get().getTaskScheduler().pushTask(core::task::Task(
			// --- Worker function (runs on thread pool) ---
			[state, gameName, gameDir, projectDir, firstScene, projectName, projectVersion, projectAuthor, projectDesc,
			 projectIcon, windowCfg, runnerSrcDir, preScanned, compress = m_packCompress,
			 obfuscate = m_packObfuscate]() -> void {
				const auto startTime = std::chrono::steady_clock::now();
				const auto packFlags =
						(compress ? io::pack::PackFlags::Compressed : io::pack::PackFlags::None) |
						(obfuscate ? io::pack::PackFlags::Obfuscated : io::pack::PackFlags::None);
				// Phase 1: Use pre-scanned assets (from validation) or re-scan if missing.
				std::vector<io::pack::AssetReference> assets;
				if (preScanned && !preScanned->empty()) {
					assets = *preScanned;
				} else {
					state->setMessage("Scanning assets...");
					assets = io::pack::AssetScanner::scanProject(projectDir, firstScene);
				}
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
						packPath, packFlags,
						[&state](const uint32_t iCurrent, const uint32_t iTotal) -> void {
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
				{
					std::error_code ec;
					std::filesystem::copy_file(runnerExe, destExe,
											   std::filesystem::copy_options::overwrite_existing, ec);
					if (ec) {
						state->setError(std::format("Failed to copy runner: {}", ec.message()));
						return;
					}
				}
#ifdef OWL_PLATFORM_LINUX
				{
					std::error_code ec;
					std::filesystem::permissions(destExe,
												 std::filesystem::perms::owner_exec |
														 std::filesystem::perms::group_exec |
														 std::filesystem::perms::others_exec,
												 std::filesystem::perm_options::add, ec);
					if (ec)
						OWL_CORE_WARN("Failed to set exec permissions on {}: {}", destExe.string(),
									  ec.message())
				}
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

				// Compute post-pack report: duration, pack size, output path.
				const auto endTime = std::chrono::steady_clock::now();
				const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
												endTime - startTime).count();
				const auto packFile = gameDir / (gameName + ".owlpack");
				std::error_code sizeEc;
				const auto packBytes = exists(packFile) ? std::filesystem::file_size(packFile, sizeEc) : 0;
				const auto mib = static_cast<double>(packBytes) / (1024.0 * 1024.0);
				state->setMessage(std::format("Packed {} assets ({:.2f} MiB)\n"
											  "Output: {}\n"
											  "Duration: {:.1f}s",
											  assets.size(), mib, gameDir.string(),
											  static_cast<double>(durationMs) / 1000.0));
				OWL_CORE_INFO("Game exported: {} ({} assets, {:.2f} MiB) -> {} in {:.1f}s",
							  projectName, assets.size(), mib, gameDir.string(),
							  static_cast<double>(durationMs) / 1000.0)
			},
			// --- Termination callback (runs on main thread) ---
			[state]() -> void { state->completed.store(true); }));
}

void EditorLayer::instantiatePrefab(const std::filesystem::path& iPrefabPath, const std::string& iAssetRelativePath) {
	auto* doc = activeSceneDocument();
	if (doc == nullptr || doc->state() != SceneDocument::State::Edit || !doc->getActiveScene())
		return;
	const auto& activeScene = doc->getActiveScene();
	auto root = scene::PrefabSerializer::instantiate(iPrefabPath, activeScene, iAssetRelativePath);
	if (!root) {
		OWL_WARN("Failed to instantiate prefab: {}", iPrefabPath.string())
		return;
	}
	const auto info = scene::PrefabSerializer::readInfo(iPrefabPath);
	const auto name = info.has_value() ? info->name : iPrefabPath.stem().string();
	doc->undoManager().push(mkUniq<commands::InstantiatePrefabCommand>(root, *activeScene, name));
	setSelectedEntity(root);
}

void EditorLayer::onContextualHelp() {
	// Mapping from component type name (component::T::name()) to the help page id that best
	// documents that area. Unmapped or no-hover → editor overview.
	static const std::unordered_map<std::string, std::string> kComponentToPage = {
		{"Transform", "scene"},      {"Hierarchy", "scene"},
		{"Visibility", "scene"},     {"Tag", "scene"},
		{"PhysicBody", "physics"},   {"Trigger", "scene"},
		{"LuaScript", "scripting"},  {"NativeScript", "scripting"},
		{"Camera", "scene"},         {"SoundSource", "sound"},
		{"SoundListener", "sound"},  {"SpriteRenderer", "renderer"},
		{"AnimatedSpriteRenderer", "renderer"}, {"CircleRenderer", "renderer"},
		{"BackgroundTexture", "renderer"}, {"Text", "renderer"},
		{"Player", "scene"},         {"PrefabLink", "scene"},
		{"EntityLink", "scene"},     {"Canvas", "editor"},
		{"UIRect", "editor"},        {"UIText", "editor"},
		{"UIImage", "editor"},       {"UIButton", "editor"},
		{"UIPanel", "editor"},       {"UIProgressBar", "editor"},
		{"UISlider", "editor"},
	};
	const auto& hovered = panel::SceneHierarchy::lastHoveredComponentName();
	if (const auto it = kComponentToPage.find(hovered); it != kComponentToPage.end()) {
		m_helpPanel.open(it->second);
		return;
	}
	m_helpPanel.open();
}

}// namespace owl::nest
