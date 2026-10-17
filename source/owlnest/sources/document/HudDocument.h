/**
 * @file HudDocument.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../commands/CommandRegistry.h"

#include <math/vectors.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/UiRect.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  Handle of the rect gizmo a mouse press can grab.
 */
enum struct HudHandle : uint8_t {
	None,///< Nothing.
	Body,///< The inside of the widget (or of the selected widgets): move.
	Left,///< Left edge: resize.
	Right,///< Right edge: resize.
	Top,///< Top edge: resize.
	Bottom,///< Bottom edge: resize.
	TopLeft,///< Top-left corner: resize.
	TopRight,///< Top-right corner: resize.
	BottomLeft,///< Bottom-left corner: resize.
	BottomRight,///< Bottom-right corner: resize.
	Pivot,///< The pivot of the widget.
	AnchorMin,///< Top-left anchor of a stretched widget.
	AnchorMax,///< Bottom-right anchor of a stretched widget.
	AnchorBoth///< The single anchor point of a widget pinned to one point of its parent.
};

/**
 * @brief
 *  Which handles a press may grab first: the pivot and the anchors often sit in the middle of the widget, where a
 *  press must move it, so they are grabbed there only on request.
 */
enum struct HudGrabMode : uint8_t {
	Normal,///< Pivot and anchors count only when they lie outside the widget.
	Pivot,///< The pivot first, then the anchors, wherever they are.
	Anchors///< The anchors first, then the pivot, wherever they are.
};

/**
 * @brief
 *  What lies behind the HUD in the preview.
 */
enum struct HudBackdropKind : uint8_t {
	Color,///< A plain colour.
	Image///< An image stretched over the whole preview (a screenshot of the game).
};

/**
 * @brief
 *  How the selection changes when a widget is picked.
 */
enum struct HudSelectMode : uint8_t {
	Replace,///< The widget becomes the only one selected.
	Toggle,///< The widget joins the selection, or leaves it when it is in.
	Add///< The widget joins the selection.
};

/**
 * @brief
 *  A preview resolution offered by the editor.
 */
struct HudResolutionPreset {
	/// Name shown in the list.
	std::string_view name;
	/// Size in pixels.
	math::vec2ui size;
};

/**
 * @brief
 *  Snapping of the gestures of the HUD editor.
 */
struct HudSnapSettings {
	/// Round the position and the edges to a grid of reference pixels.
	bool pixelGrid{true};
	/// Grid step in reference pixels.
	float gridStep{1.f};
	/// Stick to the edges and the centre of the siblings and of the parent.
	bool edges{true};
	/// Distance in screen pixels under which an edge sticks.
	float threshold{6.f};
};

/**
 * @brief
 *  The rect gizmo of the primary selected widget, in screen pixels of the preview.
 */
struct HudGizmo {
	/// Rectangle of the widget.
	scene::component::UiRect::Bounds bounds;
	/// Rectangle of its parent (where the anchors live).
	scene::component::UiRect::Bounds parent;
	/// Position of the pivot.
	math::vec2 pivot{0.f, 0.f};
	/// Position of the top-left anchor.
	math::vec2 anchorMin{0.f, 0.f};
	/// Position of the bottom-right anchor.
	math::vec2 anchorMax{0.f, 0.f};
	/// Whether the anchors are apart (the widget stretches with its parent).
	bool stretched{false};
};

/**
 * @brief
 *  Move the values of a rectangle's edges, whichever sticks, to the closest candidate.
 * @param[in] iValues Positions of the moving edges (e.g. left, centre, right).
 * @param[in] iCandidates Positions the edges may stick to.
 * @param[in] iThreshold Largest distance that sticks.
 * @return The shift to apply and the candidate stuck to, or nothing when none is close enough.
 */
[[nodiscard]] auto snapToCandidates(std::span<const float> iValues, std::span<const float> iCandidates,
									float iThreshold) -> std::optional<std::pair<float, float>>;

/**
 * @brief
 *  Round a value to a grid.
 * @param[in] iValue The value.
 * @param[in] iStep Grid step (the value stays when it is not positive).
 * @return The closest multiple of the step.
 */
[[nodiscard]] auto snapToGrid(float iValue, float iStep) -> float;

/**
 * @brief
 *  The state of the HUD editor of a scene: preview resolution and backdrop, snapping, selection, and the gizmo
 *  gestures, which turn into `hud.*` commands.
 *
 * The document edits the canvases of a `scene::Scene` (an `.owlhud` file, or the canvases embedded in a scene); it
 * owns no scene and no undo history. A gesture previews on the scene and, when it ends, puts the scene back and runs
 * one named command, so the change is undoable, journaled and reachable from the console and the MCP server. The
 * panel draws it and feeds it the mouse; every rule lives here, which keeps it testable without a window.
 */
class HudDocument final {
public:
	HudDocument() = default;

	HudDocument(const HudDocument&) = delete;

	HudDocument(HudDocument&&) = delete;

	auto operator=(const HudDocument&) -> HudDocument& = delete;

	auto operator=(HudDocument&&) -> HudDocument& = delete;

	~HudDocument() = default;

	/// Preview resolutions offered by the editor.
	static constexpr std::array<HudResolutionPreset, 6> presets{{{"640 x 360", {640, 360}},
																 {"1280 x 720", {1280, 720}},
																 {"1920 x 1080", {1920, 1080}},
																 {"2560 x 1440", {2560, 1440}},
																 {"1024 x 768 (4:3)", {1024, 768}},
																 {"720 x 1280 (portrait)", {720, 1280}}}};
	/// Smallest side of the preview, in pixels.
	static constexpr uint32_t minResolution = 64;
	/// Largest side of the preview, in pixels.
	static constexpr uint32_t maxResolution = 8192;

	/**
	 * @brief
	 *  Get the preview size.
	 * @return The size in pixels.
	 */
	[[nodiscard]] auto getResolution() const -> const math::vec2ui& { return m_resolution; }

	/**
	 * @brief
	 *  Set the preview size, kept between `minResolution` and `maxResolution` on each side.
	 * @param[in] iResolution The size in pixels.
	 */
	void setResolution(const math::vec2ui& iResolution);

	/**
	 * @brief
	 *  Get the index of the preset matching the preview size.
	 * @return The index in `presets`, or nothing for a custom size.
	 */
	[[nodiscard]] auto findPreset() const -> std::optional<size_t>;

	/**
	 * @brief
	 *  Get the kind of backdrop.
	 * @return The kind.
	 */
	[[nodiscard]] auto getBackdropKind() const -> HudBackdropKind { return m_backdropKind; }

	/**
	 * @brief
	 *  Set the kind of backdrop.
	 * @param[in] iKind The kind.
	 */
	void setBackdropKind(const HudBackdropKind iKind) { m_backdropKind = iKind; }

	/**
	 * @brief
	 *  Get the colour of the backdrop.
	 * @return The colour (RGBA).
	 */
	[[nodiscard]] auto getBackdropColor() const -> const math::vec4& { return m_backdropColor; }

	/**
	 * @brief
	 *  Set the colour of the backdrop.
	 * @param[in] iColor The colour (RGBA).
	 */
	void setBackdropColor(const math::vec4& iColor) { m_backdropColor = iColor; }

	/**
	 * @brief
	 *  Get the image of the backdrop.
	 * @return The path relative to an asset folder, empty for none.
	 */
	[[nodiscard]] auto getBackdropImage() const -> const std::string& { return m_backdropImage; }

	/**
	 * @brief
	 *  Set the image of the backdrop.
	 * @param[in] iPath The path relative to an asset folder, empty for none.
	 */
	void setBackdropImage(const std::string& iPath) { m_backdropImage = iPath; }

	/**
	 * @brief
	 *  Get the snapping settings.
	 * @return The settings.
	 */
	[[nodiscard]] auto getSnap() const -> const HudSnapSettings& { return m_snap; }

	/**
	 * @brief
	 *  Get the snapping settings, to change them.
	 * @return The settings.
	 */
	[[nodiscard]] auto getSnap() -> HudSnapSettings& { return m_snap; }

	/**
	 * @brief
	 *  Lay the scene's HUD out at the preview size (the panel's render does it too).
	 * @param[in,out] ioScene The scene.
	 */
	void refreshLayout(scene::Scene& ioScene) const;

	// --- Selection ------------------------------------------------------------------------------------------------

	/**
	 * @brief
	 *  Get the selected widgets.
	 * @return Their UUIDs; the last one is the primary one, which carries the gizmo.
	 */
	[[nodiscard]] auto getSelection() const -> const std::vector<core::UUID>& { return m_selection; }

	/**
	 * @brief
	 *  Get the primary selected widget.
	 * @return Its UUID, 0 when nothing is selected.
	 */
	[[nodiscard]] auto getPrimary() const -> core::UUID {
		return m_selection.empty() ? core::UUID{0} : m_selection.back();
	}

	/**
	 * @brief
	 *  Check whether a widget is selected.
	 * @param[in] iEntity The widget.
	 * @return True when it is.
	 */
	[[nodiscard]] auto isSelected(core::UUID iEntity) const -> bool;

	/**
	 * @brief
	 *  Change the selection.
	 * @param[in] iEntity The widget picked (0 clears the selection in `Replace` mode).
	 * @param[in] iMode How it joins the selection.
	 */
	void select(core::UUID iEntity, HudSelectMode iMode = HudSelectMode::Replace);

	/**
	 * @brief
	 *  Make a widget the primary one: it joins the selection if it is not in, and carries the gizmo.
	 * @param[in] iEntity The widget.
	 */
	void makePrimary(core::UUID iEntity);

	/**
	 * @brief
	 *  Replace the selection.
	 * @param[in] iSelection The widgets, the primary one last.
	 */
	void setSelection(std::vector<core::UUID> iSelection) { m_selection = std::move(iSelection); }

	/**
	 * @brief
	 *  Forget the widgets that no longer exist.
	 * @param[in] iScene The scene.
	 */
	void pruneSelection(const scene::Scene& iScene);

	/**
	 * @brief
	 *  Find the widget drawn on top under a point.
	 * @param[in] iScene The scene (its layout must be up to date).
	 * @param[in] iPoint The point, in preview pixels (Y down).
	 * @return The widget, 0 when there is none.
	 */
	[[nodiscard]] static auto pickWidget(const scene::Scene& iScene, const math::vec2& iPoint) -> core::UUID;

	/**
	 * @brief
	 *  Select the widgets a rectangle touches.
	 * @param[in] iScene The scene (its layout must be up to date).
	 * @param[in] iBox The rectangle, in preview pixels.
	 * @param[in] iAdditive Keep the current selection.
	 * @return The number of widgets in the box.
	 */
	auto selectInBox(const scene::Scene& iScene, const scene::component::UiRect::Bounds& iBox, bool iAdditive)
			-> size_t;

	// --- Gizmo ----------------------------------------------------------------------------------------------------

	/**
	 * @brief
	 *  Get the rect gizmo of the primary selected widget.
	 * @param[in] iScene The scene (its layout must be up to date).
	 * @return The gizmo, or nothing when no widget is selected or it is not laid out.
	 */
	[[nodiscard]] auto getGizmo(const scene::Scene& iScene) const -> std::optional<HudGizmo>;

	/**
	 * @brief
	 *  Find the handle of the gizmo under a point.
	 * @param[in] iScene The scene (its layout must be up to date).
	 * @param[in] iPoint The point, in preview pixels.
	 * @param[in] iTolerance Grab distance in preview pixels.
	 * @param[in] iGrab Which handles are grabbed first.
	 * @return The handle; `Body` inside any selected widget, `None` elsewhere.
	 */
	[[nodiscard]] auto hitHandle(const scene::Scene& iScene, const math::vec2& iPoint, float iTolerance,
								 HudGrabMode iGrab = HudGrabMode::Normal) const -> HudHandle;

	/**
	 * @brief
	 *  Start a gesture on a handle.
	 * @param[in,out] ioScene The scene (laid out at the preview size).
	 * @param[in] iHandle The handle grabbed.
	 * @param[in] iPoint The mouse, in preview pixels.
	 * @return True when a gesture started (a handle of the selection was grabbed).
	 */
	auto beginDrag(scene::Scene& ioScene, HudHandle iHandle, const math::vec2& iPoint) -> bool;

	/**
	 * @brief
	 *  Move the mouse during a gesture: the scene previews the result.
	 * @param[in,out] ioScene The scene.
	 * @param[in] iPoint The mouse, in preview pixels.
	 * @param[in] iSnap Whether snapping applies (false while the modifier key is held).
	 */
	void updateDrag(scene::Scene& ioScene, const math::vec2& iPoint, bool iSnap);

	/**
	 * @brief
	 *  End the gesture: the scene goes back to where it was and the named command runs.
	 * @param[in,out] ioScene The scene.
	 * @param[in] iTarget The registry, scene and undo manager the command runs on.
	 * @return True when a command ran.
	 */
	auto endDrag(scene::Scene& ioScene, const commands::CommandTarget& iTarget) -> bool;

	/**
	 * @brief
	 *  Abandon the gesture: the scene goes back to where it was.
	 * @param[in,out] ioScene The scene.
	 */
	void cancelDrag(scene::Scene& ioScene);

	/**
	 * @brief
	 *  Check whether a gesture is running.
	 * @return True during a gesture.
	 */
	[[nodiscard]] auto isDragging() const -> bool { return m_drag.has_value(); }

	/**
	 * @brief
	 *  Get the handle of the running gesture.
	 * @return The handle, `None` without gesture.
	 */
	[[nodiscard]] auto getDragHandle() const -> HudHandle { return m_drag ? m_drag->handle : HudHandle::None; }

	/**
	 * @brief
	 *  Get the vertical guide lines the last snap stuck to.
	 * @return Their X positions in preview pixels.
	 */
	[[nodiscard]] auto getGuidesX() const -> const std::vector<float>& { return m_guidesX; }

	/**
	 * @brief
	 *  Get the horizontal guide lines the last snap stuck to.
	 * @return Their Y positions in preview pixels.
	 */
	[[nodiscard]] auto getGuidesY() const -> const std::vector<float>& { return m_guidesY; }

	// --- Commands -------------------------------------------------------------------------------------------------

	/**
	 * @brief
	 *  Create a widget centred on a point of the preview, in the container under it.
	 * @param[in,out] ioScene The scene (laid out at the preview size).
	 * @param[in] iTarget Where the command runs.
	 * @param[in] iKind Kind of widget (`hud.add_widget`).
	 * @param[in] iPoint The point, in preview pixels.
	 * @param[in] iTexture Texture of an `image`, relative to an asset folder.
	 * @return The widget created (and selected), or why none was.
	 */
	auto addWidget(scene::Scene& ioScene, const commands::CommandTarget& iTarget, const std::string& iKind,
				   const math::vec2& iPoint, const std::string& iTexture = {})
			-> expected<core::UUID, commands::CommandFailure>;

	/**
	 * @brief
	 *  Delete the selected widgets with their children (`hud.delete`).
	 * @param[in] iTarget Where the command runs.
	 * @return True when they were deleted.
	 */
	auto deleteSelection(const commands::CommandTarget& iTarget) -> bool;

	/**
	 * @brief
	 *  Move the selected widgets (`hud.move`).
	 * @param[in] iTarget Where the command runs.
	 * @param[in] iDelta Distance in reference pixels.
	 * @return True when they moved.
	 */
	auto moveSelection(const commands::CommandTarget& iTarget, const math::vec2& iDelta) -> bool;

	/**
	 * @brief
	 *  Put the primary widget under a container (`hud.reparent`).
	 * @param[in] iTarget Where the command runs.
	 * @param[in] iParent The canvas or widget it goes under.
	 * @param[in] iIndex Index among its children (the last one when negative).
	 * @return True when it moved.
	 */
	[[nodiscard]] auto reparentPrimary(const commands::CommandTarget& iTarget, core::UUID iParent,
									   int64_t iIndex = -1) const -> bool;

	/**
	 * @brief
	 *  Move the primary widget in its parent's children, which changes the draw order (`hud.reparent`).
	 * @param[in] iTarget Where the command runs.
	 * @param[in] iDelta Number of places: positive draws it later (on top), negative earlier.
	 * @return True when it moved.
	 */
	[[nodiscard]] auto reorderPrimary(const commands::CommandTarget& iTarget, int64_t iDelta) const -> bool;

private:
	/**
	 * @brief
	 *  A widget taking part in a gesture.
	 */
	struct DragItem {
		/// The widget.
		core::UUID entity{0};
		/// Rectangle when the gesture began.
		scene::component::UiRect rect;
		/// Screen rectangle when the gesture began.
		scene::component::UiRect::Bounds bounds;
		/// Area its parent lays it in and the canvas size, in reference pixels.
		scene::component::UiRect::Frame frame;
		/// Screen pixels per reference pixel.
		float scale{1.f};
	};

	/**
	 * @brief
	 *  A gesture in progress.
	 */
	struct Drag {
		/// Handle grabbed.
		HudHandle handle{HudHandle::None};
		/// Mouse when the gesture began.
		math::vec2 start{0.f, 0.f};
		/// Widgets moved, the primary one last.
		std::vector<DragItem> items;
		/// X positions the edges may stick to.
		std::vector<float> candidatesX;
		/// Y positions the edges may stick to.
		std::vector<float> candidatesY;
		/// Rectangle the primary widget has now.
		scene::component::UiRect current;
		/// Distance a `Body` gesture moved the widgets, in reference pixels.
		math::vec2 movedBy{0.f, 0.f};
		/// Whether the mouse moved since the start.
		bool moved{false};
	};

	/**
	 * @brief
	 *  Collect the positions the edges of the moved widgets may stick to.
	 * @param[in] iScene The scene.
	 * @param[in,out] ioDrag The gesture, which receives the candidates.
	 */
	void collectCandidates(const scene::Scene& iScene, Drag& ioDrag) const;

	/**
	 * @brief
	 *  Snap one axis of a moving rectangle.
	 * @param[in] iValues Positions of its edges on the axis, in preview pixels.
	 * @param[in] iCandidates Positions the edges may stick to.
	 * @param[in] iScale Screen pixels per reference pixel, for the grid.
	 * @param[out] oGuides Guide lines stuck to.
	 * @return The shift to apply, in preview pixels.
	 */
	[[nodiscard]] auto snapAxis(std::span<const float> iValues, std::span<const float> iCandidates, float iScale,
								std::vector<float>& oGuides) const -> float;

	/**
	 * @brief
	 *  Compute the rectangle of the primary widget for the mouse position.
	 * @param[in] iPoint The mouse, in preview pixels.
	 * @param[in] iSnap Whether snapping applies.
	 * @return The rectangle.
	 */
	[[nodiscard]] auto computeRect(const math::vec2& iPoint, bool iSnap) -> scene::component::UiRect;

	/**
	 * @brief
	 *  Write the rectangles the scene shows during a gesture.
	 * @param[in,out] ioScene The scene.
	 * @param[in] iPoint The mouse, in preview pixels.
	 * @param[in] iSnap Whether snapping applies.
	 */
	void applyPreview(scene::Scene& ioScene, const math::vec2& iPoint, bool iSnap);

	/**
	 * @brief
	 *  Put the rectangles back as they were when the gesture began.
	 * @param[in,out] ioScene The scene.
	 */
	void restoreRects(scene::Scene& ioScene) const;

	/// Preview size in pixels.
	math::vec2ui m_resolution{1280, 720};
	/// What lies behind the HUD.
	HudBackdropKind m_backdropKind{HudBackdropKind::Color};
	/// Colour of the backdrop.
	math::vec4 m_backdropColor{0.18f, 0.2f, 0.24f, 1.f};
	/// Image of the backdrop.
	std::string m_backdropImage;
	/// Snapping.
	HudSnapSettings m_snap;
	/// Selected widgets, the primary one last.
	std::vector<core::UUID> m_selection;
	/// Gesture in progress.
	std::optional<Drag> m_drag;
	/// Vertical guides of the last snap.
	std::vector<float> m_guidesX;
	/// Horizontal guides of the last snap.
	std::vector<float> m_guidesY;
};

}// namespace owl::nest
