/**
 * @file EditorLayer.cpp
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"

// must be included AFTER imgui
OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
#include <ImGuizmo.h>
OWL_DIAG_POP

namespace owl {

constexpr auto vec(math::vec2 iVec) -> ImVec2 { return {iVec.x(), iVec.y()}; }

EditorLayer::EditorLayer() : Layer("EditorLayer"), m_cameraController{1280.0f / 720.0f} {}

void EditorLayer::onAttach() {
	OWL_PROFILE_FUNCTION()

	core::Application::get().enableDocking();

	const renderer::FramebufferSpecification specs{
			.size = {1280, 720},
			.attachments =
					{
							{.format = renderer::AttachmentSpecification::Format::Surface,
							 .tiling = renderer::AttachmentSpecification::Tiling::Optimal},
							{.format = renderer::AttachmentSpecification::Format::RedInteger,
							 .tiling = renderer::AttachmentSpecification::Tiling::Optimal},
							//{renderer::AttachmentSpecification::Format::Depth24Stencil8,
							// renderer::AttachmentSpecification::Tiling::Optimal}
					},
			.samples = 1,
			.swapChainTarget = false,
			.debugName = "editor"};
	m_framebuffer = renderer::Framebuffer::create(specs);

	m_activeScene = mkShared<scene::Scene>();
	m_editorCamera = renderer::CameraEditor(30.0f, 1.778f, 0.1f, 1000.0f);

	const auto iconPath = core::Application::get().getAssetDirectory() / "icons";
	m_iconPlay = renderer::Texture2D::create(iconPath / "PlayButton.png");
	m_iconStop = renderer::Texture2D::create(iconPath / "StopButton.png");
	m_contentBrowser.attach();
}

void EditorLayer::onDetach() {
	OWL_PROFILE_FUNCTION()

	m_contentBrowser.detach();
	OWL_TRACE("EditorLayer: content browser resources freed.")
	m_iconPlay.reset();
	OWL_TRACE("EditorLayer: deleted iconPlay Texture.")
	m_iconStop.reset();
	OWL_TRACE("EditorLayer: deleted iconStop Texture.")
	m_framebuffer.reset();
	OWL_TRACE("EditorLayer: deleted editor FrameBuffer.")
	m_activeScene.reset();
	OWL_TRACE("EditorLayer: deleted activeScene.")
}

void EditorLayer::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	// resize
	const auto spec = m_framebuffer->getSpecification();
	if (m_viewportSize.surface() > 0 && m_viewportSize != spec.size) {
		m_framebuffer->resize(m_viewportSize);
		m_cameraController.onResize(m_viewportSize);
		m_editorCamera.setViewportSize(m_viewportSize);
		m_activeScene->onViewportResize(m_viewportSize);
	}

	// Render
	m_framebuffer->bind();
	renderer::RenderCommand::setClearColor({0.1f, 0.1f, 0.1f, 1});
	renderer::RenderCommand::clear();

	// Clear our entity ID attachment to -1
	m_framebuffer->clearAttachment(1, -1);


	// Update scene
	switch (m_state) {
		case State::Edit:
			{
				if (m_viewportFocused)
					m_cameraController.onUpdate(iTimeStep);
				if (m_viewportHovered)
					m_editorCamera.onUpdate(iTimeStep);
				m_activeScene->onUpdateEditor(iTimeStep, m_editorCamera);
				break;
			}
		case State::Play:
			{
				m_activeScene->onUpdateRuntime(iTimeStep);
				break;
			}
	}
	//if constexpr ((false)) {
	auto [mx, my] = ImGui::GetMousePos();
	mx -= m_viewportLower.x();
	my -= m_viewportLower.y();
	const math::vec2 viewportSizeInternal = m_viewportUpper - m_viewportLower;
	my = viewportSizeInternal.y() - my;
	const int mouseX = static_cast<int>(mx);
	const int mouseY = static_cast<int>(my);

	if (mouseX >= 0 && mouseY >= 0 && mouseX < static_cast<int>(viewportSizeInternal.x()) &&
		mouseY < static_cast<int>(viewportSizeInternal.y())) {
		const int pixelData = m_framebuffer->readPixel(1, mouseX, mouseY);
		m_hoveredEntity = pixelData == -1 ? scene::Entity()
										  : scene::Entity(static_cast<entt::entity>(pixelData), m_activeScene.get());
	}
	//}

	m_framebuffer->unbind();
}

void EditorLayer::onEvent(event::Event& ioEvent) {
	m_cameraController.onEvent(ioEvent);
	m_editorCamera.onEvent(ioEvent);

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::KeyPressedEvent>(
			[this](auto&& PH1) { return onKeyPressed(std::forward<decltype(PH1)>(PH1)); });
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[this](auto&& PH1) { return onMouseButtonPressed(std::forward<decltype(PH1)>(PH1)); });
}

void EditorLayer::onImGuiRender(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	// ==================================================================
	renderStats(iTimeStep);
	//=============================================================
	renderMenu();
	//=============================================================
	m_sceneHierarchy.onImGuiRender();
	m_contentBrowser.onImGuiRender();
	//=============================================================
	renderViewport();
	//=============================================================
	renderToolbar();
}

void EditorLayer::renderStats(const core::Timestep& iTimeStep) {
	ImGui::Begin("Stats");
	ImGui::Text("%s", fmt::format("FPS: {:.2f}", iTimeStep.getFps()).c_str());
	ImGui::Separator();
	ImGui::Text("%s", fmt::format("Current used memory: {}", debug::TrackerAPI::globals().allocatedMemory).c_str());
	ImGui::Text("%s", fmt::format("Max used memory: {}", debug::TrackerAPI::globals().memoryPeek).c_str());
	ImGui::Text("%s", fmt::format("Allocation calls: {}", debug::TrackerAPI::globals().allocationCalls).c_str());
	ImGui::Text("%s", fmt::format("Deallocation calls: {}", debug::TrackerAPI::globals().deallocationCalls).c_str());
	ImGui::Separator();
	std::string name = "None";
	if (m_hoveredEntity) {
		if (m_hoveredEntity.hasComponent<scene::component::Tag>())
			name = m_hoveredEntity.getComponent<scene::component::Tag>().tag;
	}
	ImGui::Text("Hovered Entity: %s", name.c_str());
	ImGui::Separator();
	const auto stats = renderer::Renderer2D::getStats();
	ImGui::Text("Renderer2D Stats:");
	ImGui::Text("Draw Calls: %ud", stats.drawCalls);
	ImGui::Text("Quads: %ud", stats.quadCount);
	ImGui::Text("Vertices: %ud", stats.getTotalVertexCount());
	ImGui::Text("Indices: %ud", stats.getTotalIndexCount());
	ImGui::Text("Viewport size: %f %f", static_cast<double>(m_viewportSize.x()),
				static_cast<double>(m_viewportSize.y()));
	ImGui::Text("Aspect ratio: %f", static_cast<double>(m_viewportSize.ratio()));
	ImGui::End();
}

void EditorLayer::renderViewport() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	ImGui::Begin("Viewport");
	const auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
	const auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
	const auto viewportOffset = ImGui::GetWindowPos();
	m_viewportLower = {viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y};
	m_viewportUpper = {viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y};

	m_viewportFocused = ImGui::IsWindowFocused();
	m_viewportHovered = ImGui::IsWindowHovered();
	core::Application::get().getImGuiLayer()->blockEvents(!m_viewportFocused && !m_viewportHovered);

	const ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	m_viewportSize = {static_cast<uint32_t>(viewportPanelSize.x), static_cast<uint32_t>(viewportPanelSize.y)};

	// NOLINTBEGIN(performance-no-int-to-ptr)
	if (const uint64_t textureId = m_framebuffer->getColorAttachmentRendererId(0); textureId != 0)
		ImGui::Image(reinterpret_cast<void*>(textureId), viewportPanelSize, vec(m_framebuffer->getLowerData()),
					 vec(m_framebuffer->getUpperData()));
	// NOLINTEND(performance-no-int-to-ptr)

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const auto* path = static_cast<const char*>(payload->Data);
			if (const std::filesystem::path scenePath = core::Application::get().getAssetDirectory() / path;
				scenePath.extension() == ".owl")
				openScene(scenePath);
			else
				OWL_CORE_WARN("Could not load {}: not a scene file", scenePath.string())
		}
		ImGui::EndDragDropTarget();
	}

	renderGizmo();

	ImGui::End();
	ImGui::PopStyleVar();
}

void EditorLayer::renderGizmo() {
	// Gizmos

	if (const scene::Entity selectedEntity = m_sceneHierarchy.getSelectedEntity();
		selectedEntity && m_gizmoType != -1) {
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();

		ImGuizmo::SetRect(m_viewportLower.x(), m_viewportLower.y(), m_viewportUpper.x() - m_viewportLower.x(),
						  m_viewportUpper.y() - m_viewportLower.y());

		// Runtime camera from entity
		// auto cameraEntity = activeScene->getPrimaryCamera();
		// const auto &camera = cameraEntity.getComponent<scene::component::Camera>().camera;
		// const math::mat4 &cameraProjection = camera.getProjection();
		// math::mat4 cameraView = math::inverse(cameraEntity.getComponent<scene::component::Transform>().getTransform());

		// Editor camera
		math::mat4 cameraProjection = m_editorCamera.getProjection();
		if (renderer::RenderCommand::getApi() == renderer::RenderAPI::Type::Vulkan)
			cameraProjection(1, 1) *= -1.f;
		math::mat4 cameraView = m_editorCamera.getViewMatrix();

		// Entity transform
		auto& tc = selectedEntity.getComponent<scene::component::Transform>();
		math::mat4 transform = tc.getTransform();

		// Snapping
		const bool snap = input::Input::isKeyPressed(input::key::LeftControl);
		float snapValue = 0.5f;// Snap to 0.5m for translation/scale
		// Snap to 45 degrees for rotation
		if (m_gizmoType == ImGuizmo::OPERATION::ROTATE)
			snapValue = 45.0f;

		const float snapValues[3] = {snapValue, snapValue, snapValue};

		ImGuizmo::Manipulate(cameraView.data(), cameraProjection.data(), static_cast<ImGuizmo::OPERATION>(m_gizmoType),
							 ImGuizmo::LOCAL, transform.data(), nullptr, snap ? snapValues : nullptr);

		if (ImGuizmo::IsUsing()) {
			math::vec3 translation;
			math::vec3 rotation;
			math::vec3 scale;
			math::decomposeTransform(transform, translation, rotation, scale);

			const math::vec3 deltaRotation = rotation - tc.rotation;
			tc.translation = translation;
			tc.rotation += deltaRotation;
			tc.scale = scale;
		}
	}
}

void EditorLayer::renderMenu() {
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New", "Ctrl+N"))
				newScene();
			ImGui::Separator();
			if (ImGui::MenuItem("Open Scene", "Ctrl+O"))
				openScene();
			if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, !m_currentScenePath.empty()))
				saveCurrentScene();
			if (ImGui::MenuItem("Save Scene as..", "Ctrl+Shift+S"))
				saveSceneAs();
			ImGui::Separator();
			if (ImGui::MenuItem("Exit"))
				owl::core::Application::get().close();
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void EditorLayer::renderToolbar() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	const auto& colors = ImGui::GetStyle().Colors;
	const auto& buttonHovered = colors[ImGuiCol_ButtonHovered];
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f));
	const auto& buttonActive = colors[ImGuiCol_ButtonActive];
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(buttonActive.x, buttonActive.y, buttonActive.z, 0.5f));

	ImGui::Begin("##toolbar", nullptr,
				 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	const float size = ImGui::GetWindowHeight() - 4.0f;
	const shared<renderer::Texture2D> icon = m_state == State::Edit ? m_iconPlay : m_iconStop;
	ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (size * 0.5f));
	uint64_t textureId = 0;
	if (icon)
		textureId = icon->getRendererId();
	if (textureId != 0) {

		// NOLINTBEGIN(performance-no-int-to-ptr)
		if (ImGui::ImageButton("btn_start_stop", reinterpret_cast<ImTextureID>(textureId), ImVec2(size, size),
							   ImVec2(0, 0), ImVec2(1, 1))) {
			if (m_state == State::Edit)
				onScenePlay();
			else if (m_state == State::Play)
				onSceneStop();
		}
		// NOLINTEND(performance-no-int-to-ptr)
	} else {
		if (m_state == State::Edit) {
			if (ImGui::Button("play", ImVec2(size, size)))
				onScenePlay();
		} else if (m_state == State::Play) {
			if (ImGui::Button("stop", ImVec2(size, size)))
				onSceneStop();
		}
	}
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
	ImGui::End();
}
OWL_DIAG_POP

void EditorLayer::newScene() {
	m_activeScene = mkShared<scene::Scene>();
	m_activeScene->onViewportResize(m_viewportSize);
	m_sceneHierarchy.setContext(m_activeScene);
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
	if (m_state != State::Edit)
		onSceneStop();
	const auto newScene = mkShared<scene::Scene>();

	if (const scene::SceneSerializer serializer(newScene); serializer.deserialize(iScenePath)) {
		m_editorScene = newScene;
		m_editorScene->onViewportResize(m_viewportSize);
		m_sceneHierarchy.setContext(m_editorScene);
		m_activeScene = m_editorScene;
		m_currentScenePath = iScenePath;
	}
}

void EditorLayer::saveSceneAs() {
	if (const auto filepath = core::utils::FileDialog::saveFile("Owl Scene (*.owl)|owl\n"); !filepath.empty())
		saveSceneAs(filepath);
}

void EditorLayer::saveSceneAs(const std::filesystem::path& iScenePath) {
	const scene::SceneSerializer serializer(m_activeScene);
	serializer.serialize(iScenePath);
	m_currentScenePath = iScenePath;
}

void EditorLayer::saveCurrentScene() {
	if (m_currentScenePath.empty())
		saveSceneAs();
	else
		saveSceneAs(m_currentScenePath);
}

auto EditorLayer::onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool {
	// Shortcuts
	if (static_cast<int>(ioEvent.getRepeatCount()) > 0)
		return false;

	const bool control =
			input::Input::isKeyPressed(input::key::LeftControl) || input::Input::isKeyPressed(input::key::RightControl);
	const bool shift =
			input::Input::isKeyPressed(input::key::LeftShift) || input::Input::isKeyPressed(input::key::RightShift);
	switch (ioEvent.getKeyCode()) {
		case input::key::N:
			{
				if (control)
					newScene();
				break;
			}
		case input::key::O:
			{
				if (control)
					openScene();
				break;
			}
		case input::key::S:
			{
				if (control) {
					if (shift)
						saveSceneAs();
					else
						saveCurrentScene();
				}
				break;
			}
		// Scene Commands
		case input::key::D:
			{
				if (control)
					onDuplicateEntity();
				break;
			}
		// Gizmos
		case input::key::Q:
			{
				if (!ImGuizmo::IsUsing())
					m_gizmoType = -1;
				break;
			}
		case input::key::W:
			{
				if (!ImGuizmo::IsUsing())
					m_gizmoType = ImGuizmo::OPERATION::TRANSLATE;
				break;
			}
		case input::key::E:
			{
				if (!ImGuizmo::IsUsing())
					m_gizmoType = ImGuizmo::OPERATION::ROTATE;
				break;
			}
		case input::key::R:
			{
				if (!ImGuizmo::IsUsing())
					m_gizmoType = ImGuizmo::OPERATION::SCALE;
				break;
			}
		default:
			break;
	}
	return false;
}

auto EditorLayer::onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent) -> bool {
	if (ioEvent.getMouseButton() == input::mouse::ButtonLeft) {
		if (m_viewportHovered && !ImGuizmo::IsOver() && !input::Input::isKeyPressed(input::key::LeftAlt))
			m_sceneHierarchy.setSelectedEntity(m_hoveredEntity);
	}
	return false;
}

void EditorLayer::onScenePlay() {
	if (m_editorScene) {
		m_state = State::Play;
		m_activeScene = scene::Scene::copy(m_editorScene);

		m_sceneHierarchy.setContext(m_activeScene);
	}
}

void EditorLayer::onSceneStop() {
	m_state = State::Edit;

	m_activeScene = m_editorScene;

	m_sceneHierarchy.setContext(m_activeScene);
}

void EditorLayer::onDuplicateEntity() const {
	if (m_state != State::Edit)
		return;

	if (const scene::Entity selectedEntity = m_sceneHierarchy.getSelectedEntity(); selectedEntity)
		m_editorScene->duplicateEntity(selectedEntity);
}

}// namespace owl
