/**
 * @file Viewport.h
 * @author Silmaen
 * @date 10/16/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include "../UndoManager.h"

namespace owl::nest {
class EditorLayer;
class SceneDocument;
}// namespace owl::nest

namespace owl::nest::panel {
/**
 * @brief A viewport panel owned by a single `SceneDocument`.
 *
 * Each open scene gets its own `Viewport` with a unique ImGui window id
 * (`##<uuid>` suffix). ImGui's docking system groups viewports that share a
 * dock node as tabs, and the user can tear one off to see several scenes
 * side-by-side. The active document is the one whose viewport was last
 * focused.
 */
class Viewport final : public gui::BasePanel {
public:
	Viewport(const Viewport&) = delete;
	Viewport(Viewport&&) = delete;
	auto operator=(const Viewport&) -> Viewport& = delete;
	auto operator=(Viewport&&) -> Viewport& = delete;

	Viewport();
	~Viewport() override;

	void attach() override;
	void detach() override;
	void onUpdate(const core::Timestep& iTimeStep) override;
	void onEvent(event::Event& ioEvent) override;
	void onRenderInternal() override;

	/// @brief Full window setup with close button + unsaved-document flag.
	void onRender() override;

	/// @brief Defines the camera.
	void setCamera(const shared<renderer::Camera>& iCamera) { m_camera = iCamera; }

	/// @brief Define the parent editor layer.
	void attachParent(EditorLayer* iParent) { m_parent = iParent; }

	/// @brief Bind the scene document this viewport renders. Updates the window id.
	void setDocument(SceneDocument* iDocument);
	/// @brief Access the owning scene document (may be null during construction).
	[[nodiscard]] auto getDocument() const -> SceneDocument* { return mp_document; }

	/// @brief Get the hovered entity.
	[[nodiscard]] auto getHoveredEntity() const -> scene::Entity { return m_hoveredEntity; }

	void setGuizmoType(const gui::Guizmo::Type& iType);
	[[nodiscard]] auto getGuizmoType() const -> gui::Guizmo::Type;
	[[nodiscard]] auto getGuizmoTypeI() const -> uint16_t;

	/// Set the undo manager for recording gizmo transform edits.
	void setUndoManager(SceneUndoManager* iUndoManager) { mp_undoManager = iUndoManager; }

	/// @brief True while the user hasn't clicked the tab's close X this frame.
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }
	/// @brief Reset the open flag (used by `EditorLayer` after a close is handled or cancelled).
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	void renderGizmo();
	void renderOverlay() const;
	auto onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent) -> bool;
	/// The camera.
	shared<renderer::Camera> m_camera;
	/// The frame buffer where to do the render.
	shared<renderer::Framebuffer> m_framebuffer;
	/// Pointer to the parent layer.
	EditorLayer* m_parent = nullptr;
	/// Pointer to the scene document that owns this viewport.
	SceneDocument* mp_document = nullptr;
	/// The hovered entity
	scene::Entity m_hoveredEntity;
	/// The Gizmo type
	gui::Guizmo::Type m_gizmoType = gui::Guizmo::Type::None;
	/// The editor camera
	renderer::CameraEditor m_editorCamera;
	/// Undo manager (non-owning, optional).
	SceneUndoManager* mp_undoManager = nullptr;
	/// Whether the gizmo was being used last frame.
	bool m_gizmoWasUsing = false;
	/// Entity YAML snapshot captured when gizmo manipulation began.
	std::string m_gizmoBeforeYaml;
	/// Whether the window was focused last frame (used to detect a focus transition).
	bool m_wasFocused = false;
	/// ImGui close state — set to false by ImGui when the tab's close X is clicked.
	bool m_pOpen = true;
};

}// namespace owl::nest::panel
