/**
 * @file AsyncProgressModal_test.cpp
 * @author Silmaen
 * @date 25/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/AsyncProgressModal.h"
#include "testHelper.h"

#include <core/Log.h>
#include <gui/UiLayer.h>

#include <imgui.h>

#include <gtest/gtest.h>

using namespace owl;
using namespace owl::nest;

namespace {

class AsyncProgressModalTest : public testing::Test {
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
	// One frame drawing the modal, then opening iOtherPopup at the root level when it is not null.
	void frame(panel::AsyncProgressModal& ioModal, const char* iOtherPopup = nullptr) {
		m_layer.begin();
		ioModal.onImGuiRender();
		if (iOtherPopup != nullptr)
			ImGui::OpenPopup(iOtherPopup);
		if (ImGui::BeginPopup("Other"))
			ImGui::EndPopup();
		m_layer.end();
	}
	gui::UiLayer m_layer;
};

}// namespace

TEST_F(AsyncProgressModalTest, StaysUntilClosedWhileItsPopupShows) {
	panel::AsyncProgressModal modal;
	const auto state = mkShared<AsyncProgressState>();
	modal.open("Loading Scene...", state, false);
	frame(modal);
	state->completed.store(true);
	frame(modal);
	frame(modal);
	EXPECT_TRUE(modal.isActive());
	modal.close();
	EXPECT_FALSE(modal.isActive());
}

TEST_F(AsyncProgressModalTest, LetsGoOnceFinishedWhenAnotherPopupReplacedIt) {
	panel::AsyncProgressModal modal;
	const auto state = mkShared<AsyncProgressState>();
	modal.open("Loading Scene...", state, false);
	frame(modal);
	frame(modal, "Other");
	frame(modal);
	EXPECT_TRUE(modal.isActive());
	state->setError("Cannot open scene.");
	frame(modal);
	EXPECT_FALSE(modal.isActive());
}
