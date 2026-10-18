/**
 * @file ActionMapDocument_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/ActionMapDocument.h"
#include "AssetKind.h"
#include "document/BindingCapture.h"
#include "testHelper.h"

#include <input/KeyCodes.h>
#include <input/MouseCode.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

using namespace owl;
using namespace owl::nest;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class ActionMapDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_action_map_document_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_file = m_root / std::string{input::ActionMap::fileName};
	}

	void TearDown() override {
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	std::filesystem::path m_root;
	std::filesystem::path m_file;
	ActionMapDocument m_document;
};
OWL_DIAG_POP

auto keyDown(const input::KeyCode iKey) -> input::InputState {
	input::InputState state;
	state.setKey(iKey, true);
	return state;
}

auto connectedPad() -> input::InputState {
	input::InputState state;
	state.gamepads[0].connected = true;
	return state;
}

}// namespace

TEST_F(ActionMapDocumentTest, ClassifiesTheFileByName) {
	EXPECT_EQ(classifyAsset("project/input_actions.yml"), AssetKind::ActionMap);
	EXPECT_EQ(classifyAsset("project/settings.yml"), AssetKind::Code);
}

TEST_F(ActionMapDocumentTest, AddsRenamesAndRemovesActions) {
	EXPECT_TRUE(m_document.addAction("fire", input::ActionKind::Button));
	EXPECT_FALSE(m_document.addAction("fire", input::ActionKind::Axis)) << "names are unique";
	EXPECT_FALSE(m_document.addAction("", input::ActionKind::Button));
	EXPECT_TRUE(m_document.addAction("steer", input::ActionKind::Axis));
	EXPECT_EQ(m_document.selectedAction(), std::optional<std::size_t>{1});

	EXPECT_FALSE(m_document.renameAction(1, "fire"));
	EXPECT_FALSE(m_document.renameAction(1, ""));
	EXPECT_TRUE(m_document.renameAction(0, "shoot"));
	EXPECT_EQ(m_document.map().getActions()[0].name, "shoot") << "a rename keeps the place";
	EXPECT_EQ(m_document.map().getActions()[1].name, "steer");
	EXPECT_TRUE(m_document.setActionKind(1, input::ActionKind::Vector));
	EXPECT_FALSE(m_document.setActionKind(1, input::ActionKind::Vector));

	EXPECT_TRUE(m_document.removeAction(0));
	EXPECT_EQ(m_document.map().size(), 1u);
	EXPECT_FALSE(m_document.removeAction(5));
	EXPECT_EQ(m_document.uniqueName("steer"), "steer_2");
	EXPECT_EQ(m_document.uniqueName("free"), "free");
}

TEST_F(ActionMapDocumentTest, EditsBindings) {
	ASSERT_TRUE(m_document.addAction("move", input::ActionKind::Vector));
	input::ActionBinding binding;
	binding.source = input::BindingSource::Key;
	binding.code = input::key::D;
	EXPECT_TRUE(m_document.addBinding(0, binding));
	binding.code = input::key::A;
	binding.scale = -1.f;
	EXPECT_TRUE(m_document.addBinding(0, binding));
	EXPECT_EQ(m_document.map().getActions()[0].bindings.size(), 2u);

	binding.scale = 0.5f;
	EXPECT_TRUE(m_document.setBinding(0, 1, binding));
	EXPECT_EQ(m_document.map().getActions()[0].bindings[1].scale, 0.5f);
	EXPECT_FALSE(m_document.setBinding(0, 1, binding)) << "no change, no step";
	EXPECT_FALSE(m_document.setBinding(0, 9, binding));
	EXPECT_TRUE(m_document.removeBinding(0, 0));
	EXPECT_EQ(m_document.map().getActions()[0].bindings.size(), 1u);
	EXPECT_FALSE(m_document.removeBinding(0, 4));
}

TEST_F(ActionMapDocumentTest, UndoesAndRedoesEveryEdit) {
	const auto empty = m_document.map();
	ASSERT_TRUE(m_document.addAction("fire", input::ActionKind::Button));
	const auto withAction = m_document.map();
	ASSERT_TRUE(m_document.addBinding(0, input::ActionBinding{}));
	ASSERT_TRUE(m_document.renameAction(0, "shoot"));
	EXPECT_TRUE(m_document.canUndo());
	EXPECT_FALSE(m_document.canRedo());

	m_document.performUndo();
	EXPECT_EQ(m_document.map().getActions()[0].name, "fire");
	m_document.performUndo();
	EXPECT_TRUE(m_document.map().getActions()[0].bindings.empty());
	m_document.performUndo();
	EXPECT_EQ(m_document.map(), empty);
	EXPECT_FALSE(m_document.canUndo());
	EXPECT_TRUE(m_document.canRedo());

	m_document.performRedo();
	EXPECT_EQ(m_document.map(), withAction);
	m_document.performRedo();
	m_document.performRedo();
	EXPECT_EQ(m_document.map().getActions()[0].name, "shoot");
	EXPECT_EQ(m_document.selectedAction(), std::optional<std::size_t>{0});
}

TEST_F(ActionMapDocumentTest, SliderDragsMergeIntoOneStep) {
	ASSERT_TRUE(m_document.addAction("steer", input::ActionKind::Axis));
	ASSERT_TRUE(m_document.addBinding(0, input::ActionBinding{}));
	auto binding = m_document.map().getActions()[0].bindings[0];
	for (const float scale: {0.9f, 0.8f, 0.7f}) {
		binding.scale = scale;
		ASSERT_TRUE(m_document.setBinding(0, 0, binding, true));
	}
	m_document.performUndo();
	EXPECT_EQ(m_document.map().getActions()[0].bindings[0].scale, 1.f) << "the drag undoes in one step";
}

TEST_F(ActionMapDocumentTest, TracksTheDirtyState) {
	ASSERT_TRUE(m_document.addAction("fire", input::ActionKind::Button));
	m_document.saveAs(m_file);
	EXPECT_FALSE(m_document.isDirty());
	ASSERT_TRUE(m_document.renameAction(0, "shoot"));
	EXPECT_TRUE(m_document.isDirty());
	m_document.performUndo();
	EXPECT_FALSE(m_document.isDirty()) << "back to the saved map";
}

TEST_F(ActionMapDocumentTest, SavesAndReloadsTheFile) {
	EXPECT_FALSE(m_document.save()) << "no path yet";
	m_document.startFromDefaults(m_file);
	EXPECT_TRUE(m_document.isDirty());
	ASSERT_TRUE(m_document.addAction("fire", input::ActionKind::Button));
	input::ActionBinding binding;
	binding.source = input::BindingSource::GamepadAxis;
	binding.code = input::gamepad::RightTrigger;
	binding.pad = 1;
	binding.scale = -0.5f;
	binding.response = {.deadZone = 0.3f, .curve = input::ResponseCurve::Custom, .exponent = 2.5f};
	ASSERT_TRUE(m_document.addBinding(m_document.map().size() - 1, binding));
	ASSERT_TRUE(m_document.save());
	EXPECT_FALSE(m_document.isDirty());
	EXPECT_TRUE(exists(m_file));

	ActionMapDocument reloaded;
	ASSERT_TRUE(reloaded.loadFromFile(m_file));
	EXPECT_EQ(reloaded.map(), m_document.map());
	EXPECT_FALSE(reloaded.isDirty());
	EXPECT_EQ(reloaded.title(), "input_actions");
	EXPECT_FALSE(reloaded.loadFromFile(m_root / "missing.yml"));
}

TEST_F(ActionMapDocumentTest, KeepsAnUnreadableFileOutOfTheDocument) {
	std::ofstream{m_file} << "actions: [not a map";
	EXPECT_FALSE(m_document.loadFromFile(m_file));
	EXPECT_EQ(m_document.map().size(), 0u);
}

TEST_F(ActionMapDocumentTest, OffersTheBuiltInActionsTheFileLeavesOut) {
	ASSERT_TRUE(m_document.addAction("fire", input::ActionKind::Button));
	const auto missing = m_document.missingBuiltIns();
	EXPECT_EQ(missing.size(), input::ActionMap::createDefault().size());
	ASSERT_TRUE(m_document.overrideBuiltIn("jump"));
	EXPECT_FALSE(m_document.overrideBuiltIn("jump")) << "already defined";
	EXPECT_FALSE(m_document.overrideBuiltIn("fire")) << "not a built-in";
	EXPECT_EQ(m_document.map().find("jump")->bindings, input::ActionMap::createDefault().find("jump")->bindings);
	EXPECT_EQ(m_document.missingBuiltIns().size(), missing.size() - 1);

	ASSERT_TRUE(m_document.resetToDefaults());
	EXPECT_EQ(m_document.map(), input::ActionMap::createDefault());
	EXPECT_TRUE(m_document.missingBuiltIns().empty());
	EXPECT_FALSE(m_document.resetToDefaults());
}

TEST_F(ActionMapDocumentTest, RestoresAnAutosave) {
	ASSERT_TRUE(m_document.addAction("fire", input::ActionKind::Button));
	const auto snapshot = m_document.recoverySnapshot();
	ASSERT_TRUE(snapshot.has_value());
	ActionMapDocument other;
	EXPECT_TRUE(other.restoreRecoverySnapshot(*snapshot));
	EXPECT_EQ(other.map(), m_document.map());
	EXPECT_TRUE(other.isDirty());
	EXPECT_FALSE(other.restoreRecoverySnapshot("actions: [broken"));
}

TEST_F(ActionMapDocumentTest, CaptureAddsTheNextInputAsABinding) {
	ASSERT_TRUE(m_document.addAction("jump", input::ActionKind::Button));
	m_document.startCapture(0, std::nullopt, input::InputState{});
	EXPECT_TRUE(m_document.isCapturing());
	EXPECT_FALSE(m_document.updateInput(input::InputState{}));
	EXPECT_TRUE(m_document.updateInput(keyDown(input::key::Space)));
	EXPECT_FALSE(m_document.isCapturing());
	ASSERT_EQ(m_document.map().getActions()[0].bindings.size(), 1u);
	EXPECT_EQ(m_document.map().getActions()[0].bindings[0].source, input::BindingSource::Key);
	EXPECT_EQ(m_document.map().getActions()[0].bindings[0].code, input::key::Space);
	m_document.performUndo();
	EXPECT_TRUE(m_document.map().getActions()[0].bindings.empty());
}

TEST_F(ActionMapDocumentTest, CaptureRebindsKeepingScaleAndComponent) {
	ASSERT_TRUE(m_document.addAction("move", input::ActionKind::Vector));
	input::ActionBinding binding;
	binding.code = input::key::A;
	binding.scale = -1.f;
	binding.component = input::BindingComponent::X;
	ASSERT_TRUE(m_document.addBinding(0, binding));
	m_document.startCapture(0, 0, input::InputState{});
	EXPECT_EQ(m_document.captureReplaced(), std::optional<std::size_t>{0});
	EXPECT_TRUE(m_document.updateInput(keyDown(input::key::Left)));
	const auto& rebound = m_document.map().getActions()[0].bindings[0];
	EXPECT_EQ(rebound.code, input::key::Left);
	EXPECT_EQ(rebound.scale, -1.f);
	EXPECT_EQ(rebound.component, input::BindingComponent::X);
	EXPECT_EQ(m_document.map().getActions()[0].bindings.size(), 1u);
}

TEST_F(ActionMapDocumentTest, EscapeCancelsTheCapture) {
	ASSERT_TRUE(m_document.addAction("jump", input::ActionKind::Button));
	m_document.startCapture(0, std::nullopt, input::InputState{});
	EXPECT_FALSE(m_document.updateInput(keyDown(input::key::Escape)));
	EXPECT_FALSE(m_document.isCapturing());
	EXPECT_TRUE(m_document.map().getActions()[0].bindings.empty());
}

TEST_F(ActionMapDocumentTest, MonitorReadsEveryActionLive) {
	ASSERT_TRUE(m_document.addAction("jump", input::ActionKind::Button));
	ASSERT_TRUE(m_document.addBinding(
			0, input::ActionBinding{.source = input::BindingSource::Key, .code = input::key::Space}));
	ASSERT_TRUE(m_document.addAction("idle", input::ActionKind::Button));
	EXPECT_EQ(m_document.monitor().size(), 2u);
	EXPECT_FALSE(m_document.updateInput(input::InputState{}));
	EXPECT_FALSE(m_document.monitor()[0].held);
	EXPECT_FALSE(m_document.updateInput(keyDown(input::key::Space)));
	EXPECT_TRUE(m_document.monitor()[0].held);
	EXPECT_TRUE(m_document.monitor()[0].pressed);
	EXPECT_FALSE(m_document.monitor()[1].held);
	EXPECT_FALSE(m_document.updateInput(input::InputState{}));
	EXPECT_TRUE(m_document.monitor()[0].released);
}

TEST(BindingCapture, IgnoresWhatIsHeldWhenItStarts) {
	BindingCapture capture;
	auto held = keyDown(input::key::W);
	held.setMouseButton(input::mouse::ButtonLeft, true);
	capture.begin(held, input::ActionKind::Button);
	EXPECT_FALSE(capture.feed(held).has_value());
	auto released = input::InputState{};
	EXPECT_FALSE(capture.feed(released).has_value());
	const auto again = capture.feed(held);
	ASSERT_TRUE(again.has_value()) << "released then pressed again counts";
	EXPECT_EQ(again->source, input::BindingSource::Key);
	EXPECT_EQ(again->code, input::key::W);
	EXPECT_FALSE(capture.isArmed());
	EXPECT_FALSE(capture.feed(held).has_value()) << "one input per capture";
}

TEST(BindingCapture, TakesMouseButtonsAndGamepadButtons) {
	BindingCapture capture;
	capture.begin(connectedPad(), input::ActionKind::Button);
	auto state = connectedPad();
	state.setMouseButton(input::mouse::ButtonRight, true);
	auto mouse = capture.feed(state);
	ASSERT_TRUE(mouse.has_value());
	EXPECT_EQ(mouse->source, input::BindingSource::MouseButton);
	EXPECT_EQ(mouse->code, input::mouse::ButtonRight);

	capture.begin(connectedPad(), input::ActionKind::Button);
	state = connectedPad();
	state.gamepads[0].setButton(input::gamepad::B, true);
	auto pad = capture.feed(state);
	ASSERT_TRUE(pad.has_value());
	EXPECT_EQ(pad->source, input::BindingSource::GamepadButton);
	EXPECT_EQ(pad->code, input::gamepad::B);
	EXPECT_EQ(pad->pad, -1) << "any gamepad";
}

TEST(BindingCapture, TakesAWholeStickForAVectorAndAnAxisOtherwise) {
	auto pushed = connectedPad();
	pushed.gamepads[0].setAxis(input::gamepad::RightY, 0.9f);

	BindingCapture vector;
	vector.begin(connectedPad(), input::ActionKind::Vector);
	const auto stick = vector.feed(pushed);
	ASSERT_TRUE(stick.has_value());
	EXPECT_EQ(stick->source, input::BindingSource::GamepadStick);
	EXPECT_EQ(stick->code, 1);

	BindingCapture axis;
	axis.begin(connectedPad(), input::ActionKind::Axis);
	const auto single = axis.feed(pushed);
	ASSERT_TRUE(single.has_value());
	EXPECT_EQ(single->source, input::BindingSource::GamepadAxis);
	EXPECT_EQ(single->code, input::gamepad::RightY);
}

TEST(BindingCapture, NeedsAFirmPushAndIgnoresADeflectedStickAtStart) {
	BindingCapture capture;
	auto start = connectedPad();
	start.gamepads[0].setAxis(input::gamepad::LeftX, 0.8f);
	capture.begin(start, input::ActionKind::Axis);
	EXPECT_FALSE(capture.feed(start).has_value()) << "deflected at the start";
	auto weak = connectedPad();
	weak.gamepads[0].setAxis(input::gamepad::LeftTrigger, 0.4f);
	EXPECT_FALSE(capture.feed(weak).has_value()) << "below the threshold";
	weak.gamepads[0].setAxis(input::gamepad::LeftTrigger, 0.9f);
	const auto trigger = capture.feed(weak);
	ASSERT_TRUE(trigger.has_value());
	EXPECT_EQ(trigger->code, input::gamepad::LeftTrigger);
}

TEST(BindingCapture, DoesNothingUnlessArmed) {
	BindingCapture capture;
	EXPECT_FALSE(capture.feed(keyDown(input::key::A)).has_value());
	capture.begin(input::InputState{}, input::ActionKind::Button);
	capture.cancel();
	EXPECT_FALSE(capture.feed(keyDown(input::key::A)).has_value());
}
