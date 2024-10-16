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
#include "panel/Viewport.h"

namespace owl::nest {
/**
 * @brief Class EditorLayer
 */
class EditorLayer final : public core::layer::Layer {
public:
	EditorLayer(const EditorLayer&) = delete;
	EditorLayer(EditorLayer&&) = delete;
	auto operator=(const EditorLayer&) -> EditorLayer& = delete;
	auto operator=(EditorLayer&&) -> EditorLayer& = delete;
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

	enum struct State : uint8_t { Edit, Play };
	[[nodiscard]] auto getState() const -> const State& { return m_state; }

	void newScene();
	void openScene();
	void openScene(const std::filesystem::path& iScenePath);
	void saveSceneAs();
	void saveSceneAs(const std::filesystem::path& iScenePath);
	void saveCurrentScene();

	[[nodiscard]] auto getActiveScene() const -> const shared<scene::Scene>& { return m_activeScene; }
	auto getSelectedEntity() const -> scene::Entity;
	void setSelectedEntity(scene::Entity iEntity);

private:
	void renderStats(const core::Timestep& iTimeStep);
	void renderMenu();
	void renderToolbar();


	auto onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool;
	auto onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent) -> bool;
	void onScenePlay();
	void onSceneStop();
	void onDuplicateEntity() const;

	input::CameraOrthoController m_cameraController;

	State m_state = State::Edit;

	shared<scene::Scene> m_activeScene;
	shared<scene::Scene> m_editorScene;

	std::filesystem::path m_currentScenePath;

	// Panels
	panel::SceneHierarchy m_sceneHierarchy;
	panel::ContentBrowser m_contentBrowser;
	panel::Viewport m_viewport;

	// Editor resources
	shared<renderer::Texture2D> m_iconPlay;
	shared<renderer::Texture2D> m_iconStop;
};
}// namespace owl::nest
