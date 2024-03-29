/**
 * @file EditorLayer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include "panel/ContentBrowser.h"
#include "panel/SceneHierarchy.h"

namespace owl {
/**
 * @brief Class EditorLayer
 */
class EditorLayer final : public core::layer::Layer {
public:
	EditorLayer(const EditorLayer&) = delete;
	EditorLayer(EditorLayer&&) = delete;
	EditorLayer& operator=(const EditorLayer&) = delete;
	EditorLayer& operator=(EditorLayer&&) = delete;
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
	void onUpdate(const core::Timestep& iTimeStep) override;
	void onEvent(event::Event& ioEvent) override;
	void onImGuiRender(const core::Timestep& iTimeStep) override;

private:
	void renderViewport();

	void renderStats(const core::Timestep& iTimeStep);
	void renderMenu();
	void renderGizmo();
	void renderToolbar();

	void newScene();
	void openScene();
	void openScene(const std::filesystem::path& iScenePath);
	void saveSceneAs();
	void saveSceneAs(const std::filesystem::path& iScenePath);
	void saveCurrentScene();

	bool onKeyPressed(const event::KeyPressedEvent& ioEvent);
	bool onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent);
	void onScenePlay();
	void onSceneStop();
	void onDuplicateEntity()const;

	input::CameraOrthoController cameraController;

	enum class State {
		Edit,
		Play
	};

	State state = State::Edit;

	scene::Entity hoveredEntity;
	renderer::CameraEditor editorCamera;

	bool viewportFocused = false;
	bool viewportHovered = false;
	glm::vec2 viewportSize = {0.0f, 0.0f};
	glm::vec2 viewportBounds[2] = {{0.0f, 0.0f}, {0.0f, 0.0f}};
	shared<renderer::Framebuffer> framebuffer;

	shared<scene::Scene> activeScene;
	shared<scene::Scene> editorScene;

	int gizmoType = -1;
	std::filesystem::path currentScenePath{};

	// Panels
	panel::SceneHierarchy sceneHierarchy;
	panel::ContentBrowser contentBrowser;

	// Editor resources
	shared<renderer::Texture2D> iconPlay;
	shared<renderer::Texture2D> iconStop;
};
} // namespace owl
