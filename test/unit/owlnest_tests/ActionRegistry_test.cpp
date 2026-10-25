/**
 * @file ActionRegistry_test.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ActionRegistry.h"
#include "testHelper.h"

#include <core/Log.h>
#include <gui/UiLayer.h>

#include <imgui.h>

#include <gtest/gtest.h>

#include <cstddef>

using namespace owl;
using namespace owl::nest;

namespace {

/**
 * @brief
 *  Headless ImGui frames feeding key chords to an `ActionRegistry`.
 */
class ActionRegistryChordTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_layer.disableApp();
		m_layer.onAttach();
		ImGui::GetIO().IniFilename = nullptr;
		m_registry.registerAction("view.left", "Layout: Hierarchy Left", {input::key::D2, Modifiers::Alt},
								  [this]() -> void { ++m_runs; });
		m_registry.registerAction("plain.w", "Plain", {input::key::W, Modifiers::None},
								  [this]() -> void { ++m_plainRuns; });
		m_registry.registerAction("help.context", "Help", {input::key::F1, Modifiers::None},
								  [this]() -> void { ++m_helpRuns; });
	}

	void TearDown() override {
		m_layer.onDetach();
		core::Log::invalidate();
	}

	// One frame with the given keys held, dispatching the chords.
	auto frame(const bool iAlt, const bool iKey, const ImGuiKey iWhich = ImGuiKey_2) -> size_t {
		auto& io = ImGui::GetIO();
		io.AddKeyEvent(ImGuiMod_Alt, iAlt);
		io.AddKeyEvent(iWhich, iKey);
		m_layer.begin();
		const auto count = m_registry.dispatchImGuiChords();
		m_layer.end();
		return count;
	}

	gui::UiLayer m_layer;
	ActionRegistry m_registry;
	int m_runs = 0;
	int m_plainRuns = 0;
	int m_helpRuns = 0;
};

}// namespace

TEST(ActionRegistryChord, MapsShortcutsToImGuiChords) {
	EXPECT_EQ(ActionRegistry::toImGuiChord({input::key::D2, Modifiers::Alt}), ImGuiKey_2 | ImGuiMod_Alt);
	EXPECT_EQ(ActionRegistry::toImGuiChord({input::key::S, Modifiers::Ctrl | Modifiers::Shift}),
			  ImGuiKey_S | ImGuiMod_Ctrl | ImGuiMod_Shift);
	EXPECT_EQ(ActionRegistry::toImGuiChord({input::key::F5, Modifiers::None}), ImGuiKey_F5);
	EXPECT_EQ(ActionRegistry::toImGuiChord({input::key::Delete, Modifiers::None}), ImGuiKey_Delete);
	EXPECT_EQ(ActionRegistry::toImGuiChord({}), 0);
}

TEST_F(ActionRegistryChordTest, RunsAChordOncePerPress) {
	EXPECT_EQ(frame(true, false), 0u);
	EXPECT_EQ(frame(true, true), 1u);
	EXPECT_EQ(frame(true, true), 0u);
	EXPECT_EQ(frame(false, false), 0u);
	EXPECT_EQ(m_runs, 1);
}

TEST_F(ActionRegistryChordTest, IgnoresSuspendedRegistryAndPlainKeys) {
	m_registry.setSuspended(true);
	frame(true, false);
	frame(true, true);
	frame(false, false);
	EXPECT_EQ(m_runs, 0);
	m_registry.setSuspended(false);
	frame(false, false, ImGuiKey_W);
	frame(false, true, ImGuiKey_W);
	EXPECT_EQ(m_plainRuns, 0);
}

TEST_F(ActionRegistryChordTest, RunsFunctionKeysEvenWhilePanelsHoldTheKeyboard) {
	frame(false, false, ImGuiKey_F1);
	frame(false, true, ImGuiKey_F1);
	frame(false, true, ImGuiKey_F1);
	frame(false, false, ImGuiKey_F1);
	EXPECT_EQ(m_helpRuns, 1);
}

TEST(ActionRegistryRun, RunsActionsByIdentifier) {
	core::Log::init(core::Log::Level::Off);
	ActionRegistry registry;
	int runs = 0;
	registry.registerAction("scene.play", "Play", {input::key::F5, Modifiers::None}, [&runs]() -> void { ++runs; });
	EXPECT_TRUE(registry.run("scene.play"));
	EXPECT_EQ(runs, 1);
	EXPECT_FALSE(registry.run("scene.unknown"));
	EXPECT_EQ(runs, 1);
	EXPECT_EQ(registry.getShortcutString("scene.play"), "F5");
	core::Log::invalidate();
}
