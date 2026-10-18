/**
 * @file Viewport.h
 * @author Silmaen
 * @date 10/16/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owlgui.h>

#include "../UndoManager.h"
#include "NavigationToolState.h"
#include "PlayOverlay.h"

#include <renderer/RendererIsometric.h>
#include <scene/nav/Pathfinder.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace owl::nest {

class EditorLayer;
class SceneDocument;
}// namespace owl::nest

namespace owl::nest::panel {
/**
 * @brief
 *  A viewport panel owned by a single `SceneDocument`.
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

	/**
	 * @brief
	 *  Default constructor.
	 */
	Viewport();

	/**
	 * @brief
	 *  Destructor.
	 */
	~Viewport() override;

	/**
	 * @brief
	 *  Attach.
	 */
	void attach() override;

	/**
	 * @brief
	 *  Detach.
	 */
	void detach() override;

	/**
	 * @brief
	 *  Handle the update event.
	 * @param[in] iTimeStep Frame time delta.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Handle the event event.
	 * @param[in,out] ioEvent The incoming event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Handle the render internal event.
	 */
	void onRenderInternal() override;

	/**
	 * @brief
	 *  Full window setup with close button + unsaved-document flag.
	 */
	void onRender() override;

	/**
	 * @brief
	 *  Defines the camera.
	 */
	void setCamera(const shared<renderer::Camera>& iCamera) { m_camera = iCamera; }

	/**
	 * @brief
	 *  Define the parent editor layer.
	 */
	void attachParent(EditorLayer* iParent) { m_parent = iParent; }

	/**
	 * @brief
	 *  Bind the scene document this viewport renders. Updates the window id.
	 */
	void setDocument(SceneDocument* iDocument);

	/**
	 * @brief
	 *  Access the owning scene document (may be null during construction).
	 * @return Pointer to the document, or `nullptr` when not available.
	 */
	[[nodiscard]] auto getDocument() const -> SceneDocument* { return mp_document; }

	/**
	 * @brief
	 *  Get the hovered entity of the scene the document shows now.
	 * @return The hovered entity, or an empty one when the scene it was picked in has been replaced since (Stop,
	 * scene transition).
	 */
	[[nodiscard]] auto getHoveredEntity() const -> scene::Entity;

	/**
	 * @brief
	 *  Get the framebuffer the viewport renders into.
	 * @return The framebuffer (null once detached).
	 */
	[[nodiscard]] auto getFramebuffer() const -> const shared<renderer::gpu::Framebuffer>& { return m_framebuffer; }

	/**
	 * @brief
	 *  Set the guizmo type.
	 * @param[in] iType The element type.
	 */
	void setGuizmoType(const gui::Guizmo::Type& iType);

	/**
	 * @brief
	 *  Get the guizmo type.
	 * @return The guizmo type.
	 */
	[[nodiscard]] auto getGuizmoType() const -> gui::Guizmo::Type;

	/**
	 * @brief
	 *  Get the guizmo type i.
	 * @return The guizmo type i.
	 */
	[[nodiscard]] auto getGuizmoTypeI() const -> uint16_t;

	/**
	 * @brief
	 *  Set the undo manager for recording gizmo transform edits.
	 */
	void setUndoManager(SceneUndoManager* iUndoManager) { mp_undoManager = iUndoManager; }

	/**
	 * @brief
	 *  True while the user hasn't clicked the tab's close X this frame.
	 * @return True when the object is open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Check whether a gizmo drag is in progress (an edit not recorded yet).
	 * @return True while the gizmo is held.
	 */
	[[nodiscard]] auto isGizmoInUse() const -> bool { return m_gizmoWasUsing; }

	/**
	 * @brief
	 *  Reset the open flag (used by `EditorLayer` after a close is handled or cancelled).
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

	/**
	 * @brief
	 *  Centre the editor camera on the selected entity and back off until it fills the view.
	 */
	void frameSelection();

	/**
	 * @brief
	 *  Centre the editor camera on every entity of the scene and back off until they fill the view.
	 */
	void frameScene();

	/**
	 * @brief
	 *  Turn the editor camera to look along an axis, around the same reference point.
	 * @param[in] iAxis The side to look from.
	 */
	void viewAlong(const renderer::CameraEditor::ViewAxis iAxis) { m_editorCamera.viewAlong(iAxis); }

	/**
	 * @brief
	 *  Put the editor camera back to its default pose.
	 */
	void resetView();

	/**
	 * @brief
	 *  Whether the selected entity is a camera the editor view can be moved onto: a raycast camera is not, the
	 *  editor draws those scenes flat.
	 * @return True when `goToCameraViewpoint` would move the view.
	 */
	[[nodiscard]] auto canGoToCameraViewpoint() const -> bool;

	/**
	 * @brief
	 *  Move the editor camera onto the pose of the selected camera entity: the 2D view framing its orthographic size,
	 *  or the 3D view from its eye.
	 */
	void goToCameraViewpoint();

	/**
	 * @brief
	 *  Switch the editor camera between the 2D orthographic and the perspective view.
	 * @param[in] iProjection The projection.
	 */
	void setViewProjection(renderer::CameraEditor::Projection iProjection) {
		m_editorCamera.setProjection(iProjection);
	}

	/**
	 * @brief
	 *  Projection of the editor camera.
	 * @return The projection.
	 */
	[[nodiscard]] auto getViewProjection() const -> renderer::CameraEditor::Projection {
		return m_editorCamera.getProjectionMode();
	}

	/**
	 * @brief
	 *  Whether the viewport shows the scene through the isometric camera: the scene's stack holds an isometric
	 *  layer with content, the dimetric projection is then locked in Edit.
	 * @return True in the isometric mode.
	 */
	[[nodiscard]] auto isIsometric() const -> bool { return m_isometric; }

	/**
	 * @brief
	 *  Camera of the isometric mode.
	 * @return The isometric editor camera.
	 */
	[[nodiscard]] auto getIsometricCamera() const -> const renderer::IsometricCamera& { return m_isoCamera; }

	/**
	 * @brief
	 *  Tile of the first isometric tilemap under the cursor, highlighted in the isometric mode.
	 * @return The cell, or nothing when the cursor is off every isometric map.
	 */
	[[nodiscard]] auto getHoveredCell() const -> const std::optional<math::vec2ui>& { return m_hoveredCell; }

private:
	/**
	 * @brief
	 *  Enter or leave the isometric mode from the scene's stack, and take the layer's presentation.
	 * @param[in] iScene The edited scene.
	 */
	void updateIsometricMode(const shared<scene::Scene>& iScene);

	/**
	 * @brief
	 *  Pan (middle or right drag, Alt + left drag) and zoom by powers of two (wheel) the isometric camera.
	 */
	void handleIsometricInput();

	/**
	 * @brief
	 *  Find the hovered tile and the ground point under the cursor in the isometric mode.
	 * @param[in] iScene The edited scene.
	 */
	void updateIsometricCursor(const shared<scene::Scene>& iScene);

	/**
	 * @brief
	 *  Draw the world-axis cursor and the hovered tile outline in the isometric mode.
	 */
	void renderIsometricCursor() const;

	/**
	 * @brief
	 *  Draw the pathfinding graph of the isometric tilemaps (two-way moves green, one-way moves orange with an arrow)
	 *  and the test path between the two picked cells.
	 * @param[in] iScene The edited scene.
	 */
	void renderNavigationOverlay(const shared<scene::Scene>& iScene) const;

	/**
	 * @brief
	 *  Pick the ends of the test path: Shift + click sets the start, then the goal, then starts over; Shift + right
	 *  click clears both.
	 */
	void pickNavigationCells();

	/**
	 * @brief
	 *  Navigation tool: with the graph shown, hovering a road cell of the isometric view picks the side of the cell
	 *  nearest to the cursor and a left click cycles its state (two-way, exit only, enter only, closed) through the
	 *  `nav.set_edge` command; a right click opens the cost popup of the cell.
	 * @param[in] iScene The edited scene.
	 */
	void handleNavigationTool(const shared<scene::Scene>& iScene);

	/**
	 * @brief
	 *  Popup of one cell of the navigation tool: its own cost and a button clearing its exceptions.
	 */
	void renderNavigationPopup();

	/**
	 * @brief
	 *  Camera the edited scene is drawn with: the isometric one in the isometric mode, else the editor camera.
	 * @return The active editor camera.
	 */
	[[nodiscard]] auto getActiveCamera() const -> const renderer::Camera&;

	/**
	 * @brief
	 *  Render gizmo.
	 */
	void renderGizmo();

	/**
	 * @brief
	 *  Record a finished gizmo drag as an `entity.set_transform` command (journaled, undoable, replayable).
	 * @param[in] iEntity The moved entity, put back as it was, then moved by the command.
	 * @param[in] iScene The edited scene.
	 */
	void commitGizmoEdit(const scene::Entity& iEntity, const shared<scene::Scene>& iScene) const;

	/**
	 * @brief
	 *  Draw the grid of the snap step on the XY plane around the camera reference point, with the two axes.
	 * @param[in] iStep Step between two lines, in world units.
	 */
	void renderGrid(float iStep) const;

	/**
	 * @brief
	 *  Render the right overlay pill: view projection, Show menu, frame selection and stats.
	 */
	void renderViewOverlay();

	/**
	 * @brief
	 *  Context menu of a right click that did not drag: actions on the clicked entity, or view options on the empty
	 *  scene.
	 */
	void renderContextMenu();

	/**
	 * @brief
	 *  Render overlay.
	 */
	void renderOverlay() const;

	/**
	 * @brief
	 *  Render the floating in-viewport toolbar (Show toggles + gizmo selector).
	 *
	 * Mirrors the ribbon's `Show` and `Gizmo` groups so the user gets quick access
	 * without leaving the viewport. Drawn over the framebuffer image, after the
	 * gizmo so it never gets covered.
	 */
	void renderOverlayToolbar();

	/**
	 * @brief
	 *  Draw the Play, Pause, Step and Stop controls, or the corner handle that stands for them when folded.
	 * @param[in] iCollapsed Whether the controls are folded into the handle.
	 */
	void renderPlayControls(bool iCollapsed);

	/**
	 * @brief
	 *  Draw the "Playing" / "Paused" message while its timer runs.
	 * @param[in] iCollapsed Whether the controls are folded, which moves the message to the top.
	 */
	void renderPlayBanner(bool iCollapsed);

	/**
	 * @brief
	 *  Remember the screen rectangle of an overlay control, so the game does not see the clicks it takes.
	 * @param[in] iRect Rectangle of the overlay window.
	 */
	void addOverlayRect(const OverlayRect& iRect) {
		if (m_overlayCount < m_overlayRects.size())
			m_overlayRects[m_overlayCount++] = iRect;
	}

	/**
	 * @brief
	 *  Draw the view-cube in the viewport corner: the six axis ends show the camera orientation, a click on one looks
	 *  from that side, a click on the centre resets the view.
	 */
	void renderCameraGizmo();

	/**
	 * @brief
	 *  Content of the camera menu shared by the view overlay and the context menu: reset, axis views, framing and the
	 *  selected camera viewpoint.
	 */
	void renderCameraMenu();

	/**
	 * @brief
	 *  Handle the mouse button pressed event.
	 * @param[in,out] ioEvent The incoming event.
	 * @return True when the event was consumed.
	 */
	auto onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent) -> bool;

	/**
	 * @brief
	 *  Drive the voxel brush in Edit mode: raycast the editor camera, highlight the targeted block, apply edits.
	 * @param[in] iScene The active scene (holds the target `VoxelWorld`).
	 */
	void handleVoxelBrush(const shared<scene::Scene>& iScene);
	/// The camera.
	shared<renderer::Camera> m_camera;
	/// The frame buffer where to do the render.
	shared<renderer::gpu::Framebuffer> m_framebuffer;
	/// Pointer to the parent layer.
	EditorLayer* m_parent = nullptr;
	/// Pointer to the scene document that owns this viewport.
	SceneDocument* mp_document = nullptr;
	/// The hovered entity
	scene::Entity m_hoveredEntity;
	/// Scene the hovered entity was picked in: a handle of a replaced scene points at freed memory.
	const scene::Scene* mp_hoveredScene = nullptr;
	/// The Gizmo type
	gui::Guizmo::Type m_gizmoType = gui::Guizmo::Type::None;
	/// The editor camera
	renderer::CameraEditor m_editorCamera;
	/// Camera of the isometric mode.
	renderer::IsometricCamera m_isoCamera;
	/// Whether the viewport is in the isometric mode.
	bool m_isometric = false;
	/// Ground point under the cursor in the isometric mode, at the height of the hovered map.
	std::optional<math::vec3> m_isoCursor;
	/// World position of the back corner of the hovered isometric map.
	math::vec3 m_hoveredMapOrigin{0.f, 0.f, 0.f};
	/// Heights above the map origin of the corners of the hovered cell (back, right, front, left), in world units.
	std::array<float, 4> m_hoveredCornerZ{};
	/// Hovered tile of the first isometric map.
	std::optional<math::vec2ui> m_hoveredCell;
	/// Tilemap entity of the hovered isometric map.
	core::UUID m_hoveredMapEntity{0};
	/// Whether the navigation tool is on.
	NavigationToolState m_navTool;
	/// Side of the hovered cell the navigation tool would change.
	std::optional<scene::nav::Direction> m_navSide;
	/// Cell the navigation popup is about.
	std::optional<math::vec2ui> m_navPopupCell;
	/// Tilemap entity the navigation popup is about.
	core::UUID m_navPopupEntity{0};
	/// The popup was asked for this frame.
	bool m_navOpenPopup = false;
	/// Cost typed in the navigation popup.
	float m_navCostEdit = 0.f;
	/// Start of the navigation test path.
	std::optional<math::vec2ui> m_navFrom;
	/// Goal of the navigation test path.
	std::optional<math::vec2ui> m_navTo;
	/// Search that computes the test path.
	mutable uniq<scene::nav::Pathfinder> mp_navFinder;
	/// The test path.
	mutable scene::nav::PathResult m_navPath;
	/// What the test path was computed for: start, goal and graph version (0 when stale).
	mutable std::array<uint64_t, 6> m_navKey{};
	/// Undo manager (non-owning, optional).
	SceneUndoManager* mp_undoManager = nullptr;
	/// Whether the gizmo was being used last frame.
	bool m_gizmoWasUsing = false;
	/// Entity YAML snapshot captured when gizmo manipulation began.
	std::string m_gizmoBeforeYaml;
	/// Message shown for a moment when Play or Pause starts.
	PlayBanner m_playBanner;
	/// Keeps the clicks on the overlay controls away from the game.
	PlayInputGate m_playGate;
	/// Screen rectangles of the overlay controls drawn last frame.
	std::array<OverlayRect, 8> m_overlayRects{};
	/// Number of rectangles in use.
	std::size_t m_overlayCount = 0;
	/// Whether the window was focused last frame (used to detect a focus transition).
	bool m_wasFocused = false;
	/// ImGui close state — set to false by ImGui when the tab's close X is clicked.
	bool m_pOpen = true;
};

}// namespace owl::nest::panel
