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
#include "CameraPilot.h"
#include "NavigationToolState.h"
#include "PlayOverlay.h"

#include <renderer/MeshCache.h>
#include <renderer/Renderer3D.h>
#include <renderer/RendererIsometric.h>
#include <scene/SceneCamera.h>
#include <scene/component/Camera.h>
#include <scene/nav/Pathfinder.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
	 *  Move the world origin of the edited scene onto a point, rounded to the snap step (`scene.set_world_origin`).
	 * @param[in] iPoint Point in the scene's float space, relative to the current origin.
	 */
	void moveWorldOriginTo(const math::vec3& iPoint);

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
	 *  Pilot the selected camera entity: the view renders through it (its projection, the aspect of the viewport) and
	 *  every move of the view moves the camera, one undoable `entity.set_transform` per gesture.
	 */
	void pilotSelectedCamera();

	/**
	 * @brief
	 *  Whether the selected entity is a camera the view can pilot: one `goToCameraViewpoint` accepts, outside the
	 *  isometric mode.
	 * @return True when `pilotSelectedCamera` would pilot it.
	 */
	[[nodiscard]] auto canPilotSelectedCamera() const -> bool;

	/**
	 * @brief
	 *  Stop piloting and put the editor view back where it was before.
	 */
	void stopPiloting();

	/**
	 * @brief
	 *  Check whether the view pilots a camera entity.
	 * @return True while piloting.
	 */
	[[nodiscard]] auto isPiloting() const -> bool { return m_pilot.isActive(); }

	/**
	 * @brief
	 *  Check whether an entity is the piloted camera, which the view does not draw.
	 * @param[in] iEntity The entity.
	 * @return True for the piloted camera.
	 */
	[[nodiscard]] auto isPiloted(const scene::Entity& iEntity) const -> bool;

	/**
	 * @brief
	 *  Check whether the right button flies the editor camera over this viewport: the letter keys move the camera then,
	 *  they are no shortcut.
	 * @return True while flying.
	 */
	[[nodiscard]] auto isFlying() const -> bool;

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
	 *  Record a finished gizmo drag or pilot gesture as an `entity.set_transform` command (journaled, undoable,
	 *  replayable).
	 * @param[in] iEntity The moved entity, put back as it was, then moved by the command.
	 * @param[in] iScene The edited scene.
	 * @param[in] iBeforeYaml Snapshot of the entity before the move.
	 */
	void commitTransformEdit(const scene::Entity& iEntity, const shared<scene::Scene>& iScene,
							 const std::string& iBeforeYaml) const;

	/**
	 * @brief
	 *  Draw the grid of the snap step around the camera reference point, with the two axes.
	 * @param[in] iStep The snap step (doubled until the grid has few enough lines).
	 * @param[in] iOnGround Draw on the ground (XZ plane, `y = 0`) instead of the XY plane.
	 */
	void renderGrid(float iStep, bool iOnGround) const;

	/**
	 * @brief
	 *  Draw the 12 edges of the bounding box of a selected mesh, in the selection colour.
	 * @param[in] iWorld World transform of the entity.
	 * @param[in] iGeometry Geometry of the mesh (its object-space bounds).
	 */
	static void renderSelectionBox(const math::mat4& iWorld, const renderer::MeshGeometry& iGeometry);

	/**
	 * @brief
	 *  Move the editor cameras back by the move of the scene's world origin since the last frame, so the view stays put.
	 * @param[in] iScene The edited scene.
	 */
	void followWorldOrigin(const scene::Scene& iScene);

	/**
	 * @brief
	 *  Outline the silhouette of the meshes of the selection (its subtree) in the next render; nothing without one.
	 * @param[in] iScene The scene drawn.
	 */
	void updateSelectionOutline(const shared<scene::Scene>& iScene) const;

	/**
	 * @brief
	 *  Whether the camera markers are drawn as 3D models with their frustum (the 3D perspective view in Edit).
	 * @return True in the 3D perspective view.
	 */
	[[nodiscard]] auto showsCameraModels() const -> bool;

	/**
	 * @brief
	 *  Camera whose 3D model is under the cursor: the models are drawn over the id attachment that the click reads.
	 * @return The nearest camera hit, or an invalid entity.
	 */
	[[nodiscard]] auto pickCameraModel() const -> scene::Entity;

	/**
	 * @brief
	 *  Whether the light markers are drawn and pickable: the edit state, outside the isometric view, with the camera
	 *  markers option on.
	 * @return True when the light markers show.
	 */
	[[nodiscard]] auto showsLightMarkers() const -> bool;

	/**
	 * @brief
	 *  Radius of the marker of a light, which keeps about the same size on the screen.
	 * @param[in] iPosition World position of the light.
	 * @return The radius, in world units.
	 */
	[[nodiscard]] auto getLightMarkerRadius(const math::vec3& iPosition) const -> float;

	/**
	 * @brief
	 *  Draw a small cross at every light, in the colour of the light, pickable; the selected light also draws its
	 *  range (point: three circles, spot: the two cones) or its direction (directional: an arrow).
	 * @param[in] iScene The scene edited.
	 */
	void renderLightMarkers(const shared<scene::Scene>& iScene) const;

	/**
	 * @brief
	 *  Draw a sparkle facing the camera at every particle emitter, pickable, brighter on the selected one.
	 * @param[in] iScene The scene edited.
	 */
	void renderParticleMarkers(const shared<scene::Scene>& iScene) const;

	/**
	 * @brief
	 *  Draw the bones of the skinned mesh of the selected entity and the sockets it owns, pickable as the entity.
	 * @param[in] iScene The scene edited.
	 */
	void renderSkeletonMarkers(const shared<scene::Scene>& iScene) const;

	/**
	 * @brief
	 *  Draw the `Light2D` markers: a disc for an ambient light, a bulb for a point or spot one; selected, its radius
	 *  circle or its cone in the sprite plane.
	 * @param[in] iScene The scene.
	 * @param[in] iSelected The selected entity.
	 */
	void renderLight2DMarkers(const shared<scene::Scene>& iScene, const scene::Entity& iSelected) const;

	/**
	 * @brief
	 *  Draw the bulb sprite of a point light, facing the camera.
	 * @param[in] iPosition World position of the light.
	 * @param[in] iSize Side of the sprite, in world units.
	 * @param[in] iEnabled Whether the light is switched on (lit bulb) or off (grey bulb).
	 * @param[in] iEntityId Entity id written for picking.
	 */
	void drawLightBulb(const math::vec3& iPosition, float iSize, bool iEnabled, int iEntityId) const;

	/**
	 * @brief
	 *  Draw the range of the selected `Oscillator` (a line to its far end) and the route of the selected `Patrol`
	 *  (its waypoints and the wander circle); the picked waypoint is highlighted.
	 * @param[in] iScene The scene edited.
	 */
	void renderBehaviourOverlays(const shared<scene::Scene>& iScene) const;

	/**
	 * @brief
	 *  Waypoint of the selected `Patrol` under the cursor: a click ray against a sphere around each waypoint.
	 * @return The index of the nearest waypoint hit, or -1.
	 */
	[[nodiscard]] auto pickPatrolWaypoint() const -> int;

	/**
	 * @brief
	 *  Move the picked waypoint of the selected `Patrol` with a translation gizmo; the move is recorded as one
	 *  `component.set` command on release.
	 * @return True when the waypoint gizmo is shown (the entity gizmo is not).
	 */
	auto renderWaypointGizmo() -> bool;

	/**
	 * @brief
	 *  Socket of the selected entity whose marker is under the cursor.
	 * @return The socket name, nothing when the click misses every socket.
	 */
	[[nodiscard]] auto pickSocket() const -> std::optional<std::string>;

	/**
	 * @brief
	 *  Move or turn the selected socket of the `MeshSockets` of the selected entity with the gizmo, relative to its
	 *  bone; the gesture previews live and is recorded as one `entity.set_socket` command on release.
	 * @return True when the socket gizmo is shown (the entity gizmo is not).
	 */
	auto renderSocketGizmo() -> bool;

	/**
	 * @brief
	 *  Light whose marker is under the cursor: a click ray against a sphere around each light.
	 * @return The nearest light hit, or an invalid entity.
	 */
	[[nodiscard]] auto pickLight() const -> scene::Entity;

	/**
	 * @brief
	 *  Particle emitter whose marker is under the cursor.
	 * @return The nearest emitter hit, or an invalid entity.
	 */
	[[nodiscard]] auto pickParticleEmitter() const -> scene::Entity;

	/**
	 * @brief
	 *  Camera, light or particle emitter marker under the cursor, in that order.
	 * @return The entity picked, or an invalid entity.
	 */
	[[nodiscard]] auto pickMarker() const -> scene::Entity;

	/**
	 * @brief
	 *  Draw the marker of every camera: icon, arrow and field of view in 2D, frustum in the 3D view.
	 * @param[in] iScene The scene edited.
	 */
	void renderCameraMarkers(const shared<scene::Scene>& iScene) const;

	/**
	 * @brief
	 *  Draw the camera model (`meshes/camera.glb` of the editor assets) at every camera of the scene, pickable.
	 * @param[in] iScene The scene edited.
	 */
	void renderCameraModels(const shared<scene::Scene>& iScene) const;

	/**
	 * @brief
	 *  Draw the frustum of a camera as lines: its field of view (perspective) or its box (orthographic).
	 * @param[in] iCamera The camera component.
	 * @param[in] iWorld World transform of the camera, without scale.
	 * @param[in] iColor Colour of the lines.
	 * @param[in] iEntityId Entity id written for picking.
	 */
	void renderCameraFrustum(const scene::component::Camera& iCamera, const math::mat4& iWorld,
							 const math::vec4& iColor, int iEntityId) const;

	/// Size of the frustum drawn for a camera, in its local space.
	struct FrustumExtent {
		/// Half width of the far rectangle.
		float halfWidth;
		/// Half height of the far rectangle.
		float halfHeight;
		/// Distance of the far rectangle along the local -Z.
		float depth;
		/// Perspective (lines from the origin) or orthographic (a box).
		bool perspective;
	};

	/**
	 * @brief
	 *  Frustum drawn for a camera: its field of view and the aspect of the viewport, cut at a fixed depth.
	 * @param[in] iCamera The camera component.
	 * @return The extent.
	 */
	[[nodiscard]] auto getFrustumExtent(const scene::component::Camera& iCamera) const -> FrustumExtent;

	/**
	 * @brief
	 *  Colour of the marker of a camera.
	 * @param[in] iPrimary Whether the camera is the primary one.
	 * @return Yellow for the primary camera, light blue otherwise.
	 */
	[[nodiscard]] static auto getCameraMarkerColor(bool iPrimary) -> math::vec4;

	/**
	 * @brief
	 *  Follow the piloted camera, move it with the view and record each gesture; stop when the entity is gone.
	 * @param[in] iScene The edited scene.
	 * @param[in] iTimeStep The frame time.
	 */
	void updatePilot(const shared<scene::Scene>& iScene, const core::Timestep& iTimeStep);

	/**
	 * @brief
	 *  Move the editor camera onto the pose of a camera entity.
	 * @param[in] iCamera The camera entity.
	 */
	void moveViewOnto(const scene::Entity& iCamera);

	/**
	 * @brief
	 *  Apply what the running scene asked this frame: stop (`scene.quit()`), screen transition, teleport, save or load.
	 * @param[in] iScene The running scene.
	 */
	void handleRuntimeRequests(const shared<scene::Scene>& iScene);

	/**
	 * @brief
	 *  Render the view of the selected camera entity into the preview framebuffer, when the option is on and the
	 *  camera is neither piloted nor a raycast one.
	 * @param[in] iScene The edited scene.
	 */
	void renderCameraPreview(const shared<scene::Scene>& iScene);

	/**
	 * @brief
	 *  Draw the camera preview in the bottom-right corner, above the view-cube, with the name of the camera and a
	 *  button that pilots it.
	 */
	void renderCameraPreviewInset();

	/**
	 * @brief
	 *  Draw the banner naming the piloted camera, with the button that stops piloting.
	 */
	void renderPilotBanner();

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
	 *  Ray of the viewport camera through the cursor.
	 * @return The origin and the normalized direction, in world space; nothing for an empty viewport.
	 */
	[[nodiscard]] auto getCursorRay() const -> std::optional<std::pair<math::vec3, math::vec3>>;

	/**
	 * @brief
	 *  World point under the cursor where a dropped asset goes: on the hovered mesh or voxel world, else on the ground
	 *  plane (`y = 0`), else on the `z = 0` plane of a 2D view.
	 * @param[in] iScene The active scene.
	 * @return The point; nothing when the cursor ray meets neither.
	 */
	[[nodiscard]] auto getDropPoint(const shared<scene::Scene>& iScene) const -> std::optional<math::vec3>;

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
	/// Model drawn at the cameras in the 3D view, loaded on the first draw (a cache, hence mutable).
	mutable shared<renderer::MeshResource> m_cameraModel;
	/// The camera model file was not found: the 3D view draws the frustums alone.
	mutable bool m_cameraModelMissing{false};
	/// Unit triangle drawn above the frustums to show the up of the cameras, made on the first draw.
	mutable renderer::Renderer3D::MeshHandle m_upTriangle;
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
	/// Edited scene the editor cameras last followed the world origin of.
	const scene::Scene* mp_originScene = nullptr;
	/// World origin of that scene at the last frame: a move of it moves the editor cameras the same way.
	math::vec3d m_lastWorldOrigin{0.0, 0.0, 0.0};
	/// Pilot of a scene camera.
	CameraPilot m_pilot;
	/// Copy of the piloted camera, sized to the viewport and placed at the entity: the camera the view renders with.
	scene::SceneCamera m_pilotCamera;
	/// Where the selected camera's view is drawn for the preview inset; made on the first preview.
	shared<renderer::gpu::Framebuffer> m_previewFramebuffer;
	/// Copy of the previewed camera, sized to the preview and placed at the entity.
	scene::SceneCamera m_previewCamera;
	/// Camera entity drawn in the preview this frame, 0 when the inset is hidden.
	core::UUID m_previewEntity{0};
	/// Entity YAML snapshot captured when a pilot gesture began.
	std::string m_pilotBeforeYaml;
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
	/// Index of the picked waypoint of the selected `Patrol`, -1 for none.
	int m_patrolHandle = -1;
	/// Waypoints of the selected `Patrol` when the drag of a waypoint began.
	std::vector<math::vec3> m_patrolBefore;
	/// Whether the waypoint gizmo was being used last frame.
	bool m_patrolWasUsing = false;
	/// Offset of the selected socket when its drag began.
	std::optional<math::Transform> m_socketBefore;
	/// Whether the socket gizmo was being used last frame.
	bool m_socketWasUsing = false;
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
	/// False while the viewport window is hidden behind another tab or collapsed: its scene is not drawn.
	bool m_visible = true;
	/// ImGui close state — set to false by ImGui when the tab's close X is clicked.
	bool m_pOpen = true;
};

}// namespace owl::nest::panel
