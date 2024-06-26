/**
 * @file EditorLayer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <../../owl/owl.h>

#include "panel/ContentBrowser.h"
#include "panel/SceneHierarchy.h"

namespace owl {
/**
 * @brief Class EditorLayer
 */
class EditorLayer final : public core::layer::Layer {
public:
	EditorLayer(const EditorLayer &) = delete;
	EditorLayer(EditorLayer &&) = delete;
	EditorLayer &operator=(const EditorLayer &) = delete;
	EditorLayer &operator=(EditorLayer &&) = delete;
	/**
	 * @brief Default constructor.
	 */
	EditorLayer();
	/**
	 * @brief Destructor.
	 */
	~EditorLayer() override = default;

	void onAttach() override;
	void onDetach() override;
	void onUpdate(const core::Timestep &iTimeStep) override;
	void onEvent(event::Event &ioEvent) override;
	void onImGuiRender(const core::Timestep &iTimeStep) override;

private:
	void renderViewport();

	void renderStats(const core::Timestep &iTimeStep);
	void renderMenu();
	void renderGizmo();
	void renderToolbar();

	void newScene();
	void openScene();
	void openScene(const std::filesystem::path &iScenePath);
	void saveSceneAs();
	void saveSceneAs(const std::filesystem::path &iScenePath);
	void saveCurrentScene();

	bool onKeyPressed(const event::KeyPressedEvent &ioEvent);
	bool onMouseButtonPressed(const event::MouseButtonPressedEvent &ioEvent);
	void onScenePlay();
	void onSceneStop();
	void onDuplicateEntity() const;

	input::CameraOrthoController m_cameraController;

	enum struct State : uint8_t { Edit, Play };

	State m_state = State::Edit;

	scene::Entity m_hoveredEntity;
	renderer::CameraEditor m_editorCamera;

	bool m_viewportFocused = false;
	bool m_viewportHovered = false;
	math::FrameSize m_viewportSize = {0, 0};
	math::vec2 m_viewportLower = {0.0f, 0.0f};
	math::vec2 m_viewportUpper = {0.0f, 0.0f};
	shared<renderer::Framebuffer> m_framebuffer;

	shared<scene::Scene> m_activeScene;
	shared<scene::Scene> m_editorScene;

	int m_gizmoType = -1;
	std::filesystem::path m_currentScenePath;

	// Panels
	panel::SceneHierarchy m_sceneHierarchy;
	panel::ContentBrowser m_contentBrowser;

	// Editor resources
	shared<renderer::Texture2D> m_iconPlay;
	shared<renderer::Texture2D> m_iconStop;
};
}// namespace owl
