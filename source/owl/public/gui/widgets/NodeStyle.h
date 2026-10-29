/**
 * @file NodeStyle.h
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

struct ImDrawList;

namespace owl::gui::widgets {

/**
 * @brief
 *  Look of a node frame.
 */
struct NodeFrameStyle {
	/// Colour of the title bar.
	math::vec4 header{0.3f, 0.31f, 0.35f, 1.f};
	/// Height of the title bar, in canvas units.
	float headerHeight{30.f};
	/// Whether the node is selected (accent frame and halo).
	bool selected{false};
	/// Whether the mouse is over the node.
	bool hovered{false};
	/// Whether the node has an error (red frame).
	bool faulty{false};
};

/**
 * @brief
 *  Background colour of a node canvas.
 * @return The colour, the same in every theme.
 */
[[nodiscard]] OWL_API auto getNodeCanvasBackground() -> math::vec4;

/**
 * @brief
 *  Draw the grid of a node canvas: a line every 16 units, a stronger one every 128.
 * @param[in,out] ioDraw Draw list.
 * @param[in] iOrigin Top-left corner of the view on the screen.
 * @param[in] iSize Size of the view.
 * @param[in] iOffset Pan, in canvas units.
 * @param[in] iZoom Zoom factor.
 */
OWL_API void drawNodeGrid(ImDrawList* ioDraw, const math::vec2f& iOrigin, const math::vec2f& iSize,
						  const std::array<float, 2>& iOffset, float iZoom);

/**
 * @brief
 *  Draw the shadow, body, title bar and border of a node.
 * @param[in,out] ioDraw Draw list.
 * @param[in] iMin Top-left corner on the screen.
 * @param[in] iMax Bottom-right corner on the screen.
 * @param[in] iStyle Look of the node.
 * @param[in] iZoom Zoom factor.
 */
OWL_API void drawNodeFrame(ImDrawList* ioDraw, const math::vec2f& iMin, const math::vec2f& iMax,
						   const NodeFrameStyle& iStyle, float iZoom);

/**
 * @brief
 *  Draw an execution pin: an arrow, filled when linked.
 * @param[in,out] ioDraw Draw list.
 * @param[in] iCenter Centre on the screen.
 * @param[in] iSize Half height, in pixels.
 * @param[in] iColor Colour.
 * @param[in] iFilled Whether the pin is linked.
 */
OWL_API void drawExecPin(ImDrawList* ioDraw, const math::vec2f& iCenter, float iSize, const math::vec4& iColor,
						 bool iFilled);

/**
 * @brief
 *  Draw a value pin: a circle with a small arrow, filled when linked.
 * @param[in,out] ioDraw Draw list.
 * @param[in] iCenter Centre on the screen.
 * @param[in] iRadius Radius, in pixels.
 * @param[in] iColor Colour.
 * @param[in] iFilled Whether the pin is linked.
 */
OWL_API void drawDataPin(ImDrawList* ioDraw, const math::vec2f& iCenter, float iRadius, const math::vec4& iColor,
						 bool iFilled);

/**
 * @brief
 *  Draw a link from an output pin to an input pin, with a dark outline.
 * @param[in,out] ioDraw Draw list.
 * @param[in] iFrom Output pin on the screen.
 * @param[in] iTo Input pin on the screen.
 * @param[in] iColor Colour.
 * @param[in] iThickness Thickness, in pixels.
 * @param[in] iZoom Zoom factor.
 */
OWL_API void drawNodeLink(ImDrawList* ioDraw, const math::vec2f& iFrom, const math::vec2f& iTo,
						  const math::vec4& iColor, float iThickness, float iZoom);

/**
 * @brief
 *  Control points of a link curve: horizontal tangents, longer when the link runs backwards.
 * @param[in] iFrom Output pin on the screen.
 * @param[in] iTo Input pin on the screen.
 * @param[in] iZoom Zoom factor.
 * @return The four points of the cubic curve.
 */
[[nodiscard]] OWL_API auto getNodeLinkCurve(const math::vec2f& iFrom, const math::vec2f& iTo, float iZoom)
		-> std::array<math::vec2f, 4>;

/**
 * @brief
 *  Distance from a point to a link curve.
 * @param[in] iFrom Output pin on the screen.
 * @param[in] iTo Input pin on the screen.
 * @param[in] iPoint The point.
 * @param[in] iZoom Zoom factor.
 * @return The distance, in pixels.
 */
[[nodiscard]] OWL_API auto getNodeLinkDistance(const math::vec2f& iFrom, const math::vec2f& iTo,
											   const math::vec2f& iPoint, float iZoom) -> float;

/**
 * @brief
 *  Draw a link routed through waypoints: one cubic segment between each pair of consecutive points.
 * @param[in,out] ioDraw Draw list.
 * @param[in] iFrom Output pin on the screen.
 * @param[in] iTo Input pin on the screen.
 * @param[in] iWaypoints Intermediate points on the screen, from the output to the input.
 * @param[in] iColor Colour.
 * @param[in] iThickness Thickness, in pixels.
 * @param[in] iZoom Zoom factor.
 */
OWL_API void drawNodeLink(ImDrawList* ioDraw, const math::vec2f& iFrom, const math::vec2f& iTo,
						  std::span<const math::vec2f> iWaypoints, const math::vec4& iColor, float iThickness,
						  float iZoom);

/**
 * @brief
 *  Cubic segments of a link routed through waypoints, each one shaped like `getNodeLinkCurve`.
 * @param[in] iFrom Output pin on the screen.
 * @param[in] iTo Input pin on the screen.
 * @param[in] iWaypoints Intermediate points on the screen, from the output to the input.
 * @param[in] iZoom Zoom factor.
 * @return One segment more than there are waypoints.
 */
[[nodiscard]] OWL_API auto getNodeLinkSegments(const math::vec2f& iFrom, const math::vec2f& iTo,
											   std::span<const math::vec2f> iWaypoints, float iZoom)
		-> std::vector<std::array<math::vec2f, 4>>;

/**
 * @brief
 *  Distance from a point to a link routed through waypoints.
 * @param[in] iFrom Output pin on the screen.
 * @param[in] iTo Input pin on the screen.
 * @param[in] iWaypoints Intermediate points on the screen, from the output to the input.
 * @param[in] iPoint The point.
 * @param[in] iZoom Zoom factor.
 * @return The distance to the closest segment, in pixels.
 */
[[nodiscard]] OWL_API auto getNodeLinkDistance(const math::vec2f& iFrom, const math::vec2f& iTo,
											   std::span<const math::vec2f> iWaypoints, const math::vec2f& iPoint,
											   float iZoom) -> float;

/**
 * @brief
 *  Where a waypoint inserted at a point goes in the waypoint list of a link.
 * @param[in] iFrom Output pin on the screen.
 * @param[in] iTo Input pin on the screen.
 * @param[in] iWaypoints Intermediate points on the screen, from the output to the input.
 * @param[in] iPoint The point, near the link.
 * @param[in] iZoom Zoom factor.
 * @return The index of the segment closest to the point, which is the insertion index in `iWaypoints`.
 */
[[nodiscard]] OWL_API auto getNodeLinkInsertIndex(const math::vec2f& iFrom, const math::vec2f& iTo,
												  std::span<const math::vec2f> iWaypoints, const math::vec2f& iPoint,
												  float iZoom) -> size_t;

/**
 * @brief
 *  Draw the handle of a link waypoint.
 * @param[in,out] ioDraw Draw list.
 * @param[in] iCenter Centre on the screen.
 * @param[in] iRadius Radius, in pixels.
 * @param[in] iColor Colour of the link.
 * @param[in] iHovered Whether the mouse is over it (accent ring).
 */
OWL_API void drawNodeLinkWaypoint(ImDrawList* ioDraw, const math::vec2f& iCenter, float iRadius,
								  const math::vec4& iColor, bool iHovered);

}// namespace owl::gui::widgets
