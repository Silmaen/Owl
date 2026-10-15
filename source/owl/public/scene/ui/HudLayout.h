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
#include "scene/component/UiRect.h"

#include <entt/entt.hpp>

#include <cstddef>
#include <cstdint>
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
	/// Value of `parent` for a widget whose parent is the canvas.
	static constexpr size_t noParent = static_cast<size_t>(-1);
};

/**
 * @brief
 *  Layout of every visible HUD widget of a scene, computed once per frame and shared by the HUD pass (drawing) and
 *  `UiInputSystem` (hit-testing), so a click lands on the rectangle that was drawn.
 *
 * Canvases are taken in ascending `sortOrder`; under each one the widgets are listed depth first, a parent before its
 * children, children in hierarchy order: the list is the draw order, the last widget is drawn on top. An entity
 * without `UiRect` under a canvas passes its parent rectangle on to its children. Hidden entities hide their subtree.
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
	 *  Find the topmost widget under a point.
	 * @param[in] iPoint The point, in Y-down viewport pixels.
	 * @return The node drawn last among those containing the point, or nullptr.
	 */
	[[nodiscard]] auto hitTest(const math::vec2& iPoint) const -> const HudNode*;

	/**
	 * @brief
	 *  Get the number of layout passes run since the creation (a spy for the once-per-frame contract).
	 * @return The number of `compute` calls.
	 */
	[[nodiscard]] auto getComputeCount() const -> uint64_t { return m_computeCount; }

private:
	/**
	 * @brief
	 *  One entity waiting in the depth-first walk.
	 */
	struct PendingEntity {
		/// The entity.
		entt::entity entity = entt::null;
		/// Index of the nearest widget above it (`HudNode::noParent` for the canvas).
		size_t parent = HudNode::noParent;
	};

	/**
	 * @brief
	 *  Lay out the widgets of one canvas.
	 * @param[in] iScene The scene.
	 * @param[in] iCanvas The canvas entity.
	 * @param[in] iEditorMode Use the editor visibility.
	 */
	void layoutCanvas(Scene& iScene, entt::entity iCanvas, bool iEditorMode);

	/// Laid out widgets, in draw order.
	std::vector<HudNode> m_nodes;
	/// Canvases of the current pass with their sort order (kept to reuse its storage).
	std::vector<std::pair<int32_t, entt::entity>> m_canvases;
	/// Depth-first walk stack (kept to reuse its storage).
	std::vector<PendingEntity> m_pending;
	/// Viewport of the last `compute`.
	math::vec2ui m_viewport{0, 0};
	/// Whether the layout matches `m_viewport` (cleared by `invalidate`).
	bool m_valid = false;
	/// Number of `compute` calls.
	uint64_t m_computeCount = 0;
};

}// namespace owl::scene::ui
