/**
 * @file EditorLayer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "owl.h"

#include "panel/SceneHierarchy.h"

namespace owl {

/**
 * @brief Class EditorLayer
 */
class EditorLayer : public core::layer::Layer {
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
	void onUpdate(const core::Timestep &ts) override;
	void onEvent(event::Event &event) override;
	void onImGuiRender(const core::Timestep &ts) override;

private:
	void renderViewport();

	input::CameraOrthoController cameraController;

	bool viewportFocused = false;
	bool viewportHovered = false;
	glm::vec2 viewportSize = {0.0f, 0.0f};

	shrd<renderer::VertexArray> squareVA;
	shrd<renderer::Texture> checkerboardTexture;
	glm::vec4 squareColor2 = {0.2f, 0.3f, 0.8f, 1.0f};
	shrd<renderer::Framebuffer> framebuffer;

	shrd<scene::Scene> activeScene;
	scene::Entity squareEntity;

	scene::Entity cameraEntity;
	scene::Entity secondCamera;
	bool primaryCamera = true;

	panel::SceneHierarchy sceneHierarchy;
};

}// namespace owl
