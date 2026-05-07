/**
 * @file Viewport.cpp
 * @author Silmaen
 * @date 10/16/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Viewport.h"

#include "EditorLayer.h"
#include "TilePalette.h"
#include "document/DocumentManager.h"
#include "document/SceneDocument.h"

#include "../UndoManager.h"
#include "../commands/ComponentCommands.h"

#include <owl.h>
#include <scene/SceneSerializer.h>
#include <scene/component/Tilemap.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP


namespace owl::nest::panel {

namespace {

/// @brief Build the ImGui window title: display text + stable `##{uuid}` id.
auto makeWindowTitle(const SceneDocument& iDoc) -> std::string {
	const auto base = iDoc.title();
	return std::format("{}##scene_{:x}", base.empty() ? std::string{"Untitled"} : base,
					   static_cast<uint64_t>(iDoc.id()));
}

}// namespace

Viewport::Viewport() : BasePanel{"SceneView"} {}

Viewport::~Viewport() = default;

void Viewport::setDocument(SceneDocument* iDocument) {
	mp_document = iDocument;
	if (iDocument != nullptr) {
		// Stable ImGui id (after ##) so ImGui preserves dock state across renames.
		m_name = makeWindowTitle(*iDocument);
	} else {
		m_name = "SceneView";
	}
}

void Viewport::onRender() {
	if (mp_document == nullptr) {
		BasePanel::onRender();
		return;
	}

	// Refresh display title each frame so filename / play badge changes show up on the tab;
	// the stable `##<uuid>` suffix keeps the ImGui window id unchanged.
	m_name = makeWindowTitle(*mp_document);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
							 ImGuiWindowFlags_NoCollapse;
	if (mp_document->isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;

	// First-time docking hint: dock newly-opened viewports into the central node of the main
	// dockspace instead of letting them float.
	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);

	// The tab shows the native ImGui close X.  When the user clicks it `m_pOpen` is set
	// to false; `EditorLayer` polls this after rendering and routes through the dirty
	// confirmation modal before actually closing the document.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	const bool visible = ImGui::Begin(m_name.c_str(), &m_pOpen, flags);
	m_focused = ImGui::IsWindowFocused();
	m_hovered = ImGui::IsWindowHovered();
	core::Application::get().getImGuiLayer()->blockEvents(!m_focused && !m_hovered);

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

	// resize frame buffer if needed.
	if (const auto spec = m_framebuffer->getSpecification(); getSize().surface() > 0 && getSize() != spec.size) {
		m_framebuffer->resize(getSize());
		m_editorCamera.setViewportSize(getSize());
	}
	activeScene->onViewportResize(getSize());

	if (mp_document->state() == SceneDocument::State::Edit) {
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

	switch (mp_document->state()) {
		case SceneDocument::State::Edit: {
			activeScene->onUpdateEditor(iTimeStep, m_editorCamera);
			break;
		}
		case SceneDocument::State::Play: {
			// UIRect uses Y=0 at bottom; ImGui mouse Y=0 at top → always flip.
			const math::vec2 vpMouse = {mx, viewportSizeInternal.y() - my};
			const bool mousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
			scene::UIInputSystem::update(activeScene.get(), m_framebuffer->getSpecification().size, vpMouse,
										 mousePressed);
			activeScene->onUpdateRuntime(iTimeStep);
			// Handle quit request from Lua (scene.quit()) → request stop.
			if (activeScene->quitRequested)
				mp_document->requestStop();
			// `scene.transition_to(...)` from Lua schedules a load through the
			// engine-side `ScreenTransition` orchestrator; once the out-anim
			// completes, the path lands here as a teleport request, the
			// existing handler does the swap, and the Loading-phase screen
			// stays up until `minHoldDuration` elapses.
			if (auto pending = scene::ScreenTransition::pendingLoadPath(); pending) {
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
		case SceneDocument::State::Pause: {
			if (mp_document->consumeStepRequest()) {
				activeScene->onUpdateRuntime(iTimeStep);
				mp_document->handleTeleportRequest(getSize());
			} else {
				activeScene->onRenderRuntime();
			}
			break;
		}
	}

	if (mouseX >= 0 && mouseY >= 0 && mouseX < static_cast<int>(viewportSizeInternal.x()) &&
		mouseY < static_cast<int>(viewportSizeInternal.y())) {
		const int pixelData = m_framebuffer->readPixel(1, mouseX, mouseY);
		m_hoveredEntity =
				pixelData == -1 ? scene::Entity()
								: scene::Entity(static_cast<entt::entity>(pixelData), activeScene.get());
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
		renderer::Renderer2D::beginScene(m_editorCamera);
	}

	if (mp_document->state() == SceneDocument::State::Edit) {
		// Draw selected entity outline
		if (const scene::Entity selectedEntity = m_parent != nullptr ? m_parent->getSelectedEntity() : scene::Entity{}) {
			const math::Transform worldTransform = activeScene->getWorldTransform(selectedEntity);
			renderer::Renderer2D::drawRect({.transform = worldTransform, .color = math::vec4(0.95f, 0.55f, 0.f, 1)});
			renderer::Renderer2D::drawQuad(
					{.transform = worldTransform, .color = math::vec4{0.95f, 0.55f, 0.f, 0.2f}});
		}

		// Draw trigger type icons
		auto& textureLibrary = renderer::Renderer::getTextureLibrary();
		for (const auto view =
					 activeScene->registry.view<scene::component::Trigger, scene::component::Transform>();
			 const auto entity: view) {
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
	}

	renderer::Renderer2D::endScene();
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
		gui::Guizmo::initialize(m_lower, m_upper - m_lower);

		math::mat4 cameraProjection = m_editorCamera.getProjection();
		if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan)
			cameraProjection(1, 1) *= -1.f;
		const math::mat4 cameraView = m_editorCamera.getView();

		auto& tc = selectedEntity.getComponent<scene::component::Transform>();
		math::mat4 transform = activeScene->getWorldTransform(selectedEntity)();

		const bool snap = input::Input::isKeyPressed(input::key::LeftControl);
		float snapValue = 0.5f;
		if (gui::Guizmo::isRotate(m_gizmoType))
			snapValue = 45.0f;

		gui::Guizmo::manipulate(cameraView, cameraProjection, m_gizmoType, transform, snap ? snapValue : 0.f);

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

		if (!isUsing && m_gizmoWasUsing && mp_undoManager != nullptr && !m_gizmoBeforeYaml.empty()) {
			const auto afterYaml = scene::SceneSerializer::serializeEntityToString(selectedEntity);
			if (m_gizmoBeforeYaml != afterYaml) {
				auto cmd = mkUniq<commands::ModifyEntityCommand>(
						selectedEntity.getUUID(), EntitySnapshot{selectedEntity.getUUID(), m_gizmoBeforeYaml},
						"Transform (Gizmo)");
				cmd->captureAfter(selectedEntity);
				mp_undoManager->push(std::move(cmd));
			}
			m_gizmoBeforeYaml.clear();
		}
		m_gizmoWasUsing = isUsing;
	}
}

void Viewport::onEvent(event::Event& ioEvent) {
	if (mp_document != nullptr && mp_document->state() == SceneDocument::State::Edit && isHovered())
		m_editorCamera.onEvent(ioEvent);

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[this]<typename T0>(T0&& ioPh1) -> auto { return onMouseButtonPressed(std::forward<T0>(ioPh1)); });
}

auto Viewport::onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent) -> bool {
	if (mp_document == nullptr || mp_document->state() != SceneDocument::State::Edit)
		return false;
	if (ioEvent.getMouseButton() == input::mouse::ButtonLeft) {
		if (isHovered() && !gui::Guizmo::isOver() && !input::Input::isKeyPressed(input::key::LeftAlt))
			if (m_parent != nullptr)
				m_parent->setSelectedEntity(getHoveredEntity());
	}
	return false;
}

void Viewport::processTilemapPaint(const TilePalette& iPalette, scene::Entity& ioSelected) {
	if (mp_document == nullptr || mp_document->state() != SceneDocument::State::Edit)
		return;
	if (!iPalette.isPaintActive() || !ioSelected || !ioSelected.hasComponent<scene::component::Tilemap>())
		return;
	if (!isHovered() || gui::Guizmo::isOver())
		return;
	const bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	const bool rightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
	if (!leftDown && !rightDown)
		return;

	auto& tilemap = ioSelected.getComponent<scene::component::Tilemap>();
	const auto& activeScene = mp_document->getActiveScene();
	if (!activeScene)
		return;

	// Mouse → NDC (Y-up). The viewport is a sub-rectangle of the host window.
	const auto [mx, my] = ImGui::GetMousePos();
	const math::vec2 vpSize = m_upper - m_lower;
	if (vpSize.x() <= 0.f || vpSize.y() <= 0.f)
		return;
	const float ndcX = ((mx - m_lower.x()) / vpSize.x()) * 2.f - 1.f;
	const float ndcY = -(((my - m_lower.y()) / vpSize.y()) * 2.f - 1.f);

	// Unproject: world = inv(view * projection) * vec4(ndc, 0, 1).
	const math::mat4 invVp = math::inverse(m_editorCamera.getViewProjection());
	const math::vec4 worldH = invVp * math::vec4{ndcX, ndcY, 0.f, 1.f};
	const float worldX = worldH.x() / worldH.w();
	const float worldY = worldH.y() / worldH.w();

	// Convert to tilemap-local cell coords. We rely on the entity's translation +
	// the centred-grid origin used by Scene::render so the math matches the visual.
	const math::Transform worldXf = activeScene->getWorldTransform(ioSelected);
	const float cellSize = std::max(0.0001f, tilemap.cellSize);
	const float originX = -static_cast<float>(tilemap.width - 1) * 0.5f * cellSize;
	const float originY = static_cast<float>(tilemap.height - 1) * 0.5f * cellSize;
	const float relX = worldX - worldXf.translation().x() - originX;
	const float relY = worldXf.translation().y() + originY - worldY;
	const int cellX = static_cast<int>(std::floor((relX / cellSize) + 0.5f));
	const int cellY = static_cast<int>(std::floor((relY / cellSize) + 0.5f));
	if (cellX < 0 || cellY < 0 || cellX >= static_cast<int>(tilemap.width) ||
		cellY >= static_cast<int>(tilemap.height))
		return;

	const uint32_t layerIdx = iPalette.getSelectedLayer();
	if (layerIdx >= tilemap.layers.size())
		return;
	const int32_t brush =
			rightDown ? scene::component::g_EmptyTileIndex : iPalette.getSelectedTile();
	const int32_t current =
			tilemap.getTile(layerIdx, static_cast<uint32_t>(cellX), static_cast<uint32_t>(cellY));
	if (current == brush)
		return;

	// Capture pre-edit snapshot for the undo stack (one command per cell change is OK
	// thanks to UndoManager's merge coalescing on rapid identical-target edits).
	std::string before;
	if (mp_undoManager != nullptr)
		before = scene::SceneSerializer::serializeEntityToString(ioSelected);
	tilemap.setTile(layerIdx, static_cast<uint32_t>(cellX), static_cast<uint32_t>(cellY), brush);
	if (mp_undoManager != nullptr) {
		auto cmd = mkUniq<commands::ModifyEntityCommand>(
				ioSelected.getUUID(),
				EntitySnapshot{ioSelected.getUUID(), before},
				"Paint tile");
		cmd->captureAfter(ioSelected);
		mp_undoManager->push(std::move(cmd));
	}
}

}// namespace owl::nest::panel
