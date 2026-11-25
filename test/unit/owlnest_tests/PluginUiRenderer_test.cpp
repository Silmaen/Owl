/**
 * @file PluginUiRenderer_test.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/PluginUiRenderer.h"
#include "nestTestHelper.h"
#include "session/PluginUi.h"
#include "testHelper.h"

#include <gui/UiLayer.h>
#include <imgui.h>

#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::test;
using session::PluginWidget;
using session::PluginWidgetKind;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
/**
 * @brief
 *  Headless ImGui harness drawing plugin widgets in a window.
 */
class PluginUiRendererTest : public NestTest {
protected:
	void SetUp() override {
		NestTest::SetUp();
		m_layer.disableApp();
		m_layer.onAttach();
	}

	void TearDown() override {
		m_layer.onDetach();
		NestTest::TearDown();
	}

	// Run one ImGui frame drawing the widgets of a handler in a window.
	void frame(const std::function<void()>& iWidgets, const bool iNested = false) {
		m_layer.begin();
		ImGui::SetNextWindowPos({0.f, 0.f});
		ImGui::SetNextWindowSize({500.f, 700.f});
		ImGui::Begin("Plugin");
		m_ui.begin("test", iNested);
		iWidgets();
		m_ui.end();
		ImGui::End();
		m_layer.end();
	}

	/// ImGui context owner.
	gui::UiLayer m_layer;
	/// The renderer under test.
	panel::PluginUiRenderer m_ui;
};
OWL_DIAG_POP

// One widget of each kind, as a plugin handler would ask for them.
auto everyWidget() -> std::vector<PluginWidget> {
	return {{.kind = PluginWidgetKind::Section, .label = "Notes"},
			{.kind = PluginWidgetKind::Text, .label = "A hint.", .emphasis = true},
			{.kind = PluginWidgetKind::Value, .label = "Entity", .value = std::string{"Crate"}},
			{.kind = PluginWidgetKind::Checkbox, .label = "Done", .tooltip = "Whether it is done.", .value = true},
			{.kind = PluginWidgetKind::Number, .label = "Size", .value = 1.5, .min = 0.0, .max = 4.0, .step = 0.1},
			{.kind = PluginWidgetKind::Integer, .label = "Count", .value = int64_t{3}, .min = 1.0, .max = 9.0},
			{.kind = PluginWidgetKind::Input, .label = "Note", .value = std::string{"To do"}},
			{.kind = PluginWidgetKind::Vec3, .label = "Offset", .value = std::vector<double>{1.0, 2.0, 3.0}},
			{.kind = PluginWidgetKind::Color, .label = "Tint", .value = std::vector<double>{1.0, 0.5, 0.0, 1.0}},
			{.kind = PluginWidgetKind::Choice,
			 .label = "Mode",
			 .value = std::string{"line"},
			 .choices = {"line", "circle"}},
			{.kind = PluginWidgetKind::Button, .label = "Pin", .tooltip = "Pin the note.", .emphasis = true},
			{.kind = PluginWidgetKind::Section, .label = "Other"},
			{.kind = PluginWidgetKind::Text, .label = "Plain text."}};
}

}// namespace

TEST_F(PluginUiRendererTest, EveryWidgetIsDrawnAndKeepsItsValue) {
	const auto widgets = everyWidget();
	for (const bool nested: {false, true}) {
		for (int pass = 0; pass < 3; ++pass) {
			frame(
					[this, &widgets]() -> void {
						for (const auto& widget: widgets) {
							const auto result = m_ui.draw(widget);
							if (widget.kind == PluginWidgetKind::Section) {
								EXPECT_TRUE(result.changed) << widget.label;
								continue;
							}
							EXPECT_FALSE(result.changed) << widget.label;
							EXPECT_EQ(result.value.index(), widget.value.index()) << widget.label;
						}
					},
					nested);
		}
	}
}

TEST_F(PluginUiRendererTest, AButtonReportsItsClickOnce) {
	const PluginWidget button{.kind = PluginWidgetKind::Button, .label = "Pin"};
	ImVec2 center{0.f, 0.f};
	uint32_t clicks = 0;
	const auto draw = [this, &button, &center, &clicks]() -> void {
		if (m_ui.draw(button).changed)
			++clicks;
		const auto min = ImGui::GetItemRectMin();
		const auto max = ImGui::GetItemRectMax();
		center = {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
	};
	frame(draw);
	frame(draw);
	ImGui::GetIO().AddMousePosEvent(center.x, center.y);
	frame(draw);
	ImGui::GetIO().AddMouseButtonEvent(0, true);
	frame(draw);
	ImGui::GetIO().AddMouseButtonEvent(0, false);
	frame(draw);
	frame(draw);
	EXPECT_EQ(clicks, 1u);
}
