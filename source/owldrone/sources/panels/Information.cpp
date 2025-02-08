/**
 * @file Information.cpp
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "Information.h"
#include <imgui.h>

namespace drone::panels {

Information::Information() = default;

Information::~Information() = default;

void Information::onUpdate(const owl::core::Timestep&) {}

void Information::onRender() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	ImGui::Begin("Drone Misc");

	ImGui::End();
	ImGui::PopStyleVar();
}

}// namespace drone::panels
