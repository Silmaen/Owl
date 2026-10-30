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
#include "commands/Macro.h"
#include "document/ActionMapDocument.h"
#include "document/AnimationDocument.h"
#include "document/CodeEditorDocument.h"
#include "document/DataTableDocument.h"
#include "document/DialogueDocument.h"
#include "document/FontDocument.h"
#include "document/ImageDocument.h"
#include "document/LocalisationDocument.h"
#include "document/MaterialDocument.h"
#include "document/MeshDocument.h"
#include "document/NodeGraphDocument.h"
#include "document/SoundDocument.h"
#include "document/TilemapDocument.h"
#include "document/TilesetDocument.h"
#include "document/VisualScriptDocument.h"

#include <debug/LogSink.h>
#include <gui/FontPreviewCache.h>
#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/component/render.h>
#include <gui/utils.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/Tooltip.h>
#include <physics/PhysicCommand.h>
#include <platform/AtomicFile.h>
#include <platform/UserDirectory.h>
#include <renderer/gpu/RenderCommand.h>
#include <scene/component/components.h>
#include <sound/SoundCommand.h>
#include <sound/SoundSystem.h>

#include <imgui_stdlib.h>
#include <magic_enum/magic_enum.hpp>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <optional>
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

void EditorLayer::followFocusedDocument() {
	const auto* focus = ImGui::GetCurrentContext()->NavWindow;
	if (focus == nullptr || focus->RootWindow == nullptr)
		return;
	if (auto* focused = m_documents.findByWindowName(focus->RootWindow->Name); focused != nullptr)
		m_documents.setActive(focused);
}

void EditorLayer::syncActiveDocumentPanels() {
	auto* doc = activeSceneDocument();
	if (doc == nullptr) {
		static const shared<scene::Scene> s_empty;
		m_sceneHierarchy.setContext(s_empty);
		m_sceneHierarchy.setUndoManager(nullptr);
		m_sceneSettings.setScene(s_empty);
		m_sceneSettings.setUndoManager(nullptr);
		m_mixerPanel.setScene(s_empty);
		m_mixerPanel.setUndoManager(nullptr);
		m_assetImport.setScene(s_empty);
		m_assetImport.setUndoManager(nullptr);
		m_voxelPalette.setScene(s_empty);
	} else {
		m_sceneHierarchy.setContext(doc->getActiveScene());
		m_sceneHierarchy.setUndoManager(&doc->undoManager());
		m_sceneSettings.setScene(doc->getActiveScene());
		m_sceneSettings.setUndoManager(&doc->undoManager());
		m_mixerPanel.setScene(doc->getActiveScene());
		m_mixerPanel.setUndoManager(&doc->undoManager());
		m_assetImport.setScene(doc->getActiveScene());
		m_assetImport.setUndoManager(&doc->undoManager());
		m_voxelPalette.setScene(doc->getActiveScene());
		// The per-document viewport owns its own undo pointer already (set in SceneDocument::onAttach).
	}
	m_sceneSettings.setProject(m_project);
	m_sceneSettings.setCommandRegistry(&m_commands);
	m_mixerPanel.setCommandRegistry(&m_commands);
	m_assetImport.setCommandRegistry(&m_commands);
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

void EditorLayer::handleContentBrowserDrop(const std::filesystem::path& iRelativePath,
										   const std::optional<math::vec3>& iDropPoint) {
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
	else if (kind == AssetKind::Mesh)
		placeMesh(full, {}, iDropPoint);
	else if (const auto target = kind == AssetKind::Material && activeViewport() != nullptr
										 ? activeViewport()->getHoveredEntity()
										 : scene::Entity{};
			 target && target.hasComponent<scene::component::MeshRenderer>())
		runCommandFromUi("material.assign",
						 {{"entity", static_cast<int64_t>(static_cast<uint64_t>(target.getUUID()))},
						  {"slot", int64_t{-1}},
						  {"material", relString}},
						 commands::CommandSource::Gesture);
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
	m_actionRegistry.registerAction("view.pilotCamera", "Camera: Pilot Selected Camera", {},
									onViewport([this](panel::Viewport& ioViewport) -> void {
										if (ioViewport.isPiloted(getSelectedEntity()))
											ioViewport.stopPiloting();
										else
											ioViewport.pilotSelectedCamera();
									}));
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

void EditorLayer::openAssetFromInspector(const std::filesystem::path& iPath, const uint32_t iLine) {
	const auto request = [this, iLine](const std::filesystem::path& iAbsolute) -> void {
		if (iLine > 0)
			m_deferredOpenLines[iAbsolute] = iLine;
		requestDeferredOpen(iAbsolute);
	};
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (const auto candidate = assetsPath / iPath; exists(candidate)) {
			request(candidate);
			return;
		}
	}
	if (exists(iPath))
		request(iPath);
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

void EditorLayer::deleteSelection() {
	if (getState() != State::Edit)
		return;
	const auto ent = getSelectedEntity();
	if (!ent)
		return;
	const auto entity = static_cast<int64_t>(static_cast<uint64_t>(ent.getUUID()));
	if (auto& points = gui::component::getSplineSelection(); points.entity == ent.getUUID() && !points.points.empty()) {
		std::ignore = execute("spline.delete_points",
							  {{"entity", entity}, {"points", commands::formatIndices(points.points)}},
							  commands::CommandSource::Gesture);
		points.clear();
		return;
	}
	setSelectedEntity({});
	std::ignore = execute("entity.delete", {{"entity", entity}}, commands::CommandSource::Gesture);
}

void EditorLayer::bindDataTableCallbacks() {
	m_contentBrowser.setDataTableOpenCallback(
			[this](const std::filesystem::path& iPath) -> void { openDataTableFile(iPath); });
	m_contentBrowser.setDataTableCreateCallback([this](const std::filesystem::path& iPath) -> void {
		runCommandFromUi("table.create", {{"path", iPath.string()}}, commands::CommandSource::Gesture);
		openDataTableFile(iPath);
	});
}

void EditorLayer::bindDialogueCallbacks() {
	m_contentBrowser.setDialogueOpenCallback(
			[this](const std::filesystem::path& iPath) -> void { openDialogueFile(iPath); });
	m_contentBrowser.setDialogueCreateCallback([this](const std::filesystem::path& iPath) -> void {
		runCommandFromUi("dialogue.create", {{"path", iPath.string()}}, commands::CommandSource::Gesture);
		openDialogueFile(iPath);
	});
}

void EditorLayer::onAttach() {
	OWL_PROFILE_FUNCTION()

	app::Application::get().enableDocking();
	app::Application::get().setFrameTimingsEnabled(true);
	renderer::gpu::RenderCommand::setGpuTimestampsEnabled(renderer::gpu::RenderCommand::hasGpuTimestamps());
	m_hotReloadListener = app::Application::get().getHotReload().addListener(
			[this](const std::filesystem::path& iFile) -> void { onAssetFileChanged(iFile); });

	if (const auto f = platform::getUserDirectory() / "OwlNest_settings.yml"; exists(f))
		m_settings.loadFromFile(f);
	else
		m_settingsImportPrompt.open();
	utils::applyTheme(gui::Theme::fromPreset(gui::Theme::presetFromName(m_settings.themePreset)));

	m_dockLayout.setPreset(DockLayout::presetFromName(m_settings.layoutPreset));
	gui::component::setNavigationGraphSwitch(
			{.isShown = [this]() -> bool { return m_settings.showNavGraph; },
			 .setShown = [this](const bool iShown) -> void { m_settings.showNavGraph = iShown; }});

	// Create the initial scene document. Its onAttach() initialises its own Viewport.
	auto& initialDoc = ensureActiveSceneDocument();
	m_sceneHierarchy.setUndoManager(&initialDoc.undoManager());
	m_sceneHierarchy.setCommandRegistry(&m_commands);
	m_commands.setJournal(&getJournal());
	m_consolePanel.getConsole().setHistory(m_settings.consoleHistory);

	utils::buildIconBank();
	{
		std::vector<std::filesystem::path> assetDirectories;
		for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories())
			assetDirectories.push_back(assetsPath);
		m_sampleProject = panel::findSampleProject(assetDirectories).value_or(std::filesystem::path{});
		m_sampleGames = panel::findSampleGames(m_sampleProject);
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
		// Deferred to the next update: the button and the shortcut fire while the running scene is still being drawn.
		if (getState() == State::Play || getState() == State::Pause)
			requestStop();
	});
	m_actionRegistry.registerAction("scene.step", "Step Frame", {input::key::F8, Modifiers::None}, [this]() -> void {
		if (getState() == State::Pause)
			onSceneStep();
	});
	m_actionRegistry.registerAction("scene.play_controls", "Fold Play Controls", {input::key::F10, Modifiers::None},
									[this]() -> void {
										m_settings.playControlsCollapsed = !m_settings.playControlsCollapsed;
									});
	// Entity actions
	m_actionRegistry.registerAction("entity.delete", "Delete Entity", {input::key::Delete, Modifiers::None},
									[this]() -> void { deleteSelection(); });
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
	m_actionRegistry.registerAction("view.console", "Console", {input::key::GraveAccent, Modifiers::Ctrl},
									[]() -> void { ImGui::SetWindowFocus(panelWindow::console); });
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
	gui::component::setEditInGraphHandler(
			[this](const std::string& iFunction) -> void { editHandlerInGraph(iFunction); });
	gui::component::setOpenAssetAtLineHandler([this](const std::filesystem::path& iPath, const uint32_t iLine) -> void {
		openAssetFromInspector(iPath, iLine);
	});
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
	m_contentBrowser.setActionMapOpenCallback(
			[this](const std::filesystem::path& iPath) -> void { openActionMapFile(iPath); });
	m_contentBrowser.setFontOpenCallback([this](const std::filesystem::path& iPath) -> void { openFontFile(iPath); });
	m_contentBrowser.setImageOpenCallback([this](const std::filesystem::path& iPath) -> void { openImageFile(iPath); });
	m_contentBrowser.setMeshPlaceCallback([this](const std::filesystem::path& iPath) -> void { placeMesh(iPath); });
	m_contentBrowser.setMeshOpenCallback([this](const std::filesystem::path& iPath) -> void { openMeshFile(iPath); });
	m_contentBrowser.setMaterialOpenCallback(
			[this](const std::filesystem::path& iPath) -> void { openMaterialFile(iPath); });
	m_contentBrowser.setMaterialCreateCallback([this](const std::filesystem::path& iPath) -> void {
		runCommandFromUi("material.create", {{"path", iPath.string()}}, commands::CommandSource::Gesture);
		openMaterialFile(iPath);
	});
	m_contentBrowser.setLocalisationOpenCallback(
			[this](const std::filesystem::path& iPath) -> void { openLocalisationFile(iPath); });
	m_contentBrowser.setSoundOpenCallback([this](const std::filesystem::path& iPath) -> void { openSoundFile(iPath); });
	bindDataTableCallbacks();
	bindDialogueCallbacks();
	m_contentBrowser.setSoundCreateCallback([this](const std::filesystem::path& iPath) -> void {
		runCommandFromUi("sound.create", {{"path", iPath.string()}}, commands::CommandSource::Gesture);
		openSoundFile(iPath);
	});
	m_contentBrowser.setMacroRunCallback([this](const std::filesystem::path& iPath) -> void {
		runCommandFromUi("macro.run", {{"path", iPath.string()}}, commands::CommandSource::Gesture);
	});
	bindScriptCallbacks();
	m_contentBrowser.setCommandCallback([this](const std::string& iName, const commands::CommandArgs& iArgs) -> bool {
		const auto result = execute(iName, iArgs, commands::CommandSource::Gesture);
		if (!result)
			m_toasts.push(gui::widgets::ToastKind::Error, std::format("{} failed", iName), result.error().message);
		return result.has_value();
	});
	m_contentBrowser.setImportSettingsCallback(
			[this](const std::filesystem::path& iPath) -> void { m_assetImport.show(iPath); });

	mp_mcp = mkUniq<mcp::McpServer>(
			*this, mcp::McpServerOptions{
						   .dispatcher = {.helpRoot = panel::HelpPanel::resolveHelpRoot()},
						   .http = {.onQueued = []() -> void { app::Application::get().getWindow().wakeUp(); }}});

	newScene();
	if (!m_startupProject.empty())
		openProject(m_startupProject);
	if (!m_project.isLoaded())
		m_backstage.open();
}

void EditorLayer::onDetach() {
	OWL_PROFILE_FUNCTION()

	app::Application::get().getHotReload().removeListener(m_hotReloadListener);
	gui::component::setNavigationGraphSwitch({});
	renderer::gpu::RenderCommand::setGpuTimestampsEnabled(false);
	app::Application::get().setFrameTimingsEnabled(false);

	saveProjectSession();
	// Sync keybinding overrides before saving
	m_settings.keybindingOverrides = m_actionRegistry.getOverrides();
	m_settings.consoleHistory = m_consolePanel.getConsole().getHistory();
	m_settings.saveToFile(platform::getUserDirectory() / "OwlNest_settings.yml");

	// Pending MCP requests get 503 and the token file goes before any document closes.
	mp_mcp.reset();
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
	gui::component::setOpenAssetAtLineHandler({});
	gui::component::setEditInGraphHandler({});
}

void EditorLayer::revealCodeLine(const std::filesystem::path& iPath, const uint32_t iLine) {
	if (iLine == 0)
		return;
	for (const auto& docPtr: m_documents.list()) {
		auto* code = docPtr && docPtr->type() == DocumentType::Code ? static_cast<CodeEditorDocument*>(docPtr.get())
																	: nullptr;
		if (code == nullptr || code->filePath() != iPath)
			continue;
		code->revealLine(iLine);
		return;
	}
	for (const auto& docPtr: m_documents.list()) {
		if (auto* graph = dynamic_cast<VisualScriptDocument*>(docPtr.get());
			graph != nullptr && graph->filePath() == iPath)
			graph->revealLine(static_cast<int>(iLine));
	}
}

void EditorLayer::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	gui::FontPreviewCache::get().pumpPending();
	updateFramePacing();

	m_recovery.setInterval(static_cast<float>(m_settings.autosaveIntervalSeconds));
	m_toasts.update(iTimeStep.getSeconds());
	if (const auto failed = app::Application::get().getRendererFallback(); failed.has_value() && !m_fallbackNotified) {
		m_fallbackNotified = true;
		m_toasts.push(gui::widgets::ToastKind::Warning,
					  std::format("{} unavailable, using {}", magic_enum::enum_name(*failed),
								  magic_enum::enum_name(renderer::gpu::RenderCommand::getApi())),
					  app::Application::get().getRendererFallbackReason().empty()
							  ? std::string{"The log tells why. Set the renderer in Parameters to skip the attempt."}
							  : app::Application::get().getRendererFallbackReason());
	}
	gui::UiLayer::setTextScale(static_cast<float>(m_settings.textScalePercent) / 100.f);
	// Before the viewports: a preview render resets the 2D stats they show.
	if (getState() == State::Edit)
		m_contentBrowser.onUpdate();
	if (m_recovery.onUpdate(iTimeStep.getSeconds()) && m_recovery.autosave(m_documents) > 0)
		m_lastAutosave = std::chrono::steady_clock::now();

	m_documents.updateSceneless(iTimeStep);

	// Queued MCP requests run here, on the main thread, before the scene of the frame updates.
	updateMcp();

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

namespace {

// A click on the tab's close X: each document type exposes its own `isOpen`, reset since the close may be cancelled.
template<typename DocumentT>
auto consumeClose(Document* ioDocument) -> bool {
	auto* typed = static_cast<DocumentT*>(ioDocument);
	if (typed->isOpen())
		return false;
	typed->setOpen(true);
	return true;
}

auto consumeTabClose(Document* ioDocument) -> bool {
	switch (ioDocument->type()) {
		case DocumentType::Scene:
			{
				auto& viewport = static_cast<SceneDocument*>(ioDocument)->getViewport();
				if (viewport.isOpen())
					return false;
				viewport.setOpen(true);
				return true;
			}
		case DocumentType::Code:
			return consumeClose<CodeEditorDocument>(ioDocument);
		case DocumentType::NodeGraph:
			return consumeClose<NodeGraphDocument>(ioDocument);
		case DocumentType::Animation:
			return consumeClose<AnimationDocument>(ioDocument);
		case DocumentType::Tilemap:
			return consumeClose<TilemapDocument>(ioDocument);
		case DocumentType::Tileset:
			return consumeClose<TilesetDocument>(ioDocument);
		case DocumentType::Font:
			return consumeClose<FontDocument>(ioDocument);
		case DocumentType::ActionMap:
			return consumeClose<ActionMapDocument>(ioDocument);
		case DocumentType::Image:
			return consumeClose<ImageDocument>(ioDocument);
		case DocumentType::Mesh:
			return consumeClose<MeshDocument>(ioDocument);
		case DocumentType::Material:
			return consumeClose<MaterialDocument>(ioDocument);
		case DocumentType::Sound:
			return consumeClose<SoundDocument>(ioDocument);
		case DocumentType::Localisation:
			return consumeClose<LocalisationDocument>(ioDocument);
		case DocumentType::DataTable:
			return consumeClose<DataTableDocument>(ioDocument);
		case DocumentType::Dialogue:
			return consumeClose<DialogueDocument>(ioDocument);
	}
	return false;
}

}// namespace

void EditorLayer::onImGuiRender(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	std::ignore = m_actionRegistry.dispatchImGuiChords();

	if (auto project = std::exchange(m_pendingProjectSwitch, std::nullopt); project.has_value()) {
		if (project->empty())
			closeProject();
		else
			openProject(*project);
	}

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
			if (const auto line = m_deferredOpenLines.find(path); line != m_deferredOpenLines.end()) {
				revealCodeLine(path, line->second);
				m_deferredOpenLines.erase(line);
			}
			selectDeferredGraphEvent(path);
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
		if (consumeTabClose(doc))
			toClose.push_back(doc->id());
	}
	followFocusedDocument();
	if (m_documents.getActive() != activeBefore)
		syncActiveDocumentPanels();
	for (const auto id: toClose) requestCloseDocument(id);

	renderCloseDocumentModal();
	renderMeshLayerModal();
	handleRecoveryChoice(m_recoveryPrompt.onImGuiRender());
	if (m_settingsImportPrompt.onImGuiRender() == panel::SettingsImportPrompt::Choice::Import)
		importSettings();
	if (const auto request = m_newProjectDialog.onImGuiRender(); request)
		createProject(*request);
	//=============================================================
	m_sceneHierarchy.onImGuiRender();
	// The last window to appear takes the focus, which picks the tab shown in its group.
	m_logPanel.onImGuiRender();
	m_consolePanel.onImGuiRender();
	renderScriptDebug();
	renderStats(iTimeStep);
	m_contentBrowser.onImGuiRender();
	m_parameters.onImGuiRender();
	m_projectSettings.onImGuiRender();
	if (m_projectSettings.consumeActionMapRequest())
		openActionMapEditor();
	if (m_projectSettings.hasResult()) {
		const auto previousMappings = m_project.gamepadMappings;
		m_project = m_projectSettings.consumeResult();
		app::Application::get().getEngineContext().setSimulationSettings(m_project.simulation);
		app::Application::get().getEngineContext().setDefaultHud(m_project.defaultHud);
		if (m_project.gamepadMappings != previousMappings)
			std::ignore = m_project.loadGamepadMappings();

		saveProject();

		syncActiveDocumentPanels();
	}
	m_assetImport.setSelection(m_contentBrowser.getSelectedPath());
	m_assetImport.onImGuiRender();
	m_sceneSettings.onImGuiRender();
	m_mixerPanel.onImGuiRender();
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

void EditorLayer::runCommandFromUi(const std::string& iName, const commands::CommandArgs& iArgs,
								   const commands::CommandSource iSource) {
	const auto result = execute(iName, iArgs, iSource);
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
			.sampleGames = m_sampleGames,
			.newProject = [this]() -> void { newProject(); },
			.openProject = [this]() -> void { openProject(); },
			.openRecent = [this](const std::filesystem::path& iPath) -> void { requestProjectSwitch(iPath); },
			.forgetRecent = [this](const std::filesystem::path& iPath) -> void {
				m_settings.removeRecentProject(iPath);
			},
			.saveProject = [this]() -> void { saveProject(); },
			.saveProjectAs = [this]() -> void { saveProjectAs(); },
			.packGame = [this]() -> void { packGame(); },
			.editorSettings = [this]() -> void { m_settingsPanel.open(); },
			.projectSettings = [this]() -> void { m_projectSettings.open(m_project); },
			.engineSettings = [this]() -> void { m_parameters.open(); },
			.importSettings = [this]() -> void { importSettings(); },
			.help = [this](const std::string& iPage) -> void {
				if (iPage.empty())
					m_helpPanel.open();
				else
					m_helpPanel.open(iPage);
			},
			.closeProject = [this]() -> void { requestProjectSwitch({}); },
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
	if (mp_mcp) {
		info.mcp = getMcpIndicator(mp_mcp->getStatus());
		info.mcpEndpoint = mp_mcp->getEndpoint();
		info.mcpTokenFile = mp_mcp->getTokenFile().string();
	}
	return info;
}

void EditorLayer::renderStatusBar() {
	auto info = buildStatusInfo();
	info.notifications = &m_toasts;
	m_statusBar.onImGuiRender(info);
	if (m_statusBar.consumeOpenLogRequest())
		ImGui::SetWindowFocus(panelWindow::log);
	if (m_statusBar.consumeOpenMcpSettingsRequest())
		m_settingsPanel.openMcp();
}

void EditorLayer::renderStats(const core::Timestep& iTimeStep) {
	if (!m_settings.showStats) {
		m_gpuCounters.reset();
		return;
	}
	ImGui::Begin(panelWindow::title(gui::icon::gauge, "Stats", panelWindow::stats).c_str());
	ImGui::Text("%s", std::format("Displayed FPS: {:.1f} (last frame {:.2f} ms)", m_frameMeter.getDisplayFps(),
								  iTimeStep.getMilliseconds())
							  .c_str());
	const auto gpu = m_frameMeter.getGpuMs();
	ImGui::Text("%s", std::format("Frame cost: {:.2f} ms CPU, {}", m_frameMeter.getWorkMs(),
								  gpu.has_value() ? std::format("{:.2f} ms GPU", *gpu) : std::string{"GPU not timed"})
							  .c_str());
	ImGui::Text("%s", std::format("Possible FPS: {:.0f}", m_frameMeter.getPotentialFps()).c_str());
	gui::widgets::itemTooltip(
			"Possible FPS", nullptr,
			"1000 / the slower of CPU and GPU. Capped by the display, the CPU and the GPU lower their clocks: a frame "
			"costs more than at full load, so this is no capacity. Measure at Full Load gives the real rate.");
	if (m_fullLoadProbe.isRunning()) {
		ImGui::TextDisabled("Measuring at full load...");
	} else {
		if (gui::widgets::button("Measure at Full Load", {.glyph = gui::icon::gauge, .label = "Measure at Full Load"}))
			startFullLoadMeasure();
		gui::widgets::itemTooltip("Measure at Full Load", nullptr,
								  "Lift vsync, the frame rate limit and the idle wait for 3 s: 1 s for the clocks to "
								  "rise, then 2 s measured. The fans may spin up meanwhile.");
	}
	if (const auto& result = m_fullLoadProbe.getResult(); result.has_value())
		ImGui::Text("%s", std::format("Full load: {:.0f} fps, {:.2f} ms CPU, {}", result->fps, result->workMs,
									  result->gpuMs.has_value() ? std::format("{:.2f} ms GPU", *result->gpuMs)
																: std::string{"GPU not timed"})
								  .c_str());
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

void EditorLayer::updateFramePacing() {
	auto& app = app::Application::get();
	const auto now = panel::FrameMeter::clock::now();
	const double workMs = app.getLastFrameTimings().workMs;
	m_frameMeter.addFrame(workMs, now);
	for (const auto& timing: renderer::gpu::RenderCommand::popGpuFrameTimings()) {
		m_frameMeter.addGpu(timing.busyMs);
		m_fullLoadProbe.addGpu(timing.busyMs);
	}
	if (m_fullLoadProbe.isRunning()) {
		if (m_fullLoadProbe.addFrame(workMs, now))
			app.getWindow().setVSync(app.getInitParams().vSync);
		app.setFrameRateLimit(0);
		return;
	}
	// A detached panel is a window of its own (X11): its focus keeps the editor in the foreground.
	const bool focused =
			app.getWindow().isFocused() ||
			(ImGui::GetCurrentContext() != nullptr &&
			 std::ranges::any_of(ImGui::GetPlatformIO().Viewports, [](const ImGuiViewport* iViewport) -> bool {
				 return (iViewport->Flags & ImGuiViewportFlags_IsFocused) != 0;
			 }));
	// A play driven from MCP keeps its pace when the caller's window has the focus.
	const bool background = !focused && !m_playDriver.isActive();
	const int limit = background && m_settings.backgroundFrameRateLimit > 0 ? m_settings.backgroundFrameRateLimit
																			: m_settings.frameRateLimit;
	app.setFrameRateLimit(static_cast<uint32_t>(std::max(limit, 0)));
	if (m_settings.idleWaitMs > 0 && isIdle())
		app.requestIdleWait(std::chrono::milliseconds{m_settings.idleWaitMs});
}

void EditorLayer::startFullLoadMeasure() {
	m_fullLoadProbe.start(panel::FullLoadProbe::clock::now());
	app::Application::get().getWindow().setVSync(false);
}

auto EditorLayer::isIdle() const -> bool {
	if (m_playDriver.isActive() || !m_toasts.getToasts().empty() || gui::FontPreviewCache::get().hasPending() ||
		app::Application::get().getTaskScheduler().hasPendingTasks())
		return false;
	if (const auto [ready, requested] = m_contentBrowser.getPreviewProgress(); ready < requested)
		return false;
	if (std::ranges::any_of(m_documents.list(), [](const auto& iDocument) -> bool {
			return iDocument && iDocument->type() == DocumentType::Scene &&
				   static_cast<const SceneDocument*>(iDocument.get())->state() != SceneDocument::State::Edit;
		}))
		return false;
	if (ImGui::GetCurrentContext() == nullptr)
		return true;
	if (ImGui::IsAnyItemActive() || ImGui::IsAnyMouseDown())
		return false;
	for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
		// A connected gamepad drives no editor gesture; the mouse buttons are checked above.
		if (key >= ImGuiKey_GamepadStart && key <= ImGuiKey_GamepadRStickDown)
			continue;
		if (ImGui::IsKeyDown(static_cast<ImGuiKey>(key)))
			return false;
	}
	return true;
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
	// While the right button flies the view, the letter keys move the camera.
	if (const auto* viewport = activeViewport(); viewport != nullptr && viewport->isFlying())
		return false;
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

void EditorLayer::renderScriptDebug() {
	auto* doc = activeSceneDocument();
	const bool running = doc != nullptr &&
						 (doc->state() == SceneDocument::State::Play || doc->state() == SceneDocument::State::Pause);
	if (running) {
		if (auto hit = script::ScriptDebugger::takeHit(); hit.has_value()) {
			if (doc->state() == SceneDocument::State::Play)
				onScenePause();
			OWL_INFO("Script: Breakpoint reached in {} at line {}.", hit->script, hit->line)
			m_scriptDebugPanel.setHit(std::move(*hit));
		}
	} else if (m_scriptDebugPanel.getHit().has_value()) {
		m_scriptDebugPanel.clearHit();
	}
	const auto& hit = m_scriptDebugPanel.getHit();
	for (const auto& docPtr: m_documents.list()) {
		auto* graph = dynamic_cast<VisualScriptDocument*>(docPtr.get());
		if (graph == nullptr)
			continue;
		const auto* instance =
				running ? panel::findScriptInstance(*doc->getActiveScene(), m_sceneHierarchy.getSelectedEntity(),
													graph->filePath())
						: nullptr;
		std::optional<int> stopped;
		if (hit.has_value() && script::ScriptDebugger::isSameScript(hit->script, graph->filePath().string()))
			stopped = hit->line;
		graph->setDebugState(instance == nullptr ? std::vector<script::WatchValue>{} : instance->getProbeValues(),
							 stopped);
	}
	const panel::ScriptDebugPanel::Context context{.scene = running ? doc->getActiveScene().get() : nullptr,
												   .selected = running ? m_sceneHierarchy.getSelectedEntity()
																	   : scene::Entity{},
												   .paused = running && doc->state() == SceneDocument::State::Pause};
	switch (m_scriptDebugPanel.onImGuiRender(context)) {
		case panel::ScriptDebugPanel::Request::Continue:
			m_scriptDebugPanel.clearHit();
			onSceneResume();
			break;
		case panel::ScriptDebugPanel::Request::OpenHit:
			if (hit.has_value())
				openAssetFromInspector(hit->script, static_cast<uint32_t>(hit->line));
			break;
		case panel::ScriptDebugPanel::Request::None:
			break;
	}
}

void EditorLayer::bindScriptCallbacks() {
	m_contentBrowser.setEditorScriptRunCallback(
			[this](const std::filesystem::path& iPath) -> void { m_consolePanel.runEditorScript(iPath); });
	m_contentBrowser.setMacroExportCallback([this](const std::filesystem::path& iPath) -> void {
		const auto macro = commands::Macro::load(iPath);
		if (!macro) {
			m_toasts.push(gui::widgets::ToastKind::Error, "Export to Lua failed", macro.error().message);
			return;
		}
		auto target = iPath;
		target.replace_extension(".lua");
		for (uint32_t counter = 2; exists(target); ++counter)
			target = iPath.parent_path() / std::format("{}_{}.lua", iPath.stem().string(), counter);
		if (!platform::writeFileAtomic(target, session::LuaConsole::macroToLua(macro->getLines(), m_commands))) {
			m_toasts.push(gui::widgets::ToastKind::Error, "Export to Lua failed", target.string());
			return;
		}
		openAssetFile(target);
	});
}

void EditorLayer::selectDeferredGraphEvent(const std::filesystem::path& iPath) {
	for (auto event = m_deferredGraphEvents.begin(); event != m_deferredGraphEvents.end();) {
		if (!script::ScriptDebugger::isSameScript(iPath.string(), event->first)) {
			++event;
			continue;
		}
		for (const auto& docPtr: m_documents.list())
			if (auto* graph = dynamic_cast<VisualScriptDocument*>(docPtr.get());
				graph != nullptr && graph->filePath() == iPath)
				std::ignore = graph->ensureCustomEvent(event->second);
		event = m_deferredGraphEvents.erase(event);
	}
}

void EditorLayer::editHandlerInGraph(const std::string& iFunction) {
	auto* doc = activeSceneDocument();
	if (doc == nullptr || doc->state() != SceneDocument::State::Edit) {
		OWL_WARN("EditorLayer: Stop the game to edit a handler in a graph.")
		return;
	}
	const auto scene = doc->getActiveScene();
	auto script = scene->getSceneScript().scriptPath;
	if (script.empty()) {
		const auto graph = VisualScriptDocument::prepareSceneGraph(doc->filePath());
		if (!graph.has_value())
			return;
		const commands::CommandTarget target{.registry = &m_commands, .scene = scene, .undo = &doc->undoManager()};
		std::ignore = target.execute("scene.set_script", {{"script", std::format("scriptPath: {}", graph->second)}});
		script = graph->second;
	}
	m_deferredGraphEvents[script] = iFunction;
	openAssetFromInspector(script);
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
							  {{"entity", static_cast<int64_t>(static_cast<uint64_t>(selectedEntity.getUUID()))}},
							  commands::CommandSource::Gesture);
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
	if (m_asyncProgress.isActive()) {
		OWL_CORE_WARN("EditorLayer: Cannot pack the game while another operation runs.")
		return;
	}
	if (m_project.isLoaded())
		m_packager.open();
}

void EditorLayer::instantiatePrefab(const std::filesystem::path& iPrefabPath, const std::string& iAssetRelativePath) {
	auto* doc = activeSceneDocument();
	if (doc == nullptr || doc->state() != SceneDocument::State::Edit || !doc->getActiveScene())
		return;
	commands::CommandArgs args{{"path", iPrefabPath.string()}};
	if (!iAssetRelativePath.empty())
		args.set("asset_path", iAssetRelativePath);
	if (const auto result = execute("prefab.instantiate", args, commands::CommandSource::Gesture); result)
		setSelectedEntity(doc->getActiveScene()->findEntityByUUID(result->entity));
}

void EditorLayer::placeMesh(const std::filesystem::path& iMeshPath, const std::string& iAssetRelativePath,
							const std::optional<math::vec3>& iPosition) {
	auto* doc = activeSceneDocument();
	if (doc == nullptr || doc->state() != SceneDocument::State::Edit || !doc->getActiveScene())
		return;
	const auto& layers = renderer::Renderer::getRenderStack().getLayers();
	const auto meshLayer = std::ranges::find_if(layers, [](const shared<renderer::RenderLayer>& iLayer) -> bool {
		return std::string_view{iLayer->getTypeKey()} == "RendererMesh";
	});
	if (meshLayer == layers.end()) {
		m_pendingMesh = PendingMesh{.path = iMeshPath, .assetPath = iAssetRelativePath, .position = iPosition};
		m_openMeshLayerModal = true;
		return;
	}
	createMeshEntity(iMeshPath, iAssetRelativePath, iPosition,
					 meshLayer == layers.begin() ? std::string{} : (*meshLayer)->getName());
}

void EditorLayer::createMeshEntity(const std::filesystem::path& iMeshPath, const std::string& iAssetRelativePath,
								   const std::optional<math::vec3>& iPosition, const std::string& iLayer) {
	auto* doc = activeSceneDocument();
	if (doc == nullptr || doc->state() != SceneDocument::State::Edit || !doc->getActiveScene())
		return;
	commands::CommandArgs args{{"path", iMeshPath.string()}};
	if (!iAssetRelativePath.empty())
		args.set("asset_path", iAssetRelativePath);
	if (iPosition)
		args.set("translation", *iPosition);
	if (!iLayer.empty())
		args.set("layer", iLayer);
	if (const auto result = execute("mesh.create", args, commands::CommandSource::Gesture); result)
		setSelectedEntity(doc->getActiveScene()->findEntityByUUID(result->entity));
}

void EditorLayer::renderMeshLayerModal() {
	static constexpr auto kTitle = "No Mesh Layer";
	static constexpr auto kLayerName = "mesh_world";
	if (m_openMeshLayerModal) {
		ImGui::OpenPopup(kTitle);
		m_openMeshLayerModal = false;
	}
	if (!ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;
	if (!m_pendingMesh) {
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}
	ImGui::Text("No RendererMesh layer of the render stack draws '%s'.",
				m_pendingMesh->path.filename().string().c_str());
	const auto projectLayer = std::ranges::find_if(m_project.rendererStack.entries, [](const auto& iEntry) -> bool {
		return iEntry.typeKey == "RendererMesh";
	});
	const bool disabledInScene = projectLayer != m_project.rendererStack.entries.end();
	if (disabledInScene)
		ImGui::TextDisabled("The project layer '%s' is disabled in this scene (Scene Settings).",
							projectLayer->name.c_str());
	else
		ImGui::TextDisabled("The layer is added to the project settings, after the first layer.");
	ImGui::Spacing();
	const float width = ImGui::GetFontSize() * 9.f;
	if (!disabledInScene &&
		gui::widgets::button("Add the layer", {.glyph = gui::icon::layers, .label = "Add the layer", .width = width})) {
		const auto pending = *m_pendingMesh;
		m_pendingMesh.reset();
		ImGui::CloseCurrentPopup();
		const commands::CommandArgs args{{"type", std::string{"RendererMesh"}}, {"name", std::string{kLayerName}}};
		if (const auto added = execute("project.add_render_layer", args, commands::CommandSource::Gesture); added)
			createMeshEntity(pending.path, pending.assetPath, pending.position, kLayerName);
		else
			m_toasts.push(gui::widgets::ToastKind::Error, "Mesh layer not added", added.error().message);
	}
	if (!disabledInScene)
		ImGui::SameLine();
	if (gui::widgets::button("Place anyway", {.glyph = gui::icon::plus, .label = "Place anyway", .width = width})) {
		const auto pending = *m_pendingMesh;
		m_pendingMesh.reset();
		ImGui::CloseCurrentPopup();
		createMeshEntity(pending.path, pending.assetPath, pending.position, {});
	}
	ImGui::SameLine();
	if (gui::widgets::button("Cancel",
							 {.glyph = gui::icon::x, .label = "Cancel", .width = ImGui::GetFontSize() * 7.f})) {
		m_pendingMesh.reset();
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
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
			{"ReverbZone", "sound"},
			{"AdaptiveMusic", "sound"},
			{"SpriteRenderer", "renderer"},
			{"AnimatedSpriteRenderer", "renderer"},
			{"Isometric Object", "renderer"},
			{"CircleRenderer", "renderer"},
			{"MeshRenderer", "renderer"},
			{"Light", "renderer"},
			{"Light2D", "renderer"},
			{"BackgroundTexture", "renderer"},
			{"Text", "renderer"},
			{"Player", "scene"},
			{"PrefabLink", "scene"},
			{"EntityLink", "scene"},
			{"Canvas", "editor_hud"},
			{"UiRect", "editor_hud"},
			{"UiText", "editor_hud"},
			{"UiImage", "editor_hud"},
			{"UiButton", "editor_hud"},
			{"UiPanel", "editor_hud"},
			{"UiProgressBar", "editor_hud"},
			{"UiSlider", "editor_hud"},
			{"UiWindow", "editor_hud"},
			{"UiLayout", "editor_hud"},
			{"UiTooltip", "editor_hud"},
			{"UiToolCursor", "editor_hud"},
			{"UiFocus", "editor_hud"},
			{"UiScrollView", "editor_hud"},
			{"UiList", "editor_hud"},
	};
	const auto& hovered = panel::SceneHierarchy::lastHoveredComponentName();
	if (const auto it = kComponentToPage.find(hovered); it != kComponentToPage.end()) {
		m_helpPanel.open(it->second);
		return;
	}
	m_helpPanel.open();
}

}// namespace owl::nest
