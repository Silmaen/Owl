/**
 * @file Viewport.cpp
 * @author Silmaen
 * @date 10/16/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Viewport.h"

#include "EditorLayer.h"

#include "../UndoManager.h"
#include "../commands/ComponentCommands.h"

#include <owl.h>
#include <scene/SceneSerializer.h>


namespace owl::nest::panel {

Viewport::Viewport() : BasePanel{"SceneView"} {}

Viewport::~Viewport() = default;

void Viewport::attach() {
	const renderer::FramebufferSpecification specs{
			.size = {1280, 720},
			.attachments =
					{
							{.format = renderer::AttachmentSpecification::Format::Surface,
							 .tiling = renderer::AttachmentSpecification::Tiling::Optimal},
							{.format = renderer::AttachmentSpecification::Format::RedInteger,
							 .tiling = renderer::AttachmentSpecification::Tiling::Optimal},
					},
			.samples = 1,
			.swapChainTarget = false,
			.debugName = "editor"};
	m_framebuffer = renderer::Framebuffer::create(specs);
	m_editorCamera = renderer::CameraEditor(30.0f, 1.778f, 0.1f, 1000.0f);
}

void Viewport::detach() {
	OWL_TRACE("EditorLayer: deleted iconStop Texture.")
	m_framebuffer.reset();
}

void Viewport::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	// resize frame buffer if needed.
	if (const auto spec = m_framebuffer->getSpecification(); getSize().surface() > 0 && getSize() != spec.size) {
		m_framebuffer->resize(getSize());
		m_editorCamera.setViewportSize(getSize());
	}
	if (m_parent != nullptr) {
		if (m_parent->getState() == EditorLayer::State::Edit) {
			if (isHovered())
				m_editorCamera.onUpdate(iTimeStep);
		}
	}

	// Render
	m_framebuffer->bind();
	renderer::RenderCommand::setClearColor({0.1f, 0.1f, 0.1f, 1});
	renderer::RenderCommand::clear();

	// Get Mouse position.
	auto [mx, my] = ImGui::GetMousePos();
	mx -= m_lower.x();
	my -= m_lower.y();
	const math::vec2 viewportSizeInternal = m_upper - m_lower;
	if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::OpenGL)
		my = viewportSizeInternal.y() - my;
	const int mouseX = static_cast<int>(mx);
	const int mouseY = static_cast<int>(my);

	// Clear our entity ID attachment to -1
	m_framebuffer->clearAttachment(1, -1);

	if (m_parent != nullptr) {
		switch (m_parent->getState()) {
			case EditorLayer::State::Edit:
				{
					m_parent->getActiveScene()->onUpdateEditor(iTimeStep, m_editorCamera);
					break;
				}
			case EditorLayer::State::Play:
				{
					// UIRect uses Y=0 at bottom; ImGui mouse Y=0 at top → always flip.
					const math::vec2 vpMouse = {mx, viewportSizeInternal.y() - my};
					const bool mousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
					scene::UIInputSystem::update(m_parent->getActiveScene().get(),
												 m_framebuffer->getSpecification().size, vpMouse, mousePressed);
					m_parent->getActiveScene()->onUpdateRuntime(iTimeStep);
					// Handle quit request from Lua (scene.quit()) → request stop.
					if (m_parent->getActiveScene()->quitRequested)
						m_parent->requestStop();
					m_parent->handleTeleportRequest();
					m_parent->handleSaveLoadRequest();
					break;
				}
			case EditorLayer::State::Pause:
				{
					if (m_parent->consumeStepRequest()) {
						m_parent->getActiveScene()->onUpdateRuntime(iTimeStep);
						m_parent->handleTeleportRequest();
					} else {
						m_parent->getActiveScene()->onRenderRuntime();
					}
					break;
				}
		}


		if (mouseX >= 0 && mouseY >= 0 && mouseX < static_cast<int>(viewportSizeInternal.x()) &&
			mouseY < static_cast<int>(viewportSizeInternal.y())) {
			const int pixelData = m_framebuffer->readPixel(1, mouseX, mouseY);
			m_hoveredEntity = pixelData == -1 ? scene::Entity()
											  : scene::Entity(static_cast<entt::entity>(pixelData),
															  m_parent->getActiveScene().get());
		}
		renderOverlay();
	}

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
				const std::filesystem::path relPath{path};
				if (relPath.extension() == ".owlprefab") {
					if (const auto fullPath = renderer::Renderer::getTextureLibrary().find(path);
						fullPath.has_value())
						m_parent->instantiatePrefab(fullPath.value(), path);
				} else if (const auto scenePath = renderer::Renderer::getTextureLibrary().find(path);
						   scenePath.has_value() && scenePath.value().extension() == ".owl") {
					m_parent->openScene(scenePath.value());
				} else {
					OWL_CORE_WARN("Could not load {}: unsupported file type", path)
				}
			}
			ImGui::EndDragDropTarget();
		}
	}
	renderGizmo();
}

void Viewport::renderOverlay() const {
	OWL_PROFILE_FUNCTION()

	// do nothing without parent.
	if (m_parent == nullptr)
		return;

	if (m_parent->getState() != EditorLayer::State::Edit) {
		const scene::Entity camera = m_parent->getActiveScene()->getPrimaryCamera();
		auto& cam = camera.getComponent<scene::component::Camera>().camera;
		cam.setTransform(m_parent->getActiveScene()->getWorldTransform(camera)());
		renderer::Renderer2D::beginScene(cam);
	} else {
		renderer::Renderer2D::beginScene(m_editorCamera);
	}

	if (m_parent->getState() == EditorLayer::State::Edit) {
		// Draw selected entity outline
		if (const scene::Entity selectedEntity = m_parent->getSelectedEntity()) {
			const math::Transform worldTransform = m_parent->getActiveScene()->getWorldTransform(selectedEntity);
			// Orange
			// surrounding square
			renderer::Renderer2D::drawRect({.transform = worldTransform, .color = math::vec4(0.95f, 0.55f, 0.f, 1)});
			// Overlay
			renderer::Renderer2D::drawQuad(
					{.transform = worldTransform, .color = math::vec4{0.95f, 0.55f, 0.f, 0.2f}});
		}

		// Draw trigger type icons
		auto& textureLibrary = renderer::Renderer::getTextureLibrary();
		auto& activeScene = *m_parent->getActiveScene();
		for (const auto view =
					 activeScene.registry.view<scene::component::Trigger, scene::component::Transform>();
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
				const scene::Entity ent{entity, &activeScene};
				math::Transform iconTransform = activeScene.getWorldTransform(ent);
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
	if (m_parent == nullptr)
		return;
	if (m_parent->getState() != EditorLayer::State::Edit)
		return;

	// Gizmos
	if (const scene::Entity selectedEntity = m_parent->getSelectedEntity();
		selectedEntity && m_gizmoType != gui::Guizmo::Type::None) {
		gui::Guizmo::initialize(m_lower, m_upper - m_lower);

		// Editor camera
		math::mat4 cameraProjection = m_editorCamera.getProjection();
		if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::Vulkan)
			cameraProjection(1, 1) *= -1.f;
		const math::mat4 cameraView = m_editorCamera.getView();

		// Entity transform: use world transform for gizmo display.
		auto& tc = selectedEntity.getComponent<scene::component::Transform>();
		math::mat4 transform = m_parent->getActiveScene()->getWorldTransform(selectedEntity)();

		// Snapping
		const bool snap = input::Input::isKeyPressed(input::key::LeftControl);
		float snapValue = 0.5f;// Snap to 0.5m for translation/scale
		// Snap to 45 degrees for rotation
		if (gui::Guizmo::isRotate(m_gizmoType))
			snapValue = 45.0f;

		gui::Guizmo::manipulate(cameraView, cameraProjection, m_gizmoType, transform, snap ? snapValue : 0.f);

		const bool isUsing = gui::Guizmo::isUsing();

		// Capture "before" snapshot when gizmo manipulation starts.
		if (isUsing && !m_gizmoWasUsing && mp_undoManager != nullptr)
			m_gizmoBeforeYaml = scene::SceneSerializer::serializeEntityToString(selectedEntity);

		if (isUsing) {
			// Convert gizmo result (world space) back to local space.
			const auto& hierarchy = selectedEntity.getComponent<scene::component::Hierarchy>();
			math::mat4 localMat = transform;
			if (hierarchy.parentId != core::UUID{0}) {
				if (const auto parent = m_parent->getActiveScene()->findEntityByUUID(hierarchy.parentId); parent) {
					const math::mat4 parentWorldInv =
							math::inverse(m_parent->getActiveScene()->getWorldTransform(parent)());
					localMat = parentWorldInv * transform;
				}
			}
			const math::Transform newLocal{localMat};
			const math::vec3 deltaRotation = newLocal.rotation() - tc.transform.rotation();
			tc.transform.translation() = newLocal.translation();
			tc.transform.rotation() += deltaRotation;
			tc.transform.scale() = newLocal.scale();
		}

		// Push undo command when gizmo manipulation ends.
		if (!isUsing && m_gizmoWasUsing && mp_undoManager != nullptr && !m_gizmoBeforeYaml.empty()) {
			const auto afterYaml = scene::SceneSerializer::serializeEntityToString(selectedEntity);
			if (m_gizmoBeforeYaml != afterYaml) {
				auto cmd = mkUniq<commands::ModifyEntityCommand>(
						selectedEntity.getUUID(),
						EntitySnapshot{selectedEntity.getUUID(), m_gizmoBeforeYaml},
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
	if (m_parent->getState() == EditorLayer::State::Edit && isHovered())
		m_editorCamera.onEvent(ioEvent);

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[this]<typename T0>(T0&& ioPh1) { return onMouseButtonPressed(std::forward<T0>(ioPh1)); });
}

auto Viewport::onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent) -> bool {
	if (m_parent->getState() == EditorLayer::State::Edit) {
		if (ioEvent.getMouseButton() == input::mouse::ButtonLeft) {
			if (isHovered() && !gui::Guizmo::isOver() && !input::Input::isKeyPressed(input::key::LeftAlt))
				m_parent->setSelectedEntity(getHoveredEntity());
		}
	}
	return false;
}

}// namespace owl::nest::panel
