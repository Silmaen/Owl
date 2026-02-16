/**
 * @file Viewport.cpp
 * @author Silmaen
 * @date 10/16/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Viewport.h"

#include "EditorLayer.h"

#include <owl.h>


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
					m_parent->getActiveScene()->onUpdateRuntime(iTimeStep);
					break;
				}
			case EditorLayer::State::Pause:
				{
					m_parent->getActiveScene()->onRenderRuntime();
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
				// this is not a texture file, but as soon as you give a file with extension,
				// any 'AssetLibrary' will search for its path.
				if (const auto scenePath = renderer::Renderer::getTextureLibrary().find(path);
					scenePath.has_value() && scenePath.value().extension() == ".owl")
					m_parent->openScene(scenePath.value());
				else
					OWL_CORE_WARN("Could not load {}: not a scene file", path)
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
		cam.setTransform(camera.getComponent<scene::component::Transform>().transform());
		renderer::Renderer2D::beginScene(cam);
	} else {
		renderer::Renderer2D::beginScene(m_editorCamera);
	}

	if (m_parent->getState() == EditorLayer::State::Edit) {
		// Draw selected entity outline
		if (const scene::Entity selectedEntity = m_parent->getSelectedEntity()) {
			const auto [transform] = selectedEntity.getComponent<scene::component::Transform>();
			// Orange
			// surrounding square
			renderer::Renderer2D::drawRect({.transform = transform, .color = math::vec4(0.95f, 0.55f, 0.f, 1)});
			// Overlay
			renderer::Renderer2D::drawQuad({.transform = transform, .color = math::vec4{0.95f, 0.55f, 0.f, 0.2f}});
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

		// Runtime camera from entity
		// auto cameraEntity = activeScene->getPrimaryCamera();
		// const auto &camera = cameraEntity.getComponent<scene::component::Camera>().camera;
		// const math::mat4 &cameraProjection = camera.getProjection();
		// math::mat4 cameraView = math::inverse(cameraEntity.getComponent<scene::component::Transform>().getTransform());

		// Editor camera
		math::mat4 cameraProjection = m_editorCamera.getProjection();
		if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::Vulkan)
			cameraProjection(1, 1) *= -1.f;
		const math::mat4 cameraView = m_editorCamera.getView();

		// Entity transform
		auto& tc = selectedEntity.getComponent<scene::component::Transform>();
		math::mat4 transform = tc.transform();

		// Snapping
		const bool snap = input::Input::isKeyPressed(input::key::LeftControl);
		float snapValue = 0.5f;// Snap to 0.5m for translation/scale
		// Snap to 45 degrees for rotation
		if (gui::Guizmo::isRotate(m_gizmoType))
			snapValue = 45.0f;

		gui::Guizmo::manipulate(cameraView, cameraProjection, m_gizmoType, transform, snap ? snapValue : 0.f);

		if (gui::Guizmo::isUsing()) {
			math::Transform newTransform(transform);

			const math::vec3 deltaRotation = newTransform.rotation() - tc.transform.rotation();
			tc.transform.translation() = newTransform.translation();
			tc.transform.rotation() += deltaRotation;
			tc.transform.scale() = newTransform.scale();
		}
	}
}

void Viewport::onEvent(event::Event& ioEvent) {
	if (m_parent->getState() == EditorLayer::State::Edit && isHovered())
		m_editorCamera.onEvent(ioEvent);

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::KeyPressedEvent>(
			[this]<typename T0>(T0&& ioPh1) { return onKeyPressed(std::forward<T0>(ioPh1)); });
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[this]<typename T0>(T0&& ioPh1) { return onMouseButtonPressed(std::forward<T0>(ioPh1)); });
}

auto Viewport::onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool {
	// Shortcuts
	if (static_cast<int>(ioEvent.getRepeatCount()) > 0)
		return false;
	if (!isFocused() && !isHovered())
		return false;
	if (m_parent->getState() == EditorLayer::State::Edit) {
		switch (ioEvent.getKeyCode()) {
			// Gizmos
			case input::key::Q:
				{
					if (!gui::Guizmo::isUsing()) {
						m_gizmoType = gui::Guizmo::Type::None;
						return true;
					}
					break;
				}
			case input::key::W:
				{
					if (!gui::Guizmo::isUsing()) {
						m_gizmoType = gui::Guizmo::Type::Translation;
						return true;
					}
					break;
				}
			case input::key::E:
				{
					if (!gui::Guizmo::isUsing()) {
						m_gizmoType = gui::Guizmo::Type::Rotation;
						return true;
					}
					break;
				}
			case input::key::R:
				{
					if (!gui::Guizmo::isUsing()) {
						m_gizmoType = gui::Guizmo::Type::Scale;
						return true;
					}
					break;
				}
			case input::key::T:
				{
					if (!gui::Guizmo::isUsing()) {
						m_gizmoType = gui::Guizmo::Type::All;
						return true;
					}
					break;
				}
			default:
				break;
		}
	}
	return false;
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
