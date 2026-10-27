/**
 * @file NodeStyle.cpp
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/widgets/NodeStyle.h"

#include "gui/UiLayer.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <cmath>

namespace owl::gui::widgets {

namespace {

constexpr float g_rounding = 8.f;
constexpr float g_gridStep = 16.f;
constexpr int g_gridMajor = 8;

auto toIm(const math::vec2f& iPoint) -> ImVec2 { return {iPoint.x(), iPoint.y()}; }

auto toU32(const math::vec4& iColor, const float iAlpha = 1.f) -> ImU32 {
	return ImGui::ColorConvertFloat4ToU32({iColor.x(), iColor.y(), iColor.z(), iColor.w() * iAlpha});
}

auto scaled(const math::vec4& iColor, const float iFactor) -> math::vec4 {
	return {iColor.x() * iFactor, iColor.y() * iFactor, iColor.z() * iFactor, iColor.w()};
}

auto lighter(const math::vec4& iColor, const float iAmount) -> math::vec4 {
	return {iColor.x() + (1.f - iColor.x()) * iAmount, iColor.y() + (1.f - iColor.y()) * iAmount,
			iColor.z() + (1.f - iColor.z()) * iAmount, iColor.w()};
}

}// namespace

auto getNodeCanvasBackground() -> math::vec4 { return {0.102f, 0.11f, 0.125f, 1.f}; }

void drawNodeGrid(ImDrawList* ioDraw, const math::vec2f& iOrigin, const math::vec2f& iSize,
				  const std::array<float, 2>& iOffset, const float iZoom) {
	const float step = g_gridStep * iZoom;
	if (step < 6.f)
		return;
	const auto lines = [&](const float iStart, const float iEnd, const float iPan, const bool iVertical) -> void {
		const float first = iStart + std::fmod(iPan * iZoom, step) - step;
		const auto base = static_cast<int>(std::floor(-iPan * iZoom / step));
		const auto count = static_cast<int>((iEnd - first) / step) + 1;
		for (int line = 0; line < count; ++line) {
			const float at = first + static_cast<float>(line) * step;
			const ImU32 color =
					(base + line) % g_gridMajor == 0 ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 9);
			if (iVertical)
				ioDraw->AddLine({at, iOrigin.y()}, {at, iOrigin.y() + iSize.y()}, color);
			else
				ioDraw->AddLine({iOrigin.x(), at}, {iOrigin.x() + iSize.x(), at}, color);
		}
	};
	lines(iOrigin.x(), iOrigin.x() + iSize.x(), iOffset[0], true);
	lines(iOrigin.y(), iOrigin.y() + iSize.y(), iOffset[1], false);
}

void drawNodeFrame(ImDrawList* ioDraw, const math::vec2f& iMin, const math::vec2f& iMax, const NodeFrameStyle& iStyle,
				   const float iZoom) {
	const auto& theme = UiLayer::getTheme();
	const ImVec2 min = toIm(iMin);
	const ImVec2 max = toIm(iMax);
	const float rounding = g_rounding * iZoom;
	const float shadow = 6.f * iZoom;
	ioDraw->AddRectFilled({min.x + shadow * 0.3f, min.y + shadow * 0.6f}, {max.x + shadow, max.y + shadow},
						  IM_COL32(0, 0, 0, 70), rounding + shadow * 0.5f);
	ioDraw->AddRectFilled(min, max, IM_COL32(20, 22, 26, 238), rounding);
	const float header = iStyle.headerHeight * iZoom;
	const int vertexStart = ioDraw->VtxBuffer.Size;
	ioDraw->AddRectFilled(min, {max.x, min.y + header}, toU32(iStyle.header), rounding, ImDrawFlags_RoundCornersTop);
	ImGui::ShadeVertsLinearColorGradientKeepAlpha(ioDraw, vertexStart, ioDraw->VtxBuffer.Size, min, {max.x, min.y},
												  toU32(lighter(iStyle.header, 0.12f)),
												  toU32(scaled(iStyle.header, 0.45f)));
	ioDraw->AddLine({min.x + 1.f, min.y + header}, {max.x - 1.f, min.y + header},
					toU32(lighter(iStyle.header, 0.3f), 0.6f), std::max(1.f, iZoom));
	ImU32 border = IM_COL32(0, 0, 0, 160);
	float borderWidth = std::max(1.f, iZoom);
	if (iStyle.faulty) {
		border = toU32(theme.error);
		borderWidth = 2.f * iZoom;
	}
	if (iStyle.selected) {
		ioDraw->AddRect({min.x - 3.f * iZoom, min.y - 3.f * iZoom}, {max.x + 3.f * iZoom, max.y + 3.f * iZoom},
						toU32(theme.accent, 0.35f), rounding + 3.f * iZoom, 0, 4.f * iZoom);
		border = toU32(theme.accent);
		borderWidth = 2.f * iZoom;
	} else if (iStyle.hovered && !iStyle.faulty) {
		border = IM_COL32(255, 255, 255, 70);
	}
	ioDraw->AddRect(min, max, border, rounding, 0, borderWidth);
}

void drawExecPin(ImDrawList* ioDraw, const math::vec2f& iCenter, const float iSize, const math::vec4& iColor,
				 const bool iFilled) {
	const float x = iCenter.x();
	const float y = iCenter.y();
	const std::array<ImVec2, 5> points{ImVec2{x - iSize, y - iSize}, ImVec2{x + 0.15f * iSize, y - iSize},
									   ImVec2{x + iSize, y}, ImVec2{x + 0.15f * iSize, y + iSize},
									   ImVec2{x - iSize, y + iSize}};
	if (iFilled)
		ioDraw->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), toU32(iColor));
	else
		ioDraw->AddPolyline(points.data(), static_cast<int>(points.size()), toU32(iColor), ImDrawFlags_Closed,
							std::max(1.f, iSize * 0.3f));
}

void drawDataPin(ImDrawList* ioDraw, const math::vec2f& iCenter, const float iRadius, const math::vec4& iColor,
				 const bool iFilled) {
	const ImVec2 center = toIm(iCenter);
	if (iFilled)
		ioDraw->AddCircleFilled(center, iRadius, toU32(iColor));
	else
		ioDraw->AddCircle(center, iRadius, toU32(iColor), 0, std::max(1.f, iRadius * 0.32f));
	const float tip = iRadius * 0.55f;
	ioDraw->AddTriangleFilled({center.x + iRadius + tip * 0.5f, center.y - tip},
							  {center.x + iRadius + tip * 1.6f, center.y},
							  {center.x + iRadius + tip * 0.5f, center.y + tip}, toU32(iColor));
}

auto getNodeLinkCurve(const math::vec2f& iFrom, const math::vec2f& iTo, const float iZoom)
		-> std::array<math::vec2f, 4> {
	const float dx = iTo.x() - iFrom.x();
	const float control = std::max(std::abs(dx) * 0.5f, (dx < 0.f ? 90.f : 40.f) * iZoom);
	return {iFrom, math::vec2f{iFrom.x() + control, iFrom.y()}, math::vec2f{iTo.x() - control, iTo.y()}, iTo};
}

void drawNodeLink(ImDrawList* ioDraw, const math::vec2f& iFrom, const math::vec2f& iTo, const math::vec4& iColor,
				  const float iThickness, const float iZoom) {
	const auto curve = getNodeLinkCurve(iFrom, iTo, iZoom);
	ioDraw->AddBezierCubic(toIm(curve[0]), toIm(curve[1]), toIm(curve[2]), toIm(curve[3]), IM_COL32(0, 0, 0, 90),
						   iThickness + 2.f * iZoom);
	ioDraw->AddBezierCubic(toIm(curve[0]), toIm(curve[1]), toIm(curve[2]), toIm(curve[3]), toU32(iColor), iThickness);
}

auto getNodeLinkDistance(const math::vec2f& iFrom, const math::vec2f& iTo, const math::vec2f& iPoint, const float iZoom)
		-> float {
	const auto curve = getNodeLinkCurve(iFrom, iTo, iZoom);
	const auto closest = ImBezierCubicClosestPointCasteljau(toIm(curve[0]), toIm(curve[1]), toIm(curve[2]),
															toIm(curve[3]), toIm(iPoint), 1.25f);
	const float dx = closest.x - iPoint.x();
	const float dy = closest.y - iPoint.y();
	return std::sqrt(dx * dx + dy * dy);
}

}// namespace owl::gui::widgets
