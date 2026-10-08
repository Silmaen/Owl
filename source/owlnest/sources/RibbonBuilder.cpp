/**
 * @file RibbonBuilder.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "RibbonBuilder.h"

#include "EditorLayer.h"
#include "document/AnimationDocument.h"
#include "document/CodeEditorDocument.h"
#include "document/NodeGraphDocument.h"
#include "document/TilemapDocument.h"
#include "document/TilesetDocument.h"

#include <gui/IconBank.h>

#include <format>
#include <string>
#include <string_view>
#include <tuple>

namespace owl::nest {

namespace {
using State = SceneDocument::State;
}// namespace

void RibbonBuilder::build() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto shortcut = [this](const char* iActionId) -> std::string {
		return mp_editor->m_actionRegistry.getShortcutString(iActionId);
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
	m_ribbon.addButton(fileTab, gProject,
					   Button{.iconName = "new_folder",
							  .label = "New",
							  .tooltip = "Create a new project",
							  .onClick = [this]() -> void { mp_editor->newProject(); },
							  .size = Size::Large});
	m_ribbon.addButton(fileTab, gProject,
					   Button{.iconName = "open",
							  .label = "Open",
							  .tooltip = "Open an existing project",
							  .onClick = [this]() -> void { mp_editor->openProject(); },
							  .size = Size::Large});
	m_ribbon.addButton(fileTab, gProject,
					   Button{.iconName = "save",
							  .label = "Save",
							  .tooltip = "Save the current project",
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .onClick = [this]() -> void { mp_editor->saveProject(); },
							  .size = Size::Small});
	m_ribbon.addButton(fileTab, gProject,
					   Button{.iconName = "save",
							  .label = "Save As",
							  .tooltip = "Duplicate the current project to a new folder",
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .onClick = [this]() -> void { mp_editor->saveProjectAs(); },
							  .size = Size::Small});
	m_ribbon.addButton(fileTab, gProject,
					   Button{.iconName = "close",
							  .label = "Close",
							  .tooltip = "Close the current project",
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .onClick = [this]() -> void { mp_editor->closeProject(); },
							  .size = Size::Small});
	const auto gObject = m_ribbon.addGroup(fileTab, "Object");
	m_ribbon.addButton(fileTab, gObject,
					   Button{.iconName = "add_entity",
							  .label = "New",
							  .tooltip = "Create a new untitled document — pick the kind in the popup.",
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .size = Size::Large,
							  .popupContents = [this]() -> void {
								  if (gui::IconBank::instance().menuItem("comp_canvas", "New Scene"))
									  mp_editor->newScene();
								  if (gui::IconBank::instance().menuItem("comp_animated_sprite", "New Animation"))
									  mp_editor->newAnimationClip();
								  if (gui::IconBank::instance().menuItem("owltilemap_icon", "New Tilemap"))
									  mp_editor->newTilemapAsset();
								  if (gui::IconBank::instance().menuItem("owltileset_icon", "New Tileset"))
									  mp_editor->newTilesetAsset();
							  }});
	const auto gRecent = m_ribbon.addGroup(fileTab, "Recent");
	m_ribbon.addButton(fileTab, gRecent,
					   Button{.iconName = "open",
							  .label = "Recent",
							  .tooltip = "Open a recently-used project",
							  .isEnabled = [this]() -> bool { return !mp_editor->m_settings.recentProjects.empty(); },
							  .onClick = [this]() -> void { mp_editor->m_openRecentProjectsPopup = true; },
							  .size = Size::Large});
	const auto gPack = m_ribbon.addGroup(fileTab, "Package");
	m_ribbon.addButton(fileTab, gPack,
					   Button{.iconName = "pack",
							  .label = "Pack Game",
							  .tooltip = "Package the current project as a standalone game",
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .onClick = [this]() -> void { mp_editor->packGame(); },
							  .size = Size::Large});
	const auto gViews = m_ribbon.addGroup(fileTab, "Views");
	m_ribbon.addButton(fileTab, gViews,
					   Button{.iconName = "open",
							  .label = "Scene Flow",
							  .tooltip = "Show the project scenes as a graph (teleport links)",
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .onClick = [this]() -> void { mp_editor->openSceneFlowView(); },
							  .size = Size::Large});
	const auto gAssets = m_ribbon.addGroup(fileTab, "Assets");
	m_ribbon.addButton(fileTab, gAssets,
					   Button{.iconName = "new_scene",
							  .label = "New Animation",
							  .tooltip = "Create a new spritesheet animation clip (.owlanim)",
							  .onClick = [this]() -> void { mp_editor->newAnimationClip(); },
							  .size = Size::Small});
	m_ribbon.addButton(fileTab, gAssets,
					   Button{.iconName = "new_scene",
							  .label = "New Tilemap",
							  .tooltip = "Create a new tilemap level asset (.owltilemap)",
							  .onClick = [this]() -> void { mp_editor->newTilemapAsset(); },
							  .size = Size::Small});
	m_ribbon.addButton(fileTab, gAssets,
					   Button{.iconName = "new_scene",
							  .label = "New Tileset",
							  .tooltip = "Create a new tileset atlas asset (.owltileset)",
							  .onClick = [this]() -> void { mp_editor->newTilesetAsset(); },
							  .size = Size::Small});
	const auto gHelp = m_ribbon.addGroup(fileTab, "Help");
	m_ribbon.addButton(fileTab, gHelp,
					   Button{.iconName = "info",
							  .label = "Help",
							  .tooltip = tipWithShortcut("Open the editor help", "help.context"),
							  .onClick = [this]() -> void { mp_editor->m_helpPanel.open(); },
							  .size = Size::Large});
	const auto gExit = m_ribbon.addGroup(fileTab, "Session");
	m_ribbon.addButton(fileTab, gExit,
					   Button{.iconName = "exit",
							  .label = "Exit",
							  .tooltip = "Quit Owl Nest",
							  .onClick = []() -> void { app::Application::get().close(); },
							  .size = Size::Large});
	// =============================== Edit tab ===============================
	const auto editTab = m_ribbon.addTab("Edit");
	const auto gHistory = m_ribbon.addGroup(editTab, "History");
	m_ribbon.addButton(editTab, gHistory,
					   Button{.iconName = "undo",
							  .label = "Undo",
							  .tooltip = tipWithShortcut("Undo", "edit.undo"),
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->m_documents.getActive();
								  return d != nullptr && d->canUndo();
							  },
							  .onClick = [this]() -> void { mp_editor->performUndo(); },
							  .size = Size::Large});
	m_ribbon.addButton(editTab, gHistory,
					   Button{.iconName = "redo",
							  .label = "Redo",
							  .tooltip = tipWithShortcut("Redo", "edit.redo"),
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->m_documents.getActive();
								  return d != nullptr && d->canRedo();
							  },
							  .onClick = [this]() -> void { mp_editor->performRedo(); },
							  .size = Size::Large});
	const auto gSettings = m_ribbon.addGroup(editTab, "Settings");
	m_ribbon.addButton(editTab, gSettings,
					   Button{.iconName = "settings",
							  .label = "Engine",
							  .tooltip = "Open engine settings",
							  .onClick = [this]() -> void { mp_editor->m_parameters.open(); },
							  .size = Size::Small});
	m_ribbon.addButton(editTab, gSettings,
					   Button{.iconName = "settings",
							  .label = "Editor",
							  .tooltip = "Open editor settings",
							  .onClick = [this]() -> void { mp_editor->m_settingsPanel.open(); },
							  .size = Size::Small});
	m_ribbon.addButton(editTab, gSettings,
					   Button{.iconName = "settings",
							  .label = "Project",
							  .tooltip = "Open project settings",
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .onClick = [this]() -> void { mp_editor->m_projectSettings.open(mp_editor->m_project); },
							  .size = Size::Small});
	// =============================== Contextual tab =========================
	if (const auto* active = mp_editor->m_documents.getActive(); active != nullptr) {
		if (active->type() == DocumentType::Code)

			buildCodeTab();
		else if (active->type() == DocumentType::NodeGraph)

			buildNodeGraphTab();
		else if (active->type() == DocumentType::Animation)

			buildAnimationTab();
		else if (active->type() == DocumentType::Tilemap)

			buildTilemapTab();
		else if (active->type() == DocumentType::Tileset)

			buildTilesetTab();
		else

			buildSceneTab();
		m_lastDocType = active->type();
	} else {
		// No document open: show the scene tab anyway so new-scene actions stay reachable.
		buildSceneTab();
		m_lastDocType = DocumentType::Scene;
	}
}

void RibbonBuilder::buildSceneTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto tipWithShortcut = [this](std::string_view iBase, const char* iActionId) -> std::string {
		const auto sc = mp_editor->m_actionRegistry.getShortcutString(iActionId);
		return sc.empty() ? std::string{iBase} : std::format("{} ({})", iBase, sc);
	};
	const auto sceneTab = m_ribbon.addTab("Scene");
	const auto gScene = m_ribbon.addGroup(sceneTab, "File");
	m_ribbon.addButton(sceneTab, gScene,
					   Button{.iconName = "new_scene",
							  .label = "New",
							  .tooltip = tipWithShortcut("New Scene", "scene.new"),
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .onClick = [this]() -> void { mp_editor->newScene(); },
							  .size = Size::Large});
	m_ribbon.addButton(sceneTab, gScene,
					   Button{.iconName = "open",
							  .label = "Open",
							  .tooltip = tipWithShortcut("Open Scene", "scene.open"),
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .onClick = [this]() -> void { mp_editor->openScene(); },
							  .size = Size::Large});
	m_ribbon.addButton(sceneTab, gScene,
					   Button{.iconName = "save",
							  .label = "Save",
							  .tooltip = tipWithShortcut("Save Scene", "scene.save"),
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->activeSceneDocument();
								  return d != nullptr && !d->filePath().empty();
							  },
							  .onClick = [this]() -> void { mp_editor->saveCurrentScene(); },
							  .size = Size::Small});
	m_ribbon.addButton(sceneTab, gScene,
					   Button{.iconName = "save",
							  .label = "Save As",
							  .tooltip = tipWithShortcut("Save Scene As…", "scene.saveAs"),
							  .isEnabled = [this]() -> bool { return mp_editor->activeSceneDocument() != nullptr; },
							  .onClick = [this]() -> void { mp_editor->saveSceneAs(); },
							  .size = Size::Small});
	m_ribbon.addButton(sceneTab, gScene,
					   Button{.iconName = "import_file",
							  .label = "Import",
							  .tooltip = "Import an existing scene file into this project",
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .onClick = [this]() -> void { mp_editor->importScene(); },
							  .size = Size::Small});
	m_ribbon.addButton(sceneTab, gScene,
					   Button{.iconName = "close",
							  .label = "Close",
							  .tooltip = tipWithShortcut("Close Scene", "doc.close"),
							  .isEnabled = [this]() -> bool { return mp_editor->activeSceneDocument() != nullptr; },
							  .onClick = [this]() -> void { mp_editor->requestCloseActiveDocument(); },
							  .size = Size::Large});
	const auto gPlay = m_ribbon.addGroup(sceneTab, "Playback");
	m_ribbon.addButton(sceneTab, gPlay,
					   Button{.iconName = "PlayButton",
							  .label = "Play",
							  .tooltip = tipWithShortcut("Play / Resume", "scene.play"),
							  .isEnabled = [this]() -> bool {
								  return mp_editor->activeSceneDocument() != nullptr &&

										 mp_editor->getState() != State::Play;
							  },
							  .onClick = [this]() -> void {
								  if (mp_editor->getState() == State::Edit)

									  mp_editor->onScenePlay();
								  else if (mp_editor->getState() == State::Pause)

									  mp_editor->onSceneResume();
							  },
							  .size = Size::Large});
	m_ribbon.addButton(sceneTab, gPlay,
					   Button{.iconName = "StopButton",
							  .label = "Stop",
							  .tooltip = tipWithShortcut("Stop", "scene.stop"),
							  .isEnabled = [this]() -> bool { return mp_editor->getState() != State::Edit; },
							  .onClick = [this]() -> void { mp_editor->onSceneStop(); },
							  .size = Size::Large});
	m_ribbon.addButton(sceneTab, gPlay,
					   Button{.iconName = "PauseButton",
							  .label = "Pause",
							  .tooltip = tipWithShortcut("Pause", "scene.pause"),
							  .isEnabled = [this]() -> bool { return mp_editor->getState() == State::Play; },
							  .onClick = [this]() -> void { mp_editor->onScenePause(); },
							  .size = Size::Small});
	m_ribbon.addButton(sceneTab, gPlay,
					   Button{.iconName = "StepButton",
							  .label = "Step",
							  .tooltip = tipWithShortcut("Step Frame", "scene.step"),
							  .isEnabled = [this]() -> bool { return mp_editor->getState() == State::Pause; },
							  .onClick = [this]() -> void { mp_editor->onSceneStep(); },
							  .size = Size::Small});

	const auto gGizmo = m_ribbon.addGroup(sceneTab, "Gizmo");
	const auto gizmoButton = [this](gui::Guizmo::Type iType, const char* iIcon, const char* iLabel,
									const char* iShortcutAction) -> Button {
		return Button{.iconName = iIcon,
					  .label = iLabel,
					  .tooltip = std::format("{} ({})", iLabel,
											 mp_editor->m_actionRegistry.getShortcutString(iShortcutAction)),
					  .isEnabled = [this]() -> bool {
						  return mp_editor->activeViewport() != nullptr && mp_editor->getState() == State::Edit;
					  },
					  .isChecked = [this, iType]() -> bool {
						  auto* vp = mp_editor->activeViewport();
						  return vp != nullptr && vp->getGuizmoType() == iType;
					  },
					  .onClick = [this, iType]() -> void {
						  if (auto* vp = mp_editor->activeViewport(); vp != nullptr)
							  vp->setGuizmoType(vp->getGuizmoType() == iType ? gui::Guizmo::Type::None : iType);
					  },
					  .size = Size::Large};
	};
	m_ribbon.addButton(
			sceneTab, gGizmo,

			gizmoButton(gui::Guizmo::Type::Translation, "ctrl_translation", "Translate", "guizmo.translate"));
	m_ribbon.addButton(sceneTab, gGizmo,

					   gizmoButton(gui::Guizmo::Type::Rotation, "ctrl_rotation", "Rotate", "guizmo.rotate"));
	m_ribbon.addButton(sceneTab, gGizmo,

					   gizmoButton(gui::Guizmo::Type::Scale, "ctrl_scale", "Scale", "guizmo.scale"));
	m_ribbon.addButton(
			sceneTab, gGizmo,
			Button{.iconName = "snap_grid",
				   .label = "Snap",
				   .tooltip =
						   "Toggle snap-to-grid for translation gizmos. Holding Ctrl during a drag also forces snap.",
				   .isEnabled = [this]() -> bool { return mp_editor->getState() == State::Edit; },
				   .isChecked = [this]() -> bool { return mp_editor->m_settings.snapEnabled; },
				   .onClick = [this]() -> void {
					   mp_editor->m_settings.snapEnabled = !mp_editor->m_settings.snapEnabled;
				   },
				   .size = Size::Small});
	m_ribbon.addButton(sceneTab, gGizmo,
					   Button{.iconName = "snap_grid",
							  .label = "Step",
							  .tooltip = "Pick a snap step preset. With a tilemap in the scene the presets are "
										 "fractions / multiples of the cell size, otherwise raw world units.",
							  .isEnabled = [this]() -> bool { return mp_editor->getState() == State::Edit; },
							  .size = Size::Small,
							  .popupContents = [this]() -> void { mp_editor->renderSnapStepPopup(); }});

	const auto gShow = m_ribbon.addGroup(sceneTab, "Show");
	m_ribbon.addButton(sceneTab, gShow,
					   Button{.iconName = "comp_camera",
							  .label = "Cameras",
							  .tooltip = "Toggle in-viewport camera markers (icon + forward arrow + FOV cone)",
							  .isEnabled = [this]() -> bool { return mp_editor->getState() == State::Edit; },
							  .isChecked = [this]() -> bool { return mp_editor->m_settings.showCameraGizmos; },
							  .onClick = [this]() -> void {
								  mp_editor->m_settings.showCameraGizmos = !mp_editor->m_settings.showCameraGizmos;
							  },
							  .size = Size::Small});

	const auto gSceneSettings = m_ribbon.addGroup(sceneTab, "Settings");
	m_ribbon.addButton(sceneTab, gSceneSettings,
					   Button{.iconName = "settings",
							  .label = "Scene",
							  .tooltip = "Open per-scene settings (renderer overrides, …)",
							  .isEnabled = [this]() -> bool { return mp_editor->activeSceneDocument() != nullptr; },
							  .onClick = [this]() -> void { mp_editor->m_sceneSettings.open(); },
							  .size = Size::Small});

	const auto gScenePack = m_ribbon.addGroup(sceneTab, "Package");
	m_ribbon.addButton(sceneTab, gScenePack,
					   Button{.iconName = "pack",
							  .label = "Pack Scene",
							  .tooltip = "Package this scene as a standalone .owlpack",
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->activeSceneDocument();
								  return d != nullptr && !d->filePath().empty();
							  },
							  .onClick = [this]() -> void { mp_editor->packScene(); },
							  .size = Size::Large});
}

void RibbonBuilder::buildCodeTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto tipWithShortcut = [this](std::string_view iBase, const char* iActionId) -> std::string {
		const auto sc = mp_editor->m_actionRegistry.getShortcutString(iActionId);
		return sc.empty() ? std::string{iBase} : std::format("{} ({})", iBase, sc);
	};
	const auto codeTab = m_ribbon.addTab("Text");
	const auto gFile = m_ribbon.addGroup(codeTab, "File");
	m_ribbon.addButton(codeTab, gFile,
					   Button{.iconName = "save",
							  .label = "Save",
							  .tooltip = tipWithShortcut("Save", "scene.save"),
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->m_documents.getActive();
								  return d != nullptr && d->type() == DocumentType::Code && !d->filePath().empty();
							  },
							  .onClick = [this]() -> void {
								  if (auto* d = mp_editor->m_documents.getActive();
									  d != nullptr && d->type() == DocumentType::Code)
									  std::ignore = d->save();
							  },
							  .size = Size::Large});
	m_ribbon.addButton(codeTab, gFile,
					   Button{.iconName = "close",
							  .label = "Close",
							  .tooltip = tipWithShortcut("Close Document", "doc.close"),
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->m_documents.getActive();
								  return d != nullptr && d->type() == DocumentType::Code;
							  },
							  .onClick = [this]() -> void { mp_editor->requestCloseActiveDocument(); },
							  .size = Size::Large});
	const auto gPreview = m_ribbon.addGroup(codeTab, "Preview");
	m_ribbon.addButton(codeTab, gPreview,
					   Button{.iconName = "preview",
							  .label = "Preview",
							  .tooltip = "Toggle the live preview pane (Markdown / SVG)",
							  .isEnabled = [this]() -> bool {
								  auto* d = dynamic_cast<CodeEditorDocument*>(mp_editor->m_documents.getActive());
								  return d != nullptr && d->canShowPreview();
							  },
							  .onClick = [this]() -> void {
								  if (auto* d = dynamic_cast<CodeEditorDocument*>(mp_editor->m_documents.getActive()))
									  d->togglePreview();
							  },
							  .size = Size::Large});
}

void RibbonBuilder::buildNodeGraphTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto tipWithShortcut = [this](std::string_view iBase, const char* iActionId) -> std::string {
		const auto sc = mp_editor->m_actionRegistry.getShortcutString(iActionId);
		return sc.empty() ? std::string{iBase} : std::format("{} ({})", iBase, sc);
	};
	const auto graphTab = m_ribbon.addTab("Graph");
	const auto gFile = m_ribbon.addGroup(graphTab, "File");
	m_ribbon.addButton(graphTab, gFile,
					   Button{.iconName = "save",
							  .label = "Save",
							  .tooltip = tipWithShortcut("Save", "scene.save"),
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->m_documents.getActive();
								  return d != nullptr && d->type() == DocumentType::NodeGraph && !d->filePath().empty();
							  },
							  .onClick = [this]() -> void {
								  if (auto* d = mp_editor->m_documents.getActive();
									  d != nullptr && d->type() == DocumentType::NodeGraph)
									  std::ignore = d->save();
							  },
							  .size = Size::Large});
	m_ribbon.addButton(graphTab, gFile,
					   Button{.iconName = "close",
							  .label = "Close",
							  .tooltip = tipWithShortcut("Close Document", "doc.close"),
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->m_documents.getActive();
								  return d != nullptr && d->type() == DocumentType::NodeGraph;
							  },
							  .onClick = [this]() -> void { mp_editor->requestCloseActiveDocument(); },
							  .size = Size::Large});
}

void RibbonBuilder::buildAnimationTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto tipWithShortcut = [this](std::string_view iBase, const char* iActionId) -> std::string {
		const auto sc = mp_editor->m_actionRegistry.getShortcutString(iActionId);
		return sc.empty() ? std::string{iBase} : std::format("{} ({})", iBase, sc);
	};
	const auto activeAnim = [this]() -> AnimationDocument* {
		auto* d = mp_editor->m_documents.getActive();
		if (d != nullptr && d->type() == DocumentType::Animation)
			return static_cast<AnimationDocument*>(d);
		return nullptr;
	};
	const auto animTab = m_ribbon.addTab("Animation");
	const auto gPlay = m_ribbon.addGroup(animTab, "Playback");
	m_ribbon.addButton(animTab, gPlay,
					   Button{.iconName = "PlayButton",
							  .label = "Play",
							  .tooltip = "Resume preview playback",
							  .isEnabled = [activeAnim]() -> bool {
								  auto* a = activeAnim();
								  return a != nullptr && !a->isPlaying();
							  },
							  .onClick = [activeAnim]() -> void {
								  if (auto* a = activeAnim())
									  a->play();
							  },
							  .size = Size::Large});
	m_ribbon.addButton(animTab, gPlay,
					   Button{.iconName = "PauseButton",
							  .label = "Pause",
							  .tooltip = "Pause preview playback",
							  .isEnabled = [activeAnim]() -> bool {
								  auto* a = activeAnim();
								  return a != nullptr && a->isPlaying();
							  },
							  .onClick = [activeAnim]() -> void {
								  if (auto* a = activeAnim())
									  a->pause();
							  },
							  .size = Size::Small});
	m_ribbon.addButton(animTab, gPlay,
					   Button{.iconName = "StopButton",
							  .label = "Stop",
							  .tooltip = "Stop and rewind to first frame",
							  .isEnabled = [activeAnim]() -> bool { return activeAnim() != nullptr; },
							  .onClick = [activeAnim]() -> void {
								  if (auto* a = activeAnim())
									  a->stop();
							  },
							  .size = Size::Small});
	const auto gFrame = m_ribbon.addGroup(animTab, "Frame");
	m_ribbon.addButton(animTab, gFrame,
					   Button{.iconName = "back",
							  .label = "Previous",
							  .tooltip = "Step back one frame",
							  .isEnabled = [activeAnim]() -> bool { return activeAnim() != nullptr; },
							  .onClick = [activeAnim]() -> void {
								  if (auto* a = activeAnim())
									  a->stepPrevious();
							  },
							  .size = Size::Small});
	m_ribbon.addButton(animTab, gFrame,
					   Button{.iconName = "StepButton",
							  .label = "Next",
							  .tooltip = "Step forward one frame",
							  .isEnabled = [activeAnim]() -> bool { return activeAnim() != nullptr; },
							  .onClick = [activeAnim]() -> void {
								  if (auto* a = activeAnim())
									  a->stepNext();
							  },
							  .size = Size::Small});
	const auto gFile = m_ribbon.addGroup(animTab, "File");
	m_ribbon.addButton(animTab, gFile,
					   Button{.iconName = "save",
							  .label = "Save",
							  .tooltip = tipWithShortcut("Save", "scene.save"),
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->m_documents.getActive();
								  return d != nullptr && d->type() == DocumentType::Animation && !d->filePath().empty();
							  },
							  .onClick = [this]() -> void {
								  if (auto* d = mp_editor->m_documents.getActive();
									  d != nullptr && d->type() == DocumentType::Animation)
									  std::ignore = d->save();
							  },
							  .size = Size::Large});
	m_ribbon.addButton(animTab, gFile,
					   Button{.iconName = "save",
							  .label = "Save As",
							  .tooltip = "Save the clip to a new file",
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->m_documents.getActive();
								  return d != nullptr && d->type() == DocumentType::Animation;
							  },
							  .onClick = [this]() -> void {
								  auto* d = mp_editor->m_documents.getActive();
								  if (d == nullptr || d->type() != DocumentType::Animation)
									  return;
								  const std::string defaultName = d->filePath().empty()
																		  ? std::string{"Untitled.owlanim"}
																		  : d->filePath().filename().string();
								  if (const auto path = platform::FileDialog::saveFile(
											  "Owl Animation (*.owlanim)|owlanim\n", defaultName);
									  !path.empty())
									  std::ignore = d->saveAs(path);
							  },
							  .size = Size::Small});
	m_ribbon.addButton(animTab, gFile,
					   Button{.iconName = "close",
							  .label = "Close",
							  .tooltip = tipWithShortcut("Close Document", "doc.close"),
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->m_documents.getActive();
								  return d != nullptr && d->type() == DocumentType::Animation;
							  },
							  .onClick = [this]() -> void { mp_editor->requestCloseActiveDocument(); },
							  .size = Size::Large});
}

void RibbonBuilder::buildTilemapTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto activeTilemap = [this]() -> TilemapDocument* {
		auto* d = mp_editor->m_documents.getActive();
		if (d != nullptr && d->type() == DocumentType::Tilemap)
			return static_cast<TilemapDocument*>(d);
		return nullptr;
	};
	const auto tab = m_ribbon.addTab("Tilemap");
	const auto gFile = m_ribbon.addGroup(tab, "File");
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "save",
							  .label = "Save",
							  .tooltip = "Save the .owltilemap (Ctrl+S)",
							  .isEnabled = [activeTilemap]() -> bool {
								  auto* a = activeTilemap();
								  return a != nullptr && !a->filePath().empty();
							  },
							  .onClick = [activeTilemap]() -> void {
								  if (auto* a = activeTilemap())
									  std::ignore = a->save();
							  },
							  .size = Size::Large});
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "save",
							  .label = "Save As",
							  .tooltip = "Save the tilemap to a new file",
							  .isEnabled = [activeTilemap]() -> bool { return activeTilemap() != nullptr; },
							  .onClick = [activeTilemap]() -> void {
								  if (auto* a = activeTilemap()) {
									  const std::string defaultName = a->filePath().empty()
																			  ? std::string{"Untitled.owltilemap"}
																			  : a->filePath().filename().string();
									  if (const auto path = platform::FileDialog::saveFile(
												  "Owl Tilemap (*.owltilemap)|owltilemap\n", defaultName);
										  !path.empty())
										  std::ignore = a->saveAs(path);
								  }
							  },
							  .size = Size::Small});
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "close",
							  .label = "Close",
							  .tooltip = "Close the active document",
							  .isEnabled = [activeTilemap]() -> bool { return activeTilemap() != nullptr; },
							  .onClick = [this]() -> void { mp_editor->requestCloseActiveDocument(); },
							  .size = Size::Small});
}

void RibbonBuilder::buildTilesetTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto activeTileset = [this]() -> TilesetDocument* {
		auto* d = mp_editor->m_documents.getActive();
		if (d != nullptr && d->type() == DocumentType::Tileset)
			return static_cast<TilesetDocument*>(d);
		return nullptr;
	};
	const auto tab = m_ribbon.addTab("Tileset");
	const auto gFile = m_ribbon.addGroup(tab, "File");
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "save",
							  .label = "Save",
							  .tooltip = "Save the .owltileset (Ctrl+S)",
							  .isEnabled = [activeTileset]() -> bool {
								  auto* a = activeTileset();
								  return a != nullptr && !a->filePath().empty();
							  },
							  .onClick = [activeTileset]() -> void {
								  if (auto* a = activeTileset())
									  std::ignore = a->save();
							  },
							  .size = Size::Large});
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "save",
							  .label = "Save As",
							  .tooltip = "Save the tileset to a new file",
							  .isEnabled = [activeTileset]() -> bool { return activeTileset() != nullptr; },
							  .onClick = [activeTileset]() -> void {
								  if (auto* a = activeTileset()) {
									  const std::string defaultName = a->filePath().empty()
																			  ? std::string{"Untitled.owltileset"}
																			  : a->filePath().filename().string();
									  if (const auto path = platform::FileDialog::saveFile(
												  "Owl Tileset (*.owltileset)|owltileset\n", defaultName);
										  !path.empty())
										  std::ignore = a->saveAs(path);
								  }
							  },
							  .size = Size::Small});
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "close",
							  .label = "Close",
							  .tooltip = "Close the active document",
							  .isEnabled = [activeTileset]() -> bool { return activeTileset() != nullptr; },
							  .onClick = [this]() -> void { mp_editor->requestCloseActiveDocument(); },
							  .size = Size::Small});
}

void RibbonBuilder::refresh() {
	const auto* active = mp_editor->m_documents.getActive();
	const auto currentType =
			active != nullptr ? std::optional{active->type()} : std::optional<DocumentType>{DocumentType::Scene};
	if (currentType == m_lastDocType)
		return;
	build();
}

}// namespace owl::nest
