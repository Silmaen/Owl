/**
 * @file RibbonBuilder.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "RibbonBuilder.h"

#include "EditorLayer.h"
#include "EditorResources.h"
#include "document/ActionMapDocument.h"
#include "document/AnimationDocument.h"
#include "document/CodeEditorDocument.h"
#include "document/FontDocument.h"
#include "document/MaterialDocument.h"
#include "document/NodeGraphDocument.h"
#include "document/TilemapDocument.h"
#include "document/TilesetDocument.h"

#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/widgets/Menu.h>

#include <array>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

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
	const auto hasActive = [this]() -> bool { return mp_editor->m_documents.getActive() != nullptr; };
	const auto canUndo = [this]() -> bool {
		const auto* doc = mp_editor->m_documents.getActive();
		return doc != nullptr && doc->canUndo();
	};
	const auto canRedo = [this]() -> bool {
		const auto* doc = mp_editor->m_documents.getActive();
		return doc != nullptr && doc->canRedo();
	};
	const auto projectLoaded = [this]() -> bool { return mp_editor->m_project.isLoaded(); };
	m_ribbon.clear();
	m_ribbon.setBackstage("File", [this]() -> void { mp_editor->m_backstage.open(); });

	// ============================ Quick access =============================
	m_ribbon.setSearchBox("Search commands, assets, entities", "Shift Shift",
						  [this]() -> void { mp_editor->m_commandPalette.open(); });
	m_ribbon.addQuickAccess(Button{.iconName = gui::icon::save,
								   .label = "Save",
								   .tooltip = "Save the active document.",
								   .shortcut = shortcut("scene.save"),
								   .isEnabled = hasActive,
								   .onClick = [this]() -> void { mp_editor->saveActiveDocument(); }});
	m_ribbon.addQuickAccess(Button{.iconName = gui::icon::undo2,
								   .label = "Undo",
								   .tooltip = "Undo the last change of the active document.",
								   .shortcut = shortcut("edit.undo"),
								   .isEnabled = canUndo,
								   .onClick = [this]() -> void { mp_editor->performUndo(); }});
	m_ribbon.addQuickAccess(Button{.iconName = gui::icon::redo2,
								   .label = "Redo",
								   .tooltip = "Redo the change undone last.",
								   .shortcut = shortcut("edit.redo"),
								   .isEnabled = canRedo,
								   .onClick = [this]() -> void { mp_editor->performRedo(); }});

	// =============================== Home ==================================
	const auto home = m_ribbon.addTab("Home");
	const auto gEdit = m_ribbon.addGroup(home, "Edit");
	m_ribbon.addButton(home, gEdit,
					   Button{.iconName = "ribbon_undo",
							  .label = "Undo",
							  .tooltip = "Undo the last change of the active document.",
							  .shortcut = shortcut("edit.undo"),
							  .isEnabled = canUndo,
							  .onClick = [this]() -> void { mp_editor->performUndo(); }});
	m_ribbon.addButton(home, gEdit,
					   Button{.iconName = "ribbon_redo",
							  .label = "Redo",
							  .tooltip = "Redo the change undone last.",
							  .shortcut = shortcut("edit.redo"),
							  .isEnabled = canRedo,
							  .onClick = [this]() -> void { mp_editor->performRedo(); }});
	m_ribbon.addButton(home, gEdit,
					   Button{.iconName = "ribbon_copy",
							  .label = "Duplicate",
							  .tooltip = "Duplicate the selected entity.",
							  .shortcut = shortcut("entity.duplicate"),
							  .isEnabled = [this]() -> bool {
								  return mp_editor->activeSceneDocument() != nullptr &&
										 mp_editor->getState() == State::Edit;
							  },
							  .onClick = [this]() -> void { mp_editor->onDuplicateEntity(); }});
	const auto gCreate = m_ribbon.addGroup(home, "Create");
	m_ribbon.addButton(home, gCreate,
					   Button{.iconName = "ribbon_new",
							  .label = "New",
							  .tooltip = "Create an untitled document: scene, HUD, animation, tilemap or tileset.",
							  .isEnabled = projectLoaded,
							  .size = Size::Large,
							  .popupContents = [this]() -> void {
								  if (gui::IconBank::instance().menuItem("owl_icon", "Scene"))
									  mp_editor->newScene();
								  if (gui::IconBank::instance().menuItem("owlhud_icon", "HUD"))
									  mp_editor->newHud();
								  if (gui::IconBank::instance().menuItem("owlanim_icon", "Animation"))
									  mp_editor->newAnimationClip();
								  if (gui::IconBank::instance().menuItem("owltilemap_icon", "Tilemap"))
									  mp_editor->newTilemapAsset();
								  if (gui::IconBank::instance().menuItem("owltileset_icon", "Tileset"))
									  mp_editor->newTilesetAsset();
							  }});
	const auto gProject = m_ribbon.addGroup(home, "Project");
	m_ribbon.addButton(home, gProject,
					   Button{.iconName = "ribbon_flow",
							  .label = "Scene Flow",
							  .tooltip = "Show the scenes of the project as a graph of their teleport links.",
							  .isEnabled = projectLoaded,
							  .onClick = [this]() -> void { mp_editor->openSceneFlowView(); },
							  .size = Size::Large});
	m_ribbon.addButton(
			home, gProject,
			Button{.iconName = "ribbon_settings",
				   .label = "Settings",
				   .tooltip = "Name, window, asset folders and renderer stack of the project.",
				   .isEnabled = projectLoaded,
				   .onClick = [this]() -> void { mp_editor->m_projectSettings.open(mp_editor->m_project); }});
	m_ribbon.addButton(home, gProject,
					   Button{.iconName = "ribbon_tune",
							  .label = "Input Actions",
							  .tooltip = "Edit the actions of the project and their key, mouse and gamepad bindings.",
							  .isEnabled = projectLoaded,
							  .onClick = [this]() -> void { mp_editor->openActionMapEditor(); }});
	m_ribbon.addButton(home, gProject,
					   Button{.iconName = "ribbon_pack",
							  .label = "Pack Game",
							  .tooltip = "Package the project as a standalone game.",
							  .isEnabled = projectLoaded,
							  .onClick = [this]() -> void { mp_editor->packGame(); }});

	// =============================== View ==================================
	const auto view = m_ribbon.addTab("View");
	const auto gLayout = m_ribbon.addGroup(view, "Layout");
	m_ribbon.addButton(view, gLayout,
					   Button{.iconName = "ribbon_panels",
							  .label = "Hierarchy Left",
							  .tooltip = "Move the Hierarchy to its own column left of the documents.",
							  .shortcut = shortcut("view.layoutHierarchyLeft"),
							  .isChecked = [this]() -> bool {
								  return mp_editor->m_dockLayout.getPreset() == LayoutPreset::HierarchyLeft;
							  },
							  .onClick = [this]() -> void {
								  mp_editor->applyLayoutPreset(mp_editor->m_dockLayout.getPreset() ==
																			   LayoutPreset::HierarchyLeft
																	   ? LayoutPreset::Default
																	   : LayoutPreset::HierarchyLeft);
							  }});
	m_ribbon.addButton(
			view, gLayout,
			Button{.iconName = "ribbon_layout",
				   .label = "Reset Layout",
				   .tooltip = "Put every panel back in its default place.",
				   .onClick = [this]() -> void { mp_editor->applyLayoutPreset(mp_editor->m_dockLayout.getPreset()); }});
	const auto gCamera = m_ribbon.addGroup(view, "Camera");
	const auto inEdit = [this]() -> bool {
		return mp_editor->activeViewport() != nullptr && mp_editor->getState() == State::Edit;
	};
	const auto run = [this](const char* iAction) -> std::function<void()> {
		return [this, iAction]() -> void { std::ignore = mp_editor->runAction(iAction); };
	};
	m_ribbon.addButton(view, gCamera,
					   Button{.iconName = "ribbon_view_reset",
							  .label = "Reset View",
							  .tooltip = "Face the XY plane from +Z, ten units from the origin.",
							  .shortcut = shortcut("view.resetCamera"),
							  .isEnabled = inEdit,
							  .onClick = run("view.resetCamera")});
	m_ribbon.addButton(view, gCamera,
					   Button{.iconName = "ribbon_axes",
							  .label = "Axis Views",
							  .tooltip = "Look at the scene from one side, around the same point.",
							  .isEnabled = inEdit,
							  .popupContents = [this]() -> void {
								  constexpr std::array<std::pair<const char*, const char*>, 6> views{
										  {{"view.cameraFront", "Front (XY)"},
										   {"view.cameraBack", "Back"},
										   {"view.cameraTop", "Top (XZ)"},
										   {"view.cameraBottom", "Bottom"},
										   {"view.cameraRight", "Right (YZ)"},
										   {"view.cameraLeft", "Left"}}};
								  for (const auto& [action, label]: views)
									  if (gui::widgets::menuItem({.glyph = gui::icon::cuboid, .label = label}))
										  std::ignore = mp_editor->runAction(action);
							  }});
	m_ribbon.addButton(view, gCamera,
					   Button{.iconName = "ribbon_frame_scene",
							  .label = "Frame Scene",
							  .tooltip = "Back off until every entity of the scene fits the view.",
							  .shortcut = shortcut("view.frameScene"),
							  .isEnabled = inEdit,
							  .onClick = run("view.frameScene")});
	m_ribbon.addButton(view, gCamera,
					   Button{.iconName = "ribbon_viewpoint",
							  .label = "Camera Viewpoint",
							  .tooltip = "Move the view onto the selected camera entity (not a raycast one).",
							  .isEnabled = [this]() -> bool {
								  const auto* viewport = mp_editor->activeViewport();
								  return viewport != nullptr && mp_editor->getState() == State::Edit &&
										 viewport->canGoToCameraViewpoint();
							  },
							  .onClick = run("view.cameraViewpoint")});
	const auto gPanels = m_ribbon.addGroup(view, "Panels");
	m_ribbon.addButton(view, gPanels,
					   Button{.iconName = "ribbon_stats",
							  .label = "Frame Stats",
							  .tooltip = "Show the frame time and renderer counters.",
							  .isChecked = [this]() -> bool { return mp_editor->m_settings.showStats; },
							  .onClick = [this]() -> void {
								  mp_editor->m_settings.showStats = !mp_editor->m_settings.showStats;
							  }});
	m_ribbon.addButton(view, gPanels,
					   Button{.iconName = "ribbon_help",
							  .label = "Help",
							  .tooltip = "Open the help of the focused panel.",
							  .shortcut = shortcut("help.context"),
							  .onClick = [this]() -> void { mp_editor->onContextualHelp(); }});
	const auto gAppearance = m_ribbon.addGroup(view, "Appearance");
	for (const auto& [preset, name]: gui::Theme::getPresetNames()) {
		m_ribbon.addButton(
				view, gAppearance,
				Button{.iconName = "ribbon_settings",
					   .label = name,
					   .tooltip = std::format("Switch to the {} theme.", name),
					   .isChecked = [this, name]() -> bool { return mp_editor->m_settings.themePreset == name; },
					   .onClick = [this, preset, name]() -> void {
						   mp_editor->m_settings.themePreset = name;
						   utils::applyTheme(gui::Theme::fromPreset(preset));
					   }});
	}
	const auto gSettings = m_ribbon.addGroup(view, "Settings");
	m_ribbon.addButton(view, gSettings,
					   Button{.iconName = "ribbon_settings",
							  .label = "Editor",
							  .tooltip = "Fonts, autosave, snap and keyboard shortcuts of the editor.",
							  .onClick = [this]() -> void { mp_editor->m_settingsPanel.open(); }});
	m_ribbon.addButton(view, gSettings,
					   Button{.iconName = "ribbon_tune",
							  .label = "Engine",
							  .tooltip = "Engine parameters: renderer, physics, profiler.",
							  .onClick = [this]() -> void { mp_editor->m_parameters.open(); }});

	// =========================== Document tab ==============================
	DocumentType type = DocumentType::Scene;
	if (const auto* active = mp_editor->m_documents.getActive(); active != nullptr)
		type = active->type();
	switch (type) {
		case DocumentType::Code:
			buildCodeTab();
			break;
		case DocumentType::NodeGraph:
			buildNodeGraphTab();
			break;
		case DocumentType::Animation:
			buildAnimationTab();
			break;
		case DocumentType::Tilemap:
			buildTilemapTab();
			break;
		case DocumentType::Tileset:
			buildTilesetTab();
			break;
		case DocumentType::Font:
			buildFontTab();
			break;
		case DocumentType::ActionMap:
			buildActionMapTab();
			break;
		case DocumentType::Image:
			buildImageTab();
			break;
		case DocumentType::Mesh:
			buildMeshTab();
			break;
		case DocumentType::Material:
			buildMaterialTab();
			break;
		case DocumentType::Scene:
			buildSceneTab();
			break;
	}
	m_ribbon.setTabHighlighted(m_ribbon.getTabCount() - 1, true);
	if (m_lastDocType != type)
		m_ribbon.selectTab(m_ribbon.getTabLabel(m_ribbon.getTabCount() - 1));
	m_lastDocType = type;
}

void RibbonBuilder::buildSceneTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto tipWithShortcut = [this](std::string_view iBase, const char* iActionId) -> std::string {
		const auto sc = mp_editor->m_actionRegistry.getShortcutString(iActionId);
		return sc.empty() ? std::string{iBase} : std::format("{} ({})", iBase, sc);
	};
	const auto sceneTab = m_ribbon.addTab("Scene");
	const auto gScene = m_ribbon.addGroup(sceneTab, "Document");
	m_ribbon.addButton(sceneTab, gScene,
					   Button{.iconName = "ribbon_new",
							  .label = "New",
							  .tooltip = tipWithShortcut("New Scene", "scene.new"),
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .onClick = [this]() -> void { mp_editor->newScene(); },
							  .size = Size::Small});
	m_ribbon.addButton(sceneTab, gScene,
					   Button{.iconName = "ribbon_open",
							  .label = "Open",
							  .tooltip = tipWithShortcut("Open Scene", "scene.open"),
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .onClick = [this]() -> void { mp_editor->openScene(); },
							  .size = Size::Small});
	m_ribbon.addButton(sceneTab, gScene,
					   Button{.iconName = "ribbon_save",
							  .label = "Save",
							  .tooltip = tipWithShortcut("Save Scene", "scene.save"),
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->activeSceneDocument();
								  return d != nullptr && !d->filePath().empty();
							  },
							  .onClick = [this]() -> void { mp_editor->saveCurrentScene(); },
							  .size = Size::Large});
	m_ribbon.addButton(sceneTab, gScene,
					   Button{.iconName = "ribbon_save",
							  .label = "Save As",
							  .tooltip = tipWithShortcut("Save Scene As…", "scene.saveAs"),
							  .isEnabled = [this]() -> bool { return mp_editor->activeSceneDocument() != nullptr; },
							  .onClick = [this]() -> void { mp_editor->saveSceneAs(); },
							  .size = Size::Small});
	m_ribbon.addButton(sceneTab, gScene,
					   Button{.iconName = "ribbon_import",
							  .label = "Import",
							  .tooltip = "Import an existing scene file into this project",
							  .isEnabled = [this]() -> bool { return mp_editor->m_project.isLoaded(); },
							  .onClick = [this]() -> void { mp_editor->importScene(); },
							  .size = Size::Small});
	m_ribbon.addButton(sceneTab, gScene,
					   Button{.iconName = "ribbon_close",
							  .label = "Close",
							  .tooltip = tipWithShortcut("Close Scene", "doc.close"),
							  .isEnabled = [this]() -> bool { return mp_editor->activeSceneDocument() != nullptr; },
							  .onClick = [this]() -> void { mp_editor->requestCloseActiveDocument(); },
							  .size = Size::Small});
	const auto gShow = m_ribbon.addGroup(sceneTab, "Scene");
	m_ribbon.addButton(sceneTab, gShow,
					   Button{.iconName = "ribbon_cameras",
							  .label = "Cameras",
							  .tooltip = "Toggle in-viewport camera markers (icon + forward arrow + FOV cone)",
							  .isEnabled = [this]() -> bool { return mp_editor->getState() == State::Edit; },
							  .isChecked = [this]() -> bool { return mp_editor->m_settings.showCameraGizmos; },
							  .onClick = [this]() -> void {
								  mp_editor->m_settings.showCameraGizmos = !mp_editor->m_settings.showCameraGizmos;
							  },
							  .size = Size::Small});

	m_ribbon.addButton(sceneTab, gShow,
					   Button{.iconName = "ribbon_hud",
							  .label = "HUD Editor",
							  .tooltip = "Edit the HUD canvases of the scene in pixels: resolution presets, rect "
										 "gizmo, widget palette",
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->activeSceneDocument();
								  return d != nullptr && d->state() == State::Edit && d->getEditorScene() &&
										 d->getEditorScene()->getDocumentKind() == scene::DocumentKind::Scene;
							  },
							  .isChecked = [this]() -> bool {
								  const auto* d = mp_editor->activeSceneDocument();
								  return d != nullptr && d->isHudMode();
							  },
							  .onClick = [this]() -> void {
								  if (auto* d = mp_editor->activeSceneDocument(); d != nullptr)
									  d->setHudMode(!d->isHudMode());
							  },
							  .size = Size::Small});
	m_ribbon.addButton(sceneTab, gShow,
					   Button{.iconName = "ribbon_settings",
							  .label = "Settings",
							  .tooltip = "Open per-scene settings (renderer overrides, …)",
							  .isEnabled = [this]() -> bool { return mp_editor->activeSceneDocument() != nullptr; },
							  .onClick = [this]() -> void { mp_editor->m_sceneSettings.open(); },
							  .size = Size::Small});

	m_ribbon.addButton(sceneTab, gShow,
					   Button{.iconName = "ribbon_pack",
							  .label = "Pack Scene",
							  .tooltip = "Package this scene as a standalone .owlpack",
							  .isEnabled = [this]() -> bool {
								  const auto* d = mp_editor->activeSceneDocument();
								  return d != nullptr && !d->filePath().empty();
							  },
							  .onClick = [this]() -> void { mp_editor->packScene(); },
							  .size = Size::Small});
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
					   Button{.iconName = "ribbon_save",
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
					   Button{.iconName = "ribbon_close",
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
					   Button{.iconName = "ribbon_visibility",
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
					   Button{.iconName = "ribbon_save",
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
					   Button{.iconName = "ribbon_close",
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
					   Button{.iconName = "ribbon_play",
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
					   Button{.iconName = "ribbon_pause",
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
					   Button{.iconName = "ribbon_stop",
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
					   Button{.iconName = "ribbon_previous",
							  .label = "Previous",
							  .tooltip = "Step back one frame",
							  .isEnabled = [activeAnim]() -> bool { return activeAnim() != nullptr; },
							  .onClick = [activeAnim]() -> void {
								  if (auto* a = activeAnim())
									  a->stepPrevious();
							  },
							  .size = Size::Small});
	m_ribbon.addButton(animTab, gFrame,
					   Button{.iconName = "ribbon_next",
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
					   Button{.iconName = "ribbon_save",
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
					   Button{.iconName = "ribbon_save",
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
					   Button{.iconName = "ribbon_close",
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
					   Button{.iconName = "ribbon_save",
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
					   Button{.iconName = "ribbon_save",
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
					   Button{.iconName = "ribbon_close",
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
					   Button{.iconName = "ribbon_save",
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
					   Button{.iconName = "ribbon_save",
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
					   Button{.iconName = "ribbon_close",
							  .label = "Close",
							  .tooltip = "Close the active document",
							  .isEnabled = [activeTileset]() -> bool { return activeTileset() != nullptr; },
							  .onClick = [this]() -> void { mp_editor->requestCloseActiveDocument(); },
							  .size = Size::Small});
}

void RibbonBuilder::buildFontTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto activeFont = [this]() -> FontDocument* {
		auto* d = mp_editor->m_documents.getActive();
		if (d != nullptr && d->type() == DocumentType::Font)
			return static_cast<FontDocument*>(d);
		return nullptr;
	};
	const auto tab = m_ribbon.addTab("Font");
	const auto gFile = m_ribbon.addGroup(tab, "File");
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "ribbon_save",
							  .label = "Save",
							  .tooltip = "Save the charset to the .owlfont sidecar and rebuild the atlas (Ctrl+S)",
							  .isEnabled = [activeFont]() -> bool {
								  auto* a = activeFont();
								  return a != nullptr && a->isDirty();
							  },
							  .onClick = [activeFont]() -> void {
								  if (auto* a = activeFont())
									  std::ignore = a->save();
							  },
							  .size = Size::Large});
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "ribbon_close",
							  .label = "Close",
							  .tooltip = "Close the active document",
							  .isEnabled = [activeFont]() -> bool { return activeFont() != nullptr; },
							  .onClick = [this]() -> void { mp_editor->requestCloseActiveDocument(); },
							  .size = Size::Small});
}

void RibbonBuilder::buildImageTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto tab = m_ribbon.addTab("Image");
	const auto gFile = m_ribbon.addGroup(tab, "File");
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "ribbon_close",
							  .label = "Close",
							  .tooltip = "Close the active document",
							  .onClick = [this]() -> void { mp_editor->requestCloseActiveDocument(); },
							  .size = Size::Small});
}

void RibbonBuilder::buildMeshTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto tab = m_ribbon.addTab("Mesh");
	const auto gFile = m_ribbon.addGroup(tab, "File");
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "ribbon_close",
							  .label = "Close",
							  .tooltip = "Close the active document",
							  .onClick = [this]() -> void { mp_editor->requestCloseActiveDocument(); },
							  .size = Size::Small});
}

void RibbonBuilder::buildMaterialTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto activeMaterial = [this]() -> MaterialDocument* {
		auto* d = mp_editor->m_documents.getActive();
		if (d != nullptr && d->type() == DocumentType::Material)
			return static_cast<MaterialDocument*>(d);
		return nullptr;
	};
	const auto tab = m_ribbon.addTab("Material");
	const auto gFile = m_ribbon.addGroup(tab, "File");
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "ribbon_save",
							  .label = "Save",
							  .tooltip = "Save the material; every entity using it updates (Ctrl+S)",
							  .isEnabled = [activeMaterial]() -> bool {
								  auto* m = activeMaterial();
								  return m != nullptr && !m->filePath().empty();
							  },
							  .onClick = [activeMaterial]() -> void {
								  if (auto* m = activeMaterial())
									  std::ignore = m->save();
							  },
							  .size = Size::Large});
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "ribbon_close",
							  .label = "Close",
							  .tooltip = "Close the active document",
							  .onClick = [this]() -> void { mp_editor->requestCloseActiveDocument(); },
							  .size = Size::Small});
}

void RibbonBuilder::buildActionMapTab() {
	using Button = gui::widgets::Ribbon::Button;
	using Size = gui::widgets::Ribbon::ButtonSize;
	const auto activeMap = [this]() -> ActionMapDocument* {
		auto* d = mp_editor->m_documents.getActive();
		if (d != nullptr && d->type() == DocumentType::ActionMap)
			return static_cast<ActionMapDocument*>(d);
		return nullptr;
	};
	const auto tab = m_ribbon.addTab("Input Actions");
	const auto gFile = m_ribbon.addGroup(tab, "File");
	m_ribbon.addButton(
			tab, gFile,
			Button{.iconName = "ribbon_save",
				   .label = "Save",
				   .tooltip = "Save input_actions.yml and apply it to the editor and a running Play (Ctrl+S)",
				   .isEnabled = [activeMap]() -> bool {
					   auto* a = activeMap();
					   return a != nullptr && !a->filePath().empty();
				   },
				   .onClick = [activeMap]() -> void {
					   if (auto* a = activeMap())
						   std::ignore = a->save();
				   },
				   .size = Size::Large});
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "ribbon_save",
							  .label = "Save As",
							  .tooltip = "Save the action map to another file",
							  .isEnabled = [activeMap]() -> bool { return activeMap() != nullptr; },
							  .onClick = [activeMap]() -> void {
								  if (auto* a = activeMap()) {
									  const std::string defaultName = a->filePath().empty()
																			  ? std::string{input::ActionMap::fileName}
																			  : a->filePath().filename().string();
									  if (const auto path = platform::FileDialog::saveFile(
												  "Input actions (*.yml)|yml\n", defaultName);
										  !path.empty())
										  std::ignore = a->saveAs(path);
								  }
							  },
							  .size = Size::Small});
	m_ribbon.addButton(tab, gFile,
					   Button{.iconName = "ribbon_close",
							  .label = "Close",
							  .tooltip = "Close the active document",
							  .isEnabled = [activeMap]() -> bool { return activeMap() != nullptr; },
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
