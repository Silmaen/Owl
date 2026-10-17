/**
 * @file HudLayout.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "scene/component/UiLayout.h"
#include "scene/component/UiRect.h"
#include "scene/ui/ScrollMath.h"

#include <entt/entt.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace owl::scene {
class Scene;
}// namespace owl::scene

/**
 * @brief
 *  HUD: layout of the `UiRect` widgets of the canvases and the pixel pass drawing them.
 */
namespace owl::scene::ui {

/**
 * @brief
 *  One laid out widget: its screen rectangle, in Y-down pixels with the origin at the top left of the viewport.
 */
struct OWL_API HudNode {
	/// Entity carrying the `UiRect`.
	entt::entity entity = entt::null;
	/// Canvas the widget belongs to.
	entt::entity canvas = entt::null;
	/// Index of the parent widget in the layout (`noParent` for a direct child of the canvas).
	size_t parent = noParent;
	/// Screen rectangle in pixels.
	component::UiRect::Bounds bounds;
	/// Screen pixels per reference pixel (the canvas scale).
	float scale = 1.f;
	/// Area the children are laid out in, in screen pixels: the bounds, minus the title bar of a `UiWindow`.
	component::UiRect::Bounds content;
	/// Rectangle the widget is cut to (an ancestor window content or clipping panel); only meaningful when `clipped`.
	component::UiRect::Bounds clip;
	/// Whether the widget is cut to `clip`.
	bool clipped = false;
	/// Whether the children are cut to `content` (a window, a panel with `clipChildren`).
	bool clipsChildren = false;
	/// Index of the nearest window above the widget, itself included (`noParent` when it is in none).
	size_t window = noParent;
	/// Distance the children are scrolled by (a `UiScrollView`), in screen pixels; zero for any other widget.
	math::vec2 scrollOffset{0.f, 0.f};
	/// Index of the widget in `HudLayout::getScrolls()` (a `UiScrollView` or a `UiList`), `noParent` for none.
	size_t scroll = noParent;
	/// Index of the nearest `UiScrollView` or `UiList` above the widget, `noParent` when it is in none.
	size_t scrollAncestor = noParent;
	/// Value of `parent`, `window`, `scroll` and `scrollAncestor` for a widget with none.
	static constexpr size_t noParent = static_cast<size_t>(-1);

	/**
	 * @brief
	 *  Get the rectangle the children are placed in: the content area, scrolled.
	 * @return The rectangle, in screen pixels.
	 */
	[[nodiscard]] auto childArea() const -> component::UiRect::Bounds {
		return {.position = content.position - scrollOffset, .size = content.size};
	}

	/**
	 * @brief
	 *  Check whether a point reaches the widget: inside its bounds and inside its clip rectangle.
	 * @param[in] iPoint The point, in Y-down viewport pixels.
	 * @return True when the widget is visible there.
	 */
	[[nodiscard]] auto isHit(const math::vec2& iPoint) const -> bool {
		return bounds.contains(iPoint) && (!clipped || clip.contains(iPoint));
	}
};

/**
 * @brief
 *  A laid out scroll container (`UiScrollView` or `UiList`): its viewport, its bars and where it is scrolled to.
 */
struct OWL_API HudScroll {
	/// Entity carrying the component.
	entt::entity entity = entt::null;
	/// Viewport, content and bars, in screen pixels.
	ScrollGeometry geometry;
	/// Scroll offset in screen pixels, kept inside the content (x is 0 when the container does not scroll sideways).
	math::vec2 offset{0.f, 0.f};
	/// Screen pixels per reference pixel (the canvas scale).
	float scale = 1.f;
	/// Whether the container is a `UiList` (its rows are drawn by the HUD pass).
	bool isList = false;
	/// Height of a list row, in screen pixels.
	float rowHeight = 0.f;
	/// Space between two list rows, in screen pixels.
	float rowSpacing = 0.f;
	/// Number of list rows.
	size_t rowCount = 0;
	/// Colour of the bar tracks.
	math::vec4 trackColor{1.f, 1.f, 1.f, 0.08f};
	/// Colour of the bar thumbs.
	math::vec4 thumbColor{1.f, 1.f, 1.f, 0.45f};
	/// Shortest thumb, in screen pixels.
	float minThumb = 16.f;

	/**
	 * @brief
	 *  Get the thumb of the vertical bar.
	 * @return The thumb rectangle in screen pixels (empty without the bar).
	 */
	[[nodiscard]] auto verticalThumb() const -> component::UiRect::Bounds;

	/**
	 * @brief
	 *  Get the thumb of the horizontal bar.
	 * @return The thumb rectangle in screen pixels (empty without the bar).
	 */
	[[nodiscard]] auto horizontalThumb() const -> component::UiRect::Bounds;
};

/**
 * @brief
 *  Layout of every visible HUD widget of a scene, computed once per frame and shared by the HUD pass (drawing) and
 *  `UiInputSystem` (hit-testing), so a click lands on the rectangle that was drawn.
 *
 * Canvases are taken in ascending `sortOrder`; under each one the widgets are listed depth first, a parent before its
 * children, children in hierarchy order: the list is the draw order, the last widget is drawn on top. An entity
 * without `UiRect` under a canvas passes its parent rectangle on to its children. Hidden entities hide their subtree.
 *
 * The children of a `UiWindow` are laid out in the area under its title bar, and a window stays inside the screen. A
 * widget carrying a `UiLayout` places its children itself. Windows, panels with `clipChildren`, scroll views and lists
 * give their descendants a clip rectangle; the children of a scroll view are shifted by its offset, which the layout
 * keeps inside the content (and writes back to the component).
 */
class OWL_API HudLayout final {
public:
	/**
	 * @brief
	 *  Default constructor: an empty layout, computed for no viewport.
	 */
	HudLayout() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	HudLayout(const HudLayout&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	HudLayout(HudLayout&&) = default;

	/**
	 * @brief
	 *  Copy assignment.
	 * @return This layout.
	 */
	auto operator=(const HudLayout&) -> HudLayout& = default;

	/**
	 * @brief
	 *  Move assignment.
	 * @return This layout.
	 */
	auto operator=(HudLayout&&) -> HudLayout& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~HudLayout() = default;

	/**
	 * @brief
	 *  Lay out the HUD of a scene for a viewport.
	 * @param[in] iScene The scene.
	 * @param[in] iViewport The viewport size in pixels (nothing is laid out when it is empty).
	 * @param[in] iEditorMode Use the editor visibility instead of the game one.
	 */
	void compute(Scene& iScene, const math::vec2ui& iViewport, bool iEditorMode);

	/**
	 * @brief
	 *  Forget the layout: the next `isComputedFor` returns false.
	 */
	void invalidate();

	/**
	 * @brief
	 *  Check whether the layout was computed for a viewport and not invalidated since.
	 * @param[in] iViewport The viewport size in pixels.
	 * @return True when the layout can be used for that viewport.
	 */
	[[nodiscard]] auto isComputedFor(const math::vec2ui& iViewport) const -> bool;

	/**
	 * @brief
	 *  Get the laid out widgets, in draw order.
	 * @return The widgets.
	 */
	[[nodiscard]] auto getNodes() const -> std::span<const HudNode> { return m_nodes; }

	/**
	 * @brief
	 *  Find the layout of a widget.
	 * @param[in] iEntity The widget entity.
	 * @return The node, or nullptr when the entity is not a visible widget.
	 */
	[[nodiscard]] auto findNode(entt::entity iEntity) const -> const HudNode*;

	/**
	 * @brief
	 *  Get the scroll containers, one per `UiScrollView` or `UiList` laid out.
	 * @return The containers, indexed by `HudNode::scroll`.
	 */
	[[nodiscard]] auto getScrolls() const -> std::span<const HudScroll> { return m_scrolls; }

	/**
	 * @brief
	 *  Find the topmost widget under a point.
	 * @param[in] iPoint The point, in Y-down viewport pixels.
	 * @return The node drawn last among those containing the point, or nullptr.
	 */
	[[nodiscard]] auto hitTest(const math::vec2& iPoint) const -> const HudNode*;

	/**
	 * @brief
	 *  Get the viewport of the last `compute`.
	 * @return The viewport size in pixels.
	 */
	[[nodiscard]] auto getViewport() const -> const math::vec2ui& { return m_viewport; }

	/**
	 * @brief
	 *  Get the number of layout passes run since the creation (a spy for the once-per-frame contract).
	 * @return The number of `compute` calls.
	 */
	[[nodiscard]] auto getComputeCount() const -> uint64_t { return m_computeCount; }

private:
	/// The component storages of a scene, looked up once per `compute` instead of once per widget.
	struct Pools;

	/**
	 * @brief
	 *  One entity waiting in the depth-first walk.
	 */
	struct PendingEntity {
		/// The entity.
		entt::entity entity = entt::null;
		/// Index of the nearest widget above it (`HudNode::noParent` for the canvas).
		size_t parent = HudNode::noParent;
		/// Whether `slot` places the widget (its parent carries a `UiLayout`).
		bool slotted = false;
		/// Top-left corner in the parent content area, in reference pixels.
		math::vec2 slot{0.f, 0.f};
	};

	/**
	 * @brief
	 *  One child waiting for its cell in a `UiLayout`.
	 */
	struct LayoutChild {
		/// The child entity.
		entt::entity entity = entt::null;
		/// Its size in reference pixels.
		math::vec2 size{0.f, 0.f};
		/// Whether the layout places it (a visible widget with a `UiRect`).
		bool placed = false;
	};

	/**
	 * @brief
	 *  Queue the children of a widget for the walk, placed by its `UiLayout` when it has one.
	 * @param[in] iScene The scene.
	 * @param[in] iPools The component storages of the scene.
	 * @param[in] iEntity The widget (or canvas) entity.
	 * @param[in] iParent Index of the widget in the layout (`HudNode::noParent` for a canvas).
	 * @param[in] iEditorMode Use the editor visibility.
	 */
	void pushChildren(Scene& iScene, const Pools& iPools, entt::entity iEntity, size_t iParent, bool iEditorMode);

	/**
	 * @brief
	 *  Compute the cells of the children of a `UiLayout`.
	 * @param[in] iLayout The arrangement.
	 * @param[in] iContent Size of the content area, in reference pixels.
	 * @return The extent of the placed children plus the padding, in reference pixels.
	 *
	 * Fills `m_slots`, one cell per child of `m_layoutChildren`.
	 */
	auto arrange(const component::UiLayout& iLayout, const math::vec2& iContent) -> math::vec2;

	/**
	 * @brief
	 *  List the children of a widget that a `UiLayout` of it would place, in `m_layoutChildren`.
	 * @param[in] iScene The scene.
	 * @param[in] iPools The component storages of the scene.
	 * @param[in] iEntity The widget entity.
	 * @param[in] iContent Size of the content area, in reference pixels (the parent of the ratio units).
	 * @param[in] iEditorMode Use the editor visibility.
	 */
	void collectLayoutChildren(Scene& iScene, const Pools& iPools, entt::entity iEntity, const math::vec2& iContent,
							   bool iEditorMode);

	/**
	 * @brief
	 *  Measure the children a `UiLayout` of a widget places.
	 * @param[in] iScene The scene.
	 * @param[in] iPools The component storages of the scene.
	 * @param[in] iEntity The widget entity.
	 * @param[in] iContent Size of the content area, in reference pixels.
	 * @param[in] iEditorMode Use the editor visibility.
	 * @return The extent in reference pixels, nothing when the widget has no `UiLayout`.
	 */
	auto measureLayout(Scene& iScene, const Pools& iPools, entt::entity iEntity, const math::vec2& iContent,
					   bool iEditorMode) -> std::optional<math::vec2>;

	/**
	 * @brief
	 *  Turn a widget into a scroll container when it carries a `UiScrollView` or a `UiList`.
	 * @param[in] iScene The scene.
	 * @param[in] iPools The component storages of the scene.
	 * @param[in,out] ioNode The widget, its content area and clipping completed.
	 * @param[in] iEditorMode Use the editor visibility.
	 */
	void attachScroll(Scene& iScene, const Pools& iPools, HudNode& ioNode, bool iEditorMode);

	/**
	 * @brief
	 *  Lay out the widgets of one canvas.
	 * @param[in] iScene The scene.
	 * @param[in] iPools The component storages of the scene.
	 * @param[in] iCanvas The canvas entity.
	 * @param[in] iEditorMode Use the editor visibility.
	 */
	void layoutCanvas(Scene& iScene, const Pools& iPools, entt::entity iCanvas, bool iEditorMode);

	/// Laid out widgets, in draw order.
	std::vector<HudNode> m_nodes;
	/// Canvases of the current pass with their sort order (kept to reuse its storage).
	std::vector<std::pair<int32_t, entt::entity>> m_canvases;
	/// Depth-first walk stack (kept to reuse its storage).
	std::vector<PendingEntity> m_pending;
	/// Children of the layout being arranged (kept to reuse its storage).
	std::vector<LayoutChild> m_layoutChildren;
	/// Cells of `m_layoutChildren`, in reference pixels (kept to reuse its storage).
	std::vector<math::vec2> m_slots;
	/// Scroll containers, in draw order.
	std::vector<HudScroll> m_scrolls;
	/// Viewport of the last `compute`.
	math::vec2ui m_viewport{0, 0};
	/// Size of the canvas being laid out, in reference pixels (what the `Screen*` units are relative to).
	math::vec2 m_screenReference{0.f, 0.f};
	/// Whether the layout matches `m_viewport` (cleared by `invalidate`).
	bool m_valid = false;
	/// Number of `compute` calls.
	uint64_t m_computeCount = 0;
};

}// namespace owl::scene::ui
