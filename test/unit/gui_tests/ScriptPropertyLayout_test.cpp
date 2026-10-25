/**
 * @file ScriptPropertyLayout_test.cpp
 * @author Silmaen
 * @date 25/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Macros.h>
#include <gui/UiLayer.h>
#include <gui/component/render.h>
#include <gui/widgets/SectionHeader.h>
#include <scene/component/components.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

using namespace owl;
using script::ScriptProperty;
using script::ScriptPropertyType;

namespace {

auto propertyOf(const ScriptPropertyType iType) -> ScriptProperty {
	ScriptProperty property{.name = "a_rather_long_property_name", .type = iType};
	switch (iType) {
		case ScriptPropertyType::Float:
			property.value = 2.f;
			break;
		case ScriptPropertyType::Int:
			property.value = int64_t{3};
			break;
		case ScriptPropertyType::String:
			property.value = std::string{"A sentence long enough to fill the field and more."};
			break;
		case ScriptPropertyType::Bool:
			property.value = true;
			break;
		case ScriptPropertyType::Color:
			property.value = std::array{1.f, 0.5f, 0.25f, 1.f};
			break;
		case ScriptPropertyType::Vec2:
			property.value = std::array{1.f, 2.f};
			break;
		case ScriptPropertyType::Vec3:
			property.value = std::array{1.f, 2.f, 3.f};
			break;
		case ScriptPropertyType::Entity:
			property.value = uint64_t{0};
			break;
		case ScriptPropertyType::Numbers:
			property.value = std::vector{1.0, 2.0};
			break;
		case ScriptPropertyType::Strings:
			property.value = std::vector<std::string>{"one", "two"};
			break;
	}
	property.defaultValue = property.value;
	return property;
}

}// namespace

TEST(ScriptPropertyLayout, EveryPropertyRowFitsTheInspectorWidth) {
	core::Log::init(core::Log::Level::Off);
	gui::UiLayer layer;
	layer.disableApp();
	layer.onAttach();
	ImGui::GetIO().IniFilename = nullptr;
	constexpr std::array types{ScriptPropertyType::Float,  ScriptPropertyType::Int,    ScriptPropertyType::String,
							   ScriptPropertyType::Bool,   ScriptPropertyType::Color,  ScriptPropertyType::Vec2,
							   ScriptPropertyType::Vec3,   ScriptPropertyType::Entity, ScriptPropertyType::Numbers,
							   ScriptPropertyType::Strings};
	for (const float width: {420.f, 600.f}) {
		for (const auto type: types) {
			scene::component::LuaScript script;
			script.scriptPath = "missing.lua";
			script.properties.push_back(propertyOf(type));
			for (int frame = 0; frame < 3; ++frame) {
				layer.begin();
				ImGui::SetNextWindowPos({0.f, 0.f});
				ImGui::SetNextWindowSize({width, 120.f});
				ImGui::Begin(std::format("Inspector {} {}", width, static_cast<int>(type)).c_str());
				ImGuiWindow* window = ImGui::GetCurrentWindow();
				gui::UiLayer::pushTextStyle(gui::TextStyle::Label);
				if (gui::widgets::sectionHeader("Lua Script").open) {
					window->DC.CursorMaxPos.x = window->WorkRect.Min.x;
					// Owl Nest draws each component body in a group (InspectorEditTracker).
					ImGui::BeginGroup();
					gui::component::renderProps(script);
					ImGui::EndGroup();
					gui::widgets::endSection();
				}
				gui::UiLayer::popTextStyle();
				EXPECT_LE(window->DC.CursorMaxPos.x, window->WorkRect.Max.x + 0.5f)
						<< "type " << static_cast<int>(type) << " at width " << width;
				ImGui::End();
				layer.end();
			}
		}
	}
	layer.onDetach();
	core::Log::invalidate();
}
