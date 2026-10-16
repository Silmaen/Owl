/**
 * @file Viewport.cpp
 * @author Silmaen
 * @date 10/16/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Viewport.h"

#include "../DockLayout.h"

#include "EditorLayer.h"
#include "document/DocumentManager.h"
#include "document/SceneDocument.h"

#include "../UndoManager.h"
#include "../commands/ComponentCommands.h"
#include "../commands/VoxelCommands.h"

#include <data/voxel/VoxelRaycast.h>
#include <scene/component/VoxelWorld.h>

#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/Tooltip.h>
#include <owlgui.h>
#include <renderer/RenderLayer.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <renderer/RendererIsometricLayer.h>
#include <scene/SceneSerializer.h>
#include <scene/TilemapAsset.h>
#include <scene/TilemapTerrain.h>
#include <scene/Tileset.h>
#include <scene/component/RaycastDoor.h>
#include <scene/component/RaycastPushWall.h>
#include <scene/component/RendererTag.h>
#include <scene/component/Tilemap.h>
#include <scene/component/Visibility.h>
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
#include <filesystem>
#include <format>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest::panel {

namespace {
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
							{.format = renderer::gpu::AttachmentSpecification::Format::Depth24Stencil8,
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
	if (mp_document->state() == SceneDocument::State::Edit && m_isometric) {
		if (isHovered())
			handleIsometricInput();
		updateIsometricCursor(activeScene);
		pickNavigationCells();
	} else if (mp_document->state() == SceneDocument::State::Edit) {
		if (m_parent != nullptr) {
			const auto& settings = m_parent->getSettings();
			m_editorCamera.setNavigation({.rotate = static_cast<float>(settings.cameraRotatePercent) / 100.f,
										  .pan = static_cast<float>(settings.cameraPanPercent) / 100.f,
										  .zoom = static_cast<float>(settings.cameraZoomPercent) / 100.f,
										  .invertY = settings.cameraInvertY,
										  .deadZone = static_cast<float>(settings.cameraDeadZone)});
		}
		if (isHovered())
			m_editorCamera.onUpdate(iTimeStep);
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
				activeScene->onUpdateEditor(iTimeStep, getActiveCamera());
				break;
			}
		case SceneDocument::State::Play:
			{
				// Click to capture the cursor (hidden/locked), Esc to free; no focus/hover release (the captured cursor's unbounded virtual position would read as "left the viewport" and cancel capture every frame). NoMouseCursorChange stops ImGui from re-showing the cursor while captured.
				auto& window = app::Application::get().getWindow();
				if (window.getCursorMode() == window::CursorMode::Normal) {
					if (m_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
						activeScene->wantsCursorCapture()) {
						window.setCursorMode(window::CursorMode::Disabled);
						ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
					}
				} else if (input::Input::isKeyPressed(input::key::Escape)) {
					window.setCursorMode(window::CursorMode::Normal);
					ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
				}
				// The HUD is Y-down from the top left: undo the OpenGL flip of the picking coordinates.
				const bool flipped = renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::OpenGL;
				const math::vec2 vpMouse = {mx, flipped ? viewportSizeInternal.y() - my : my};
				const bool mousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
				const math::vec2 wheel = m_hovered ? math::vec2{ImGui::GetIO().MouseWheelH, ImGui::GetIO().MouseWheel}
												   : math::vec2{0.f, 0.f};
				scene::UiInputSystem::update(activeScene.get(), m_framebuffer->getSpecification().size, vpMouse,
											 mousePressed, iTimeStep.getSeconds(), wheel);
				activeScene->onUpdateRuntime(iTimeStep);
				// Handle quit request from Lua (scene.quit()) → request stop.
				if (activeScene->quitRequested)
					mp_document->requestStop();
				if (auto pending = app::Application::get().getEngineContext().getScreenTransition().pendingLoadPath();
					pending) {
					activeScene->teleportRequest.pending = true;
					activeScene->teleportRequest.levelName = *pending;
					activeScene->teleportRequest.targetName.clear();
					activeScene->teleportRequest.initialVelocity = {0.f, 0.f};
					activeScene->teleportRequest.rotationDelta = 0.f;
				}
				mp_document->handleTeleportRequest(getSize());
				mp_document->handleSaveLoadRequest(getSize());
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
	}
	renderOverlay();

	m_framebuffer->unbind();
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
				m_parent->handleContentBrowserDrop(std::filesystem::path{path});
			}
			ImGui::EndDragDropTarget();
		}
	}
	renderGizmo();
	renderOverlayToolbar();
	renderCameraGizmo();
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
		m_parent->setSelectedEntity(m_hoveredEntity);
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
	if (gui::widgets::menuItem(
				{.glyph = gui::icon::video, .label = "Selected Camera Viewpoint", .enabled = canGoToCameraViewpoint()}))
		goToCameraViewpoint();
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
		const scene::Entity camera = activeScene->getPrimaryCamera();
		auto& cam = camera.getComponent<scene::component::Camera>().camera;
		cam.setTransform(activeScene->getWorldTransform(camera)());
		renderer::Renderer2D::beginScene(cam);
	} else {
		renderer::Renderer2D::beginScene(getActiveCamera());
	}

	if (mp_document->state() == SceneDocument::State::Edit) {
		// Draw selected entity outline
		if (const scene::Entity selectedEntity =
					m_parent != nullptr ? m_parent->getSelectedEntity() : scene::Entity{}) {
			const math::Transform worldTransform = activeScene->getWorldTransform(selectedEntity);
			renderer::Renderer2D::drawRect({.transform = worldTransform, .color = math::vec4(0.95f, 0.55f, 0.f, 1)});
			renderer::Renderer2D::drawQuad({.transform = worldTransform, .color = math::vec4{0.95f, 0.55f, 0.f, 0.2f}});
		}

		if (m_parent != nullptr && m_parent->getSettings().showGrid)
			renderGrid(resolveTranslationSnapStep(m_parent->getSettings(), findFirstTilemapInfo(activeScene.get())));
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

		if (m_parent != nullptr && m_parent->getSettings().showCameraGizmos) {
			const auto cameraIcon = textureLibrary.get("icons/triggers/camera");
			for (const auto camView =
						 activeScene->registry.view<scene::component::Camera, scene::component::Transform>();
				 const auto entity: camView) {
				const auto& camComp = camView.get<scene::component::Camera>(entity);
				const scene::Entity ent{entity, activeScene.get()};
				const math::Transform worldT = activeScene->getWorldTransform(ent);
				const math::mat4 worldMat = worldT();
				// Local +Y is the camera forward (matches `RendererRaycast::beginScene`).
				const math::vec4 worldFwd4 = worldMat * math::vec4{0.f, 1.f, 0.f, 0.f};
				math::vec2 fwd2{worldFwd4.x(), worldFwd4.y()};
				if (fwd2.normSq() < 1e-8f)
					fwd2 = math::vec2{0.f, 1.f};
				fwd2.normalize();
				const math::vec3 origin{worldT.translation().x(), worldT.translation().y(),
										worldT.translation().z() + 0.02f};

				const bool isPrimary = camComp.primary;
				const math::vec4 mainColor =
						isPrimary ? math::vec4{1.f, 0.78f, 0.16f, 0.95f} : math::vec4{0.55f, 0.85f, 0.95f, 0.75f};

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
				renderer::Renderer2D::drawLine({.point1 = origin,
												.point2 = {origin.x() + rightDir.x() * coneLen,
														   origin.y() + rightDir.y() * coneLen, origin.z()},
												.color = coneColor,
												.entityId = static_cast<int>(entity)});
			}
		}

		const scene::Entity selectedForGizmo = m_parent != nullptr ? m_parent->getSelectedEntity() : scene::Entity{};
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
}

namespace {

// Translucent pill holding one cluster of viewport controls, anchored at a corner of the viewport.
auto beginOverlay(const std::string& iId, const ImVec2& iPos, const ImVec2& iPivot) -> bool {
	const auto& theme = gui::UiLayer::getTheme();
	ImGui::SetNextWindowPos(iPos, ImGuiCond_Always, iPivot);
	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
									   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
									   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing |
									   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
									   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{3.f, 3.f});
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{2.f, 2.f});
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, gui::vec(gui::Theme::withAlpha(theme.surface1, 0.9f)));
	ImGui::PushStyleColor(ImGuiCol_Border, gui::vec(gui::Theme::withAlpha(theme.line, 0.8f)));
	return ImGui::Begin(iId.c_str(), nullptr, flags);
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

void Viewport::renderGrid(const float iStep) const {
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
	const float minX = std::floor((focal.x() - extent) / step) * step;
	const float minY = std::floor((focal.y() - extent) / step) * step;
	const float maxX = focal.x() + extent;
	const float maxY = focal.y() + extent;
	const auto& theme = gui::UiLayer::getTheme();
	const math::vec4 lineColor = gui::Theme::withAlpha(theme.text, 0.07f);
	const auto columns = static_cast<int>((maxX - minX) / step) + 1;
	for (int i = 0; i < columns; ++i) {
		const float x = minX + step * static_cast<float>(i);
		const bool axis = std::abs(x) < step * 0.5f;
		renderer::Renderer2D::drawLine({.point1 = {x, minY, 0.f},
										.point2 = {x, maxY, 0.f},
										.color = axis ? gui::Theme::withAlpha(theme.axisY, 0.45f) : lineColor});
	}
	const auto rows = static_cast<int>((maxY - minY) / step) + 1;
	for (int i = 0; i < rows; ++i) {
		const float y = minY + step * static_cast<float>(i);
		const bool axis = std::abs(y) < step * 0.5f;
		renderer::Renderer2D::drawLine({.point1 = {minX, y, 0.f},
										.point2 = {maxX, y, 0.f},
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

	// Playback, centred.
	if (beginOverlay(std::format("##vpPlay{:x}", id), {(m_lower.x() + m_upper.x()) * 0.5f, m_lower.y() + margin},
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
	}
	endOverlay();

	// View options, top-right.
	if (editing)
		renderViewOverlay();
	else if (beginOverlay(
					 std::format("##vpPlaying{:x}", id),
					 {(m_lower.x() + m_upper.x()) * 0.5f, m_lower.y() + margin * 2.f + ImGui::GetFrameHeight() + 6.f},
					 {0.5f, 0.f})) {
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
	if (!editing)
		endOverlay();

	// Frame time chip, bottom-left.
	if (beginOverlay(std::format("##vpFps{:x}", id), {m_lower.x() + margin, m_upper.y() - margin}, {0.f, 1.f})) {
		const auto stats = renderer::Renderer2D::getStats();
		const float fps = ImGui::GetIO().Framerate;
		gui::UiLayer::pushTextStyle(gui::TextStyle::Value);
		const auto text =
				std::format("{:.0f} fps  {:.2f} ms  {} draws", fps, fps > 0.f ? 1000.f / fps : 0.f, stats.drawCalls);
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(gui::vec(theme.textMuted), " %s ", text.c_str());
		gui::UiLayer::popTextStyle();
		if (ImGui::IsItemClicked()) {
			settings.showStats = true;
			ImGui::SetWindowFocus(panelWindow::stats);
		}
		gui::widgets::itemTooltip("Frame stats", nullptr, "Click to open the Stats panel.");
	}
	endOverlay();
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
	const scene::Entity selected = m_parent->getSelectedEntity();
	const auto& sceneCamera = selected.getComponent<scene::component::Camera>().camera;
	const math::Transform world = mp_document->getActiveScene()->getWorldTransform(selected);
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
		if (iconButton("show", {.glyph = gui::icon::eye,
								.tooltip = "Show",
								.description = "Grid, camera markers and trigger icons drawn over the scene."}))
			ImGui::OpenPopup("##show");
		if (ImGui::BeginPopup("##show")) {
			const auto option = [](const char* iLabel, bool& ioFlag) -> void {
				if (gui::widgets::menuItem({.glyph = ioFlag ? gui::icon::check : nullptr, .label = iLabel}))
					ioFlag = !ioFlag;
			};
			gui::widgets::menuHeader("Show");
			option("Grid", settings.showGrid);
			option("Camera markers", settings.showCameraGizmos);
			option("Trigger icons", settings.showTriggerIcons);
			if (m_isometric)
				option("Navigation graph", settings.showNavGraph);
			ImGui::EndPopup();
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

void Viewport::renderNavigationOverlay(const shared<scene::Scene>& iScene) const {
	if (m_parent == nullptr || !m_parent->getSettings().showNavGraph || !iScene)
		return;
	constexpr math::vec4 twoWay{0.30f, 0.90f, 0.40f, 0.9f};
	constexpr math::vec4 oneWay{1.f, 0.60f, 0.10f, 1.f};
	constexpr math::vec4 pathColor{1.f, 0.95f, 0.20f, 1.f};
	constexpr size_t maxEdges = 40000;
	size_t drawn = 0;
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
			const auto corners = asset.getCellCorners(iX, iY);
			const float height = static_cast<float>(corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f *
								 scene::terrain::heightStep;
			return origin + math::vec3{static_cast<float>(iX) + 0.5f, static_cast<float>(iY) + 0.5f, height + 0.03f};
		};
		const auto& size = map.getSize();
		for (uint32_t y = 0; y < size.y() && drawn < maxEdges; ++y) {
			for (uint32_t x = 0; x < size.x() && drawn < maxEdges; ++x) {
				if (!map.isWalkable(x, y))
					continue;
				for (const auto direction: {scene::nav::Direction::East, scene::nav::Direction::South}) {
					const uint32_t nx = x + static_cast<uint32_t>(scene::nav::offsetX(direction));
					const uint32_t ny = y + static_cast<uint32_t>(scene::nav::offsetY(direction));
					const bool forward = map.getEdgeCost(x, y, direction) != 0;
					const bool backward = map.getEdgeCost(nx, ny, scene::nav::opposite(direction)) != 0;
					if (!forward && !backward)
						continue;
					const auto from = center(x, y);
					const auto to = center(nx, ny);
					renderer::Renderer2D::drawLine(
							{.point1 = from, .point2 = to, .color = forward && backward ? twoWay : oneWay});
					if (forward != backward) {
						const auto tip = forward ? to : from;
						const auto tail = forward ? from : to;
						const auto along = (tail - tip) * 0.3f;
						const math::vec3 side{-along.y(), along.x(), 0.f};
						renderer::Renderer2D::drawLine(
								{.point1 = tip, .point2 = tip + along + side * 0.5f, .color = oneWay});
						renderer::Renderer2D::drawLine(
								{.point1 = tip, .point2 = tip + along - side * 0.5f, .color = oneWay});
					}
					++drawn;
				}
			}
		}
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

	if (const scene::Entity selectedEntity = m_parent->getSelectedEntity();
		selectedEntity && m_gizmoType != gui::Guizmo::Type::None) {
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
			snapValue = 45.0f;
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
			commitGizmoEdit(selectedEntity, activeScene);
			m_gizmoBeforeYaml.clear();
		}
		m_gizmoWasUsing = isUsing;
	}
}

void Viewport::commitGizmoEdit(const scene::Entity& iEntity, const shared<scene::Scene>& iScene) const {
	const auto afterYaml = scene::SceneSerializer::serializeEntityToString(iEntity);
	if (m_gizmoBeforeYaml == afterYaml)
		return;
	const auto transform = iEntity.getComponent<scene::component::Transform>().transform;
	// The command records the move from the state before the drag.
	std::ignore = scene::SceneSerializer::applyEntityFromString(iEntity, m_gizmoBeforeYaml);
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

	// Ray from the editor camera through the cursor. The Vulkan editor projection negates Y (screen-top → NDC y -1); OpenGL keeps the GL convention (screen-top → +1).
	const math::vec2 vpSize = m_upper - m_lower;
	if (vpSize.x() <= 0.f || vpSize.y() <= 0.f) {
		iScene->setEditorVoxelHighlight(false);
		return;
	}
	const auto mousePos = ImGui::GetMousePos();
	const float ndcX = 2.f * (mousePos.x - m_lower.x()) / vpSize.x() - 1.f;
	const float screenY = 2.f * (mousePos.y - m_lower.y()) / vpSize.y() - 1.f;
	const bool isOpenGl = renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::OpenGL;
	const float ndcY = isOpenGl ? -screenY : screenY;
	const math::mat4 vpInv = inverse(m_editorCamera.getProjection() * m_editorCamera.getView());
	const math::vec4 farPoint = vpInv * math::vec4{ndcX, ndcY, 0.f, 1.f};
	if (std::abs(farPoint.w()) < 1e-6f) {
		iScene->setEditorVoxelHighlight(false);
		return;
	}
	const math::vec3 origin = m_editorCamera.getPosition();
	const math::vec3 target{farPoint.x() / farPoint.w(), farPoint.y() / farPoint.w(), farPoint.z() / farPoint.w()};
	const math::vec3 direction = (target - origin).normalized();

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
	if (ioEvent.getMouseButton() == input::mouse::ButtonLeft) {
		if (isHovered() && !gui::Guizmo::isOver() && !input::Input::isKeyPressed(input::key::LeftAlt))
			if (m_parent != nullptr)
				m_parent->setSelectedEntity(getHoveredEntity());
	}
	return false;
}

}// namespace owl::nest::panel
