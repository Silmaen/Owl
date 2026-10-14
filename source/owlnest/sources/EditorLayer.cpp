/**
 * @file EditorLayer.cpp
 * @author Silmaen
 * @date 21/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"

#include "AssetKind.h"
#include "EditorResources.h"
#include "document/AnimationDocument.h"
#include "document/CodeEditorDocument.h"
#include "document/FontDocument.h"
#include "document/NodeGraphDocument.h"
#include "document/SceneFlowDocument.h"
#include "document/TilemapDocument.h"
#include "document/TilesetDocument.h"

#include <debug/LogSink.h>
#include <gui/FontPreviewCache.h>
#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/component/render.h>
#include <gui/utils.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <physics/PhysicCommand.h>
#include <platform/AtomicFile.h>
#include <renderer/gpu/RenderCommand.h>
#include <scene/component/components.h>
#include <sound/SoundCommand.h>
#include <sound/SoundSystem.h>

#include <imgui_stdlib.h>
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <fstream>
#include <functional>
#include <sstream>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>


namespace owl::nest {


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
			if (!scene->filePath().empty() && std::filesystem::weakly_canonical(scene->filePath()) == canonical)
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
		m_sceneSettings.setScene(s_empty);
		m_sceneSettings.setUndoManager(nullptr);
		m_voxelPalette.setScene(s_empty);
	} else {
		m_sceneHierarchy.setContext(doc->getActiveScene());
		m_sceneHierarchy.setUndoManager(&doc->undoManager());
		m_sceneSettings.setScene(doc->getActiveScene());
		m_sceneSettings.setUndoManager(&doc->undoManager());
		m_voxelPalette.setScene(doc->getActiveScene());
		// The per-document viewport owns its own undo pointer already (set in SceneDocument::onAttach).
	}
	m_sceneSettings.setProject(m_project);
	m_voxelPalette.setProjectDirectory(m_project.projectDirectory);
	m_sceneHierarchy.setActiveDocument(m_documents.getActive());
	m_sceneHierarchy.setParentEditor(this);
	if (doc != nullptr && doc->getActiveScene()) {
		const auto& stackCfg = m_project.rendererStack.isEmpty() ? renderer::RendererStackConfig::makeDefault()
																 : m_project.rendererStack;
		auto stack = renderer::RenderStack::buildFromConfig(stackCfg, doc->getActiveScene()->getEnabledRenderers());
		renderer::Renderer::setRenderStack(std::move(stack));
	} else {
		renderer::Renderer::setRenderStack(renderer::RenderStack{});
	}
	refreshWindowTitle();
	m_ribbonBuilder.refresh();
}

void EditorLayer::handleContentBrowserDrop(const std::filesystem::path& iRelativePath) {
	const auto relString = iRelativePath.generic_string();
	const auto kind = classifyAsset(iRelativePath);
	if (kind == AssetKind::Unsupported) {
		OWL_CORE_WARN("Could not load {}: unsupported file type.", relString)
		return;
	}
	// The path is relative to the root the browser shows: another asset folder may hold a file of the same name.
	std::filesystem::path full;
	if (const auto inBrowsed = m_contentBrowser.getRootPath() / iRelativePath; exists(inBrowsed))
		full = inBrowsed;
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (!full.empty())
			break;
		if (const auto candidate = assetsPath / iRelativePath; exists(candidate))
			full = candidate;
	}
	if (full.empty()) {
		OWL_CORE_WARN("Could not resolve dropped file: {}.", relString)
		return;
	}
	if (kind == AssetKind::Prefab)
		instantiatePrefab(full, relString);
	else
		requestDeferredOpen(full);
}

void EditorLayer::registerCameraActions() {
	const auto onViewport = [this](std::function<void(panel::Viewport&)> iAction) -> std::function<void()> {
		return [this, action = std::move(iAction)]() -> void {
			if (auto* vp = activeViewport(); vp != nullptr && getState() == State::Edit)
				action(*vp);
		};
	};
	m_actionRegistry.registerAction("view.resetCamera", "Camera: Reset View", {input::key::Home, Modifiers::None},
									onViewport([](panel::Viewport& ioViewport) -> void { ioViewport.resetView(); }));
	m_actionRegistry.registerAction("view.frameScene", "Camera: Frame Scene", {input::key::F, Modifiers::Shift},
									onViewport([](panel::Viewport& ioViewport) -> void { ioViewport.frameScene(); }));
	m_actionRegistry.registerAction(
			"view.cameraViewpoint", "Camera: Selected Camera Viewpoint", {},
			onViewport([](panel::Viewport& ioViewport) -> void { ioViewport.goToCameraViewpoint(); }));
	using Axis = renderer::CameraEditor::ViewAxis;
	constexpr std::array<std::tuple<Axis, const char*, const char*>, 6> views{
			{{Axis::Front, "view.cameraFront", "Camera: Front View (XY)"},
			 {Axis::Back, "view.cameraBack", "Camera: Back View"},
			 {Axis::Top, "view.cameraTop", "Camera: Top View (XZ)"},
			 {Axis::Bottom, "view.cameraBottom", "Camera: Bottom View"},
			 {Axis::Right, "view.cameraRight", "Camera: Right View (YZ)"},
			 {Axis::Left, "view.cameraLeft", "Camera: Left View"}}};
	for (const auto& [axis, id, label]: views)
		m_actionRegistry.registerAction(
				id, label, {}, onViewport([axis](panel::Viewport& ioViewport) -> void { ioViewport.viewAlong(axis); }));
}

void EditorLayer::registerTextScaleActions() {
	constexpr int step = 10;
	m_actionRegistry.registerAction(
			"view.textLarger", "Text Size: Larger", {input::key::Equal, Modifiers::Ctrl},
			[this]() -> void { m_settings.textScalePercent = std::min(150, m_settings.textScalePercent + step); });
	m_actionRegistry.registerAction(
			"view.textSmaller", "Text Size: Smaller", {input::key::Minus, Modifiers::Ctrl},
			[this]() -> void { m_settings.textScalePercent = std::max(75, m_settings.textScalePercent - step); });
	m_actionRegistry.registerAction("view.textReset", "Text Size: Reset", {input::key::D0, Modifiers::Ctrl},
									[this]() -> void { m_settings.textScalePercent = 100; });
}

void EditorLayer::renderDocumentTabMenu(Document& ioDocument) {
	if (!ImGui::BeginPopupContextItem("##tabMenu"))
		return;
	const auto path = ioDocument.filePath();
	gui::widgets::menuHeader(ioDocument.title().c_str(), path.empty() ? "Unsaved" : nullptr);
	const auto saveKey = getActionShortcut("scene.save");
	if (gui::widgets::menuItem({.glyph = gui::icon::save, .label = "Save", .shortcut = saveKey.c_str()})) {
		m_documents.setActive(&ioDocument);
		saveActiveDocument();
	}
	ImGui::Separator();
	const auto closeKey = getActionShortcut("doc.close");
	if (gui::widgets::menuItem({.glyph = gui::icon::x, .label = "Close", .shortcut = closeKey.c_str()}))
		requestCloseDocument(ioDocument.id());
	if (gui::widgets::menuItem({.label = "Close Others", .enabled = m_documents.list().size() > 1})) {
		for (const auto& doc: m_documents.list()) {
			if (doc && doc.get() != &ioDocument)
				requestCloseDocument(doc->id());
		}
	}
	ImGui::Separator();
	if (gui::widgets::menuItem(
				{.glyph = gui::icon::folderSearch, .label = "Reveal in Content Browser", .enabled = !path.empty()})) {
		m_contentBrowser.reveal(path);
		ImGui::SetWindowFocus(panelWindow::contentBrowser);
	}
	if (gui::widgets::menuItem({.glyph = gui::icon::clipboardCopy, .label = "Copy Path", .enabled = !path.empty()}))
		ImGui::SetClipboardText(path.string().c_str());
	ImGui::EndPopup();
}

void EditorLayer::openAssetFromInspector(const std::filesystem::path& iPath) {
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (const auto candidate = assetsPath / iPath; exists(candidate)) {
			requestDeferredOpen(candidate);
			return;
		}
	}
	if (exists(iPath))
		requestDeferredOpen(iPath);
	else
		OWL_WARN("EditorLayer: Cannot open {}, it is in no asset folder.", iPath.string())
}

void EditorLayer::closeDocument(const core::UUID iId) {
	const bool wasActive = (m_documents.getActive() != nullptr && m_documents.getActive()->id() == iId);
	if (!m_documents.remove(iId))
		return;
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

void EditorLayer::renderSnapStepPopup() {
	const auto* doc = activeSceneDocument();
	const auto& scene = doc != nullptr ? doc->getActiveScene() : shared<scene::Scene>{};
	const bool hasTilemap = scene && scene->registry.view<scene::component::Tilemap>().begin() !=
											 scene->registry.view<scene::component::Tilemap>().end();

	if (hasTilemap && m_settings.snapAutoFromTilemap) {
		ImGui::TextDisabled("Snap step (× cell size)");
		ImGui::Separator();
		struct Preset {
			const char* label;
			float value;
		};
		constexpr std::array presets{Preset{"1/4 cell", 0.25f}, Preset{"1/2 cell", 0.5f}, Preset{"1 cell", 1.f},
									 Preset{"2 cells", 2.f},    Preset{"5 cells", 5.f},   Preset{"10 cells", 10.f}};
		for (const auto& p: presets) {
			const bool selected = std::abs(m_settings.snapMultiplier - p.value) < 0.0001f;
			if (ImGui::MenuItem(p.label, nullptr, selected)) {
				m_settings.snapMultiplier = p.value;
				ImGui::CloseCurrentPopup();
			}
		}
	} else {
		ImGui::TextDisabled("Snap step (world units)");
		ImGui::Separator();
		struct Preset {
			const char* label;
			float value;
		};
		constexpr std::array presets{Preset{"0.25", 0.25f}, Preset{"0.5", 0.5f}, Preset{"1", 1.f}, Preset{"5", 5.f},
									 Preset{"10", 10.f}};
		for (const auto& p: presets) {
			const bool selected = std::abs(m_settings.snapStep - p.value) < 0.0001f;
			if (ImGui::MenuItem(p.label, nullptr, selected)) {
				m_settings.snapStep = p.value;
				ImGui::CloseCurrentPopup();
			}
		}
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
			if (gui::widgets::button("Discard changes", {.glyph = gui::icon::trash2,
														 .label = "Discard changes",
														 .width = ImGui::GetFontSize() * 9.f})) {
				const auto id = m_pendingCloseDocId;
				m_pendingCloseDocId = core::UUID{0};
				ImGui::CloseCurrentPopup();
				m_deferredCloseIds.push_back(id);
			}
			ImGui::SameLine();
			if (gui::widgets::button("Cancel",
									 {.glyph = gui::icon::x, .label = "Cancel", .width = ImGui::GetFontSize() * 7.f})) {
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

	app::Application::get().enableDocking();
	m_hotReloadListener = app::Application::get().getHotReload().addListener(
			[this](const std::filesystem::path& iFile) -> void { onAssetFileChanged(iFile); });

	if (const auto f = app::Application::get().getWorkingDirectory() / "OwlNest_settings.yml"; exists(f))
		m_settings.loadFromFile(f);
	utils::applyTheme(gui::Theme::fromPreset(gui::Theme::presetFromName(m_settings.themePreset)));

	m_dockLayout.setPreset(DockLayout::presetFromName(m_settings.layoutPreset));

	// Create the initial scene document. Its onAttach() initialises its own Viewport.
	auto& initialDoc = ensureActiveSceneDocument();
	m_sceneHierarchy.setUndoManager(&initialDoc.undoManager());
	m_sceneHierarchy.setCommandRegistry(&m_commands);

	utils::buildIconBank();
	{
		std::vector<std::filesystem::path> assetDirectories;
		for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories())
			assetDirectories.push_back(assetsPath);
		m_sampleProject = panel::findSampleProject(assetDirectories).value_or(std::filesystem::path{});
	}

	utils::loadTriggerTextures();

	utils::loadSounds();

	// clang-format off
	m_actionRegistry.registerAction("scene.new", "New Scene", {input::key::N, Modifiers::Ctrl}, [this]() -> void {
		if (getState() == State::Edit)
			newScene();
	});
	m_actionRegistry.registerAction("scene.open", "Open Scene", {input::key::O, Modifiers::Ctrl}, [this]() -> void {
		if (getState() == State::Edit)
			openScene();
	});
	m_actionRegistry.registerAction("scene.save", "Save Document", {input::key::S, Modifiers::Ctrl},
									[this]() -> void { saveActiveDocument(); });
	m_actionRegistry.registerAction("scene.saveAs", "Save Scene As",
									{input::key::S, Modifiers::Ctrl | Modifiers::Shift}, [this]() -> void {
										if (getState() == State::Edit)
											saveSceneAs();
									});
	m_actionRegistry.registerAction("entity.duplicate", "Duplicate Entity", {input::key::D, Modifiers::Ctrl},
									[this]() -> void {
										if (getState() == State::Edit)
											onDuplicateEntity();
									});
	m_actionRegistry.registerAction("guizmo.none", "Gizmo: None", {input::key::Q, Modifiers::None}, [this]() -> void {
		if (auto* vp = activeViewport(); vp != nullptr && getState() == State::Edit &&
										 (vp->isFocused() || vp->isHovered()) && !gui::Guizmo::isUsing())
			vp->setGuizmoType(gui::Guizmo::Type::None);
	});
	m_actionRegistry.registerAction(
			"guizmo.translate", "Gizmo: Translate", {input::key::W, Modifiers::None}, [this]() -> void {
				if (auto* vp = activeViewport(); vp != nullptr && getState() == State::Edit &&
												 (vp->isFocused() || vp->isHovered()) && !gui::Guizmo::isUsing())
					vp->setGuizmoType(gui::Guizmo::Type::Translation);
			});
	m_actionRegistry.registerAction(
			"guizmo.rotate", "Gizmo: Rotate", {input::key::E, Modifiers::None}, [this]() -> void {
				if (auto* vp = activeViewport(); vp != nullptr && getState() == State::Edit &&
												 (vp->isFocused() || vp->isHovered()) && !gui::Guizmo::isUsing())
					vp->setGuizmoType(gui::Guizmo::Type::Rotation);
			});
	m_actionRegistry.registerAction(
			"guizmo.scale", "Gizmo: Scale", {input::key::R, Modifiers::None}, [this]() -> void {
				if (auto* vp = activeViewport(); vp != nullptr && getState() == State::Edit &&
												 (vp->isFocused() || vp->isHovered()) && !gui::Guizmo::isUsing())
					vp->setGuizmoType(gui::Guizmo::Type::Scale);
			});
	m_actionRegistry.registerAction("guizmo.all", "Gizmo: All", {input::key::T, Modifiers::None}, [this]() -> void {
		if (auto* vp = activeViewport(); vp != nullptr && getState() == State::Edit &&
										 (vp->isFocused() || vp->isHovered()) && !gui::Guizmo::isUsing())
			vp->setGuizmoType(gui::Guizmo::Type::All);
	});
	registerTextScaleActions();
	m_actionRegistry.registerAction(
			"view.frameSelection", "Frame Selection", {input::key::F, Modifiers::None}, [this]() -> void {
				if (auto* vp = activeViewport(); vp != nullptr && getState() == State::Edit &&
												 (vp->isFocused() || vp->isHovered()))
					vp->frameSelection();
			});
	registerCameraActions();
	m_actionRegistry.registerAction("view.toggleProjection", "Toggle 2D / 3D View", {}, [this]() -> void {
		if (auto* vp = activeViewport(); vp != nullptr) {
			using Projection = renderer::CameraEditor::Projection;
			vp->setViewProjection(vp->getViewProjection() == Projection::Orthographic ? Projection::Perspective
																					  : Projection::Orthographic);
		}
	});
	// Playback actions
	m_actionRegistry.registerAction("scene.play", "Play/Resume", {input::key::F5, Modifiers::None}, [this]() -> void {
		if (getState() == State::Edit)

			onScenePlay();
		else if (getState() == State::Pause)

			onSceneResume();
	});
	m_actionRegistry.registerAction("scene.pause", "Pause", {input::key::F6, Modifiers::None}, [this]() -> void {
		if (getState() == State::Play)
			onScenePause();
	});
	m_actionRegistry.registerAction("scene.stop", "Stop", {input::key::F7, Modifiers::None}, [this]() -> void {
		if (getState() == State::Play || getState() == State::Pause)
			onSceneStop();
	});
	m_actionRegistry.registerAction("scene.step", "Step Frame", {input::key::F8, Modifiers::None}, [this]() -> void {
		if (getState() == State::Pause)
			onSceneStep();
	});
	// Entity actions
	m_actionRegistry.registerAction("entity.delete", "Delete Entity", {input::key::Delete, Modifiers::None},
									[this]() -> void {
										if (getState() != State::Edit)
											return;
										if (const auto ent = getSelectedEntity(); ent) {
											setSelectedEntity({});
											std::ignore = execute(
													"entity.delete",
													{{"entity", static_cast<int64_t>(static_cast<uint64_t>(ent.getUUID()))}});
										}
									});
	// Undo/Redo
	m_actionRegistry.registerAction("edit.undo", "Undo", {input::key::Z, Modifiers::Ctrl},
									[this]() -> void { performUndo(); });
	m_actionRegistry.registerAction("edit.redo", "Redo", {input::key::Y, Modifiers::Ctrl},
									[this]() -> void { performRedo(); });
	// Document-level shortcuts
	m_actionRegistry.registerAction("doc.close", "Close Document", {input::key::W, Modifiers::Ctrl},
									[this]() -> void { requestCloseActiveDocument(); });
	m_actionRegistry.registerAction("doc.next", "Next Document", {input::key::Tab, Modifiers::Ctrl}, [this]() -> void {
		const auto& docs = m_documents.list();
		if (docs.size() < 2)
			return;
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
									{input::key::Tab, Modifiers::Ctrl | Modifiers::Shift}, [this]() -> void {
										const auto& docs = m_documents.list();
										if (docs.size() < 2)
											return;
										auto* current = m_documents.getActive();
										for (size_t i = 0; i < docs.size(); ++i) {
											if (docs[i].get() == current) {
												m_documents.setActive(docs[(i + docs.size() - 1) % docs.size()].get());

												syncActiveDocumentPanels();
												return;
											}
										}
									});
	m_actionRegistry.registerAction("help.context", "Help", {input::key::F1, Modifiers::None},
									[this]() -> void { onContextualHelp(); });
	m_actionRegistry.registerAction("view.layoutDefault", "Layout: Default", {input::key::D1, Modifiers::Alt},
									[this]() -> void { applyLayoutPreset(LayoutPreset::Default); });
	m_actionRegistry.registerAction("view.layoutHierarchyLeft", "Layout: Hierarchy Left", {input::key::D2, Modifiers::Alt},
									[this]() -> void { applyLayoutPreset(LayoutPreset::HierarchyLeft); });
	m_actionRegistry.registerAction("view.toggleRibbon", "Collapse / Expand the Ribbon", {input::key::F1, Modifiers::Ctrl},
									[this]() -> void { m_ribbonBuilder.toggleCollapsed(); });
	m_actionRegistry.registerAction("view.resetLayout", "Reset Layout", {},
									[this]() -> void { applyLayoutPreset(m_dockLayout.getPreset()); });
	m_actionRegistry.registerAction("view.commandPalette", "Command Palette", {},
									[this]() -> void { m_commandPalette.open(); });
	// clang-format on

	// Apply saved keybinding overrides
	m_actionRegistry.loadOverrides(m_settings.keybindingOverrides);

	m_ribbonBuilder.build();
	if (auto ui = app::Application::get().getImGuiLayer(); ui != nullptr) {
		ui->setTopBarCallback([this]() -> void { m_ribbonBuilder.onRender(); });
		ui->setBottomBarCallback([this]() -> void { renderStatusBar(); });
		ui->setDockLayoutCallback([this](const uint32_t iDockSpaceId) -> void {
			std::ignore = m_dockLayout.onDockSpace(iDockSpaceId, m_settings.layoutVersion);
		});
	}

	m_contentBrowser.attach();
	gui::component::setOpenAssetHandler(
			[this](const std::filesystem::path& iPath) -> void { openAssetFromInspector(iPath); });
	m_contentBrowser.setSceneOpenCallback([this](const std::filesystem::path& iPath) -> void { openScene(iPath); });
	m_contentBrowser.setCodeOpenCallback([this](const std::filesystem::path& iPath) -> void { openCodeFile(iPath); });
	m_contentBrowser.setNodeGraphOpenCallback(
			[this](const std::filesystem::path& iPath) -> void { openNodeGraphFile(iPath); });
	m_contentBrowser.setAnimationOpenCallback(
			[this](const std::filesystem::path& iPath) -> void { openAnimationFile(iPath); });
	m_contentBrowser.setTilemapOpenCallback(
			[this](const std::filesystem::path& iPath) -> void { openTilemapFile(iPath); });
	m_contentBrowser.setTilesetOpenCallback(
			[this](const std::filesystem::path& iPath) -> void { openTilesetFile(iPath); });
	m_contentBrowser.setFontOpenCallback([this](const std::filesystem::path& iPath) -> void { openFontFile(iPath); });

	newScene();
	if (!m_startupProject.empty())
		openProject(m_startupProject);
	if (!m_project.isLoaded())
		m_backstage.open();
}

void EditorLayer::onDetach() {
	OWL_PROFILE_FUNCTION()

	app::Application::get().getHotReload().removeListener(m_hotReloadListener);

	saveProjectSession();
	// Sync keybinding overrides before saving
	m_settings.keybindingOverrides = m_actionRegistry.getOverrides();
	m_settings.saveToFile(app::Application::get().getWorkingDirectory() / "OwlNest_settings.yml");

	m_contentBrowser.detach();
	OWL_TRACE("EditorLayer: deleted editor FrameBuffer.")
	if (auto ui = app::Application::get().getImGuiLayer(); ui != nullptr) {
		ui->setTopBarCallback({});
		ui->setBottomBarCallback({});
		ui->setDockLayoutCallback({});
	}
	m_ribbonBuilder.clear();

	// Unsaved changes survive a quit too: Nest asks nothing on exit.
	m_recovery.autosave(m_documents);
	m_documents.clear();
	OWL_TRACE("EditorLayer: closed all documents (and their viewports).")

	// Free the static IconBank's atlas while the device is still valid, else it outlives vkDestroyDevice and leaks.
	gui::IconBank::instance().clear();
	gui::component::setOpenAssetHandler({});
}

void EditorLayer::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	gui::FontPreviewCache::get().pumpPending();

	m_recovery.setInterval(static_cast<float>(m_settings.autosaveIntervalSeconds));
	m_toasts.update(iTimeStep.getSeconds());
	gui::UiLayer::setTextScale(static_cast<float>(m_settings.textScalePercent) / 100.f);
	// Before the viewports: a preview render resets the 2D stats they show.
	if (getState() == State::Edit)
		m_contentBrowser.onUpdate();
	if (m_recovery.onUpdate(iTimeStep.getSeconds()) && m_recovery.autosave(m_documents) > 0)
		m_lastAutosave = std::chrono::steady_clock::now();

	m_documents.updateSceneless(iTimeStep);

	// The document of a bounded play stopped playing or was closed: the play ends there.
	if (m_playDriver.isActive() && !isPlayDocumentPlaying())
		finishBoundedPlay(/*iInterrupted=*/true);

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

	const auto& activeScene = activeDoc->getActiveScene();
	if ((activeDoc->state() == SceneDocument::State::Play || activeDoc->state() == SceneDocument::State::Pause) &&
		activeScene && activeScene->status == scene::Scene::Status::Editing) {
		activeScene->onStartRuntime();
		activeDoc->applyPendingTeleportVelocity();
	}

	// Handle deferred quit request from Lua (scene.quit()).
	if (activeDoc->isStopRequested()) {
		activeDoc->clearStopRequest();
		if (activeDoc->state() == SceneDocument::State::Play || activeDoc->state() == SceneDocument::State::Pause)
			onSceneStop();
		return;
	}

	// Drive the active document's scene update (which renders into its own framebuffer).
	updateActiveDocument(*activeDoc, iTimeStep);

	if (activeDoc->consumeSceneSwapped())
		syncActiveDocumentPanels();
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

	std::ignore = m_actionRegistry.dispatchImGuiChords();

	if (!m_deferredCloseIds.empty()) {
		auto ids = std::move(m_deferredCloseIds);
		m_deferredCloseIds.clear();
		for (const auto id: ids) closeDocument(id);
	}

	if (!m_deferredOpenPaths.empty()) {
		auto paths = std::move(m_deferredOpenPaths);
		m_deferredOpenPaths.clear();
		for (const auto& path: paths) {
			if (!exists(path))
				continue;
			openAssetFile(path);
		}
	}

	const auto* activeBefore = m_documents.getActive();
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
		} else if (doc->type() == DocumentType::Animation) {
			auto* anim = static_cast<AnimationDocument*>(doc);
			if (!anim->isOpen()) {
				toClose.push_back(anim->id());
				anim->setOpen(true);
			}
		} else if (doc->type() == DocumentType::Tilemap) {
			auto* tilemap = static_cast<TilemapDocument*>(doc);
			if (!tilemap->isOpen()) {
				toClose.push_back(tilemap->id());
				tilemap->setOpen(true);
			}
		} else if (doc->type() == DocumentType::Tileset) {
			auto* tileset = static_cast<TilesetDocument*>(doc);
			if (!tileset->isOpen()) {
				toClose.push_back(tileset->id());
				tileset->setOpen(true);
			}
		} else if (doc->type() == DocumentType::Font) {
			auto* font = static_cast<FontDocument*>(doc);
			if (!font->isOpen()) {
				toClose.push_back(font->id());
				font->setOpen(true);
			}
		}
	}
	if (m_documents.getActive() != activeBefore)

		syncActiveDocumentPanels();
	for (const auto id: toClose) requestCloseDocument(id);

	renderCloseDocumentModal();
	handleRecoveryChoice(m_recoveryPrompt.onImGuiRender());
	if (const auto request = m_newProjectDialog.onImGuiRender(); request)
		createProject(*request);
	//=============================================================
	m_sceneHierarchy.onImGuiRender();
	// The last window to appear takes the focus, which picks the tab shown in its group.
	m_logPanel.onImGuiRender();
	renderStats(iTimeStep);
	m_contentBrowser.onImGuiRender();
	m_parameters.onImGuiRender();
	m_projectSettings.onImGuiRender();
	if (m_projectSettings.hasResult()) {
		const auto previousMappings = m_project.gamepadMappings;
		m_project = m_projectSettings.consumeResult();
		app::Application::get().getEngineContext().setSimulationSettings(m_project.simulation);
		if (m_project.gamepadMappings != previousMappings)
			std::ignore = m_project.loadGamepadMappings();

		saveProject();

		syncActiveDocumentPanels();
	}
	m_sceneSettings.onImGuiRender();
	if (m_sceneSettings.consumeDirtyFlag()) {
		syncActiveDocumentPanels();
	}
	m_settingsPanel.onImGuiRender(m_settings, m_actionRegistry);
	m_helpPanel.onImGuiRender(iTimeStep);
	{
		// The palette paints the edited scene only: not while it plays, nor behind another document.
		const auto* sceneDoc = activeSceneDocument();
		m_voxelPalette.onImGuiRender(m_voxelBrush, sceneDoc != nullptr && sceneDoc == m_documents.getActive() &&
														   sceneDoc->state() == SceneDocument::State::Edit);
	}
	m_asyncProgress.onImGuiRender();

	renderBackstage();

	m_packager.onRender();
	renderCommandPalette();
	renderCommandForm();
	m_toasts.onImGuiRender();
}

void EditorLayer::renderCommandPalette() {
	m_commandPalette.pollDoubleShift();
	if (!m_commandPalette.isOpen())
		return;
	if (m_commandPalette.consumeOpened()) {
		m_paletteAssets.clear();
		for (const auto& [title, path]: app::Application::get().getAssetDirectories()) {
			for (const auto& file: panel::CommandPalette::collectAssets(path))
				m_paletteAssets.emplace_back(file, std::filesystem::relative(file, path).generic_string());
		}
	}
	std::vector<panel::PaletteItem> items;
	for (const auto& action: m_actionRegistry.getActions()) {
		if (action.id == "view.commandPalette")
			continue;
		items.push_back({.category = panel::CommandPalette::categoryOf(action.id),
						 .label = action.displayName,
						 .shortcut = m_actionRegistry.getShortcutString(action.id),
						 .run = [this, id = action.id]() -> void { std::ignore = m_actionRegistry.run(id); }});
	}
	const bool playing = getState() != State::Edit;
	for (const auto& spec: m_commands.getCommands()) {
		items.push_back({.category = "Command",
						 .label = spec.name,
						 .shortcut = {},
						 .run = [this, name = spec.name]() -> void {
							 const auto* command = m_commands.find(name);
							 if (command == nullptr)
								 return;
							 if (command->args.empty())
								 runCommandFromUi(name, {});
							 else
								 m_commandForm.open(*command, getSelectedEntity() ? getSelectedEntity().getUUID()
																				  : core::UUID{0});
						 },
						 .enabled = commands::CommandForm::isAvailable(spec, playing)});
	}
	if (const auto& scene = getActiveScene(); scene != nullptr) {
		for (const auto& entity: scene->getAllEntities()) {
			if (!entity.hasComponent<scene::component::Tag>())
				continue;
			items.push_back({.category = "Select",
							 .label = entity.getComponent<scene::component::Tag>().tag,
							 .shortcut = {},
							 .run = [this, entity]() -> void { setSelectedEntity(entity); }});
		}
	}
	for (const auto& [path, name]: m_paletteAssets)
		items.push_back({.category = "Asset", .label = name, .shortcut = {}, .run = [this, path]() -> void {
							 openAssetFile(path);
						 }});
	m_commandPalette.onImGuiRender(items);
}

void EditorLayer::renderCommandForm() {
	if (!m_commandForm.isOpen())
		return;
	std::vector<panel::EntityChoice> entities;
	if (const auto& scene = getActiveScene(); scene != nullptr) {
		for (const auto& entity: scene->getAllEntities()) {
			if (entity.hasComponent<scene::component::Tag>())
				entities.push_back({.uuid = entity.getUUID(), .tag = entity.getComponent<scene::component::Tag>().tag});
		}
	}
	if (const auto submission = m_commandForm.onImGuiRender(entities); submission)
		runCommandFromUi(submission->command, submission->args);
}

void EditorLayer::runCommandFromUi(const std::string& iName, const commands::CommandArgs& iArgs) {
	const auto result = execute(iName, iArgs);
	if (!result) {
		m_toasts.push(gui::widgets::ToastKind::Error, std::format("{} failed", iName), result.error().message);
		return;
	}
	m_toasts.push(gui::widgets::ToastKind::Success, result->description.empty() ? iName : result->description,
				  commands::CommandForm::describeData(result->data));
	if (const auto& scene = getActiveScene(); scene != nullptr && result->entity != core::UUID{0}) {
		if (const auto entity = scene->findEntityByUUID(result->entity); entity)
			setSelectedEntity(entity);
	}
}

void EditorLayer::renderBackstage() {
	if (!m_backstage.isOpen())
		return;
	const panel::BackstageContext context{
			.projectName = m_project.isLoaded() ? m_project.name : std::string{},
			.projectDirectory = m_project.projectDirectory,
			.recentProjects = m_settings.recentProjects,
			.sampleProject = m_sampleProject,
			.newProject = [this]() -> void { newProject(); },
			.openProject = [this]() -> void { openProject(); },
			.openRecent = [this](const std::filesystem::path& iPath) -> void { openProject(iPath); },
			.forgetRecent = [this](const std::filesystem::path& iPath) -> void {
				m_settings.removeRecentProject(iPath);
			},
			.saveProject = [this]() -> void { saveProject(); },
			.saveProjectAs = [this]() -> void { saveProjectAs(); },
			.packGame = [this]() -> void { packGame(); },
			.editorSettings = [this]() -> void { m_settingsPanel.open(); },
			.projectSettings = [this]() -> void { m_projectSettings.open(m_project); },
			.engineSettings = [this]() -> void { m_parameters.open(); },
			.help = [this](const std::string& iPage) -> void {
				if (iPage.empty())
					m_helpPanel.open();
				else
					m_helpPanel.open(iPage);
			},
			.closeProject = [this]() -> void { closeProject(); },
			.exit = []() -> void { app::Application::get().close(); }};
	m_backstage.onImGuiRender(context);
}

void EditorLayer::applyLayoutPreset(const LayoutPreset iPreset) {
	m_dockLayout.request(iPreset);
	m_settings.layoutPreset = DockLayout::getPresetName(iPreset);
}

auto EditorLayer::buildStatusInfo() const -> panel::StatusInfo {
	panel::StatusInfo info;
	if (const auto* doc = m_documents.getActive(); doc != nullptr)
		info.document = doc->title();
	if (const auto* playing = findPlayingSceneDocument(); playing != nullptr)
		info.mode = playing->state() == SceneDocument::State::Pause ? panel::StatusInfo::Mode::Pause
																	: panel::StatusInfo::Mode::Play;
	if (const auto* scene = activeSceneDocument(); scene != nullptr && scene == m_documents.getActive()) {
		if (const auto& active = scene->getActiveScene(); active)
			info.entityCount = active->getEntityCount();
	}
	if (m_lastAutosave.has_value())
		info.secondsSinceAutosave =
				std::chrono::duration<float>(std::chrono::steady_clock::now() - *m_lastAutosave).count();
	info.backend = std::string{magic_enum::enum_name(renderer::gpu::RenderCommand::getApi())};
	if (auto device = renderer::gpu::RenderCommand::getDeviceName(); device != "none")
		info.device = std::move(device);
	if (const auto [ready, total] = m_contentBrowser.getPreviewProgress(); ready < total)
		info.previews = std::pair{ready, total};
	info.warningCount = core::Log::getLogBuffer().getWarningCount();
	info.errorCount = core::Log::getLogBuffer().getErrorCount();
	return info;
}

void EditorLayer::renderStatusBar() {
	auto info = buildStatusInfo();
	info.notifications = &m_toasts;
	m_statusBar.onImGuiRender(info);
	if (m_statusBar.consumeOpenLogRequest())
		ImGui::SetWindowFocus(panelWindow::log);
}

void EditorLayer::renderStats(const core::Timestep& iTimeStep) {
	if (!m_settings.showStats) {
		m_gpuCounters.reset();
		return;
	}
	ImGui::Begin(panelWindow::title(gui::icon::gauge, "Stats", panelWindow::stats).c_str());
	ImGui::Text("%s", std::format("FPS: {:.2f}", iTimeStep.getFps()).c_str());
	ImGui::Separator();
	ImGui::Text("%s", std::format("Profiler: {}{}", magic_enum::enum_name(debug::getProfilerBackend()),
								  debug::isProfilerConnected() ? " (connected)" : "")
							  .c_str());
#ifdef OWL_TRACKER_ACTIVE
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
#else
	ImGui::TextUnformatted("Memory tracker off (OWL_ENABLE_MEMORY_TRACKER).");
#endif
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
	renderGpuCounters();
	ImGui::End();
}

void EditorLayer::renderGpuCounters() {
	ImGui::Separator();
	if (renderer::gpu::RenderCommand::getApi() != renderer::gpu::RenderAPI::Type::Vulkan) {
		m_gpuCounters.reset();
		ImGui::TextDisabled("GPU queue counters: Vulkan only.");
		return;
	}
	m_gpuCounters.update(renderer::gpu::RenderCommand::getRenderCounters(), panel::GpuCounterMonitor::clock::now());
	const auto& last = m_gpuCounters.getLast();
	const auto peak = m_gpuCounters.getWindowMax();
	ImGui::TextUnformatted("Vulkan (per frame / max 2 s):");
	ImGui::Text("Submits: %llu / %llu", static_cast<unsigned long long>(last.submits),
				static_cast<unsigned long long>(peak.submits));
	ImGui::Text("Queue wait idles: %llu / %llu", static_cast<unsigned long long>(last.queueWaitIdles),
				static_cast<unsigned long long>(peak.queueWaitIdles));
	ImGui::Text("Device wait idles: %llu / %llu", static_cast<unsigned long long>(last.deviceWaitIdles),
				static_cast<unsigned long long>(peak.deviceWaitIdles));
	ImGui::Text("Fence waits: %llu / %llu", static_cast<unsigned long long>(last.fenceWaits),
				static_cast<unsigned long long>(peak.fenceWaits));
	ImGui::Text("Pace wait: %.2f / %.2f ms", last.paceWaitMs, peak.paceWaitMs);
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
	if (m_playDriver.isActive() && doc == mp_playDocument)
		finishBoundedPlay(/*iInterrupted=*/true);
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

void EditorLayer::handleSaveLoadRequest() {
	auto* doc = activeSceneDocument();
	if (doc == nullptr)
		return;
	doc->handleSaveLoadRequest(activeViewportSize());
	m_sceneHierarchy.setContext(doc->getActiveScene());
}

void EditorLayer::refreshWindowTitle() {
	auto& app = app::Application::get();
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

	if (const scene::Entity selectedEntity = m_sceneHierarchy.getSelectedEntity(); selectedEntity)
		std::ignore = execute("entity.duplicate",
							  {{"entity", static_cast<int64_t>(static_cast<uint64_t>(selectedEntity.getUUID()))}});
}

void EditorLayer::performUndo() {
	auto* doc = m_documents.getActive();
	if (doc == nullptr)
		return;
	doc->performUndo();
	// Scene-level undo also propagates a selection hint into the global selection.
	if (doc->type() == DocumentType::Scene) {
		auto* sceneDoc = static_cast<SceneDocument*>(doc);
		const auto& undo = sceneDoc->undoManager();
		if (const auto hint = undo.lastSelectionHint(); hint != core::UUID{0}) {
			if (auto entity = sceneDoc->getActiveScene()->findEntityByUUID(hint); entity)
				setSelectedEntity(entity);
		}
	}
}

void EditorLayer::performRedo() {
	auto* doc = m_documents.getActive();
	if (doc == nullptr)
		return;
	doc->performRedo();
	if (doc->type() == DocumentType::Scene) {
		auto* sceneDoc = static_cast<SceneDocument*>(doc);
		const auto& undo = sceneDoc->undoManager();
		if (const auto hint = undo.lastSelectionHint(); hint != core::UUID{0}) {
			if (auto entity = sceneDoc->getActiveScene()->findEntityByUUID(hint); entity)
				setSelectedEntity(entity);
		}
	}
}

auto EditorLayer::getSelectedEntity() const -> scene::Entity { return m_sceneHierarchy.getSelectedEntity(); }

void EditorLayer::setSelectedEntity(const scene::Entity iEntity) { m_sceneHierarchy.setSelectedEntity(iEntity); }

void EditorLayer::packScene() {
	if (const auto* doc = activeSceneDocument(); doc != nullptr)
		m_packager.packScene(doc->filePath());
}

void EditorLayer::packGame() {
	if (m_project.isLoaded() && !m_asyncProgress.isActive())
		m_packager.open();
}

void EditorLayer::instantiatePrefab(const std::filesystem::path& iPrefabPath, const std::string& iAssetRelativePath) {
	auto* doc = activeSceneDocument();
	if (doc == nullptr || doc->state() != SceneDocument::State::Edit || !doc->getActiveScene())
		return;
	commands::CommandArgs args{{"path", iPrefabPath.string()}};
	if (!iAssetRelativePath.empty())
		args.set("asset_path", iAssetRelativePath);
	if (const auto result = execute("prefab.instantiate", args); result)
		setSelectedEntity(doc->getActiveScene()->findEntityByUUID(result->entity));
}

void EditorLayer::onContextualHelp() {
	static const std::unordered_map<std::string, std::string> kComponentToPage = {
			{"Transform", "scene"},
			{"Hierarchy", "scene"},
			{"Visibility", "scene"},
			{"Tag", "scene"},
			{"PhysicBody", "physics"},
			{"Trigger", "scene"},
			{"LuaScript", "scripting"},
			{"NativeScript", "scripting"},
			{"Camera", "scene"},
			{"SoundSource", "sound"},
			{"SoundListener", "sound"},
			{"SpriteRenderer", "renderer"},
			{"AnimatedSpriteRenderer", "renderer"},
			{"CircleRenderer", "renderer"},
			{"BackgroundTexture", "renderer"},
			{"Text", "renderer"},
			{"Player", "scene"},
			{"PrefabLink", "scene"},
			{"EntityLink", "scene"},
			{"Canvas", "editor"},
			{"UiRect", "editor"},
			{"UiText", "editor"},
			{"UiImage", "editor"},
			{"UiButton", "editor"},
			{"UiPanel", "editor"},
			{"UiProgressBar", "editor"},
			{"UiSlider", "editor"},
	};
	const auto& hovered = panel::SceneHierarchy::lastHoveredComponentName();
	if (const auto it = kComponentToPage.find(hovered); it != kComponentToPage.end()) {
		m_helpPanel.open(it->second);
		return;
	}
	m_helpPanel.open();
}

}// namespace owl::nest
