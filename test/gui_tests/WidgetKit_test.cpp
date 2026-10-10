/**
 * @file WidgetKit_test.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SearchField.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Toast.h>
#include <gui/widgets/Tooltip.h>

#include <imgui.h>

#include <functional>
#include <string>
#include <tuple>

using namespace owl;
using namespace owl::gui;
using namespace owl::gui::widgets;

namespace {

/**
 * @brief
 *  Headless ImGui frames with simulated mouse input.
 */
class WidgetKitTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_layer.disableApp();
		m_layer.onAttach();
		ImGui::GetIO().IniFilename = nullptr;
	}

	void TearDown() override {
		m_layer.onDetach();
		core::Log::invalidate();
	}

	// One frame drawing the given widgets in a fixed window.
	void frame(const std::function<void()>& iBody) {
		m_layer.begin();
		ImGui::SetNextWindowPos({0.f, 0.f});
		ImGui::SetNextWindowSize({400.f, 500.f});
		ImGui::Begin("Kit");
		iBody();
		ImGui::End();
		m_layer.end();
	}

	// Click on the centre of the rectangle recorded by the body: move, press, release.
	void click(const std::function<void()>& iBody) {
		const ImVec2 centre{(m_min.x + m_max.x) * 0.5f, (m_min.y + m_max.y) * 0.5f};
		ImGui::GetIO().AddMousePosEvent(centre.x, centre.y);
		frame(iBody);
		ImGui::GetIO().AddMouseButtonEvent(0, true);
		frame(iBody);
		ImGui::GetIO().AddMouseButtonEvent(0, false);
		frame(iBody);
	}

	void record() {
		m_min = ImGui::GetItemRectMin();
		m_max = ImGui::GetItemRectMax();
	}

	gui::UiLayer m_layer;
	ImVec2 m_min{0.f, 0.f};
	ImVec2 m_max{0.f, 0.f};
};

}// namespace

TEST(WidgetKit, FilterMatchesEveryWordIgnoringCase) {
	EXPECT_TRUE(matchesFilter("MainCamera", ""));
	EXPECT_TRUE(matchesFilter("MainCamera", "   "));
	EXPECT_TRUE(matchesFilter("MainCamera", "camera"));
	EXPECT_TRUE(matchesFilter("MainCamera", "CAM main"));
	EXPECT_FALSE(matchesFilter("MainCamera", "main light"));
	EXPECT_FALSE(matchesFilter("", "a"));
	EXPECT_TRUE(matchesFilter("lava_tile.png", "tile png"));
}

TEST(WidgetKit, ToastsFoldExpireAndStayBounded) {
	ToastQueue queue;
	queue.push(ToastKind::Success, "Saved main_menu.owl");
	queue.push(ToastKind::Success, "Saved main_menu.owl");
	ASSERT_EQ(queue.getToasts().size(), 1u);
	EXPECT_EQ(queue.getToasts().front().count, 2u);
	queue.push(ToastKind::Error, "Could not save", "See the Log.");
	EXPECT_EQ(queue.getToasts().size(), 2u);
	for (int i = 0; i < 10; ++i) queue.push(ToastKind::Info, std::to_string(i));
	EXPECT_EQ(queue.getToasts().size(), ToastQueue::maxToasts);
	EXPECT_EQ(queue.getToasts().back().title, "9");

	queue.update(ToastQueue::lifetime);
	EXPECT_EQ(queue.getToasts().size(), ToastQueue::maxToasts);
	queue.update(ToastQueue::fadeTime);
	EXPECT_TRUE(queue.getToasts().empty());

	EXPECT_FLOAT_EQ(ToastQueue::getOpacity(0.f), 1.f);
	EXPECT_FLOAT_EQ(ToastQueue::getOpacity(ToastQueue::lifetime), 1.f);
	EXPECT_NEAR(ToastQueue::getOpacity(ToastQueue::lifetime + ToastQueue::fadeTime * 0.5f), 0.5f, 1e-4f);
	EXPECT_FLOAT_EQ(ToastQueue::getOpacity(ToastQueue::lifetime + ToastQueue::fadeTime * 2.f), 0.f);
	queue.push(ToastKind::Warning, "x");
	queue.clear();
	EXPECT_TRUE(queue.getToasts().empty());
}

TEST_F(WidgetKitTest, EveryWidgetDrawsWithoutInput) {
	std::string search = "cam";
	float value = 2.f;
	math::vec3 position{0.f, 1.f, 0.f};
	bool flag = false;
	ToastQueue toasts;
	toasts.push(ToastKind::Success, "Saved", "scenes/main_menu.owl");
	toasts.push(ToastKind::Warning, "Slow frame");
	for (int i = 0; i < 2; ++i) {
		frame([&]() -> void {
			EXPECT_FALSE(iconButton("save", {.glyph = icon::save, .tooltip = "Save", .shortcut = "Ctrl+S"}));
			EXPECT_FALSE(iconButton("off", {.glyph = icon::play, .tooltip = "Play", .disabled = true}));
			EXPECT_FALSE(iconButton("on", {.glyph = icon::move, .selected = true}));
			EXPECT_FALSE(searchField("search", search, "Search entities"));
			badge("12");
			badge("2", BadgeKind::Warning);
			notice("No sprite sheet set.");
			notice("Saved.", BadgeKind::Success);
			notice("Broken.", BadgeKind::Error);
			EXPECT_FALSE(chip("Textures##chip", true));
			const auto header = sectionHeader("Transform", nullptr);
			EXPECT_FALSE(header.menuClicked);
			EXPECT_GT(header.max.y(), header.min.y());
			if (header.open) {
				if (beginProperties("props")) {
					EXPECT_FALSE(propertyFloat("Size", value, 1.f));
					EXPECT_FALSE(propertyVec3("Position", position, {0.f, 0.f, 0.f}));
					EXPECT_FALSE(propertyBool("Primary", flag, false));
					endProperties();
				}
				endSection();
			}
			EXPECT_FALSE(emptyState(icon::box, "No entity selected", "Select an entity to edit it.", "Add entity"));
			richTooltip("Title", "F1", "Description.");
			toasts.onImGuiRender();
		});
	}
	EXPECT_EQ(search, "cam");
	EXPECT_FLOAT_EQ(value, 2.f);
}

TEST_F(WidgetKitTest, IconButtonAndChipReportClicks) {
	int clicks = 0;
	const auto body = [&]() -> void {
		if (iconButton("save", {.glyph = icon::save, .tooltip = "Save"}))
			++clicks;
		record();
	};
	frame(body);
	click(body);
	EXPECT_EQ(clicks, 1);

	bool selected = false;
	const auto chipBody = [&]() -> void {
		if (chip("Scenes", selected))
			selected = !selected;
		record();
	};
	frame(chipBody);
	click(chipBody);
	EXPECT_TRUE(selected);
}

TEST_F(WidgetKitTest, ResetButtonRestoresTheDefault) {
	float value = 5.f;
	const auto body = [&]() -> void {
		if (beginProperties("props")) {
			std::ignore = propertyFloat("Size", value, 1.f);
			record();
			endProperties();
		}
	};
	frame(body);
	click(body);
	EXPECT_FLOAT_EQ(value, 1.f);
}

TEST_F(WidgetKitTest, DisabledIconButtonIgnoresClicks) {
	int clicks = 0;
	const auto body = [&]() -> void {
		if (iconButton("play", {.glyph = icon::play, .disabled = true}))
			++clicks;
		record();
	};
	frame(body);
	click(body);
	EXPECT_EQ(clicks, 0);
}
