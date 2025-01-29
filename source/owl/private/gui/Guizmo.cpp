/**
 * @file Guizmo.cpp
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/Guizmo.h"

#include <imgui.h>

#include <ImGuizmo.h>

namespace owl::gui {

void Guizmo::initialize(const math::vec2& iPosition, const math::vec2& iSize) {
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();

	ImGuizmo::SetRect(iPosition.x(), iPosition.y(), iSize.x(), iSize.y());
}

auto Guizmo::isUsing() -> bool { return ImGuizmo::IsUsing(); }
auto Guizmo::isOver() -> bool { return ImGuizmo::IsOver(); }
auto Guizmo::isRotate(const Type& iType) -> bool { return static_cast<uint16_t>(iType) == ImGuizmo::OPERATION::ROTATE; }

void Guizmo::manipulate(const math::mat4& iCameraView, const math::mat4& iCameraProjection, const Type& iType,
						math::mat4& ioTransform, const float& iSnap) {
	const float snapValues[3] = {iSnap, iSnap, iSnap};
	Manipulate(iCameraView.data(), iCameraProjection.data(), static_cast<ImGuizmo::OPERATION>(iType), ImGuizmo::LOCAL,
			   ioTransform.data(), nullptr, iSnap > 0 ? snapValues : nullptr);
}

}// namespace owl::gui
