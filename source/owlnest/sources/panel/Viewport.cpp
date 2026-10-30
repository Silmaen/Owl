/**
 * @file Viewport.cpp
 * @author Silmaen
 * @date 10/16/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Viewport.h"

#include "PlayOverlay.h"

#include "../DockLayout.h"

#include "EditorLayer.h"
#include "document/DocumentManager.h"
#include "document/SceneDocument.h"

#include "../UndoManager.h"
#include "../commands/ComponentCommands.h"
#include "../commands/VoxelCommands.h"

#include <app/Application.h>
#include <data/voxel/VoxelRaycast.h>
#include <scene/component/Animator.h>
#include <scene/component/Light.h>
#include <scene/component/MeshRenderer.h>
#include <scene/component/ReflectionProbe.h>
#include <scene/component/ReverbZone.h>
#include <scene/component/VoxelWorld.h>

#include <data/geometry/MeshLoader.h>
#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/Tooltip.h>
#include <math/linAlgebra.h>
#include <owlgui.h>
#include <renderer/ClipSpace.h>
#include <renderer/MeshCache.h>
#include <renderer/RenderLayer.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <renderer/RendererIsometricLayer.h>
#include <renderer/RendererMesh.h>
#include <scene/SceneSerializer.h>
#include <scene/TilemapAsset.h>
#include <scene/TilemapTerrain.h>
#include <scene/Tileset.h>
#include <scene/WorldStreamer.h>
#include <scene/component/RaycastDoor.h>
#include <scene/component/RaycastPushWall.h>
#include <scene/component/RendererTag.h>
#include <scene/component/Tilemap.h>
#include <scene/component/Visibility.h>
#include <scene/nav/NavDebugDraw.h>
#include <scene/nav/NavWorld.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <limits>
#include <numbers>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest::panel {

namespace {
constexpr math::vec4 g_selectionColor{0.95f, 0.55f, 0.f, 1.f};

auto makeWindowTitle(const SceneDocument& iDoc, const EditorLayer* iParent) -> std::string {
	std::string base;
	if (iParent != nullptr)
		base = iParent->getDocumentManager().displayTitleFor(&iDoc);
	if (base.empty())
		base = iDoc.title();
	return documentWindowTitle(DocumentType::Scene, base, static_cast<uint64_t>(iDoc.id()));
}

struct TilemapSnapInfo {
	math::vec3 worldTranslation{0.f, 0.f, 0.f};///< World-space translation of the tilemap entity.
	float cellSize = 1.f;///< Cell size in world units.
	uint32_t width = 1;///< Grid width in cells.
	uint32_t height = 1;///< Grid height in cells.
};

auto findFirstTilemapInfo(const scene::Scene* iScene) -> std::optional<TilemapSnapInfo> {
	if (iScene == nullptr)
		return std::nullopt;
	const auto view = iScene->registry.view<scene::component::Tilemap>();
	for (const auto entity: view) {
		const auto& tilemap = view.get<scene::component::Tilemap>(entity);
		if (!tilemap.asset)
			continue;
		const scene::Entity sceneEntity{entity, const_cast<scene::Scene*>(iScene)};
		TilemapSnapInfo info;
		info.worldTranslation = iScene->getWorldTransform(sceneEntity).translation();
		info.cellSize = std::max(0.0001f, tilemap.asset->cellSize);
		info.width = std::max(1u, tilemap.asset->width);
		info.height = std::max(1u, tilemap.asset->height);
		return info;
	}
	return std::nullopt;
}

auto resolveTranslationSnapStep(const EditorSettings& iSettings, const std::optional<TilemapSnapInfo>& iTilemap)
		-> float {
	if (iSettings.snapAutoFromTilemap && iTilemap.has_value())
		return std::max(0.0001f, iTilemap->cellSize * iSettings.snapMultiplier);
	return std::max(0.0001f, iSettings.snapStep);
}

auto snapToTilemapCellCenter(const math::vec3& iWorldTranslation, const TilemapSnapInfo& iTilemap, const float iStep)
		-> math::vec3 {
	const float offsetX = (iTilemap.width % 2 == 0) ? iTilemap.cellSize * 0.5f : 0.f;
	const float offsetY = (iTilemap.height % 2 == 0) ? iTilemap.cellSize * 0.5f : 0.f;
	const float relX = iWorldTranslation.x() - iTilemap.worldTranslation.x() - offsetX;
	const float relY = iWorldTranslation.y() - iTilemap.worldTranslation.y() - offsetY;
	const float kX = std::round(relX / iStep);
	const float kY = std::round(relY / iStep);
	return math::vec3{
			iTilemap.worldTranslation.x() + offsetX + kX * iStep,
			iTilemap.worldTranslation.y() + offsetY + kY * iStep,
			iWorldTranslation.z(),
	};
}

// Draws the reverb zones of a scene, the selected one with the edge of its fade (defined with the light markers).
void drawReverbZoneMarkers(const shared<scene::Scene>& iScene, const scene::Entity& iSelected);

}// namespace

Viewport::Viewport() : BasePanel{"SceneView"} {}

Viewport::~Viewport() = default;

void Viewport::setDocument(SceneDocument* iDocument) {
	mp_document = iDocument;
	if (iDocument != nullptr) {
		// Stable ImGui id (after ###) so ImGui preserves dock state across renames.
		m_name = makeWindowTitle(*iDocument, m_parent);
	} else {
		m_name = "SceneView";
	}
}

void Viewport::onRender() {
	if (mp_document == nullptr) {
		BasePanel::onRender();
		return;
	}

	m_name = makeWindowTitle(*mp_document, m_parent);

	ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;
	if (mp_document->isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;

	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	const bool visible = ImGui::Begin(m_name.c_str(), &m_pOpen, flags);
	m_visible = visible;
	if (m_parent != nullptr)
		m_parent->renderDocumentTabMenu(*mp_document);
	m_focused = ImGui::IsWindowFocused();
	m_hovered = ImGui::IsWindowHovered();
	app::Application::get().getImGuiLayer()->blockEvents(!m_focused && !m_hovered);

	// Focus transition → this viewport's document becomes the active document.
	if (m_focused && !m_wasFocused && m_parent != nullptr)
		m_parent->getDocumentManager().setActive(mp_document);
	m_wasFocused = m_focused;

	if (visible) {
		const auto cursor = ImGui::GetCursorScreenPos();
		const auto avail = ImGui::GetContentRegionAvail();
		const float safeW = std::max(0.f, avail.x);
		const float safeH = std::max(0.f, avail.y);
		m_lower = {cursor.x, cursor.y};
		m_upper = {cursor.x + safeW, cursor.y + safeH};
		m_size = {static_cast<uint32_t>(safeW), static_cast<uint32_t>(safeH)};
		if (mp_document->isHudEditing())
			mp_document->getHudEditor().onRender(*mp_document);
		else
			onRenderInternal();
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void Viewport::attach() {
	const renderer::gpu::FramebufferSpecification specs{
			.size = {1280, 720},
			.attachments =
					{
							{.format = renderer::gpu::AttachmentSpecification::Format::Surface,
							 .tiling = renderer::gpu::AttachmentSpecification::Tiling::Optimal},
							{.format = renderer::gpu::AttachmentSpecification::Format::RedInteger,
							 .tiling = renderer::gpu::AttachmentSpecification::Tiling::Optimal},
							{.format = renderer::gpu::AttachmentSpecification::Format::Depth32F,
							 .tiling = renderer::gpu::AttachmentSpecification::Tiling::Optimal},
					},
			.samples = 1,
			.swapChainTarget = false,
			.debugName = "editor"};
	m_framebuffer = renderer::gpu::Framebuffer::create(specs);
	m_editorCamera = renderer::CameraEditor(30.0f, 1.778f, 0.1f, 1000.0f);
}

void Viewport::detach() {
	OWL_TRACE("Viewport: framebuffer freed.")
	m_framebuffer.reset();
	m_previewFramebuffer.reset();
}

void Viewport::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	if (mp_document == nullptr)
		return;
	const auto& activeScene = mp_document->getActiveScene();
	if (!activeScene)
		return;
	if (mp_document->isHudEditing()) {
		mp_document->getHudEditor().onUpdate(*mp_document->getEditorScene());
		return;
	}

	// resize frame buffer if needed.
	if (const auto spec = m_framebuffer->getSpecification(); getSize().surface() > 0 && getSize() != spec.size) {
		m_framebuffer->resize(getSize());
		m_editorCamera.setViewportSize(getSize());
	}
	activeScene->onViewportResize(getSize());

	if (mp_document->state() == SceneDocument::State::Edit)
		updateIsometricMode(activeScene);
	if (m_pilot.isActive() && (mp_document->state() != SceneDocument::State::Edit || m_isometric))
		stopPiloting();
	if (m_parent != nullptr)
		m_navTool.sync(m_parent->getSettings().showNavGraph,
					   mp_document->state() == SceneDocument::State::Edit && m_isometric);
	if (mp_document->state() == SceneDocument::State::Edit && m_isometric) {
		if (isHovered())
			handleIsometricInput();
		updateIsometricCursor(activeScene);
		pickNavigationCells();
		handleNavigationTool(activeScene);
	} else if (mp_document->state() == SceneDocument::State::Edit) {
		if (m_parent != nullptr) {
			const auto& settings = m_parent->getSettings();
			m_editorCamera.setNavigation({.rotate = static_cast<float>(settings.cameraRotatePercent) / 100.f,
										  .pan = static_cast<float>(settings.cameraPanPercent) / 100.f,
										  .zoom = static_cast<float>(settings.cameraZoomPercent) / 100.f,
										  .invertY = settings.cameraInvertY,
										  .deadZone = static_cast<float>(settings.cameraDeadZone),
										  .mouseLook = !m_parent->getVoxelBrush().active});
		}
		if (m_pilot.isActive())
			updatePilot(activeScene, iTimeStep);
		else if (isHovered())
			m_editorCamera.onUpdate(iTimeStep);
	}

	// A viewport hidden behind another tab draws nothing; a running scene still steps.
	if (!m_visible && (m_parent == nullptr || !m_parent->rendersHiddenViewports())) {
		if (mp_document->state() == SceneDocument::State::Play) {
			activeScene->onUpdateRuntime(iTimeStep, /*iRender=*/false);
			handleRuntimeRequests(activeScene);
		}
		return;
	}

	// Render
	m_framebuffer->bind();
	renderer::gpu::RenderCommand::setClearColor({0.1f, 0.1f, 0.1f, 1});
	renderer::gpu::RenderCommand::clear();

	// Get Mouse position.
	auto [mx, my] = ImGui::GetMousePos();
	mx -= m_lower.x();
	my -= m_lower.y();
	const math::vec2 viewportSizeInternal = m_upper - m_lower;
	if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::OpenGL)
		my = viewportSizeInternal.y() - my;
	const int mouseX = static_cast<int>(mx);
	const int mouseY = static_cast<int>(my);

	// Clear our entity ID attachment to -1
	m_framebuffer->clearAttachment(1, -1);

	// Never leave the cursor captured outside Play (e.g. after Stop), or it would be stuck hidden in the editor.
	if (mp_document->state() != SceneDocument::State::Play &&
		app::Application::get().getWindow().getCursorMode() == window::CursorMode::Disabled) {
		app::Application::get().getWindow().setCursorMode(window::CursorMode::Normal);
		ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
	}

	switch (mp_document->state()) {
		case SceneDocument::State::Edit:
			{
				handleVoxelBrush(activeScene);
				if (m_parent != nullptr) {
					const float ambient = m_parent->getSettings().editorAmbient;
					activeScene->setEditorAmbient({ambient, ambient, ambient});
					activeScene->setEditorHudVisible(m_parent->getSettings().showHud);
				}
				followWorldOrigin(*activeScene);
				updateSelectionOutline(activeScene);
				activeScene->onUpdateEditor(iTimeStep, getActiveCamera());
				break;
			}
		case SceneDocument::State::Play:
			{
				// Click to capture the cursor (hidden/locked), Esc to free; no focus/hover release (the captured cursor's unbounded virtual position would read as "left the viewport" and cancel capture every frame). NoMouseCursorChange stops ImGui from re-showing the cursor while captured.
				auto& window = app::Application::get().getWindow();
				const auto cursorMode = window.getCursorMode();
				// A scene that stops wanting capture (level change, voxel player gone) gives the cursor back too.
				const auto nextMode = window::nextCursorMode(cursorMode, activeScene->wantsCursorCapture(),
															 m_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left),
															 input::Input::isKeyPressed(input::key::Escape));
				if (nextMode != cursorMode) {
					window.setCursorMode(nextMode);
					if (nextMode == window::CursorMode::Disabled)
						ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
					else
						ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
				}
				// The HUD is Y-down from the top left: undo the OpenGL flip of the picking coordinates.
				const bool flipped = renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::OpenGL;
				const math::vec2 vpMouse = {mx, flipped ? viewportSizeInternal.y() - my : my};
				// A press reaches the game only when it starts on the game: not on an overlay control, not on another panel.
				const bool captured = window.getCursorMode() == window::CursorMode::Disabled;
				const bool overOverlay =
						anyContains(m_overlayRects, m_overlayCount, ImGui::GetMousePos().x, ImGui::GetMousePos().y);
				const bool mousePressed = m_playGate.filter(ImGui::IsMouseDown(ImGuiMouseButton_Left),
															captured || m_hovered, !captured && overOverlay);
				const math::vec2 wheel = m_hovered ? math::vec2{ImGui::GetIO().MouseWheelH, ImGui::GetIO().MouseWheel}
												   : math::vec2{0.f, 0.f};
				scene::UiInputSystem::update(activeScene.get(), m_framebuffer->getSpecification().size, vpMouse,
											 mousePressed, iTimeStep.getSeconds(), wheel);
				activeScene->onUpdateRuntime(iTimeStep);
				handleRuntimeRequests(activeScene);
				break;
			}
		case SceneDocument::State::Pause:
			{
				if (mp_document->consumeStepRequest()) {
					// One step of Pause is one simulation tick, whatever the frame duration.
					activeScene->onUpdateRuntime(activeScene->getSimulationClock().getStepTimestep());
					mp_document->handleTeleportRequest(getSize());
				} else {
					activeScene->onRenderRuntime();
				}
				break;
			}
	}

	if (mouseX >= 0 && mouseY >= 0 && mouseX < static_cast<int>(viewportSizeInternal.x()) &&
		mouseY < static_cast<int>(viewportSizeInternal.y())) {
		// The pick may lag a frame or two (OpenGL reads asynchronously): the entity can be gone since.
		const int pixelData = m_framebuffer->readPixel(1, mouseX, mouseY);
		const auto handle = static_cast<entt::entity>(pixelData);
		m_hoveredEntity = pixelData == -1 || !activeScene->registry.valid(handle)
								  ? scene::Entity()
								  : scene::Entity(handle, activeScene.get());
		mp_hoveredScene = activeScene.get();
	}
	renderOverlay();

	m_framebuffer->unbind();
	renderCameraPreview(activeScene);
}

void Viewport::handleRuntimeRequests(const shared<scene::Scene>& iScene) {
	// Handle quit request from Lua (scene.quit()) → request stop.
	if (iScene->quitRequested)
		mp_document->requestStop();
	if (auto pending = app::Application::get().getEngineContext().getScreenTransition().pendingLoadPath(); pending) {
		iScene->teleportRequest.pending = true;
		iScene->teleportRequest.levelName = *pending;
		iScene->teleportRequest.targetName.clear();
		iScene->teleportRequest.initialVelocity = {0.f, 0.f};
		iScene->teleportRequest.rotationDelta = 0.f;
	}
	mp_document->handleTeleportRequest(getSize());
	mp_document->handleSaveLoadRequest(getSize());
}

void Viewport::renderCameraPreview(const shared<scene::Scene>& iScene) {
	m_previewEntity = core::UUID{0};
	if (m_parent == nullptr || !m_parent->getSettings().showCameraPreview || m_isometric ||
		mp_document->state() != SceneDocument::State::Edit || !canGoToCameraViewpoint())
		return;
	const scene::Entity selected = m_parent->getSelectedEntity();
	if (isPiloted(selected) || getSize().surface() == 0)
		return;
	const auto viewport = getSize();
	const float width = std::clamp(static_cast<float>(viewport.x()) * 0.28f, 160.f, 480.f);
	const math::vec2ui size{static_cast<uint32_t>(width),
							std::max(1u, static_cast<uint32_t>(width / viewport.ratio()))};
	if (!m_previewFramebuffer) {
		using Attachment = renderer::gpu::AttachmentSpecification;
		m_previewFramebuffer = renderer::gpu::Framebuffer::create(
				{.size = size,
				 .attachments = {{.format = Attachment::Format::Surface, .tiling = Attachment::Tiling::Optimal},
								 {.format = Attachment::Format::RedInteger, .tiling = Attachment::Tiling::Optimal},
								 {.format = Attachment::Format::Depth32F, .tiling = Attachment::Tiling::Optimal}},
				 .samples = 1,
				 .swapChainTarget = false,
				 .debugName = "camera_preview"});
	} else if (m_previewFramebuffer->getSpecification().size != size) {
		m_previewFramebuffer->resize(size);
	}
	m_previewCamera = selected.getComponent<scene::component::Camera>().camera;
	m_previewCamera.setViewportSize(size);
	m_previewCamera.setTransform(iScene->getWorldTransform(selected)());
	m_previewFramebuffer->bind();
	renderer::gpu::RenderCommand::setClearColor({0.1f, 0.1f, 0.1f, 1});
	renderer::gpu::RenderCommand::clear();
	m_previewFramebuffer->clearAttachment(1, -1);
	iScene->renderEditorView(m_previewCamera);
	m_previewFramebuffer->unbind();
	m_previewEntity = selected.getUUID();
}


void Viewport::onRenderInternal() {
	OWL_PROFILE_FUNCTION()

	if (const auto tex = gui::imTexture(m_framebuffer, 0); tex.has_value())
		ImGui::Image(tex.value(), gui::vec(getSize()), gui::vec(m_framebuffer->getLowerData()),
					 gui::vec(m_framebuffer->getUpperData()));
	if (m_parent != nullptr) {
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				const auto* path = static_cast<const char*>(payload->Data);
				m_parent->handleContentBrowserDrop(std::filesystem::path{path},
												   mp_document != nullptr ? getDropPoint(mp_document->getActiveScene())
																		  : std::nullopt);
			}
			ImGui::EndDragDropTarget();
		}
	}
	renderGizmo();
	renderOverlayToolbar();
	renderCameraGizmo();
	renderCameraPreviewInset();
	renderContextMenu();
}

void Viewport::renderContextMenu() {
	if (mp_document == nullptr || m_parent == nullptr || mp_document->state() != SceneDocument::State::Edit)
		return;
	// A right click that did not drag (the camera pans with Ctrl or Alt + right drag; the voxel brush erases).
	const auto& io = ImGui::GetIO();
	constexpr float clickTravel = 4.f;
	const bool click = io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < clickTravel * clickTravel;
	if (m_hovered && click && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !io.KeyCtrl && !io.KeyAlt &&
		!m_parent->getVoxelBrush().active && !gui::Guizmo::isOver()) {
		const auto camera = pickMarker();
		m_parent->setSelectedEntity(camera ? camera : getHoveredEntity());
		ImGui::OpenPopup("##viewportMenu");
	}
	if (!ImGui::BeginPopup("##viewportMenu"))
		return;
	if (const scene::Entity selected = m_parent->getSelectedEntity(); selected) {
		gui::widgets::menuHeader(selected.getName().c_str(), "Entity");
		const auto frameKey = m_parent->getActionShortcut("view.frameSelection");
		if (gui::widgets::menuItem({.glyph = gui::icon::focus, .label = "Frame", .shortcut = frameKey.c_str()}))
			frameSelection();
		if (canGoToCameraViewpoint() &&
			gui::widgets::menuItem({.glyph = gui::icon::video, .label = "Camera Viewpoint"}))
			goToCameraViewpoint();
		if (isPiloted(selected)) {
			if (gui::widgets::menuItem({.glyph = gui::icon::logOut, .label = "Stop Piloting"}))
				stopPiloting();
		} else if (canPilotSelectedCamera() &&
				   gui::widgets::menuItem({.glyph = gui::icon::move3d, .label = "Pilot Camera"})) {
			pilotSelectedCamera();
		}
		const auto duplicateKey = m_parent->getActionShortcut("entity.duplicate");
		if (gui::widgets::menuItem({.glyph = gui::icon::copy, .label = "Duplicate", .shortcut = duplicateKey.c_str()}))
			std::ignore = m_parent->runAction("entity.duplicate");
		if (selected.hasComponent<scene::component::Visibility>()) {
			auto& visibility = selected.getComponent<scene::component::Visibility>();
			if (gui::widgets::menuItem(
						{.glyph = visibility.editorVisible ? gui::icon::eyeOff : gui::icon::eye,
						 .label = visibility.editorVisible ? "Hide in the Editor" : "Show in the Editor"}))
				visibility.editorVisible = !visibility.editorVisible;
		}
		ImGui::Separator();
		const auto deleteKey = m_parent->getActionShortcut("entity.delete");
		if (gui::widgets::menuItem(
					{.glyph = gui::icon::trash2, .label = "Delete", .shortcut = deleteKey.c_str(), .danger = true}))
			std::ignore = m_parent->runAction("entity.delete");
	} else {
		using Projection = renderer::CameraEditor::Projection;
		gui::widgets::menuHeader(mp_document->title().c_str(), "Scene");
		const bool ortho = getViewProjection() == Projection::Orthographic;
		if (m_isometric) {
			std::ignore = gui::widgets::menuItem({.glyph = gui::icon::check, .label = "Isometric", .enabled = false});
		} else {
			if (gui::widgets::menuItem({.glyph = ortho ? gui::icon::check : nullptr, .label = "2D Orthographic"}))
				setViewProjection(Projection::Orthographic);
			if (gui::widgets::menuItem({.glyph = ortho ? nullptr : gui::icon::check, .label = "3D Perspective"}))
				setViewProjection(Projection::Perspective);
		}
		ImGui::Separator();
		auto& settings = m_parent->getSettings();
		if (gui::widgets::menuItem({.glyph = settings.showGrid ? gui::icon::check : nullptr, .label = "Grid"}))
			settings.showGrid = !settings.showGrid;
		if (gui::widgets::menuItem(
					{.glyph = settings.gridOnGround ? gui::icon::check : nullptr, .label = "Grid on the Ground"}))
			settings.gridOnGround = !settings.gridOnGround;
		if (gui::widgets::menuItem({.glyph = gui::icon::house, .label = "Reset View"}))
			resetView();
		if (gui::widgets::menuItem({.glyph = gui::icon::scan, .label = "Frame Scene"}))
			frameScene();
	}
	ImGui::EndPopup();
}

void Viewport::renderCameraGizmo() {
	if (mp_document == nullptr || mp_document->state() != SceneDocument::State::Edit || m_isometric)
		return;
	using Axis = renderer::CameraEditor::ViewAxis;
	struct Tip {
		math::vec3 direction;
		ImU32 color;
		const char* label;
		Axis from;
		const char* name;
	};
	constexpr ImU32 red = IM_COL32(230, 80, 80, 255);
	constexpr ImU32 green = IM_COL32(110, 210, 110, 255);
	constexpr ImU32 blue = IM_COL32(90, 140, 235, 255);
	constexpr std::array<Tip, 6> tips{{{{1.f, 0.f, 0.f}, red, "X", Axis::Right, "Right (+X)"},
									   {{-1.f, 0.f, 0.f}, red, nullptr, Axis::Left, "Left (-X)"},
									   {{0.f, 1.f, 0.f}, green, "Y", Axis::Top, "Top (+Y)"},
									   {{0.f, -1.f, 0.f}, green, nullptr, Axis::Bottom, "Bottom (-Y)"},
									   {{0.f, 0.f, 1.f}, blue, "Z", Axis::Front, "Front (+Z)"},
									   {{0.f, 0.f, -1.f}, blue, nullptr, Axis::Back, "Back (-Z)"}}};
	constexpr float radius = 30.f;
	constexpr float tipRadius = 7.f;
	constexpr float margin = 10.f;
	constexpr float side = 2.f * (radius + tipRadius + 2.f);
	ImGui::SetNextWindowPos({m_upper.x() - margin, m_upper.y() - margin}, ImGuiCond_Always, {1.f, 1.f});
	ImGui::SetNextWindowSize({side, side});
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.f, 0.f});
	constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;
	const bool open =
			ImGui::Begin(std::format("##vpCube{:x}", static_cast<uint64_t>(mp_document->id())).c_str(), nullptr, flags);
	ImGui::PopStyleVar();
	if (!open) {
		ImGui::End();
		return;
	}
	const ImVec2 origin = ImGui::GetWindowPos();
	const ImVec2 center{origin.x + side * 0.5f, origin.y + side * 0.5f};
	const math::mat4 view = m_editorCamera.getView();
	// World axes mapped into view space: (x, -y) is their direction on screen, z orders them back to front.
	std::array<std::pair<ImVec2, float>, tips.size()> projected{};
	for (size_t i = 0; i < tips.size(); ++i) {
		const auto& dir = tips[i].direction;
		const math::vec4 v = view * math::vec4{dir.x(), dir.y(), dir.z(), 0.f};
		projected[i] = {{center.x + v.x() * radius, center.y - v.y() * radius}, v.z()};
	}
	std::array<size_t, tips.size()> order{0, 1, 2, 3, 4, 5};
	std::ranges::sort(order, [&projected](const size_t iA, const size_t iB) -> bool {
		return projected[iA].second < projected[iB].second;
	});
	const ImVec2 mouse = ImGui::GetMousePos();
	const bool windowHovered = ImGui::IsWindowHovered();
	const auto distanceSq = [&mouse](const ImVec2& iPoint) -> float {
		return (mouse.x - iPoint.x) * (mouse.x - iPoint.x) + (mouse.y - iPoint.y) * (mouse.y - iPoint.y);
	};
	// The front-most tip under the cursor wins; the centre resets the view.
	std::optional<size_t> hovered;
	if (windowHovered) {
		for (const size_t index: order)
			if (distanceSq(projected[index].first) <= tipRadius * tipRadius)
				hovered = index;
	}
	const bool centreHovered = windowHovered && !hovered && distanceSq(center) <= tipRadius * tipRadius;
	auto* drawList = ImGui::GetWindowDrawList();
	const auto& theme = gui::UiLayer::getTheme();
	drawList->AddCircleFilled(center, radius + tipRadius,
							  windowHovered ? IM_COL32(20, 20, 24, 170) : IM_COL32(20, 20, 24, 110));
	drawList->AddCircleFilled(center, centreHovered ? 5.f : 3.f,
							  ImGui::GetColorU32(gui::vec(centreHovered ? theme.accent : theme.textMuted)));
	for (const size_t index: order) {
		const auto& tip = tips[index];
		const ImVec2 pos = projected[index].first;
		const bool isHovered = hovered == index;
		if (tip.label != nullptr) {
			drawList->AddLine(center, pos, tip.color, 2.f);
			drawList->AddCircleFilled(pos, tipRadius, tip.color);
			const ImVec2 textSize = ImGui::CalcTextSize(tip.label);
			drawList->AddText({pos.x - textSize.x * 0.5f, pos.y - textSize.y * 0.5f}, IM_COL32(20, 20, 24, 255),
							  tip.label);
		} else {
			drawList->AddCircleFilled(pos, tipRadius - 1.f, (tip.color & 0x00FFFFFFu) | 0x50000000u);
			drawList->AddCircle(pos, tipRadius - 1.f, tip.color, 0, 1.5f);
		}
		if (isHovered)
			drawList->AddCircle(pos, tipRadius + 2.f, ImGui::GetColorU32(gui::vec(theme.accent)), 0, 2.f);
	}
	if (hovered) {
		gui::widgets::richTooltip(std::format("View from {}", tips[*hovered].name).c_str(), nullptr,
								  "Turn the camera to look from this side, around the same point.");
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			m_editorCamera.viewAlong(tips[*hovered].from);
	} else if (centreHovered) {
		gui::widgets::richTooltip("Reset View", nullptr, "Face the XY plane from +Z, ten units from the origin.");
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			m_editorCamera.resetView();
	}
	ImGui::End();
}

void Viewport::renderCameraMenu() {
	using Axis = renderer::CameraEditor::ViewAxis;
	gui::widgets::menuHeader("Camera");
	if (gui::widgets::menuItem({.glyph = gui::icon::house, .label = "Reset View"}))
		resetView();
	const bool hasSelection = m_parent != nullptr && m_parent->getSelectedEntity();
	const auto frameKey = m_parent != nullptr ? m_parent->getActionShortcut("view.frameSelection") : std::string{};
	if (gui::widgets::menuItem({.glyph = gui::icon::focus,
								.label = "Frame Selection",
								.shortcut = frameKey.c_str(),
								.enabled = hasSelection}))
		frameSelection();
	if (gui::widgets::menuItem({.glyph = gui::icon::scan, .label = "Frame Scene"}))
		frameScene();
	if (gui::widgets::menuItem({.glyph = gui::icon::globe, .label = "Move World Origin Here"}))
		moveWorldOriginTo(m_isometric ? m_isoCamera.getTarget() : m_editorCamera.getFocalPoint());
	if (gui::widgets::menuItem(
				{.glyph = gui::icon::globe, .label = "Move World Origin to Selection", .enabled = hasSelection}) &&
		mp_document->getActiveScene())
		moveWorldOriginTo(
				mp_document->getActiveScene()->getWorldTransform(m_parent->getSelectedEntity()).translation());
	if (gui::widgets::menuItem(
				{.glyph = gui::icon::video, .label = "Selected Camera Viewpoint", .enabled = canGoToCameraViewpoint()}))
		goToCameraViewpoint();
	if (isPiloting()) {
		if (gui::widgets::menuItem({.glyph = gui::icon::logOut, .label = "Stop Piloting"}))
			stopPiloting();
	} else if (gui::widgets::menuItem({.glyph = gui::icon::move3d,
									   .label = "Pilot Selected Camera",
									   .enabled = canPilotSelectedCamera()})) {
		pilotSelectedCamera();
	}
	ImGui::Separator();
	constexpr std::array<std::pair<Axis, const char*>, 6> views{{{Axis::Front, "Front (XY)"},
																 {Axis::Back, "Back"},
																 {Axis::Top, "Top (XZ)"},
																 {Axis::Bottom, "Bottom"},
																 {Axis::Right, "Right (YZ)"},
																 {Axis::Left, "Left"}}};
	for (const auto& [axis, label]: views)
		if (gui::widgets::menuItem({.glyph = gui::icon::cuboid, .label = label}))
			viewAlong(axis);
}

void Viewport::renderOverlay() const {
	OWL_PROFILE_FUNCTION()

	if (mp_document == nullptr)
		return;
	const auto& activeScene = mp_document->getActiveScene();
	if (!activeScene)
		return;

	if (mp_document->state() != SceneDocument::State::Edit) {
		// The running scene draws the overlay itself, with its own camera, once asked.
		activeScene->getNavigation().setDebugVisible(scene::nav::NavWorld::DebugSource::Editor,
													 m_parent != nullptr && m_parent->getSettings().showNavGraph);
		const scene::Entity camera = activeScene->getPrimaryCamera();
		// A scene played without a primary camera (an empty one) has no view to draw the overlay through.
		if (!camera)
			return;
		auto& cam = camera.getComponent<scene::component::Camera>().camera;
		cam.setTransform(activeScene->getWorldTransform(camera)());
		renderer::Renderer2D::beginScene(cam);
	} else {
		renderer::Renderer2D::beginScene(getActiveCamera());
	}

	if (mp_document->state() == SceneDocument::State::Edit) {
		// Draw selected entity outline
		if (const scene::Entity selectedEntity = m_parent != nullptr ? m_parent->getSelectedEntity() : scene::Entity{};
			selectedEntity && !isPiloted(selectedEntity)) {
			const math::Transform worldTransform = activeScene->getWorldTransform(selectedEntity);
			// The silhouette of the meshes of a selection is outlined by the scene render itself.
			const bool outlined =
					activeScene->getOutline() && activeScene->getOutline()->root == selectedEntity.getUUID();
			if (!outlined && selectedEntity.hasComponent<scene::component::Camera>() && showsCameraModels() &&
				m_cameraModel) {
				math::Transform rigid = worldTransform;
				rigid.scale() = {1.f, 1.f, 1.f};
				renderSelectionBox(rigid(), m_cameraModel->getGeometry());
			} else if (!outlined && (!selectedEntity.hasComponent<scene::component::Light>() || m_isometric)) {
				renderer::Renderer2D::drawRect({.transform = worldTransform, .color = g_selectionColor});
				renderer::Renderer2D::drawQuad(
						{.transform = worldTransform, .color = gui::Theme::withAlpha(g_selectionColor, 0.2f)});
			}
		}

		if (m_parent != nullptr && m_parent->getSettings().showGrid)
			renderGrid(resolveTranslationSnapStep(m_parent->getSettings(), findFirstTilemapInfo(activeScene.get())),
					   m_parent->getSettings().gridOnGround && !m_isometric &&
							   getViewProjection() == renderer::CameraEditor::Projection::Perspective);
		if (m_isometric) {
			renderIsometricCursor();
			renderNavigationOverlay(activeScene);
		}

		// Draw trigger type icons
		auto& textureLibrary = renderer::Renderer::getTextureLibrary();
		const bool showTriggers = m_parent == nullptr || m_parent->getSettings().showTriggerIcons;
		for (const auto view = activeScene->registry.view<scene::component::Trigger, scene::component::Transform>();
			 const auto entity: view) {
			if (!showTriggers)
				break;
			auto [trigger, tc] = view.get<scene::component::Trigger, scene::component::Transform>(entity);
			std::string iconName;
			switch (trigger.trigger.type) {
				case scene::SceneTrigger::TriggerType::Victory:
					iconName = "icons/triggers/victory";
					break;
				case scene::SceneTrigger::TriggerType::Death:
					iconName = "icons/triggers/death";
					break;
				case scene::SceneTrigger::TriggerType::Target:
					iconName = "icons/triggers/target";
					break;
				case scene::SceneTrigger::TriggerType::Teleport:
					iconName = "icons/triggers/teleport";
					break;
				case scene::SceneTrigger::TriggerType::Timer:
					iconName = "icons/triggers/timer";
					break;
				case scene::SceneTrigger::TriggerType::Interaction:
					iconName = "icons/triggers/interaction";
					break;
				case scene::SceneTrigger::TriggerType::LuaCallback:
					iconName = "icons/triggers/lua_callback";
					break;
			}
			if (!iconName.empty()) {
				if (const auto icon = textureLibrary.get(iconName); icon != nullptr) {
					const scene::Entity ent{entity, activeScene.get()};
					math::Transform iconTransform = activeScene->getWorldTransform(ent);
					iconTransform.translation().z() += 0.01f;
					iconTransform.scale() = {0.5f, 0.5f, 1.f};
					renderer::Renderer2D::drawQuad({.transform = iconTransform,
													.color = {1.f, 1.f, 1.f, 0.7f},
													.texture = icon,
													.entityId = static_cast<int>(entity)});
				}
			}
		}

		if (m_parent != nullptr && m_parent->getSettings().showCameraGizmos)
			renderCameraMarkers(activeScene);
		if (showsLightMarkers()) {
			renderLightMarkers(activeScene);
			renderParticleMarkers(activeScene);
		}
		renderBehaviourOverlays(activeScene);
		renderSkeletonMarkers(activeScene);
		renderWorldCells(activeScene);

		const scene::Entity selectedForGizmo = m_parent != nullptr ? m_parent->getSelectedEntity() : scene::Entity{};
		drawReverbZoneMarkers(activeScene, selectedForGizmo);
		constexpr math::vec4 kPushwallOutline{0.25f, 0.85f, 0.35f, 1.f};
		constexpr math::vec4 kDestinationLine{0.95f, 0.85f, 0.25f, 1.f};
		// 1. Green outline around every pushwall.
		for (const auto view =
					 activeScene->registry.view<scene::component::RaycastPushWall, scene::component::Transform>();
			 const auto entity: view) {
			const scene::Entity ent{entity, activeScene.get()};
			math::Transform worldTransform = activeScene->getWorldTransform(ent);
			worldTransform.scale().x() = 1.f;
			worldTransform.scale().y() = 1.f;
			renderer::Renderer2D::drawRect(
					{.transform = worldTransform, .color = kPushwallOutline, .entityId = static_cast<int>(entity)});
		}
		// 2. Destination guide for the selected pushwall.
		if (selectedForGizmo && selectedForGizmo.hasComponent<scene::component::RaycastPushWall>()) {
			const auto& push = selectedForGizmo.getComponent<scene::component::RaycastPushWall>();
			const math::Transform wt = activeScene->getWorldTransform(selectedForGizmo);
			const math::vec3 origin{wt.translation().x(), wt.translation().y(), wt.translation().z() + 0.03f};
			const math::vec3 dest{origin.x() + push.slideDirection.x() * push.slideDistance,
								  origin.y() + push.slideDirection.y() * push.slideDistance, origin.z()};
			renderer::Renderer2D::drawLine({.point1 = origin,
											.point2 = dest,
											.color = kDestinationLine,
											.entityId = static_cast<int>(static_cast<uint32_t>(selectedForGizmo))});
			math::Transform circleTr;
			circleTr.translation() = dest;
			circleTr.scale() = math::vec3{0.2f, 0.2f, 1.f};
			renderer::Renderer2D::drawCircle({.transform = circleTr,
											  .color = kDestinationLine,
											  .thickness = 0.5f,
											  .fade = 0.01f,
											  .entityId = static_cast<int>(static_cast<uint32_t>(selectedForGizmo))});
		}
		if (selectedForGizmo && selectedForGizmo.hasComponent<scene::component::RaycastDoor>()) {
			const auto& door = selectedForGizmo.getComponent<scene::component::RaycastDoor>();
			using OD = scene::component::RaycastDoor::OpeningDirection;
			float dx = 0.f;
			float dy = 0.f;
			switch (door.openingDirection) {
				case OD::East:
					dx = 1.f;
					break;
				case OD::West:
					dx = -1.f;
					break;
				case OD::North:
					dy = 1.f;
					break;
				case OD::South:
					dy = -1.f;
					break;
			}
			const math::Transform wt = activeScene->getWorldTransform(selectedForGizmo);
			const math::vec3 origin{wt.translation().x(), wt.translation().y(), wt.translation().z() + 0.03f};
			const math::vec3 dest{origin.x() + dx, origin.y() + dy, origin.z()};
			renderer::Renderer2D::drawLine({.point1 = origin,
											.point2 = dest,
											.color = kDestinationLine,
											.entityId = static_cast<int>(static_cast<uint32_t>(selectedForGizmo))});
			math::Transform circleTr;
			circleTr.translation() = dest;
			circleTr.scale() = math::vec3{0.2f, 0.2f, 1.f};
			renderer::Renderer2D::drawCircle({.transform = circleTr,
											  .color = kDestinationLine,
											  .thickness = 0.5f,
											  .fade = 0.01f,
											  .entityId = static_cast<int>(static_cast<uint32_t>(selectedForGizmo))});
		}
	}

	renderer::Renderer2D::endScene();
	// After the lines: the overlay ignores the depth, so a model drawn before would show the grid through it.
	if (mp_document->state() == SceneDocument::State::Edit && m_parent != nullptr &&
		m_parent->getSettings().showCameraGizmos && showsCameraModels())
		renderCameraModels(activeScene);
}

namespace {

// Translucent pill holding one cluster of viewport controls, anchored at a corner of the viewport.
auto beginOverlay(const std::string& iId, const ImVec2& iPos, const ImVec2& iPivot, const bool iNoInputs = false)
		-> bool {
	const auto& theme = gui::UiLayer::getTheme();
	ImGui::SetNextWindowPos(iPos, ImGuiCond_Always, iPivot);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
							 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
							 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
							 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
							 ImGuiWindowFlags_NoDocking;
	if (iNoInputs)
		flags |= ImGuiWindowFlags_NoInputs;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{3.f, 3.f});
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{2.f, 2.f});
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, gui::vec(gui::Theme::withAlpha(theme.surface1, 0.9f)));
	ImGui::PushStyleColor(ImGuiCol_Border, gui::vec(gui::Theme::withAlpha(theme.line, 0.8f)));
	return ImGui::Begin(iId.c_str(), nullptr, flags);
}

auto toPhase(const SceneDocument::State iState) -> PlayPhase {
	switch (iState) {
		case SceneDocument::State::Play:
			return PlayPhase::Play;
		case SceneDocument::State::Pause:
			return PlayPhase::Pause;
		case SceneDocument::State::Edit:
			break;
	}
	return PlayPhase::Edit;
}

// Screen rectangle of the current overlay window, to keep the game from seeing the clicks it takes.
auto currentOverlayRect() -> OverlayRect {
	const auto pos = ImGui::GetWindowPos();
	const auto size = ImGui::GetWindowSize();
	return {pos.x, pos.y, pos.x + size.x, pos.y + size.y};
}

void endOverlay() {
	ImGui::End();
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(4);
}

void overlaySeparator() {
	ImGui::SameLine();
	const auto pos = ImGui::GetCursorScreenPos();
	const float height = ImGui::GetFrameHeight();
	ImGui::GetWindowDrawList()->AddLine({pos.x + 2.f, pos.y + 4.f}, {pos.x + 2.f, pos.y + height - 4.f},
										ImGui::GetColorU32(gui::vec(gui::UiLayer::getTheme().line)));
	ImGui::Dummy({5.f, height});
	ImGui::SameLine();
}

}// namespace

void Viewport::updateSelectionOutline(const shared<scene::Scene>& iScene) const {
	const scene::Entity selected = m_parent != nullptr ? m_parent->getSelectedEntity() : scene::Entity{};
	if (!selected || isPiloted(selected)) {
		iScene->setOutline(std::nullopt);
		return;
	}
	const auto root = static_cast<entt::entity>(selected);
	const auto meshes = iScene->registry.view<scene::component::MeshRenderer>();
	const bool hasMesh = std::ranges::any_of(meshes, [&](const auto iHandle) -> bool {
		return meshes.get<scene::component::MeshRenderer>(iHandle).mesh && iScene->isInSubtree(iHandle, root);
	});
	iScene->setOutline(
			hasMesh ? std::optional{scene::Scene::Outline{.root = selected.getUUID(), .color = g_selectionColor}}
					: std::nullopt);
}

void Viewport::renderSelectionBox(const math::mat4& iWorld, const renderer::MeshGeometry& iGeometry) {
	const auto& lo = iGeometry.boundsMin;
	const auto& hi = iGeometry.boundsMax;
	std::array<math::vec3, 8> corners;
	for (size_t i = 0; i < corners.size(); ++i) {
		const math::vec4 corner{(i & 1u) != 0 ? hi.x() : lo.x(), (i & 2u) != 0 ? hi.y() : lo.y(),
								(i & 4u) != 0 ? hi.z() : lo.z(), 1.f};
		const math::vec4 world = iWorld * corner;
		corners[i] = {world.x(), world.y(), world.z()};
	}
	// The 12 edges join the corners whose indices differ by one bit.
	for (size_t a = 0; a < corners.size(); ++a) {
		for (const size_t bit: {1u, 2u, 4u}) {
			if ((a & bit) == 0)
				renderer::Renderer2D::drawLine(
						{.point1 = corners[a], .point2 = corners[a | bit], .color = g_selectionColor});
		}
	}
}

void Viewport::renderCameraMarkers(const shared<scene::Scene>& iScene) const {
	const auto cameraIcon = renderer::Renderer::getTextureLibrary().get("icons/triggers/camera");
	for (const auto camView = iScene->registry.view<scene::component::Camera, scene::component::Transform>();
		 const auto entity: camView) {
		const auto& camComp = camView.get<scene::component::Camera>(entity);
		const scene::Entity ent{entity, iScene.get()};
		if (isPiloted(ent))
			continue;
		const math::Transform worldT = iScene->getWorldTransform(ent);
		const math::mat4 worldMat = worldT();
		// Local +Y is the camera forward (matches `RendererRaycast::beginScene`).
		const math::vec4 worldFwd4 = worldMat * math::vec4{0.f, 1.f, 0.f, 0.f};
		math::vec2 fwd2{worldFwd4.x(), worldFwd4.y()};
		if (fwd2.normSq() < 1e-8f)
			fwd2 = math::vec2{0.f, 1.f};
		fwd2.normalize();
		const math::vec3 origin{worldT.translation().x(), worldT.translation().y(), worldT.translation().z() + 0.02f};

		const bool isPrimary = camComp.primary;
		const math::vec4 mainColor = getCameraMarkerColor(isPrimary);
		if (showsCameraModels()) {
			math::Transform rigid = worldT;
			rigid.scale() = {1.f, 1.f, 1.f};
			renderCameraFrustum(camComp, rigid(), mainColor, static_cast<int>(entity));
			continue;
		}

		if (cameraIcon != nullptr) {
			math::Transform iconTransform;
			iconTransform.translation() = origin;
			iconTransform.rotation() = worldT.rotation();
			const float iconScale = isPrimary ? 0.55f : 0.45f;
			iconTransform.scale() = {iconScale, iconScale, 1.f};
			renderer::Renderer2D::drawQuad({.transform = iconTransform,
											.color = mainColor,
											.texture = cameraIcon,
											.entityId = static_cast<int>(entity)});
		}

		// Forward arrow.
		constexpr float arrowLen = 1.0f;
		const math::vec3 tip{origin.x() + fwd2.x() * arrowLen, origin.y() + fwd2.y() * arrowLen, origin.z()};
		renderer::Renderer2D::drawLine(
				{.point1 = origin, .point2 = tip, .color = mainColor, .entityId = static_cast<int>(entity)});

		float fovRad = math::radians(75.f);
		if (camComp.camera.getProjectionType() == scene::SceneCamera::ProjectionType::Perspective)
			fovRad = camComp.camera.getPerspectiveVerticalFov();
		constexpr float coneLen = 2.0f;
		const float cosH = std::cos(fovRad * 0.5f);
		const float sinH = std::sin(fovRad * 0.5f);
		const math::vec2 leftDir{fwd2.x() * cosH - fwd2.y() * sinH, fwd2.x() * sinH + fwd2.y() * cosH};
		const math::vec2 rightDir{fwd2.x() * cosH + fwd2.y() * sinH, -fwd2.x() * sinH + fwd2.y() * cosH};
		const math::vec4 coneColor{mainColor.x(), mainColor.y(), mainColor.z(), 0.45f};
		renderer::Renderer2D::drawLine(
				{.point1 = origin,
				 .point2 = {origin.x() + leftDir.x() * coneLen, origin.y() + leftDir.y() * coneLen, origin.z()},
				 .color = coneColor,
				 .entityId = static_cast<int>(entity)});
		renderer::Renderer2D::drawLine(
				{.point1 = origin,
				 .point2 = {origin.x() + rightDir.x() * coneLen, origin.y() + rightDir.y() * coneLen, origin.z()},
				 .color = coneColor,
				 .entityId = static_cast<int>(entity)});
	}
}

auto Viewport::showsCameraModels() const -> bool {
	return !m_isometric && getViewProjection() == renderer::CameraEditor::Projection::Perspective;
}

void Viewport::renderCameraModels(const shared<scene::Scene>& iScene) const {
	if (!m_cameraModel && !m_cameraModelMissing) {
		for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
			const auto file = assetsPath / "meshes" / "camera.glb";
			if (!exists(file))
				continue;
			if (const auto loaded = data::geometry::MeshLoader::loadMesh(file); loaded.mesh)
				m_cameraModel = mkShared<renderer::MeshResource>(file, loaded);
			break;
		}
		m_cameraModelMissing = !m_cameraModel;
		if (m_cameraModelMissing)
			OWL_WARN("Viewport: meshes/camera.glb not found in the editor assets, cameras show their frustum only.")
	}
	if (!m_upTriangle) {
		const std::array<renderer::Mesh3DVertex, 3> vertices{
				renderer::Mesh3DVertex{.position = {-1.f, 0.f, 0.f}, .normal = {0.f, 0.f, 1.f}, .uv = {0.f, 0.f}},
				renderer::Mesh3DVertex{.position = {1.f, 0.f, 0.f}, .normal = {0.f, 0.f, 1.f}, .uv = {1.f, 0.f}},
				renderer::Mesh3DVertex{.position = {0.f, 1.f, 0.f}, .normal = {0.f, 0.f, 1.f}, .uv = {0.5f, 1.f}}};
		constexpr std::array<uint32_t, 3> indices{0, 1, 2};
		m_upTriangle = renderer::Renderer3D::createMesh(vertices, indices);
	}
	const std::array<shared<renderer::gpu::Texture2D>, 1> white{renderer::Renderer3D::getWhiteTexture()};
	renderer::RendererMesh::beginScene(getActiveCamera(), {});
	for (const auto view = iScene->registry.view<scene::component::Camera, scene::component::Transform>();
		 const auto entity: view) {
		if (isPiloted(scene::Entity{entity, iScene.get()}))
			continue;
		math::Transform rigid = iScene->getWorldTransform(scene::Entity{entity, iScene.get()});
		rigid.scale() = {1.f, 1.f, 1.f};
		const auto& camera = view.get<scene::component::Camera>(entity);
		const auto id = static_cast<int32_t>(entity);
		if (m_cameraModel)
			renderer::RendererMesh::drawMesh(
					*m_cameraModel, rigid(), m_cameraModel->getBaseColorTexture(),
					{.tint = camera.primary ? math::vec4{1.f, 1.f, 1.f, 1.f} : math::vec4{0.75f, 0.85f, 1.f, 1.f},
					 .entityId = id},
					false);
		// The filled triangle above the far rectangle of the frustum points to the up of the camera.
		const auto extent = getFrustumExtent(camera);
		const float side = extent.halfWidth * 0.35f;
		const math::mat4 local =
				math::scale(math::translate(math::identity<float, 4>(),
											math::vec3{0.f, extent.halfHeight + side * 0.15f, -extent.depth}),
							math::vec3{side, side * 0.8f, 1.f});
		renderer::Renderer3D::drawMesh(m_upTriangle, rigid() * local, white,
									   {.tint = getCameraMarkerColor(camera.primary), .entityId = id});
	}
	renderer::RendererMesh::endScene();
}

auto Viewport::getFrustumExtent(const scene::component::Camera& iCamera) const -> FrustumExtent {
	constexpr float depth = 1.5f;
	const auto& size = m_framebuffer ? m_framebuffer->getSpecification().size : math::vec2ui{16, 9};
	const float aspect = size.y() > 0 ? static_cast<float>(size.x()) / static_cast<float>(size.y()) : 1.f;
	const bool perspective = iCamera.camera.getProjectionType() == scene::SceneCamera::ProjectionType::Perspective;
	const float halfHeight = perspective ? depth * std::tan(iCamera.camera.getPerspectiveVerticalFov() * 0.5f)
										 : iCamera.camera.getOrthographicSize() * 0.5f;
	return {.halfWidth = halfHeight * aspect, .halfHeight = halfHeight, .depth = depth, .perspective = perspective};
}

auto Viewport::getCameraMarkerColor(const bool iPrimary) -> math::vec4 {
	return iPrimary ? math::vec4{1.f, 0.78f, 0.16f, 0.95f} : math::vec4{0.55f, 0.85f, 0.95f, 0.75f};
}

void Viewport::renderCameraFrustum(const scene::component::Camera& iCamera, const math::mat4& iWorld,
								   const math::vec4& iColor, const int iEntityId) const {
	const auto [halfWidth, halfHeight, depth, perspective] = getFrustumExtent(iCamera);
	const auto toWorld = [&iWorld](const float iX, const float iY, const float iZ) -> math::vec3 {
		const math::vec4 point = iWorld * math::vec4{iX, iY, iZ, 1.f};
		return {point.x(), point.y(), point.z()};
	};
	// The camera looks down its local -Z.
	const std::array<math::vec3, 4> far{toWorld(-halfWidth, -halfHeight, -depth),
										toWorld(halfWidth, -halfHeight, -depth), toWorld(halfWidth, halfHeight, -depth),
										toWorld(-halfWidth, halfHeight, -depth)};
	for (size_t i = 0; i < far.size(); ++i) {
		const math::vec3 start =
				perspective ? toWorld(0.f, 0.f, 0.f)
							: toWorld(i == 0 || i == 3 ? -halfWidth : halfWidth, i < 2 ? -halfHeight : halfHeight, 0.f);
		renderer::Renderer2D::drawLine({.point1 = start, .point2 = far[i], .color = iColor, .entityId = iEntityId});
		renderer::Renderer2D::drawLine(
				{.point1 = far[i], .point2 = far[(i + 1) % far.size()], .color = iColor, .entityId = iEntityId});
	}
}

void Viewport::renderGrid(const float iStep, const bool iOnGround) const {
	if (iStep <= 0.f)
		return;
	const auto& focal = m_isometric ? m_isoCamera.getTarget() : m_editorCamera.getFocalPoint();
	// Half of the visible height at the reference point, times a margin for the sides and the tilt.
	float extent = std::max(1.f, m_editorCamera.getDistance()) * 1.6f;
	if (m_isometric) {
		const auto& tile = m_isoCamera.getConfig().tileSize;
		const auto& size = m_isoCamera.getViewportSize();
		extent = (static_cast<float>(size.x()) / static_cast<float>(tile.x()) +
				  static_cast<float>(size.y()) / static_cast<float>(tile.y())) /
				 m_isoCamera.getZoom();
	}
	float step = iStep;
	constexpr float maxLines = 120.f;
	while (extent * 2.f / step > maxLines) step *= 2.f;
	// The second axis of the plane: Y on the XY plane, Z on the ground.
	const float focalV = iOnGround ? focal.z() : focal.y();
	const float minX = std::floor((focal.x() - extent) / step) * step;
	const float minV = std::floor((focalV - extent) / step) * step;
	const float maxX = focal.x() + extent;
	const float maxV = focalV + extent;
	const auto at = [iOnGround](const float iX, const float iV) -> math::vec3 {
		return iOnGround ? math::vec3{iX, 0.f, iV} : math::vec3{iX, iV, 0.f};
	};
	const auto& theme = gui::UiLayer::getTheme();
	const math::vec4 lineColor = gui::Theme::withAlpha(theme.text, 0.07f);
	const math::vec4 secondAxis = gui::Theme::withAlpha(iOnGround ? theme.axisZ : theme.axisY, 0.45f);
	const auto columns = static_cast<int>((maxX - minX) / step) + 1;
	for (int i = 0; i < columns; ++i) {
		const float x = minX + step * static_cast<float>(i);
		const bool axis = std::abs(x) < step * 0.5f;
		renderer::Renderer2D::drawLine(
				{.point1 = at(x, minV), .point2 = at(x, maxV), .color = axis ? secondAxis : lineColor});
	}
	const auto rows = static_cast<int>((maxV - minV) / step) + 1;
	for (int i = 0; i < rows; ++i) {
		const float v = minV + step * static_cast<float>(i);
		const bool axis = std::abs(v) < step * 0.5f;
		renderer::Renderer2D::drawLine({.point1 = at(minX, v),
										.point2 = at(maxX, v),
										.color = axis ? gui::Theme::withAlpha(theme.axisX, 0.45f) : lineColor});
	}
}

void Viewport::frameSelection() {
	if (mp_document == nullptr || m_parent == nullptr)
		return;
	const auto& activeScene = mp_document->getActiveScene();
	const scene::Entity selected = m_parent->getSelectedEntity();
	if (!activeScene || !selected)
		return;
	const math::Transform world = activeScene->getWorldTransform(selected);
	// A unit quad scaled by the transform: half its diagonal encloses it.
	if (m_isometric) {
		m_isoCamera.setTarget(world.translation());
		return;
	}
	const float radius =
			0.5f * std::sqrt(world.scale().x() * world.scale().x() + world.scale().y() * world.scale().y());
	m_editorCamera.frame(world.translation(), radius);
}

void Viewport::moveWorldOriginTo(const math::vec3& iPoint) {
	if (mp_document == nullptr || m_parent == nullptr || !mp_document->getActiveScene())
		return;
	const auto& scene = mp_document->getActiveScene();
	// On a snap step the float grid of the scene stays the grid of the world.
	const auto step = static_cast<double>(resolveTranslationSnapStep(m_parent->getSettings(), std::nullopt));
	const auto& origin = scene->getWorldOrigin();
	const auto onStep = [step](const double iValue) -> double { return std::round(iValue / step) * step; };
	const commands::CommandTarget target{.registry = &m_parent->getRegistry(), .scene = scene, .undo = mp_undoManager};
	std::ignore =
			target.execute("scene.set_world_origin", {{"x", onStep(origin.x() + static_cast<double>(iPoint.x()))},
													  {"y", onStep(origin.y() + static_cast<double>(iPoint.y()))},
													  {"z", onStep(origin.z() + static_cast<double>(iPoint.z()))}});
}

void Viewport::followWorldOrigin(const scene::Scene& iScene) {
	const auto& origin = iScene.getWorldOrigin();
	if (mp_originScene == &iScene && origin != m_lastWorldOrigin) {
		const math::vec3d delta = origin - m_lastWorldOrigin;
		const math::vec3 shift{static_cast<float>(delta.x()), static_cast<float>(delta.y()),
							   static_cast<float>(delta.z())};
		m_editorCamera.moveBy(-shift);
		m_isoCamera.setTarget(m_isoCamera.getTarget() - shift);
	}
	mp_originScene = &iScene;
	m_lastWorldOrigin = origin;
}

void Viewport::renderOverlayToolbar() {
	if (mp_document == nullptr || m_parent == nullptr)
		return;
	using gui::widgets::iconButton;
	const auto& theme = gui::UiLayer::getTheme();
	const auto state = mp_document->state();
	const bool editing = state == SceneDocument::State::Edit;
	const auto id = static_cast<uint64_t>(mp_document->id());
	const float margin = 8.f;
	auto& settings = m_parent->getSettings();
	const auto shortcut = [this](const char* iAction) -> std::string { return m_parent->getActionShortcut(iAction); };

	m_overlayCount = 0;
	m_playBanner.update(toPhase(state), ImGui::GetIO().DeltaTime);
	if (!editing)
		ImGui::GetWindowDrawList()->AddRect({m_lower.x() + 1.f, m_lower.y() + 1.f},
											{m_upper.x() - 1.f, m_upper.y() - 1.f},
											ImGui::GetColorU32(gui::vec(theme.play)), 0.f, 2.f);

	// Tools: gizmo mode and snap.
	if (editing) {
		if (beginOverlay(std::format("##vpTools{:x}", id), {m_lower.x() + margin, m_lower.y() + margin}, {0.f, 0.f})) {
			struct Tool {
				gui::Guizmo::Type type;
				const char* glyph;
				const char* name;
				const char* action;
				const char* description;
			};
			constexpr std::array tools{Tool{gui::Guizmo::Type::None, gui::icon::mousePointer2, "Select", "guizmo.none",
											"Click an entity to select it; no gizmo."},
									   Tool{gui::Guizmo::Type::Translation, gui::icon::move, "Move", "guizmo.translate",
											"Drag an axis to move the selection. Hold Ctrl to snap."},
									   Tool{gui::Guizmo::Type::Rotation, gui::icon::rotate3d, "Rotate", "guizmo.rotate",
											"Drag a ring to rotate the selection."},
									   Tool{gui::Guizmo::Type::Scale, gui::icon::scale3d, "Scale", "guizmo.scale",
											"Drag an axis to scale the selection."}};
			for (const auto& tool: tools) {
				const auto key = shortcut(tool.action);
				if (iconButton(tool.name, {.glyph = tool.glyph,
										   .tooltip = tool.name,
										   .shortcut = key.c_str(),
										   .description = tool.description,
										   .selected = m_gizmoType == tool.type}))
					setGuizmoType(tool.type);
				ImGui::SameLine();
			}
			overlaySeparator();
			if (iconButton("space", {.glyph = settings.gizmoWorldSpace ? gui::icon::globe : gui::icon::box,
									 .tooltip = settings.gizmoWorldSpace ? "World axes" : "Local axes",
									 .description = "Move along the world axes, or along the axes of the selection."}))
				settings.gizmoWorldSpace = !settings.gizmoWorldSpace;
			ImGui::SameLine();
			if (iconButton("snap", {.glyph = gui::icon::magnet,
									.tooltip = "Snap to grid",
									.description = "Moves snap to the grid step; Ctrl snaps while dragging anyway.",
									.selected = settings.snapEnabled}))
				settings.snapEnabled = !settings.snapEnabled;
			ImGui::SameLine();
			const auto& scene = mp_document->getActiveScene();
			const float step =
					resolveTranslationSnapStep(settings, findFirstTilemapInfo(scene ? scene.get() : nullptr));
			const auto stepText = std::format("{:g}", step);
			if (gui::widgets::button(
						"snapStep",
						{.label = stepText.c_str(),
						 .tooltip = "Snap step",
						 .description = "Presets from the tilemap cell when there is one, else world units."}))
				ImGui::OpenPopup("##snapStep");
			if (ImGui::BeginPopup("##snapStep")) {
				m_parent->renderSnapStepPopup();
				ImGui::EndPopup();
			}
		}
		endOverlay();
	}

	// Playback, centred; while the scene runs it folds into a handle in the top-right corner.
	const bool collapsed = !editing && settings.playControlsCollapsed;
	renderPlayControls(collapsed);

	// View options, top-right.
	if (editing) {
		renderViewOverlay();
		renderPilotBanner();
		renderNavigationPopup();
	} else
		renderPlayBanner(collapsed);

	// Frame time chip, bottom-left (hidden from the Show menu or its own right-click menu).
	const bool frameChip = settings.showFrameChip;
	if (frameChip &&
		beginOverlay(std::format("##vpFps{:x}", id), {m_lower.x() + margin, m_upper.y() - margin}, {0.f, 1.f})) {
		const auto& meter = m_parent->getFrameMeter();
		const auto gpu = meter.getGpuMs();
		gui::UiLayer::pushTextStyle(gui::TextStyle::Value);
		const auto text = std::format("{:.2f} ms CPU{}", meter.getWorkMs(),
									  gpu.has_value() ? std::format("  {:.2f} ms GPU", *gpu) : "");
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(gui::vec(theme.textMuted), " %s ", text.c_str());
		gui::UiLayer::popTextStyle();
		if (ImGui::IsItemClicked()) {
			settings.showStats = true;
			ImGui::SetWindowFocus(panelWindow::stats);
		}
		if (ImGui::BeginPopupContextItem("##fpsChipMenu")) {
			if (gui::widgets::menuItem({.label = "Hide the frame stats"}))
				settings.showFrameChip = false;
			ImGui::EndPopup();
		}
		gui::widgets::itemTooltip(
				"Frame stats", nullptr,
				std::format("Cost of a frame, the waits for the display excluded ({:.0f} fps displayed). Capped by the "
							"display, the CPU and the GPU lower their clocks, so a frame costs more than at full load: "
							"Stats > Measure at Full Load gives the real rate. Click to open the Stats panel, "
							"right-click to hide it (Show menu to bring it back).",
							meter.getDisplayFps())
						.c_str());
		addOverlayRect(currentOverlayRect());
	}
	if (frameChip)
		endOverlay();

	// While the scene runs, the navigation overlay (graph and routes of the vehicles) is one toggle, bottom-right.
	if (!editing && m_isometric) {
		if (beginOverlay(std::format("##vpPlayShow{:x}", id), {m_upper.x() - margin, m_upper.y() - margin},
						 {1.f, 1.f})) {
			if (gui::widgets::iconButton("playShow", {.glyph = gui::icon::eye,
													  .tooltip = "Navigation graph and routes",
													  .description = "Draw the road graph and the route of each "
																	 "vehicle over the running scene.",
													  .selected = settings.showNavGraph}))
				settings.showNavGraph = !settings.showNavGraph;
			addOverlayRect(currentOverlayRect());
		}
		endOverlay();
	}
}

void Viewport::renderPlayControls(const bool iCollapsed) {
	using gui::widgets::iconButton;
	const auto& theme = gui::UiLayer::getTheme();
	const auto state = mp_document->state();
	const bool editing = state == SceneDocument::State::Edit;
	const auto id = static_cast<uint64_t>(mp_document->id());
	const float margin = 8.f;
	auto& settings = m_parent->getSettings();
	const auto shortcut = [this](const char* iAction) -> std::string { return m_parent->getActionShortcut(iAction); };
	const bool collapsed = iCollapsed;
	if (collapsed) {
		if (beginOverlay(std::format("##vpPlayHandle{:x}", id), {m_upper.x() - margin, m_lower.y() + margin},
						 {1.f, 0.f})) {
			const bool paused = state == SceneDocument::State::Pause;
			const auto key = shortcut("scene.play_controls");
			ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(paused ? theme.textMuted : theme.play));
			if (iconButton("handle",
						   {.glyph = paused ? gui::icon::pause : gui::icon::play,
							.tooltip = paused ? "Paused: show the play controls" : "Playing: show the play controls",
							.shortcut = key.c_str(),
							.description = "Click to bring back Play, Pause, Step and Stop over the game."}))
				settings.playControlsCollapsed = false;
			ImGui::PopStyleColor();
			addOverlayRect(currentOverlayRect());
		}
		endOverlay();
	} else if (beginOverlay(std::format("##vpPlay{:x}", id), {(m_lower.x() + m_upper.x()) * 0.5f, m_lower.y() + margin},
							{0.5f, 0.f})) {
		const bool playing = state == SceneDocument::State::Play;
		const bool paused = state == SceneDocument::State::Pause;
		const auto playKey = shortcut("scene.play");
		if (iconButton("play",
					   {.glyph = gui::icon::play,
						.tooltip = paused ? "Resume" : "Play",
						.shortcut = playKey.c_str(),
						.description = "Run the scene in the viewport; changes made while playing are reverted.",
						.selected = playing,
						.disabled = playing}))
			std::ignore = m_parent->runAction("scene.play");
		ImGui::SameLine();
		const auto pauseKey = shortcut("scene.pause");
		if (iconButton("pause", {.glyph = gui::icon::pause,
								 .tooltip = "Pause",
								 .shortcut = pauseKey.c_str(),
								 .selected = paused,
								 .disabled = !playing}))
			std::ignore = m_parent->runAction("scene.pause");
		ImGui::SameLine();
		const auto stepKey = shortcut("scene.step");
		if (iconButton("step", {.glyph = gui::icon::skipForward,
								.tooltip = "Step one frame",
								.shortcut = stepKey.c_str(),
								.disabled = !paused}))
			std::ignore = m_parent->runAction("scene.step");
		ImGui::SameLine();
		const auto stopKey = shortcut("scene.stop");
		if (iconButton("stop", {.glyph = gui::icon::square,
								.tooltip = "Stop",
								.shortcut = stopKey.c_str(),
								.description = "Leave the play mode and restore the scene.",
								.disabled = editing}))
			std::ignore = m_parent->runAction("scene.stop");
		if (!editing) {
			overlaySeparator();
			const auto key = shortcut("scene.play_controls");
			if (iconButton("fold", {.glyph = gui::icon::chevronUp,
									.tooltip = "Hide the play controls",
									.shortcut = key.c_str(),
									.description = "Folds the controls into a small handle in the corner, so they "
												   "never cover the game."}))
				settings.playControlsCollapsed = true;
		}
		addOverlayRect(currentOverlayRect());
	}
	if (!collapsed)
		endOverlay();
}

void Viewport::renderPlayBanner(const bool iCollapsed) {
	const auto& theme = gui::UiLayer::getTheme();
	const auto state = mp_document->state();
	const auto id = static_cast<uint64_t>(mp_document->id());
	const float margin = 8.f;
	const bool collapsed = iCollapsed;
	if (!m_playBanner.visible())
		return;
	{
		// Brief, non-interactive message when Play or Pause starts; the state stays readable on the handle.
		const float y = collapsed ? margin : margin * 2.f + ImGui::GetFrameHeight() + 6.f;
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_playBanner.alpha());
		if (beginOverlay(std::format("##vpPlaying{:x}", id), {(m_lower.x() + m_upper.x()) * 0.5f, m_lower.y() + y},
						 {0.5f, 0.f}, true)) {
			ImGui::AlignTextToFramePadding();
			ImGui::TextColored(gui::vec(theme.play), " %s ", gui::icon::play);
			ImGui::SameLine();
			gui::UiLayer::pushTextStyle(gui::TextStyle::Label);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(state == SceneDocument::State::Pause ? "Paused" : "Playing");
			ImGui::SameLine();
			ImGui::AlignTextToFramePadding();
			ImGui::TextColored(gui::vec(theme.textMuted), "%s", "— changes are reverted on Stop ");
			gui::UiLayer::popTextStyle();
		}
		endOverlay();
		ImGui::PopStyleVar();
	}
}

void Viewport::frameScene() {
	if (mp_document == nullptr)
		return;
	const auto& activeScene = mp_document->getActiveScene();
	if (!activeScene)
		return;
	std::optional<std::pair<math::vec3, math::vec3>> bounds;
	for (const auto entity: activeScene->registry.view<scene::component::Transform>()) {
		const math::Transform world = activeScene->getWorldTransform(scene::Entity{entity, activeScene.get()});
		const math::vec3 half = world.scale() * 0.5f;
		const math::vec3 low = world.translation() - half;
		const math::vec3 high = world.translation() + half;
		if (!bounds) {
			bounds = {low, high};
			continue;
		}
		for (size_t axis = 0; axis < 3; ++axis) {
			bounds->first[axis] = std::min(bounds->first[axis], low[axis]);
			bounds->second[axis] = std::max(bounds->second[axis], high[axis]);
		}
	}
	if (!bounds) {
		m_editorCamera.resetView();
		return;
	}
	const math::vec3 center = (bounds->first + bounds->second) * 0.5f;
	if (m_isometric) {
		m_isoCamera.setTarget({center.x(), center.y(), 0.f});
		return;
	}
	m_editorCamera.frame(center, (bounds->second - bounds->first).norm() * 0.5f);
}

auto Viewport::canGoToCameraViewpoint() const -> bool {
	if (m_parent == nullptr)
		return false;
	const scene::Entity selected = m_parent->getSelectedEntity();
	if (!selected || !selected.hasComponent<scene::component::Camera>())
		return false;
	const auto& stack = renderer::Renderer::getRenderStack();
	if (stack.isEmpty())
		return true;
	shared<renderer::RenderLayer> layer;
	if (selected.hasComponent<scene::component::RendererTag>())
		layer = stack.findByName(selected.getComponent<scene::component::RendererTag>().rendererName);
	if (!layer)
		layer = stack.getDefaultLayer();
	return !layer || std::string_view{layer->getTypeKey()} != "RendererRaycast";
}

void Viewport::goToCameraViewpoint() {
	if (!canGoToCameraViewpoint() || mp_document == nullptr || !mp_document->getActiveScene())
		return;
	moveViewOnto(m_parent->getSelectedEntity());
}

void Viewport::moveViewOnto(const scene::Entity& iCamera) {
	const auto& sceneCamera = iCamera.getComponent<scene::component::Camera>().camera;
	const math::Transform world = mp_document->getActiveScene()->getWorldTransform(iCamera);
	if (m_isometric) {
		m_isoCamera.setTarget({world.translation().x(), world.translation().y(), 0.f});
		return;
	}
	using Projection = renderer::CameraEditor::Projection;
	if (sceneCamera.getProjectionType() == scene::SceneCamera::ProjectionType::Orthographic) {
		// The 2D view shows `distance * tan(fov / 2)` above and below its centre: match the camera's half size.
		const float halfHeight = sceneCamera.getOrthographicSize() * 0.5f;
		const float distance = std::max(1.f, halfHeight / std::tan(math::radians(m_editorCamera.getFov()) * 0.5f));
		m_editorCamera.setDistance(distance);
		m_editorCamera.setProjection(Projection::Orthographic);
		const math::vec3 eye{world.translation().x(), world.translation().y(), distance};
		m_editorCamera.lookFrom(eye, {0.f, 0.f, -1.f});
		return;
	}
	const math::vec4 forward = world() * math::vec4{0.f, 0.f, -1.f, 0.f};
	m_editorCamera.setProjection(Projection::Perspective);
	m_editorCamera.lookFrom(world.translation(), {forward.x(), forward.y(), forward.z()});
}

auto Viewport::canPilotSelectedCamera() const -> bool { return !m_isometric && canGoToCameraViewpoint(); }

void Viewport::pilotSelectedCamera() {
	if (!canPilotSelectedCamera() || mp_document == nullptr || mp_document->state() != SceneDocument::State::Edit)
		return;
	const scene::Entity selected = m_parent->getSelectedEntity();
	if (isPiloted(selected))
		return;
	renderer::CameraEditor saved = m_editorCamera;
	if (m_pilot.isActive()) {
		const auto& scene = mp_document->getActiveScene();
		if (const auto camera = scene ? scene->findEntityByUUID(m_pilot.getEntity()) : scene::Entity{};
			camera && m_pilot.isGestureActive() && mp_undoManager != nullptr)
			commitTransformEdit(camera, scene, m_pilotBeforeYaml);
		saved = m_pilot.stop().value_or(saved);
	}
	moveViewOnto(selected);
	m_pilot.start(selected.getUUID(), saved, m_editorCamera);
	m_pilotBeforeYaml.clear();
}

void Viewport::stopPiloting() {
	if (!m_pilot.isActive())
		return;
	if (const auto& scene = mp_document != nullptr ? mp_document->getActiveScene() : nullptr;
		scene && m_pilot.isGestureActive() && mp_undoManager != nullptr) {
		if (const auto camera = scene->findEntityByUUID(m_pilot.getEntity()); camera)
			commitTransformEdit(camera, scene, m_pilotBeforeYaml);
	}
	m_pilotBeforeYaml.clear();
	if (auto saved = m_pilot.stop(); saved.has_value()) {
		m_editorCamera = *saved;
		m_editorCamera.setViewportSize(getSize());
	}
}

auto Viewport::isFlying() const -> bool {
	return mp_document != nullptr && mp_document->state() == SceneDocument::State::Edit && !m_isometric &&
		   isHovered() && m_editorCamera.isFlying();
}

auto Viewport::isPiloted(const scene::Entity& iEntity) const -> bool {
	return m_pilot.isActive() && iEntity && iEntity.getUUID() == m_pilot.getEntity();
}

void Viewport::updatePilot(const shared<scene::Scene>& iScene, const core::Timestep& iTimeStep) {
	const scene::Entity camera = iScene->findEntityByUUID(m_pilot.getEntity());
	if (!camera || !camera.hasComponent<scene::component::Camera>()) {
		stopPiloting();
		return;
	}
	if (m_pilot.shouldFollowEntity(m_editorCamera)) {
		moveViewOnto(camera);
		m_pilot.markFollowed(m_editorCamera);
	}
	if (isHovered())
		m_editorCamera.onUpdate(iTimeStep);
	const bool held = input::Input::isMouseButtonPressed(input::mouse::ButtonLeft) ||
					  input::Input::isMouseButtonPressed(input::mouse::ButtonRight) ||
					  input::Input::isMouseButtonPressed(input::mouse::ButtonMiddle);
	const auto& sceneCamera = camera.getComponent<scene::component::Camera>().camera;
	switch (m_pilot.track(m_editorCamera, held)) {
		case CameraPilot::Step::Begin:
			m_pilotBeforeYaml = scene::SceneSerializer::serializeEntityToString(camera);
			[[fallthrough]];
		case CameraPilot::Step::Move:
			{
				const bool ortho = sceneCamera.getProjectionType() == scene::SceneCamera::ProjectionType::Orthographic;
				const math::Transform world =
						CameraPilot::getCameraWorld(m_editorCamera, iScene->getWorldTransform(camera), ortho);
				math::mat4 local = world();
				if (const auto& hierarchy = camera.getComponent<scene::component::Hierarchy>();
					hierarchy.parentId != core::UUID{0}) {
					if (const auto parent = iScene->findEntityByUUID(hierarchy.parentId); parent)
						local = math::inverse(iScene->getWorldTransform(parent)()) * local;
				}
				auto& transform = camera.getComponent<scene::component::Transform>().transform;
				const math::vec3 scale = transform.scale();
				transform = math::Transform{local};
				transform.scale() = scale;
				break;
			}
		case CameraPilot::Step::Commit:
			if (mp_undoManager != nullptr)
				commitTransformEdit(camera, iScene, m_pilotBeforeYaml);
			m_pilotBeforeYaml.clear();
			break;
		case CameraPilot::Step::Idle:
			break;
	}
	m_pilotCamera = sceneCamera;
	m_pilotCamera.setViewportSize(getSize());
	m_pilotCamera.setTransform(iScene->getWorldTransform(camera)());
}

void Viewport::renderCameraPreviewInset() {
	if (m_previewEntity == core::UUID{0} || !m_previewFramebuffer || mp_document == nullptr)
		return;
	const auto& scene = mp_document->getActiveScene();
	const scene::Entity camera = scene ? scene->findEntityByUUID(m_previewEntity) : scene::Entity{};
	const auto texture = gui::imTexture(m_previewFramebuffer, 0);
	if (!camera || !texture.has_value())
		return;
	// Above the view-cube of `renderCameraGizmo` (its side and margin).
	constexpr float cubeSide = 78.f;
	constexpr float margin = 10.f;
	if (beginOverlay(std::format("##vpPreview{:x}", static_cast<uint64_t>(mp_document->id())),
					 {m_upper.x() - margin, m_upper.y() - margin - cubeSide - 6.f}, {1.f, 1.f})) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(std::format("{}  {}", gui::icon::video, camera.getName()).c_str());
		ImGui::SameLine();
		if (gui::widgets::iconButton("pilotPreview", {.glyph = gui::icon::move3d,
													  .tooltip = "Pilot Camera",
													  .description = "Render the view through this camera and move "
																	 "it with the view."}))
			pilotSelectedCamera();
		const auto size = m_previewFramebuffer->getSpecification().size;
		ImGui::Image(texture.value(), {static_cast<float>(size.x()), static_cast<float>(size.y())},
					 gui::vec(m_previewFramebuffer->getLowerData()), gui::vec(m_previewFramebuffer->getUpperData()));
	}
	endOverlay();
}

void Viewport::renderPilotBanner() {
	if (!m_pilot.isActive() || mp_document == nullptr || !mp_document->getActiveScene())
		return;
	const scene::Entity camera = mp_document->getActiveScene()->findEntityByUUID(m_pilot.getEntity());
	if (!camera)
		return;
	constexpr float margin = 8.f;
	if (beginOverlay(std::format("##vpPilot{:x}", static_cast<uint64_t>(mp_document->id())),
					 {(m_lower.x() + m_upper.x()) * 0.5f, m_upper.y() - margin}, {0.5f, 1.f})) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(std::format("{}  Piloting {}", gui::icon::video, camera.getName()).c_str());
		ImGui::SameLine();
		if (gui::widgets::button("stopPilot", {.glyph = gui::icon::logOut,
											   .label = "Stop",
											   .tooltip = "Stop piloting",
											   .description = "Put the editor view back where it was before."}))
			stopPiloting();
	}
	endOverlay();
}

void Viewport::renderViewOverlay() {
	using gui::widgets::iconButton;
	using Projection = renderer::CameraEditor::Projection;
	const auto id = static_cast<uint64_t>(mp_document->id());
	constexpr float margin = 8.f;
	auto& settings = m_parent->getSettings();
	if (beginOverlay(std::format("##vpView{:x}", id), {m_upper.x() - margin, m_lower.y() + margin}, {1.f, 0.f})) {
		const bool ortho = getViewProjection() == Projection::Orthographic;
		if (m_isometric) {
			std::ignore = gui::widgets::button(
					"projection",
					{.glyph = gui::icon::grid3x3,
					 .label = "Iso",
					 .tooltip = "Isometric view",
					 .description = "The scene draws an isometric layer: the 2:1 dimetric view is locked. "
									"Pan with the middle or right drag, zoom by powers of two with the "
									"wheel.",
					 .disabled = true});
		} else if (gui::widgets::button("projection",
										{.glyph = ortho ? gui::icon::square : gui::icon::cuboid,
										 .label = ortho ? "2D" : "3D",
										 .tooltip = "View",
										 .description = "2D: orthographic front view of the scene plane. 3D: "
														"perspective, orbit with Ctrl + drag."}))
			ImGui::OpenPopup("##projection");
		if (ImGui::BeginPopup("##projection")) {
			if (gui::widgets::menuItem({.glyph = ortho ? gui::icon::check : nullptr, .label = "2D Orthographic"}))
				setViewProjection(Projection::Orthographic);
			if (gui::widgets::menuItem({.glyph = ortho ? nullptr : gui::icon::check, .label = "3D Perspective"}))
				setViewProjection(Projection::Perspective);
			ImGui::EndPopup();
		}
		ImGui::SameLine();
		if (iconButton("show",
					   {.glyph = gui::icon::eye,
						.tooltip = "Show",
						.description = "Grid, camera markers, trigger icons, the HUD and the editor ambient light."}))
			ImGui::OpenPopup("##show");
		if (ImGui::BeginPopup("##show")) {
			const auto option = [](const char* iLabel, bool& ioFlag) -> void {
				if (gui::widgets::menuItem({.glyph = ioFlag ? gui::icon::check : nullptr, .label = iLabel}))
					ioFlag = !ioFlag;
			};
			gui::widgets::menuHeader("Show");
			option("Grid", settings.showGrid);
			option("Grid on the ground (3D)", settings.gridOnGround);
			option("Camera markers", settings.showCameraGizmos);
			option("Camera preview", settings.showCameraPreview);
			option("Trigger icons", settings.showTriggerIcons);
			option("HUD", settings.showHud);
			option("Frame stats", settings.showFrameChip);
			if (m_isometric)
				option("Navigation graph and routes", settings.showNavGraph);
			ImGui::Separator();
			ImGui::SetNextItemWidth(160.f);
			ImGui::SliderFloat("Editor ambient", &settings.editorAmbient, 0.f, 2.f, "%.2f");
			gui::widgets::itemTooltip("Editor ambient", nullptr,
									  "Grey light added to every lit 3D surface in Edit mode only, to work in dark "
									  "places; Play shows the scene's own lighting.");
			ImGui::EndPopup();
		}
		if (m_isometric) {
			ImGui::SameLine();
			if (iconButton("navTool",
						   {.glyph = gui::icon::signpost,
							.tooltip = "Navigation tool",
							.description = "Edit one-way roads, closed sides and slower cells on the graph. Click an "
										   "arrow of a cell edge to cycle two-way, exit only, enter only, closed; "
										   "right-click a cell to set its cost.",
							.selected = m_navTool.active})) {
				m_navTool.toggle(settings.showNavGraph);
			}
		}
		ImGui::SameLine();
		const auto frameKey = m_parent->getActionShortcut("view.frameSelection");
		if (iconButton("frame", {.glyph = gui::icon::focus,
								 .tooltip = "Frame selection",
								 .shortcut = frameKey.c_str(),
								 .description = "Centre the view on the selected entity.",
								 .disabled = !m_parent->getSelectedEntity()}))
			frameSelection();
		ImGui::SameLine();
		if (iconButton("camera", {.glyph = gui::icon::video,
								  .tooltip = "Camera",
								  .description = "Reset the view, look along an axis, frame the scene or move onto the "
												 "selected camera."}))
			ImGui::OpenPopup("##camera");
		if (ImGui::BeginPopup("##camera")) {
			renderCameraMenu();
			ImGui::EndPopup();
		}
		ImGui::SameLine();
		if (iconButton("stats", {.glyph = gui::icon::gauge,
								 .tooltip = "Frame stats",
								 .description = "Show the Stats panel: frame time, renderer and GPU counters.",
								 .selected = settings.showStats}))
			settings.showStats = !settings.showStats;
	}
	endOverlay();
}

void Viewport::resetView() {
	if (m_isometric) {
		m_isoCamera.setTarget({0.f, 0.f, 0.f});
		m_isoCamera.setZoomLevel(0);
		return;
	}
	m_editorCamera.resetView();
}

auto Viewport::getActiveCamera() const -> const renderer::Camera& {
	if (m_isometric)
		return m_isoCamera;
	if (m_pilot.isActive())
		return m_pilotCamera;
	return m_editorCamera;
}

void Viewport::updateIsometricMode(const shared<scene::Scene>& iScene) {
	const auto layer = iScene ? iScene->getSelfDrawnLayer() : nullptr;
	const auto* isometric = dynamic_cast<const renderer::RendererIsometricLayer*>(layer.get());
	const bool entering = isometric != nullptr && !m_isometric;
	m_isometric = isometric != nullptr;
	if (!m_isometric) {
		m_isoCursor.reset();
		m_hoveredCell.reset();
		return;
	}
	m_isoCamera.setConfig(isometric->getConfig());
	m_isoCamera.setViewportSize(getSize());
	if (entering)
		m_isoCamera.setZoomLevel(isometric->getZoomLevel());
}

void Viewport::handleIsometricInput() {
	const auto& io = ImGui::GetIO();
	const auto mouse = ImGui::GetMousePos();
	const math::vec2 pixel{mouse.x - m_lower.x(), mouse.y - m_lower.y()};
	if (io.MouseWheel > 0.f || io.MouseWheel < 0.f) {
		// Zoom around the cursor: the ground point under it stays put.
		const float height = m_isoCamera.getTarget().z();
		const math::vec3 before = m_isoCamera.viewportToWorld(pixel, height);
		m_isoCamera.setZoomLevel(m_isoCamera.getZoomLevel() + (io.MouseWheel > 0.f ? 1 : -1));
		const math::vec3 after = m_isoCamera.viewportToWorld(pixel, height);
		m_isoCamera.setTarget(m_isoCamera.getTarget() + (before - after));
	}
	const bool drag = ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
					  ImGui::IsMouseDragging(ImGuiMouseButton_Right) ||
					  (io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left));
	if (drag && !gui::Guizmo::isUsing())
		m_isoCamera.pan({io.MouseDelta.x, io.MouseDelta.y});
}

void Viewport::updateIsometricCursor(const shared<scene::Scene>& iScene) {
	m_isoCursor.reset();
	m_hoveredCell.reset();
	m_hoveredCornerZ.fill(0.f);
	if (!isHovered() || !iScene)
		return;
	const auto mouse = ImGui::GetMousePos();
	const math::vec2 pixel{mouse.x - m_lower.x(), mouse.y - m_lower.y()};
	float height = 0.f;
	for (const auto view = iScene->registry.view<scene::component::Tilemap>(); const auto entity: view) {
		const auto& asset = view.get<scene::component::Tilemap>(entity).asset;
		if (!asset || !asset->tileset || asset->tileset->kind != scene::Tileset::Kind::Isometric)
			continue;
		const math::vec3 origin = iScene->getWorldTransform(scene::Entity{entity, iScene.get()}).translation();
		height = origin.z();
		m_hoveredMapOrigin = origin;
		m_hoveredMapEntity = scene::Entity{entity, iScene.get()}.getUUID();
		if (const auto hit = scene::terrain::pickTerrain(*asset, m_isoCamera, pixel, origin); hit.has_value()) {
			m_hoveredCell = hit->cell;
			const auto corners = asset->getCellCorners(hit->cell.x(), hit->cell.y());
			for (size_t i = 0; i < corners.size(); ++i)
				m_hoveredCornerZ[i] = static_cast<float>(corners[i]) * scene::terrain::heightStep;
			m_isoCursor = hit->point;
			return;
		}
		break;
	}
	m_isoCursor = m_isoCamera.viewportToWorld(pixel, height);
}

void Viewport::pickNavigationCells() {
	if (m_parent == nullptr || !m_parent->getSettings().showNavGraph || !m_hovered)
		return;
	const auto& io = ImGui::GetIO();
	if (!io.KeyShift)
		return;
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		m_navFrom.reset();
		m_navTo.reset();
		return;
	}
	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || !m_hoveredCell.has_value())
		return;
	if (!m_navFrom.has_value() || m_navTo.has_value()) {
		m_navFrom = m_hoveredCell;
		m_navTo.reset();
	} else {
		m_navTo = m_hoveredCell;
	}
}

void Viewport::handleNavigationTool(const shared<scene::Scene>& iScene) {
	m_navSide.reset();
	if (!m_navTool.active || m_parent == nullptr || !m_parent->getSettings().showNavGraph || !m_hovered ||
		!m_hoveredCell.has_value() || !m_isoCursor.has_value() || !iScene)
		return;
	const auto& io = ImGui::GetIO();
	if (io.KeyShift || io.KeyAlt)
		return;
	const auto entity = iScene->findEntityByUUID(m_hoveredMapEntity);
	if (!entity || !entity.hasComponent<scene::component::Tilemap>())
		return;
	const auto& tilemap = entity.getComponent<scene::component::Tilemap>();
	if (!tilemap.asset || tilemap.navigation.isEmpty())
		return;
	auto& entry = iScene->getNavigation().acquire(static_cast<uint64_t>(entity.getUUID()), *tilemap.asset,
												  tilemap.navigation);
	const auto cell = *m_hoveredCell;
	const auto base = entry.map.getBaseCell(*tilemap.asset, cell.x(), cell.y());
	if (!base.has_value())
		return;
	const commands::CommandTarget target{.registry = &m_parent->getRegistry(), .scene = iScene, .undo = mp_undoManager};
	const auto id = static_cast<int64_t>(static_cast<uint64_t>(entity.getUUID()));
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
		const auto drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right, 0.f);
		if (drag.x * drag.x + drag.y * drag.y < 16.f) {
			m_navPopupCell = cell;
			m_navPopupEntity = entity.getUUID();
			m_navOpenPopup = true;
			const auto* exception = tilemap.navigation.findOverride(cell.x(), cell.y());
			m_navCostEdit = exception != nullptr ? exception->cost : 0.f;
		}
	}
	// The side nearest to the cursor inside the cell.
	const float fx = m_isoCursor->x() - m_hoveredMapOrigin.x() - static_cast<float>(cell.x()) - 0.5f;
	const float fy = m_isoCursor->y() - m_hoveredMapOrigin.y() - static_cast<float>(cell.y()) - 0.5f;
	auto side = fy > 0.f ? scene::nav::Direction::South : scene::nav::Direction::North;
	if (std::abs(fx) >= std::abs(fy))
		side = fx > 0.f ? scene::nav::Direction::East : scene::nav::Direction::West;
	const auto nx = static_cast<int64_t>(cell.x()) + scene::nav::offsetX(side);
	const auto ny = static_cast<int64_t>(cell.y()) + scene::nav::offsetY(side);
	if (nx < 0 || ny < 0)
		return;
	const auto neighbour = entry.map.getBaseCell(*tilemap.asset, static_cast<uint32_t>(nx), static_cast<uint32_t>(ny));
	const auto bit = scene::nav::directionBit(side);
	const auto back = scene::nav::directionBit(scene::nav::opposite(side));
	if (!neighbour.has_value())
		return;
	const bool forward = (base->exits & bit) != 0 && (neighbour->enters & bit) != 0;
	const bool backward = (neighbour->exits & back) != 0 && (base->enters & back) != 0;
	if (!forward && !backward)
		return;
	m_navSide = side;
	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		return;
	const auto next = scene::nav::nextSideState(tilemap.navigation.getSideState(cell.x(), cell.y(), side));
	std::ignore = target.execute("nav.set_edge",
								 {{"entity", id},
								  {"x", static_cast<int64_t>(cell.x())},
								  {"y", static_cast<int64_t>(cell.y())},
								  {"side", std::string{scene::nav::directionNames[static_cast<size_t>(side)]}},
								  {"state", std::string{scene::nav::sideStateNames[static_cast<size_t>(next)]}}});
}

void Viewport::renderNavigationPopup() {
	if (m_navOpenPopup) {
		ImGui::OpenPopup("##navCell");
		m_navOpenPopup = false;
	}
	if (!ImGui::BeginPopup("##navCell"))
		return;
	const auto& scene = mp_document->getActiveScene();
	if (!m_navPopupCell.has_value() || !scene || m_parent == nullptr) {
		ImGui::EndPopup();
		return;
	}
	const auto cell = *m_navPopupCell;
	const commands::CommandTarget target{.registry = &m_parent->getRegistry(), .scene = scene, .undo = mp_undoManager};
	const auto id = static_cast<int64_t>(static_cast<uint64_t>(m_navPopupEntity));
	ImGui::TextUnformatted(std::format("Road cell ({}, {})", cell.x(), cell.y()).c_str());
	ImGui::SetNextItemWidth(120.f);
	ImGui::DragFloat("Cost", &m_navCostEdit, 0.05f, 0.f, 64.f, m_navCostEdit > 0.f ? "%.2f" : "tile default");
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		std::ignore = target.execute("nav.set_cell_cost", {{"entity", id},
														   {"x", static_cast<int64_t>(cell.x())},
														   {"y", static_cast<int64_t>(cell.y())},
														   {"cost", static_cast<double>(m_navCostEdit)}});
	}
	ImGui::SetItemTooltip("%s",
						  "Cost of crossing the cell, in cells (1 is a normal road, 3 three times slower). 0 keeps "
						  "the cost of its tile.");
	if (ImGui::Button("Clear exceptions")) {
		std::ignore = target.execute(
				"nav.clear_cell",
				{{"entity", id}, {"x", static_cast<int64_t>(cell.x())}, {"y", static_cast<int64_t>(cell.y())}});
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void Viewport::renderNavigationOverlay(const shared<scene::Scene>& iScene) const {
	if (m_parent == nullptr || !m_parent->getSettings().showNavGraph || !iScene)
		return;
	constexpr math::vec4 pathColor{1.f, 0.95f, 0.20f, 1.f};
	scene::nav::drawNavigationGraph(*iScene);
	scene::nav::drawNavigationRoutes(*iScene);
	if (m_navTool.active && m_hoveredCell.has_value() && m_isometric)
		scene::nav::drawNavigationCellEditor(*iScene, static_cast<uint64_t>(m_hoveredMapEntity), m_hoveredCell->x(),
											 m_hoveredCell->y(), m_navSide);
	bool pathDone = false;
	for (const auto view = iScene->registry.view<scene::component::Tilemap>(); const auto entity: view) {
		const auto& tilemap = view.get<scene::component::Tilemap>(entity);
		if (!tilemap.asset || tilemap.navigation.isEmpty())
			continue;
		const scene::Entity sceneEntity{entity, iScene.get()};
		const auto& asset = *tilemap.asset;
		const math::vec3 origin = iScene->getWorldTransform(sceneEntity).translation();
		auto& entry = iScene->getNavigation().acquire(static_cast<uint64_t>(sceneEntity.getUUID()), asset,
													  tilemap.navigation);
		const auto& map = entry.map;
		const auto center = [&](const uint32_t iX, const uint32_t iY) -> math::vec3 {
			return scene::nav::getCellCenter(asset, origin, iX, iY);
		};
		if (pathDone || !m_navFrom.has_value() || !m_navTo.has_value())
			continue;
		pathDone = true;
		const std::array<uint64_t, 6> key{m_navFrom->x(),   m_navFrom->y(),
										  m_navTo->x(),     m_navTo->y(),
										  map.getVersion(), static_cast<uint64_t>(sceneEntity.getUUID())};
		if (key != m_navKey) {
			m_navKey = key;
			if (mp_navFinder == nullptr)
				mp_navFinder = mkUniq<scene::nav::Pathfinder>();
			if (!mp_navFinder->find(map, *m_navFrom, *m_navTo, scene::nav::PathMode::Auto, m_navPath))
				m_navPath.cells.clear();
		}
		for (size_t i = 1; i < m_navPath.cells.size(); ++i) {
			const auto from =
					center(m_navPath.cells[i - 1].x(), m_navPath.cells[i - 1].y()) + math::vec3{0.f, 0.f, 0.03f};
			const auto to = center(m_navPath.cells[i].x(), m_navPath.cells[i].y()) + math::vec3{0.f, 0.f, 0.03f};
			renderer::Renderer2D::drawLine({.point1 = from, .point2 = to, .color = pathColor});
		}
		for (const auto& end: {*m_navFrom, *m_navTo}) {
			const auto c = center(end.x(), end.y());
			const std::array<math::vec3, 4> ring{c + math::vec3{-0.3f, 0.f, 0.05f}, c + math::vec3{0.f, -0.3f, 0.05f},
												 c + math::vec3{0.3f, 0.f, 0.05f}, c + math::vec3{0.f, 0.3f, 0.05f}};
			for (size_t i = 0; i < ring.size(); ++i)
				renderer::Renderer2D::drawLine(
						{.point1 = ring[i], .point2 = ring[(i + 1) % ring.size()], .color = pathColor});
		}
	}
}

void Viewport::renderIsometricCursor() const {
	const auto& theme = gui::UiLayer::getTheme();
	if (m_hoveredCell.has_value()) {
		const auto cx = static_cast<float>(m_hoveredCell->x());
		const auto cy = static_cast<float>(m_hoveredCell->y());
		const std::array<math::vec3, 4> corners{m_hoveredMapOrigin + math::vec3{cx, cy, m_hoveredCornerZ[0]},
												m_hoveredMapOrigin + math::vec3{cx + 1.f, cy, m_hoveredCornerZ[1]},
												m_hoveredMapOrigin +
														math::vec3{cx + 1.f, cy + 1.f, m_hoveredCornerZ[2]},
												m_hoveredMapOrigin + math::vec3{cx, cy + 1.f, m_hoveredCornerZ[3]}};
		for (size_t i = 0; i < corners.size(); ++i)
			renderer::Renderer2D::drawLine(
					{.point1 = corners[i], .point2 = corners[(i + 1) % corners.size()], .color = theme.accent});
	}
	if (!m_isoCursor.has_value())
		return;
	const math::vec3& origin = *m_isoCursor;
	renderer::Renderer2D::drawLine(
			{.point1 = origin, .point2 = origin + math::vec3{1.f, 0.f, 0.f}, .color = theme.axisX});
	renderer::Renderer2D::drawLine(
			{.point1 = origin, .point2 = origin + math::vec3{0.f, 1.f, 0.f}, .color = theme.axisY});
	renderer::Renderer2D::drawLine(
			{.point1 = origin, .point2 = origin + math::vec3{0.f, 0.f, 1.f}, .color = theme.axisZ});
}

void Viewport::setGuizmoType(const gui::Guizmo::Type& iType) { m_gizmoType = iType; }

auto Viewport::getGuizmoType() const -> gui::Guizmo::Type { return m_gizmoType; }

auto Viewport::getGuizmoTypeI() const -> uint16_t { return static_cast<uint16_t>(m_gizmoType); }

void Viewport::renderGizmo() {
	// Gizmos
	if (mp_document == nullptr || m_parent == nullptr)
		return;
	if (mp_document->state() != SceneDocument::State::Edit)
		return;
	const auto& activeScene = mp_document->getActiveScene();
	if (!activeScene)
		return;

	if (renderWaypointGizmo() || renderSocketGizmo())
		return;
	if (const scene::Entity selectedEntity = m_parent->getSelectedEntity();
		selectedEntity && m_gizmoType != gui::Guizmo::Type::None && !isPiloted(selectedEntity)) {
		gui::Guizmo::initialize(m_lower, m_upper - m_lower,
								m_isometric || m_editorCamera.getProjectionMode() ==
													   renderer::CameraEditor::Projection::Orthographic);

		const auto& camera = getActiveCamera();
		math::mat4 cameraProjection = camera.getProjection();
		if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan)
			cameraProjection(1, 1) *= -1.f;
		const math::mat4 cameraView = camera.getView();

		auto& tc = selectedEntity.getComponent<scene::component::Transform>();
		math::mat4 transform = activeScene->getWorldTransform(selectedEntity)();

		const bool ctrlHeld = input::Input::isKeyPressed(input::key::LeftControl);
		const bool snapEnabledInSettings = m_parent != nullptr && m_parent->getSettings().snapEnabled;
		const bool snap = ctrlHeld || snapEnabledInSettings;
		const auto tilemapInfo = findFirstTilemapInfo(activeScene.get());
		float snapValue = 0.5f;
		if (gui::Guizmo::isRotate(m_gizmoType)) {
			snapValue = m_parent->getSettings().snapRotationDegrees;
		} else if (m_parent != nullptr) {
			snapValue = resolveTranslationSnapStep(m_parent->getSettings(), tilemapInfo);
		}

		gui::Guizmo::manipulate(cameraView, cameraProjection, m_gizmoType, transform, snap ? snapValue : 0.f,
								m_parent->getSettings().gizmoWorldSpace ? gui::Guizmo::Space::World
																		: gui::Guizmo::Space::Local);

		const bool isUsing = gui::Guizmo::isUsing();

		if (isUsing && !m_gizmoWasUsing && mp_undoManager != nullptr)
			m_gizmoBeforeYaml = scene::SceneSerializer::serializeEntityToString(selectedEntity);

		if (isUsing) {
			const auto& hierarchy = selectedEntity.getComponent<scene::component::Hierarchy>();
			math::mat4 localMat = transform;
			if (hierarchy.parentId != core::UUID{0}) {
				if (const auto parent = activeScene->findEntityByUUID(hierarchy.parentId); parent) {
					const math::mat4 parentWorldInv = math::inverse(activeScene->getWorldTransform(parent)());
					localMat = parentWorldInv * transform;
				}
			}
			const math::Transform newLocal{localMat};
			const math::vec3 deltaRotation = newLocal.rotation() - tc.transform.rotation();
			tc.transform.translation() = newLocal.translation();
			tc.transform.rotation() += deltaRotation;
			tc.transform.scale() = newLocal.scale();
		}

		if (!isUsing && m_gizmoWasUsing && snap && (m_gizmoType == gui::Guizmo::Type::Translation) &&
			tilemapInfo.has_value() && m_parent != nullptr && m_parent->getSettings().snapAutoFromTilemap) {
			const math::Transform worldT = activeScene->getWorldTransform(selectedEntity);
			const float effectiveStep =
					std::max(0.0001f, tilemapInfo->cellSize * m_parent->getSettings().snapMultiplier);
			const math::vec3 snappedWorld = snapToTilemapCellCenter(worldT.translation(), *tilemapInfo, effectiveStep);
			if (snappedWorld != worldT.translation()) {
				const math::Transform targetWorld{snappedWorld, worldT.rotation(), worldT.scale()};
				math::mat4 newLocalMat = targetWorld();
				const auto& hierarchy = selectedEntity.getComponent<scene::component::Hierarchy>();
				if (hierarchy.parentId != core::UUID{0}) {
					if (const auto parent = activeScene->findEntityByUUID(hierarchy.parentId); parent) {
						const math::mat4 parentWorldInv = math::inverse(activeScene->getWorldTransform(parent)());
						newLocalMat = parentWorldInv * targetWorld();
					}
				}
				const math::Transform newLocal{newLocalMat};
				tc.transform.translation() = newLocal.translation();
			}
		}

		if (!isUsing && m_gizmoWasUsing && mp_undoManager != nullptr && !m_gizmoBeforeYaml.empty()) {
			commitTransformEdit(selectedEntity, activeScene, m_gizmoBeforeYaml);
			m_gizmoBeforeYaml.clear();
		}
		m_gizmoWasUsing = isUsing;
	}
}

void Viewport::commitTransformEdit(const scene::Entity& iEntity, const shared<scene::Scene>& iScene,
								   const std::string& iBeforeYaml) const {
	const auto afterYaml = scene::SceneSerializer::serializeEntityToString(iEntity);
	if (iBeforeYaml == afterYaml)
		return;
	const auto transform = iEntity.getComponent<scene::component::Transform>().transform;
	// The command records the move from the state before the drag.
	std::ignore = scene::SceneSerializer::applyEntityFromString(iEntity, iBeforeYaml);
	const commands::CommandTarget target{.registry = &m_parent->getRegistry(), .scene = iScene, .undo = mp_undoManager};
	if (const auto moved = target.execute("entity.set_transform",
										  {{"entity", static_cast<int64_t>(static_cast<uint64_t>(iEntity.getUUID()))},
										   {"translation", transform.translation()},
										   {"rotation", transform.rotation()},
										   {"scale", transform.scale()}});
		!moved)
		std::ignore = scene::SceneSerializer::applyEntityFromString(iEntity, afterYaml);
}

void Viewport::onEvent(event::Event& ioEvent) {
	if (mp_document != nullptr && mp_document->state() == SceneDocument::State::Edit && isHovered() && !m_isometric)
		m_editorCamera.onEvent(ioEvent);

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[this]<typename T0>(T0&& ioPh1) -> auto { return onMouseButtonPressed(std::forward<T0>(ioPh1)); });
}

auto Viewport::getCursorRay() const -> std::optional<std::pair<math::vec3, math::vec3>> {
	const math::vec2 vpSize = m_upper - m_lower;
	if (vpSize.x() <= 0.f || vpSize.y() <= 0.f)
		return std::nullopt;
	const auto mousePos = ImGui::GetMousePos();
	const float ndcX = 2.f * (mousePos.x - m_lower.x()) / vpSize.x() - 1.f;
	const float screenY = 2.f * (mousePos.y - m_lower.y()) / vpSize.y() - 1.f;
	const bool isOpenGl = renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::OpenGL;
	const float ndcY = isOpenGl ? -screenY : screenY;
	const auto& camera = getActiveCamera();
	const math::mat4 vpInv = inverse(camera.getProjection() * camera.getView());
	const auto unproject = [&](const float iDepth) -> std::optional<math::vec3> {
		const math::vec4 point = vpInv * math::vec4{ndcX, ndcY, iDepth, 1.f};
		if (std::abs(point.w()) < 1e-6f)
			return std::nullopt;
		return math::vec3{point.x() / point.w(), point.y() / point.w(), point.z() / point.w()};
	};
	// Reversed-Z: the near plane at depth 1, a farther point halfway (finite with an infinite far plane).
	const auto nearPoint = unproject(renderer::g_nearDepth);
	const auto farPoint = unproject(0.5f);
	if (!nearPoint || !farPoint || (*farPoint - *nearPoint).normSq() < 1e-12f)
		return std::nullopt;
	return std::pair{*nearPoint, (*farPoint - *nearPoint).normalized()};
}

auto Viewport::getDropPoint(const shared<scene::Scene>& iScene) const -> std::optional<math::vec3> {
	const auto ray = getCursorRay();
	if (!ray || !iScene)
		return std::nullopt;
	const auto& [origin, direction] = *ray;
	if (const auto hovered = getHoveredEntity(); hovered && hovered.hasComponent<scene::component::MeshRenderer>()) {
		if (const auto& mesh = hovered.getComponent<scene::component::MeshRenderer>().mesh; mesh) {
			const math::mat4 model = iScene->getWorldTransform(hovered)();
			const math::mat4 toObject = inverse(model);
			const math::vec4 localOrigin = toObject * math::vec4{origin.x(), origin.y(), origin.z(), 1.f};
			const math::vec4 localDirection = toObject * math::vec4{direction.x(), direction.y(), direction.z(), 0.f};
			if (const auto hit =
						mesh->getGeometry().raycast({localOrigin.x(), localOrigin.y(), localOrigin.z()},
													{localDirection.x(), localDirection.y(), localDirection.z()});
				hit) {
				const math::vec4 world = model * math::vec4{hit->x(), hit->y(), hit->z(), 1.f};
				return math::vec3{world.x(), world.y(), world.z()};
			}
		}
	}
	if (const auto hovered = getHoveredEntity(); hovered && hovered.hasComponent<scene::component::VoxelWorld>()) {
		const auto& voxel = hovered.getComponent<scene::component::VoxelWorld>();
		const auto targetable = [&voxel](const int32_t iX, const int32_t iY, const int32_t iZ) -> bool {
			return !voxel.registry.isAir(voxel.world.getBlock(math::vec3i{iX, iY, iZ}));
		};
		if (const auto hit = data::voxel::raycastVoxel(targetable, origin, direction, 256.f); hit)
			return math::vec3{static_cast<float>(hit->block.x()) + 0.5f + 0.5f * static_cast<float>(hit->normal.x()),
							  static_cast<float>(hit->block.y()) + 0.5f + 0.5f * static_cast<float>(hit->normal.y()),
							  static_cast<float>(hit->block.z()) + 0.5f + 0.5f * static_cast<float>(hit->normal.z())};
	}
	for (const size_t axis: {size_t{1}, size_t{2}}) {
		if (std::abs(direction[axis]) < 1e-4f)
			continue;
		if (const float distance = -origin[axis] / direction[axis]; distance > 0.f) {
			math::vec3 point = origin + direction * distance;
			point[axis] = 0.f;
			return point;
		}
	}
	return std::nullopt;
}

void Viewport::handleVoxelBrush(const shared<scene::Scene>& iScene) {
	if (m_parent == nullptr || !iScene) {
		if (iScene)
			iScene->setEditorVoxelHighlight(false);
		return;
	}
	auto& brush = m_parent->getVoxelBrush();
	if (!brush.active || !isHovered()) {
		iScene->setEditorVoxelHighlight(false);
		return;
	}
	// Find the first voxel world in the scene.
	scene::Entity worldEntity;
	scene::component::VoxelWorld* voxelWorld = nullptr;
	if (const auto view = iScene->registry.view<scene::component::VoxelWorld>(); view.begin() != view.end()) {
		const auto entity = *view.begin();
		worldEntity = scene::Entity{entity, iScene.get()};
		voxelWorld = &view.get<scene::component::VoxelWorld>(entity);
	}
	if (voxelWorld == nullptr) {
		iScene->setEditorVoxelHighlight(false);
		return;
	}

	const auto ray = getCursorRay();
	if (!ray) {
		iScene->setEditorVoxelHighlight(false);
		return;
	}
	const auto& [origin, direction] = *ray;

	auto& world = voxelWorld->world;
	const auto& registry = voxelWorld->registry;
	const auto targetable = [&](const int32_t iX, const int32_t iY, const int32_t iZ) -> bool {
		return !registry.isAir(world.getBlock(math::vec3i{iX, iY, iZ}));
	};
	const auto hit = data::voxel::raycastVoxel(targetable, origin, direction, 256.f);
	if (!hit) {
		iScene->setEditorVoxelHighlight(false);
		return;
	}
	iScene->setEditorVoxelHighlight(true, hit->block, hit->normal);

	if (gui::Guizmo::isOver() || mp_undoManager == nullptr)
		return;
	// Alt / Ctrl + click drive the editor camera, not the brush.
	if (input::Input::isKeyPressed(input::key::LeftAlt) || input::Input::isKeyPressed(input::key::LeftControl))
		return;
	const bool place = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	const bool erase = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
	if (!place && !erase)
		return;
	const math::vec3i adjacent{hit->block.x() + hit->normal.x(), hit->block.y() + hit->normal.y(),
							   hit->block.z() + hit->normal.z()};
	std::vector<commands::VoxelBlockEdit> edits;
	const auto addEdit = [&](const math::vec3i& iCoord, const data::voxel::BlockId iNew,
							 const data::voxel::PackedMeta iNewMeta) -> void {
		const data::voxel::BlockId old = world.getBlock(iCoord);
		const data::voxel::PackedMeta oldMeta = world.getMeta(iCoord);
		if (old != iNew || oldMeta != iNewMeta)
			edits.push_back(
					{.coord = iCoord, .before = old, .after = iNew, .beforeMeta = oldMeta, .afterMeta = iNewMeta});
	};
	std::string description;
	if (erase || (place && brush.eraser && !brush.structure.has_value())) {
		addEdit(hit->block, data::voxel::g_AirBlock, data::voxel::g_DefaultMeta);
		description = "Erase voxel";
	} else if (place && brush.structure.has_value()) {
		brush.structure->forEachSolid([&](const math::vec3i& iLocal, const data::voxel::BlockId iId,
										  const data::voxel::PackedMeta iMeta) -> void {
			addEdit(math::vec3i{adjacent.x() + iLocal.x(), adjacent.y() + iLocal.y(), adjacent.z() + iLocal.z()}, iId,
					iMeta);
		});
		description = "Stamp structure";
	} else if (place) {
		addEdit(adjacent, brush.paintBlock,
				data::voxel::packMeta({.orientation = brush.orientation, .state = brush.state}));
		description = "Paint voxel";
	}
	if (edits.empty())
		return;
	auto command = mkUniq<commands::VoxelEditCommand>(worldEntity.getUUID(), std::move(edits), description);
	mp_undoManager->execute(std::move(command), *iScene);
}

auto Viewport::onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent) -> bool {
	if (mp_document == nullptr || mp_document->state() != SceneDocument::State::Edit)
		return false;
	// The voxel brush owns clicks when active; don't also change the entity selection.
	if (m_parent != nullptr && m_parent->getVoxelBrush().active)
		return false;
	// So does the navigation tool of the isometric view.
	if (m_navTool.active && m_isometric && m_parent != nullptr && m_parent->getSettings().showNavGraph)
		return false;
	if (ioEvent.getMouseButton() == input::mouse::ButtonLeft) {
		if (isHovered() && !gui::Guizmo::isOver() && !input::Input::isKeyPressed(input::key::LeftAlt) &&
			m_parent != nullptr) {
			if (const auto socket = pickSocket(); socket) {
				gui::component::getSocketSelection() = {.entity = m_parent->getSelectedEntity().getUUID(),
														.socket = *socket};
				return false;
			}
			gui::component::getSocketSelection().clear();
			if (const int handle = pickPatrolWaypoint(); handle >= 0) {
				m_patrolHandle = handle;
				return false;
			}
			m_patrolHandle = -1;
			const auto camera = pickMarker();
			m_parent->setSelectedEntity(camera ? camera : getHoveredEntity());
		}
	}
	return false;
}

auto Viewport::pickCameraModel() const -> scene::Entity {
	if (m_parent == nullptr || !m_parent->getSettings().showCameraGizmos || !showsCameraModels() || !m_cameraModel)
		return {};
	const auto& scene = mp_document->getActiveScene();
	const auto ray = getCursorRay();
	if (!scene || !ray)
		return {};
	const auto& [origin, direction] = *ray;
	scene::Entity nearest;
	float nearestDistance = std::numeric_limits<float>::max();
	for (const auto view = scene->registry.view<scene::component::Camera, scene::component::Transform>();
		 const auto entity: view) {
		const scene::Entity camera{entity, scene.get()};
		if (isPiloted(camera))
			continue;
		math::Transform rigid = scene->getWorldTransform(camera);
		rigid.scale() = {1.f, 1.f, 1.f};
		const math::mat4 model = rigid();
		const math::mat4 toObject = inverse(model);
		const math::vec4 localOrigin = toObject * math::vec4{origin.x(), origin.y(), origin.z(), 1.f};
		const math::vec4 localDirection = toObject * math::vec4{direction.x(), direction.y(), direction.z(), 0.f};
		const auto hit =
				m_cameraModel->getGeometry().raycast({localOrigin.x(), localOrigin.y(), localOrigin.z()},
													 {localDirection.x(), localDirection.y(), localDirection.z()});
		if (!hit)
			continue;
		const math::vec4 world = model * math::vec4{hit->x(), hit->y(), hit->z(), 1.f};
		if (const float distance = (math::vec3{world.x(), world.y(), world.z()} - origin).norm();
			distance < nearestDistance) {
			nearestDistance = distance;
			nearest = camera;
		}
	}
	return nearest;
}

auto Viewport::showsLightMarkers() const -> bool {
	return m_parent != nullptr && mp_document != nullptr && mp_document->state() == SceneDocument::State::Edit &&
		   !m_isometric && m_parent->getSettings().showCameraGizmos;
}

auto Viewport::getLightMarkerRadius(const math::vec3& iPosition) const -> float {
	constexpr float screenFraction = 0.025f;
	const float distance = getViewProjection() == renderer::CameraEditor::Projection::Perspective
								   ? (iPosition - m_editorCamera.getPosition()).norm()
								   : m_editorCamera.getDistance();
	return std::max(0.02f, distance * screenFraction);
}

namespace {

// World position and unit direction (-Z of the world transform) of a light entity.
auto lightFrame(const shared<scene::Scene>& iScene, const scene::Entity& iEntity) -> std::pair<math::vec3, math::vec3> {
	math::Transform rigid = iScene->getWorldTransform(iEntity);
	rigid.scale() = {1.f, 1.f, 1.f};
	const math::vec4 forward = rigid() * math::vec4{0.f, 0.f, -1.f, 0.f};
	math::vec3 direction{forward.x(), forward.y(), forward.z()};
	if (direction.normSq() < 1e-12f)
		direction = {0.f, 0.f, -1.f};
	return {rigid.translation(), direction.normalized()};
}

// Draw a circle of lines around a centre, in the plane spanned by two unit vectors.
void drawLightCircle(const math::vec3& iCenter, const math::vec3& iU, const math::vec3& iV, const float iRadius,
					 const math::vec4& iColor, const int iEntityId) {
	constexpr int segments = 48;
	constexpr float step = 2.f * std::numbers::pi_v<float> / static_cast<float>(segments);
	auto previous = iCenter + iU * iRadius;
	for (int i = 1; i <= segments; ++i) {
		const float angle = step * static_cast<float>(i);
		const math::vec3 next = iCenter + iU * (std::cos(angle) * iRadius) + iV * (std::sin(angle) * iRadius);
		renderer::Renderer2D::drawLine({.point1 = previous, .point2 = next, .color = iColor, .entityId = iEntityId});
		previous = next;
	}
}

// Draw a sphere as its three great circles in the world axis planes.
void drawLightRange(const math::vec3& iCenter, const float iRadius, const math::vec4& iColor, const int iEntityId) {
	drawLightCircle(iCenter, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, iRadius, iColor, iEntityId);
	drawLightCircle(iCenter, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, iRadius, iColor, iEntityId);
	drawLightCircle(iCenter, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, iRadius, iColor, iEntityId);
}

// Unit X axis of the world transform of a light entity (the width of a rectangle light).
auto lightAxis(const shared<scene::Scene>& iScene, const scene::Entity& iEntity) -> math::vec3 {
	const math::vec4 axis = iScene->getWorldTransform(iEntity)() * math::vec4{1.f, 0.f, 0.f, 0.f};
	const math::vec3 result{axis.x(), axis.y(), axis.z()};
	return result.normSq() > 1e-12f ? result.normalized() : math::vec3{1.f, 0.f, 0.f};
}

// Draw an arrow from a point along a unit direction, its head a cone of four lines.
void drawLightArrow(const math::vec3& iStart, const math::vec3& iDirection, const float iLength, const math::vec3& iU,
					const math::vec3& iV, const float iHead, const math::vec4& iColor, const int iEntityId) {
	const math::vec3 tip = iStart + iDirection * iLength;
	renderer::Renderer2D::drawLine({.point1 = iStart, .point2 = tip, .color = iColor, .entityId = iEntityId});
	const math::vec3 base = tip - iDirection * (iHead * 2.f);
	for (const math::vec3& side: {iU, iV, iU * -1.f, iV * -1.f})
		renderer::Renderer2D::drawLine(
				{.point1 = tip, .point2 = base + side * iHead, .color = iColor, .entityId = iEntityId});
}

// Draw the twelve edges of an axis-aligned box.
void drawProbeBox(const math::vec3& iCenter, const math::vec3& iExtents, const math::vec4& iColor,
				  const int iEntityId) {
	const auto corner = [&](const uint32_t iBits) -> math::vec3 {
		return iCenter + math::vec3{(iBits & 1U) != 0 ? iExtents.x() : -iExtents.x(),
									(iBits & 2U) != 0 ? iExtents.y() : -iExtents.y(),
									(iBits & 4U) != 0 ? iExtents.z() : -iExtents.z()};
	};
	for (uint32_t bits = 0; bits < 8; ++bits) {
		for (const uint32_t axis: {1U, 2U, 4U}) {
			if ((bits & axis) != 0)
				continue;
			renderer::Renderer2D::drawLine(
					{.point1 = corner(bits), .point2 = corner(bits | axis), .color = iColor, .entityId = iEntityId});
		}
	}
}

// Draw the reflection probes: a small sphere at the capture point and the volume of influence, brighter selected.
void drawProbeMarkers(const shared<scene::Scene>& iScene, const scene::Entity& iSelected,
					  const std::function<float(const math::vec3&)>& iMarkerRadius) {
	using Shape = scene::component::ReflectionProbe::Shape;
	for (const auto view = iScene->registry.view<scene::component::ReflectionProbe, scene::component::Transform>();
		 const auto entity: view) {
		const auto& probe = view.get<scene::component::ReflectionProbe>(entity);
		const scene::Entity ent{entity, iScene.get()};
		const auto id = static_cast<int>(entity);
		const math::vec3 position = iScene->getWorldTransform(ent).translation();
		const bool isSelected = iSelected && static_cast<uint32_t>(ent) == static_cast<uint32_t>(iSelected);
		const math::vec4 color{0.45f, 0.85f, 1.f, 1.f};
		const math::vec4 volume{0.45f, 0.85f, 1.f, isSelected ? 0.9f : 0.3f};
		drawLightRange(position, iMarkerRadius(position) * 1.2f, color, id);
		if (probe.shape == Shape::Sphere)
			drawLightRange(position, probe.radius, volume, id);
		else
			drawProbeBox(position, probe.extents, volume, id);
	}
}

// Draw the twelve edges of a box of half size `iExtents` in the frame of a world matrix.
void drawOrientedBox(const math::mat4& iWorld, const math::vec3& iExtents, const math::vec4& iColor,
					 const int iEntityId) {
	const auto corner = [&](const uint32_t iBits) -> math::vec3 {
		const math::vec4 local{(iBits & 1U) != 0 ? iExtents.x() : -iExtents.x(),
							   (iBits & 2U) != 0 ? iExtents.y() : -iExtents.y(),
							   (iBits & 4U) != 0 ? iExtents.z() : -iExtents.z(), 1.f};
		const math::vec4 world = iWorld * local;
		return {world.x(), world.y(), world.z()};
	};
	for (uint32_t bits = 0; bits < 8; ++bits) {
		for (const uint32_t axis: {1U, 2U, 4U}) {
			if ((bits & axis) != 0)
				continue;
			renderer::Renderer2D::drawLine(
					{.point1 = corner(bits), .point2 = corner(bits | axis), .color = iColor, .entityId = iEntityId});
		}
	}
}

// Draw the reverb zones: their volume, and selected, the outer edge of their fade.
void drawReverbZoneMarkers(const shared<scene::Scene>& iScene, const scene::Entity& iSelected) {
	using Shape = scene::component::ReverbZone::Shape;
	for (const auto view = iScene->registry.view<scene::component::ReverbZone, scene::component::Transform>();
		 const auto entity: view) {
		const auto& zone = view.get<scene::component::ReverbZone>(entity);
		const scene::Entity ent{entity, iScene.get()};
		const auto id = static_cast<int>(entity);
		const math::mat4 world = iScene->getWorldTransform(ent)();
		const bool isSelected = iSelected && static_cast<uint32_t>(ent) == static_cast<uint32_t>(iSelected);
		const math::vec4 volume{0.62f, 0.83f, 0.42f, isSelected ? 0.95f : 0.35f};
		const math::vec4 fade{0.62f, 0.83f, 0.42f, 0.25f};
		const math::vec3 center{world(0, 3), world(1, 3), world(2, 3)};
		float scale = 0.f;
		std::array<float, 3> axisLength{};
		for (size_t axis = 0; axis < 3; ++axis) {
			axisLength.at(axis) = math::vec3{world(0, axis), world(1, axis), world(2, axis)}.norm();
			scale = std::max(scale, axisLength.at(axis));
		}
		if (zone.shape == Shape::Sphere) {
			drawLightRange(center, zone.radius * scale, volume, id);
			if (isSelected && zone.blendDistance > 0.f)
				drawLightRange(center, zone.radius * scale + zone.blendDistance, fade, id);
			continue;
		}
		drawOrientedBox(world, zone.extents, volume, id);
		if (isSelected && zone.blendDistance > 0.f) {
			math::vec3 outer = zone.extents;
			for (size_t axis = 0; axis < 3; ++axis)
				if (axisLength.at(axis) > 1e-6f)
					outer[axis] += zone.blendDistance / axisLength.at(axis);
			drawOrientedBox(world, outer, fade, id);
		}
	}
}

}// namespace

void Viewport::renderSkeletonMarkers(const shared<scene::Scene>& iScene) const {
	const scene::Entity selected = m_parent != nullptr ? m_parent->getSelectedEntity() : scene::Entity{};
	if (!selected || selected.getScene() != iScene.get())
		return;
	const auto id = static_cast<int>(static_cast<uint32_t>(selected));
	const math::mat4 world = iScene->getWorldTransform(selected)();
	const auto toWorld = [&world](const math::mat4& iLocal) -> math::vec3 {
		const math::vec4 point = world * math::vec4{iLocal(0, 3), iLocal(1, 3), iLocal(2, 3), 1.f};
		return {point.x(), point.y(), point.z()};
	};
	if (selected.hasComponent<scene::component::MeshRenderer>()) {
		const auto& mesh = selected.getComponent<scene::component::MeshRenderer>().mesh;
		if (const auto skeleton = mesh ? mesh->getSkeleton() : nullptr; skeleton) {
			std::vector<math::mat4> bones(skeleton->getBoneCount());
			const auto* animator = selected.hasComponent<scene::component::Animator>()
										   ? &selected.getComponent<scene::component::Animator>()
										   : nullptr;
			if (animator != nullptr && animator->hasPose(bones.size()))
				bones = animator->boneMatrices;
			else
				skeleton->computeModelMatrices(skeleton->getRestPose(), bones);
			constexpr math::vec4 boneColor{0.47f, 0.78f, 1.f, 0.9f};
			for (size_t index = 0; index < bones.size(); ++index) {
				if (const int32_t parent = skeleton->getBones()[index].parent; parent >= 0)
					renderer::Renderer2D::drawLine({.point1 = toWorld(bones[static_cast<size_t>(parent)]),
													.point2 = toWorld(bones[index]),
													.color = boneColor,
													.entityId = id});
			}
		}
	}
	constexpr math::vec4 socketColor{0.47f, 1.f, 0.55f, 1.f};
	constexpr math::vec4 selectedColor{1.f, 0.8f, 0.25f, 1.f};
	const auto& selection = gui::component::getSocketSelection();
	for (const auto& name: iScene->getSocketNames(selected)) {
		const auto local = iScene->getSocketTransform(selected, name);
		if (!local)
			continue;
		const math::vec3 centre = toWorld(*local);
		const bool picked = selection.is(selected.getUUID(), name);
		const float size = getLightMarkerRadius(centre) * (picked ? 0.8f : 0.5f);
		const math::vec4& color = picked ? selectedColor : socketColor;
		for (size_t axis = 0; axis < 3; ++axis) {
			math::vec3 offset{0.f, 0.f, 0.f};
			offset[axis] = size;
			renderer::Renderer2D::drawLine(
					{.point1 = centre - offset, .point2 = centre + offset, .color = color, .entityId = id});
		}
		if (picked)
			drawLightRange(centre, size * 0.6f, color, id);
	}
}

void Viewport::renderLightMarkers(const shared<scene::Scene>& iScene) const {
	using LightType = scene::component::Light::Type;
	const scene::Entity selected = m_parent != nullptr ? m_parent->getSelectedEntity() : scene::Entity{};
	for (const auto view = iScene->registry.view<scene::component::Light, scene::component::Transform>();
		 const auto entity: view) {
		const auto& light = view.get<scene::component::Light>(entity);
		const scene::Entity ent{entity, iScene.get()};
		const auto id = static_cast<int>(entity);
		const auto [position, direction] = lightFrame(iScene, ent);
		// A light switched off is drawn grey.
		const math::vec4 color = light.enabled ? math::vec4{light.color.x(), light.color.y(), light.color.z(), 1.f}
											   : math::vec4{0.45f, 0.45f, 0.45f, 0.6f};
		const math::vec4 dim{color.x(), color.y(), color.z(), color.w() * 0.45f};
		const float radius = getLightMarkerRadius(position);
		const math::vec3 helper =
				std::abs(direction.y()) < 0.9f ? math::vec3{0.f, 1.f, 0.f} : math::vec3{1.f, 0.f, 0.f};
		const math::vec3 u = (direction ^ helper).normalized();
		const math::vec3 v = direction ^ u;
		const bool isSelected = selected && static_cast<uint32_t>(ent) == static_cast<uint32_t>(selected);
		switch (light.type) {
			case LightType::Directional:
				{
					// A sun disc with rays, and the arrow of the light direction.
					drawLightCircle(position, u, v, radius, color, id);
					constexpr int rays = 8;
					for (int ray = 0; ray < rays; ++ray) {
						const float angle = 2.f * std::numbers::pi_v<float> * static_cast<float>(ray) / rays;
						const math::vec3 out = u * std::cos(angle) + v * std::sin(angle);
						renderer::Renderer2D::drawLine({.point1 = position + out * (radius * 1.3f),
														.point2 = position + out * (radius * 1.8f),
														.color = color,
														.entityId = id});
					}
					drawLightArrow(position, direction, radius * 5.f, u, v, radius * 0.4f, color, id);
					// Selected: parallel rays around it.
					if (isSelected) {
						for (const math::vec3& side: {u, v, u * -1.f, v * -1.f})
							drawLightArrow(position + side * (radius * 2.5f), direction, radius * 8.f, u, v,
										   radius * 0.4f, dim, id);
					}
					break;
				}
			case LightType::Point:
				{
					// A bulb sprite facing the camera, lit or switched off; selected, the range sphere.
					drawLightBulb(position, radius * 2.4f, light.enabled, id);
					if (isSelected) {
						drawLightRange(position, light.range, color, id);
					}
					break;
				}
			case LightType::Spot:
				{
					// A small cone of the outer angle; selected, the cones down to the range.
					const float length = isSelected ? light.range : radius * 4.f;
					const math::vec3 center = position + direction * length;
					drawLightCircle(position, u, v, radius * 0.3f, color, id);
					for (const auto& [angle, coneColor]:
						 {std::pair{light.outerAngle, color}, std::pair{light.innerAngle, dim}}) {
						if (!isSelected && coneColor.w() < color.w())
							continue;
						const float coneRadius = length * std::tan(angle);
						drawLightCircle(center, u, v, coneRadius, coneColor, id);
						for (const math::vec3& side: {u, v, u * -1.f, v * -1.f})
							renderer::Renderer2D::drawLine({.point1 = position,
															.point2 = center + side * coneRadius,
															.color = coneColor,
															.entityId = id});
					}
					break;
				}
			case LightType::Rectangle:
				{
					// The emitting rectangle in the XY plane of the entity and the arrow of its face; selected, the range.
					const math::vec3 width = lightAxis(iScene, ent) * (0.5f * light.size.x());
					const math::vec3 height = (width.normalized() ^ direction) * (0.5f * light.size.y());
					const std::array corners{position + width + height, position - width + height,
											 position - width - height, position + width - height};
					for (size_t corner = 0; corner < corners.size(); ++corner)
						renderer::Renderer2D::drawLine({.point1 = corners[corner],
														.point2 = corners[(corner + 1) % corners.size()],
														.color = color,
														.entityId = id});
					drawLightArrow(position, direction, radius * 4.f, u, v, radius * 0.4f, color, id);
					if (isSelected)
						drawLightRange(position, light.range, dim, id);
					break;
				}
			case LightType::Disc:
				{
					// The emitting disc and the arrow of its face; selected, the range.
					drawLightCircle(position, u, v, light.radius, color, id);
					drawLightArrow(position, direction, radius * 4.f, u, v, radius * 0.4f, color, id);
					if (isSelected)
						drawLightRange(position, light.range, dim, id);
					break;
				}
			case LightType::Sphere:
				{
					// The emitting sphere; selected, the range.
					drawLightRange(position, light.radius, color, id);
					if (isSelected)
						drawLightRange(position, light.range, dim, id);
					break;
				}
		}
	}
	renderLight2DMarkers(iScene, selected);
	drawProbeMarkers(iScene, selected,
					 [this](const math::vec3& iPosition) -> float { return getLightMarkerRadius(iPosition); });
}

void Viewport::renderLight2DMarkers(const shared<scene::Scene>& iScene, const scene::Entity& iSelected) const {
	using LightType = scene::component::Light2D::Type;
	constexpr math::vec3 axisX{1.f, 0.f, 0.f};
	constexpr math::vec3 axisY{0.f, 1.f, 0.f};
	for (const auto view = iScene->registry.view<scene::component::Light2D, scene::component::Transform>();
		 const auto entity: view) {
		const auto& light = view.get<scene::component::Light2D>(entity);
		const scene::Entity ent{entity, iScene.get()};
		const auto id = static_cast<int>(entity);
		const math::mat4 world = iScene->getWorldTransform(ent)();
		const math::vec3 position{world(0, 3), world(1, 3), world(2, 3)};
		const math::vec4 color = light.enabled ? math::vec4{light.color.x(), light.color.y(), light.color.z(), 1.f}
											   : math::vec4{0.45f, 0.45f, 0.45f, 0.6f};
		const float radius = getLightMarkerRadius(position);
		const bool isSelected = iSelected && static_cast<uint32_t>(ent) == static_cast<uint32_t>(iSelected);
		if (light.type == LightType::Ambient) {
			// A sun disc: an ambient light has no position, only a handle to select it.
			drawLightCircle(position, axisX, axisY, radius, color, id);
			drawLightCircle(position, axisX, axisY, radius * 0.5f, color, id);
			continue;
		}
		drawLightBulb(position, radius * 2.4f, light.enabled, id);
		if (!isSelected)
			continue;
		if (light.type == LightType::Point) {
			drawLightCircle(position, axisX, axisY, light.radius, color, id);
			continue;
		}
		// Spot: its cone in the sprite plane, along the +X axis of the entity.
		math::vec3 direction{world(0, 0), world(1, 0), 0.f};
		direction = direction.normSq() > 1e-12f ? direction.normalized() : axisX;
		const math::vec3 side{-direction.y(), direction.x(), 0.f};
		for (const float angle: {light.outerAngle, -light.outerAngle, light.innerAngle, -light.innerAngle}) {
			const math::vec3 edge = direction * std::cos(angle) + side * std::sin(angle);
			renderer::Renderer2D::drawLine(
					{.point1 = position, .point2 = position + edge * light.radius, .color = color, .entityId = id});
		}
	}
}

void Viewport::drawLightBulb(const math::vec3& iPosition, const float iSize, const bool iEnabled,
							 const int iEntityId) const {
	const auto texture =
			renderer::Renderer::getTextureLibrary().get(iEnabled ? "icons/lights/bulb_on" : "icons/lights/bulb_off");
	if (texture == nullptr)
		return;
	// The rotation of the camera turns the quad (+Z normal) towards it.
	math::mat4 billboard = inverse(getActiveCamera().getView());
	billboard(0, 3) = iPosition.x();
	billboard(1, 3) = iPosition.y();
	billboard(2, 3) = iPosition.z();
	math::Transform transform{billboard};
	transform.scale() = {iSize, iSize, 1.f};
	renderer::Renderer2D::drawQuad({.transform = transform, .texture = texture, .entityId = iEntityId});
}

void Viewport::renderParticleMarkers(const shared<scene::Scene>& iScene) const {
	const scene::Entity selected = m_parent != nullptr ? m_parent->getSelectedEntity() : scene::Entity{};
	const math::mat4 cameraToWorld = inverse(getActiveCamera().getView());
	const math::vec3 right{cameraToWorld(0, 0), cameraToWorld(1, 0), cameraToWorld(2, 0)};
	const math::vec3 up{cameraToWorld(0, 1), cameraToWorld(1, 1), cameraToWorld(2, 1)};
	for (const auto view = iScene->registry.view<scene::component::ParticleEmitter, scene::component::Transform>();
		 const auto entity: view) {
		const scene::Entity emitter{entity, iScene.get()};
		const math::vec3 position = iScene->getWorldTransform(emitter).translation();
		const bool isSelected = selected && static_cast<uint32_t>(emitter) == static_cast<uint32_t>(selected);
		const math::vec4 color = isSelected ? math::vec4{1.f, 0.8f, 0.45f, 1.f} : math::vec4{1.f, 0.66f, 0.3f, 0.8f};
		const float radius = getLightMarkerRadius(position);
		// Four long rays and four short diagonal ones.
		constexpr int rays = 8;
		for (int ray = 0; ray < rays; ++ray) {
			const float angle = std::numbers::pi_v<float> * static_cast<float>(ray) / 4.f;
			const float length = radius * (ray % 2 == 0 ? 1.4f : 0.7f);
			const math::vec3 out = right * std::cos(angle) + up * std::sin(angle);
			renderer::Renderer2D::drawLine({.point1 = position + out * (radius * 0.2f),
											.point2 = position + out * length,
											.color = color,
											.entityId = static_cast<int>(entity)});
		}
	}
}

void Viewport::renderWorldCells(const shared<scene::Scene>& iScene) {
	const auto& settings = iScene->getWorldPartitionSettings();
	if (settings.cells.empty())
		return;
	const auto& origin = iScene->getWorldOrigin();
	const auto size = static_cast<float>(std::max(settings.cellSize, scene::WorldPartitionSettings::minCellSize));
	const bool streamed = iScene->hasWorldStreamer();
	for (const auto& cell: settings.cells) {
		const math::vec3d corner = settings.getCellCorner(cell.coords) - origin;
		const math::vec3 low{static_cast<float>(corner.x()), static_cast<float>(corner.y()),
							 static_cast<float>(corner.z())};
		const bool loaded =
				streamed && iScene->getWorldStreamer().getState(cell.coords) == scene::WorldCellState::Loaded;
		const math::vec4 color = loaded ? math::vec4{0.35f, 0.9f, 0.45f, 0.9f} : math::vec4{0.4f, 0.6f, 1.f, 0.6f};
		const auto at = [&](const int iX, const int iY, const int iZ) -> math::vec3 {
			return low + math::vec3{static_cast<float>(iX) * size, static_cast<float>(iY) * size,
									static_cast<float>(iZ) * size};
		};
		// The twelve edges of the cube: from each of four corners, the edges toward its three neighbours.
		for (const auto& [x, y, z]:
			 {std::array{0, 0, 0}, std::array{1, 1, 0}, std::array{1, 0, 1}, std::array{0, 1, 1}}) {
			for (const auto& [dx, dy, dz]: {std::array{1, 0, 0}, std::array{0, 1, 0}, std::array{0, 0, 1}}) {
				const int nx = dx != 0 ? 1 - x : x;
				const int ny = dy != 0 ? 1 - y : y;
				const int nz = dz != 0 ? 1 - z : z;
				renderer::Renderer2D::drawLine(
						{.point1 = at(x, y, z), .point2 = at(nx, ny, nz), .color = color, .entityId = -1});
			}
		}
	}
}

auto Viewport::pickParticleEmitter() const -> scene::Entity {
	if (!showsLightMarkers())
		return {};
	const auto& scene = mp_document->getActiveScene();
	const auto ray = getCursorRay();
	if (!scene || !ray)
		return {};
	const auto& [origin, direction] = *ray;
	scene::Entity nearest;
	float nearestDistance = std::numeric_limits<float>::max();
	for (const auto view = scene->registry.view<scene::component::ParticleEmitter, scene::component::Transform>();
		 const auto entity: view) {
		const scene::Entity emitter{entity, scene.get()};
		const math::vec3 center = scene->getWorldTransform(emitter).translation();
		const float radius = getLightMarkerRadius(center) * 1.4f;
		const math::vec3 toCenter = center - origin;
		const float along = toCenter * direction;
		if (along < 0.f || toCenter.normSq() - along * along > radius * radius || along >= nearestDistance)
			continue;
		nearestDistance = along;
		nearest = emitter;
	}
	return nearest;
}

auto Viewport::pickLight() const -> scene::Entity {
	if (!showsLightMarkers())
		return {};
	const auto& scene = mp_document->getActiveScene();
	const auto ray = getCursorRay();
	if (!scene || !ray)
		return {};
	const auto& [origin, direction] = *ray;
	scene::Entity nearest;
	float nearestDistance = std::numeric_limits<float>::max();
	for (const auto view = scene->registry.view<scene::component::Light, scene::component::Transform>();
		 const auto entity: view) {
		const scene::Entity light{entity, scene.get()};
		const math::vec3 center = scene->getWorldTransform(light).translation();
		// A sphere a little larger than the cross, so that it is easy to hit.
		const float radius = getLightMarkerRadius(center) * 1.6f;
		const math::vec3 toCenter = center - origin;
		const float along = toCenter * direction;
		if (along < 0.f)
			continue;
		if (const float missSq = toCenter.normSq() - along * along; missSq > radius * radius)
			continue;
		if (along < nearestDistance) {
			nearestDistance = along;
			nearest = light;
		}
	}
	return nearest;
}

namespace {

// Space of the waypoints of a patrol: the world transform of the parent, identity for a root.
auto patrolSpace(const shared<scene::Scene>& iScene, const scene::Entity& iEntity) -> math::mat4 {
	const auto parentId = iEntity.getComponent<scene::component::Hierarchy>().parentId;
	if (parentId != core::UUID{0}) {
		if (const auto parent = iScene->findEntityByUUID(parentId); parent)
			return iScene->getWorldTransform(parent)();
	}
	return math::identity<float, 4>();
}

auto transformPoint(const math::mat4& iMatrix, const math::vec3& iPoint) -> math::vec3 {
	const math::vec4 point = iMatrix * math::vec4{iPoint.x(), iPoint.y(), iPoint.z(), 1.f};
	return {point.x(), point.y(), point.z()};
}

// The waypoints as the YAML `values` of `component.set`.
auto waypointValues(const std::vector<math::vec3>& iWaypoints) -> std::string {
	std::string text = "{waypoints: [";
	for (std::size_t index = 0; index < iWaypoints.size(); ++index)
		text += std::format("{}[{}, {}, {}]", index == 0 ? "" : ", ", iWaypoints[index].x(), iWaypoints[index].y(),
							iWaypoints[index].z());
	return text + "]}";
}

}// namespace

void Viewport::renderBehaviourOverlays(const shared<scene::Scene>& iScene) const {
	if (m_parent == nullptr || mp_document == nullptr || mp_document->state() != SceneDocument::State::Edit)
		return;
	const scene::Entity selected = m_parent->getSelectedEntity();
	if (!selected)
		return;
	const auto id = static_cast<int>(static_cast<uint32_t>(selected));
	constexpr math::vec4 range{0.35f, 0.85f, 1.f, 1.f};
	constexpr math::vec4 route{1.f, 0.8f, 0.25f, 1.f};
	if (selected.hasComponent<scene::component::Oscillator>()) {
		const auto& oscillator = selected.getComponent<scene::component::Oscillator>();
		const math::vec3 start = iScene->getWorldTransform(selected).translation();
		const math::vec3 end = start + oscillator.offset;
		const float radius = getLightMarkerRadius(start);
		renderer::Renderer2D::drawLine({.point1 = start, .point2 = end, .color = range, .entityId = id});
		for (const auto& point: {start, end})
			drawLightCircle(point, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, radius, range, id);
	}
	if (selected.hasComponent<scene::component::Patrol>()) {
		const auto& patrol = selected.getComponent<scene::component::Patrol>();
		const math::mat4 space = patrolSpace(iScene, selected);
		const float z = selected.getComponent<scene::component::Transform>().transform.translation().z();
		math::vec3 previous;
		for (std::size_t index = 0; index < patrol.waypoints.size(); ++index) {
			const math::vec3 point =
					transformPoint(space, {patrol.waypoints[index].x(), patrol.waypoints[index].y(), z});
			const float radius = getLightMarkerRadius(point);
			if (index > 0)
				renderer::Renderer2D::drawLine({.point1 = previous, .point2 = point, .color = route, .entityId = id});
			const bool picked = static_cast<int>(index) == m_patrolHandle;
			drawLightCircle(point, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, radius * (picked ? 1.6f : 1.f),
							picked ? math::vec4{1.f, 1.f, 1.f, 1.f} : route, id);
			previous = point;
		}
		if (patrol.waypoints.empty() && patrol.wanderRadius > 0.f) {
			const math::vec3 centre = iScene->getWorldTransform(selected).translation();
			drawLightCircle(centre, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, patrol.wanderRadius, route, id);
		}
	}
}

auto Viewport::pickPatrolWaypoint() const -> int {
	if (m_parent == nullptr || mp_document == nullptr)
		return -1;
	const auto& scene = mp_document->getActiveScene();
	const scene::Entity selected = m_parent->getSelectedEntity();
	const auto ray = getCursorRay();
	if (!scene || !selected || !selected.hasComponent<scene::component::Patrol>() || !ray)
		return -1;
	const auto& [origin, direction] = *ray;
	const auto& patrol = selected.getComponent<scene::component::Patrol>();
	const math::mat4 space = patrolSpace(scene, selected);
	const float z = selected.getComponent<scene::component::Transform>().transform.translation().z();
	int nearest = -1;
	float nearestDistance = std::numeric_limits<float>::max();
	for (std::size_t index = 0; index < patrol.waypoints.size(); ++index) {
		const math::vec3 point = transformPoint(space, {patrol.waypoints[index].x(), patrol.waypoints[index].y(), z});
		const math::vec3 toPoint = point - origin;
		const float along = toPoint.x() * direction.x() + toPoint.y() * direction.y() + toPoint.z() * direction.z();
		if (along <= 0.f)
			continue;
		const float offAxis = (toPoint - direction * along).norm();
		if (offAxis <= getLightMarkerRadius(point) * 1.8f && along < nearestDistance) {
			nearestDistance = along;
			nearest = static_cast<int>(index);
		}
	}
	return nearest;
}

auto Viewport::renderWaypointGizmo() -> bool {
	if (m_parent == nullptr || mp_document == nullptr || m_patrolHandle < 0)
		return false;
	const auto& scene = mp_document->getActiveScene();
	const scene::Entity selected = m_parent->getSelectedEntity();
	if (!scene || !selected || !selected.hasComponent<scene::component::Patrol>() ||
		mp_document->state() != SceneDocument::State::Edit) {
		m_patrolHandle = -1;
		return false;
	}
	auto& waypoints = selected.getComponent<scene::component::Patrol>().waypoints;
	if (static_cast<std::size_t>(m_patrolHandle) >= waypoints.size()) {
		m_patrolHandle = -1;
		return false;
	}
	auto& waypoint = waypoints[static_cast<std::size_t>(m_patrolHandle)];
	const math::mat4 space = patrolSpace(scene, selected);
	const float z = selected.getComponent<scene::component::Transform>().transform.translation().z();
	gui::Guizmo::initialize(m_lower, m_upper - m_lower,
							m_isometric || m_editorCamera.getProjectionMode() ==
												   renderer::CameraEditor::Projection::Orthographic);
	const auto& camera = getActiveCamera();
	math::mat4 cameraProjection = camera.getProjection();
	if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan)
		cameraProjection(1, 1) *= -1.f;
	const math::vec3 world = transformPoint(space, {waypoint.x(), waypoint.y(), z});
	math::mat4 handle = math::Transform{world, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}}();
	const bool snap = input::Input::isKeyPressed(input::key::LeftControl) || m_parent->getSettings().snapEnabled;
	gui::Guizmo::manipulate(camera.getView(), cameraProjection, gui::Guizmo::Type::Translation, handle,
							snap ? resolveTranslationSnapStep(m_parent->getSettings(), std::nullopt) : 0.f,
							gui::Guizmo::Space::World);
	const bool isUsing = gui::Guizmo::isUsing();
	if (isUsing && !m_patrolWasUsing)
		m_patrolBefore = waypoints;
	if (isUsing) {
		const math::vec3 moved = transformPoint(inverse(space), math::Transform{handle}.translation());
		waypoint = {moved.x(), moved.y(), waypoint.z()};
	}
	if (!isUsing && m_patrolWasUsing && !m_patrolBefore.empty()) {
		const auto afterValues = waypointValues(waypoints);
		if (waypointValues(m_patrolBefore) != afterValues) {
			waypoints = m_patrolBefore;
			const commands::CommandTarget target{.registry = &m_parent->getRegistry(),
												 .scene = scene,
												 .undo = mp_undoManager};
			std::ignore = target.execute("component.set",
										 {{"entity", static_cast<int64_t>(static_cast<uint64_t>(selected.getUUID()))},
										  {"component", std::string{"Patrol"}},
										  {"values", afterValues}});
		}
		m_patrolBefore.clear();
	}
	m_patrolWasUsing = isUsing;
	return true;
}

auto Viewport::pickSocket() const -> std::optional<std::string> {
	if (m_parent == nullptr || mp_document == nullptr)
		return std::nullopt;
	const auto& scene = mp_document->getActiveScene();
	const scene::Entity selected = m_parent->getSelectedEntity();
	const auto ray = getCursorRay();
	if (!scene || !selected || selected.getScene() != scene.get() || !ray)
		return std::nullopt;
	const auto& [origin, direction] = *ray;
	const math::mat4 world = scene->getWorldTransform(selected)();
	std::optional<std::string> nearest;
	float nearestDistance = std::numeric_limits<float>::max();
	for (const auto& name: scene->getSocketNames(selected)) {
		const auto local = scene->getSocketTransform(selected, name);
		if (!local)
			continue;
		const math::mat4 socket = world * *local;
		const math::vec3 point{socket(0, 3), socket(1, 3), socket(2, 3)};
		const math::vec3 toPoint = point - origin;
		const float along = toPoint.x() * direction.x() + toPoint.y() * direction.y() + toPoint.z() * direction.z();
		if (along <= 0.f)
			continue;
		const float offAxis = (toPoint - direction * along).norm();
		if (offAxis <= getLightMarkerRadius(point) * 0.8f && along < nearestDistance) {
			nearestDistance = along;
			nearest = name;
		}
	}
	return nearest;
}

auto Viewport::renderSocketGizmo() -> bool {
	const auto& selection = gui::component::getSocketSelection();
	if (m_parent == nullptr || mp_document == nullptr || selection.entity == core::UUID{0})
		return false;
	const auto& scene = mp_document->getActiveScene();
	const scene::Entity selected = m_parent->getSelectedEntity();
	if (!scene || !selected || selected.getUUID() != selection.entity ||
		!selected.hasComponent<scene::component::MeshSockets>())
		return false;
	auto& sockets = selected.getComponent<scene::component::MeshSockets>().sockets;
	const auto found = std::ranges::find(sockets, selection.socket, &scene::component::MeshSockets::Socket::name);
	const auto local = scene->getSocketTransform(selected, selection.socket);
	if (found == sockets.end() || !local)
		return false;
	// The frame of the socket: the entity, then the bone the offset is relative to.
	const math::mat4 frame = scene->getWorldTransform(selected)() * *local * math::inverse(found->offset());
	gui::Guizmo::initialize(m_lower, m_upper - m_lower,
							m_isometric || m_editorCamera.getProjectionMode() ==
												   renderer::CameraEditor::Projection::Orthographic);
	const auto& camera = getActiveCamera();
	math::mat4 cameraProjection = camera.getProjection();
	if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan)
		cameraProjection(1, 1) *= -1.f;
	math::mat4 handle = frame * found->offset();
	const auto type = m_gizmoType == gui::Guizmo::Type::None ? gui::Guizmo::Type::Translation : m_gizmoType;
	const bool snap = input::Input::isKeyPressed(input::key::LeftControl) || m_parent->getSettings().snapEnabled;
	const float snapValue = gui::Guizmo::isRotate(type)
									? m_parent->getSettings().snapRotationDegrees
									: resolveTranslationSnapStep(m_parent->getSettings(), std::nullopt);
	gui::Guizmo::manipulate(camera.getView(), cameraProjection, type, handle, snap ? snapValue : 0.f,
							m_parent->getSettings().gizmoWorldSpace ? gui::Guizmo::Space::World
																	: gui::Guizmo::Space::Local);
	const bool isUsing = gui::Guizmo::isUsing();
	if (isUsing && !m_socketWasUsing)
		m_socketBefore = found->offset;
	if (isUsing)
		found->offset = math::Transform{math::inverse(frame) * handle};
	if (!isUsing && m_socketWasUsing && m_socketBefore) {
		const math::Transform after = found->offset;
		found->offset = *m_socketBefore;
		const bool changed = after.translation() != m_socketBefore->translation() ||
							 after.rotation() != m_socketBefore->rotation() || after.scale() != m_socketBefore->scale();
		if (changed) {
			const commands::CommandTarget target{.registry = &m_parent->getRegistry(),
												 .scene = scene,
												 .undo = mp_undoManager};
			if (!target.execute("entity.set_socket",
								{{"entity", static_cast<int64_t>(static_cast<uint64_t>(selected.getUUID()))},
								 {"socket", selection.socket},
								 {"translation", after.translation()},
								 {"rotation", after.rotation()},
								 {"scale", after.scale()}}))
				found->offset = after;
		}
		m_socketBefore.reset();
	}
	m_socketWasUsing = isUsing;
	return true;
}

auto Viewport::pickMarker() const -> scene::Entity {
	if (const auto camera = pickCameraModel(); camera)
		return camera;
	if (const auto light = pickLight(); light)
		return light;
	return pickParticleEmitter();
}

auto Viewport::getHoveredEntity() const -> scene::Entity {
	if (mp_document == nullptr || mp_document->getActiveScene().get() != mp_hoveredScene)
		return {};
	return m_hoveredEntity;
}

}// namespace owl::nest::panel
