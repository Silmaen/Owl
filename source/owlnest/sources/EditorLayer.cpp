/**
 * @file EditorLayer.cpp
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"

#include <fmt/format.h>
#include <gui/ImGuiUtils.h>
#include <sound/SoundCommand.h>
#include <sound/SoundSystem.h>

namespace owl::nest {
namespace {
void loadIcons() {
	auto& textureLibrary = renderer::Renderer::getTextureLibrary();
	textureLibrary.load("icons/control/ctrl_rotation");
	textureLibrary.load("icons/control/ctrl_scale");
	textureLibrary.load("icons/control/ctrl_translation");
	textureLibrary.load("icons/PlayButton");
	textureLibrary.load("icons/StopButton");
}
void loadSounds() {
	auto& soundLibrary = sound::SoundSystem::getSoundLibrary();
	soundLibrary.load("clic.wav");
}

}// namespace
EditorLayer::EditorLayer() : Layer("EditorLayer"), m_cameraController{1280.0f / 720.0f} {}

void EditorLayer::onAttach() {
	OWL_PROFILE_FUNCTION()

	core::Application::get().enableDocking();


	m_viewport.attach();
	m_viewport.attachParent(this);

	loadIcons();
	loadSounds();

	m_controlBar.init(gui::widgets::ButtonBarData{{.id = "##controlBar", .visible = true}, false, false, true});
	m_controlBar.addButton({{.id = "##ctrlTranslation", .visible = true},
							"icons/control/ctrl_translation",
							"T",
							[this] { return m_viewport.getGuizmoType() == gui::Guizmo::Type::Translation; },
							[this] {
								if (m_viewport.getGuizmoType() == gui::Guizmo::Type::Translation)
									m_viewport.setGuizmoType(gui::Guizmo::Type::None);
								else
									m_viewport.setGuizmoType(gui::Guizmo::Type::Translation);
							},
							{32, 32}});
	m_controlBar.addButton({{.id = "##ctrlRotation", .visible = true},
							"icons/control/ctrl_rotation",
							"T",
							[this] { return m_viewport.getGuizmoType() == gui::Guizmo::Type::Rotation; },
							[this] {
								if (m_viewport.getGuizmoType() == gui::Guizmo::Type::Rotation)
									m_viewport.setGuizmoType(gui::Guizmo::Type::None);
								else
									m_viewport.setGuizmoType(gui::Guizmo::Type::Rotation);
							},
							{32, 32}});
	m_controlBar.addButton({{.id = "##ctrlScale", .visible = true},
							"icons/control/ctrl_scale",
							"T",
							[this] { return m_viewport.getGuizmoType() == gui::Guizmo::Type::Scale; },
							[this] {
								if (m_viewport.getGuizmoType() == gui::Guizmo::Type::Scale)
									m_viewport.setGuizmoType(gui::Guizmo::Type::None);
								else
									m_viewport.setGuizmoType(gui::Guizmo::Type::Scale);
							},
							{32, 32}});

	m_contentBrowser.attach();
	newScene();
}

void EditorLayer::onDetach() {
	OWL_PROFILE_FUNCTION()

	m_viewport.detach();
	OWL_TRACE("EditorLayer: viewport freed.")
	m_contentBrowser.detach();
	OWL_TRACE("EditorLayer: deleted editor FrameBuffer.")

	m_controlBar.clearButtons();

	m_activeScene.reset();
	OWL_TRACE("EditorLayer: deleted activeScene.")
}

void EditorLayer::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	// resize
	m_cameraController.onResize(m_viewport.getSize());
	m_activeScene->onViewportResize(m_viewport.getSize());

	// Update scene
	if (m_state == State::Edit) {
		if (m_viewport.isFocused())
			m_cameraController.onUpdate(iTimeStep);
	}

	// update the viewport
	m_viewport.onUpdate(iTimeStep);
}

void EditorLayer::onEvent(event::Event& ioEvent) {
	m_cameraController.onEvent(ioEvent);
	m_viewport.onEvent(ioEvent);

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::KeyPressedEvent>(
			[this]<typename T0>(T0&& ioPh1) { return onKeyPressed(std::forward<T0>(ioPh1)); });
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[this]<typename T0>(T0&& ioPh1) { return onMouseButtonPressed(std::forward<T0>(ioPh1)); });
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
	m_viewport.onRender();
	//=============================================================
	renderToolbar();
	if (m_state == State::Edit) {
		m_controlBar.onRender();
	}
}

void EditorLayer::renderStats(const core::Timestep& iTimeStep) {
	if (!m_showStats)
		return;
	ImGui::Begin("Stats");
	ImGui::Text("%s", fmt::format("FPS: {:.2f}", iTimeStep.getFps()).c_str());
	ImGui::Separator();
	ImGui::Text("%s",
				fmt::format("Current used memory: {}", core::utils::sizeToString(debug::TrackerAPI::globals().allocatedMemory)).c_str());
	ImGui::Text("%s", fmt::format("Max used memory: {}", core::utils::sizeToString(debug::TrackerAPI::globals().memoryPeek)).c_str());
	ImGui::Text("%s", fmt::format("Allocation calls: {}", debug::TrackerAPI::globals().allocationCalls).c_str());
	ImGui::Text("%s", fmt::format("Deallocation calls: {}", debug::TrackerAPI::globals().deallocationCalls).c_str());
	ImGui::Text("%s",
				fmt::format("Frame allocation: {}", debug::TrackerAPI::globals().allocationCalls - m_lastAllocCalls)
						.c_str());
	ImGui::Text("%s", fmt::format("Frame deallocation: {}",
								  debug::TrackerAPI::globals().deallocationCalls - m_lastDeallocCalls)
							  .c_str());
	m_lastAllocCalls = debug::TrackerAPI::globals().allocationCalls;
	m_lastDeallocCalls = debug::TrackerAPI::globals().deallocationCalls;
	ImGui::Separator();
	std::string name = "None";
	if (const auto ent = m_viewport.getHoveredEntity()) {
		if (ent.hasComponent<scene::component::Tag>())
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
	ImGui::Text("Viewport size: %f %f", static_cast<double>(m_viewport.getSize().x()),
				static_cast<double>(m_viewport.getSize().y()));
	ImGui::Text("Aspect ratio: %f", static_cast<double>(m_viewport.getSize().ratio()));
	ImGui::End();
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
				core::Application::get().close();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Settings")) {
			if (ImGui::MenuItem("Show Stats", nullptr, m_showStats)) {
				m_showStats = !m_showStats;
			}
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
	auto& textureLibrary = renderer::Renderer::getTextureLibrary();
	const shared<renderer::Texture> icon =
			m_state == State::Edit ? textureLibrary.get("icons/PlayButton") : textureLibrary.get("icons/StopButton");
	ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (size * 0.5f));
	if (const auto tex = gui::imTexture(icon); tex.has_value()) {
		if (ImGui::ImageButton("btn_start_stop", tex.value(), {size, size})) {
			if (m_state == State::Edit)
				onScenePlay();
			else if (m_state == State::Play)
				onSceneStop();
		}
	} else {
		if (m_state == State::Edit) {
			if (ImGui::Button("play", {size, size}))
				onScenePlay();
		} else if (m_state == State::Play) {
			if (ImGui::Button("stop", {size, size}))
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
	m_activeScene->onViewportResize(m_viewport.getSize());
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
		m_editorScene->onViewportResize(m_viewport.getSize());
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
	if (m_state == State::Edit) {
		const bool control = input::Input::isKeyPressed(input::key::LeftControl) ||
							 input::Input::isKeyPressed(input::key::RightControl);
		const bool shift =
				input::Input::isKeyPressed(input::key::LeftShift) || input::Input::isKeyPressed(input::key::RightShift);
		switch (ioEvent.getKeyCode()) {
			case input::key::N:
				{
					if (control) {
						newScene();
						return true;
					}
					break;
				}
			case input::key::O:
				{
					if (control) {
						openScene();
						return true;
					}
					break;
				}
			case input::key::S:
				{
					if (control) {
						if (shift) {
							saveSceneAs();
							return true;
						}
						saveCurrentScene();
						return true;
					}
					break;
				}
			// Scene Commands
			case input::key::D:
				{
					if (control) {
						onDuplicateEntity();
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

auto EditorLayer::onMouseButtonPressed([[maybe_unused]] const event::MouseButtonPressedEvent& ioEvent) -> bool {
	// noting yet
	return false;
}

void EditorLayer::onScenePlay() {
	if (m_editorScene) {
		auto& soundLibrary = sound::SoundSystem::getSoundLibrary();
		sound::SoundCommand::playSound(soundLibrary.get("clic.wav"));

		m_state = State::Play;
		m_activeScene = scene::Scene::copy(m_editorScene);
		m_activeScene->onStartRuntime();

		m_sceneHierarchy.setContext(m_activeScene);
	}
}

void EditorLayer::onSceneStop() {
	m_state = State::Edit;

	m_activeScene->onEndRuntime();
	m_activeScene = m_editorScene;

	m_sceneHierarchy.setContext(m_activeScene);
}

void EditorLayer::onDuplicateEntity() const {
	if (m_state != State::Edit)
		return;

	if (const scene::Entity selectedEntity = m_sceneHierarchy.getSelectedEntity(); selectedEntity)
		m_editorScene->duplicateEntity(selectedEntity);
}

auto EditorLayer::getSelectedEntity() const -> scene::Entity { return m_sceneHierarchy.getSelectedEntity(); }

void EditorLayer::setSelectedEntity(const scene::Entity iEntity) { m_sceneHierarchy.setSelectedEntity(iEntity); }

}// namespace owl::nest
