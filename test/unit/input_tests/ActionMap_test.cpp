/**
 * @file ActionMap_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <input/ActionFrame.h>
#include <input/ActionMap.h>
#include <input/InputNames.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::input;

namespace {

class ActionMapTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }
};

constexpr auto g_sample = R"(FormatVersion: 1
actions:
  - name: jump
    type: button
    bindings:
      - {key: Space}
      - {gamepad_button: A, pad: 1}
      - {gamepad_axis: right_trigger}
  - name: throttle
    type: axis
    bindings:
      - {key: W}
      - {key: S, scale: -1}
  - name: move
    type: vector
    bindings:
      - {key: D, component: x}
      - {key: A, component: x, scale: -1}
      - {key: S, component: y}
      - {key: W, component: y, scale: -1}
      - {gamepad_stick: left, dead_zone: 0.25, curve: custom, exponent: 1.5}
)";

auto withKeys(std::initializer_list<KeyCode> iKeys) -> InputState {
	InputState state;
	for (const auto key: iKeys) state.setKey(key, true);
	return state;
}

auto makeAction(const std::string& iName, const ActionKind iKind, std::vector<ActionBinding> iBindings)
		-> ActionDefinition {
	return {.name = iName, .kind = iKind, .bindings = std::move(iBindings)};
}

}// namespace

TEST_F(ActionMapTest, NamesRoundTrip) {
	EXPECT_EQ(names::keyName(key::Space), "Space");
	EXPECT_EQ(names::keyName(key::LeftShift), "LeftShift");
	EXPECT_EQ(names::keyName(1000), "1000");
	EXPECT_EQ(names::parseKey("W"), key::W);
	EXPECT_EQ(names::parseKey("F12"), key::F12);
	EXPECT_EQ(names::parseKey("87"), key::W);
	EXPECT_FALSE(names::parseKey("Nope").has_value());
	EXPECT_FALSE(names::parseKey("9999").has_value());
	for (const KeyCode key: {key::A, key::D5, key::Escape, key::Kp3, key::RightAlt, key::Menu})
		EXPECT_EQ(names::parseKey(names::keyName(key)), key);
	EXPECT_EQ(names::parseMouseButton("Right"), mouse::ButtonRight);
	EXPECT_EQ(names::mouseButtonName(mouse::Button4), "Button4");
	EXPECT_FALSE(names::parseMouseButton("Button9").has_value());
	EXPECT_EQ(names::parseGamepadButton("DpadLeft"), gamepad::DpadLeft);
	EXPECT_EQ(names::gamepadButtonName(gamepad::Start), "Start");
	EXPECT_FALSE(names::parseGamepadButton("15").has_value());
	EXPECT_EQ(names::parseGamepadAxis("right_trigger"), gamepad::RightTrigger);
	EXPECT_EQ(names::gamepadAxisName(gamepad::LeftY), "left_y");
}

TEST_F(ActionMapTest, ParsesTheDocumentedFormat) {
	const auto parsed = ActionMap::fromYaml(g_sample);
	ASSERT_TRUE(parsed.has_value());
	ASSERT_EQ(parsed->size(), 3u);
	const auto* jump = parsed->find("jump");
	ASSERT_NE(jump, nullptr);
	EXPECT_EQ(jump->kind, ActionKind::Button);
	ASSERT_EQ(jump->bindings.size(), 3u);
	EXPECT_EQ(jump->bindings[0].source, BindingSource::Key);
	EXPECT_EQ(jump->bindings[0].code, key::Space);
	EXPECT_EQ(jump->bindings[1].pad, 1);
	EXPECT_EQ(jump->bindings[2].source, BindingSource::GamepadAxis);
	EXPECT_EQ(jump->bindings[2].pad, -1);
	const auto* throttle = parsed->find("throttle");
	ASSERT_NE(throttle, nullptr);
	EXPECT_EQ(throttle->kind, ActionKind::Axis);
	EXPECT_FLOAT_EQ(throttle->bindings[1].scale, -1.f);
	const auto* move = parsed->find("move");
	ASSERT_NE(move, nullptr);
	EXPECT_EQ(move->bindings[2].component, BindingComponent::Y);
	EXPECT_EQ(move->bindings[4].source, BindingSource::GamepadStick);
	EXPECT_FLOAT_EQ(move->bindings[4].response.deadZone, 0.25f);
	EXPECT_EQ(move->bindings[4].response.curve, ResponseCurve::Custom);
	EXPECT_FLOAT_EQ(move->bindings[4].response.exponent, 1.5f);
	EXPECT_EQ(parsed->indexOf("move"), 2u);
	EXPECT_FALSE(parsed->indexOf("missing").has_value());
}

TEST_F(ActionMapTest, YamlRoundTripKeepsEverything) {
	const auto parsed = ActionMap::fromYaml(g_sample);
	ASSERT_TRUE(parsed.has_value());
	const auto text = parsed->toYaml();
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);
	const auto again = ActionMap::fromYaml(text);
	ASSERT_TRUE(again.has_value());
	EXPECT_EQ(*parsed, *again);
	const auto defaults = ActionMap::createDefault();
	const auto defaultsAgain = ActionMap::fromYaml(defaults.toYaml());
	ASSERT_TRUE(defaultsAgain.has_value());
	EXPECT_EQ(defaults, *defaultsAgain);
}

TEST_F(ActionMapTest, RejectsBrokenFiles) {
	const auto expectError = [](const std::string& iBody, const std::string& iPart) -> void {
		const auto parsed = ActionMap::fromYaml("FormatVersion: 1\nactions:\n" + iBody);
		EXPECT_FALSE(parsed.has_value()) << iBody;
		if (!parsed) {
			EXPECT_NE(parsed.error().find(iPart), std::string::npos) << parsed.error();
		}
	};
	expectError("  - {type: button}\n", "no `name`");
	expectError("  - {name: a, type: slider}\n", "unknown type");
	expectError("  - {name: a, bindings: [{pad: 0}]}\n", "no source");
	expectError("  - {name: a, bindings: [{key: Nope}]}\n", "unknown key");
	expectError("  - {name: a, bindings: [{key: A, gamepad_button: A}]}\n", "several sources");
	expectError("  - {name: a, bindings: [{key: A, colour: red}]}\n", "unknown binding entry");
	expectError("  - {name: a, bindings: [{key: A, pad: 16}]}\n", "out of range");
	expectError("  - {name: a, bindings: [{gamepad_axis: left_x, curve: wavy}]}\n", "unknown curve");
	expectError("  - {name: a, bindings: [{gamepad_axis: left_x, dead_zone: 1}]}\n", "dead_zone");
	expectError("  - {name: a}\n  - {name: a}\n", "defined twice");
	EXPECT_FALSE(ActionMap::fromYaml("- not a map").has_value());
	EXPECT_FALSE(ActionMap::fromYaml("FormatVersion: 99\nactions: []\n").has_value());
	EXPECT_FALSE(ActionMap::fromYaml("actions: [unterminated").has_value());
	EXPECT_TRUE(ActionMap::fromYaml("FormatVersion: 1\n").has_value());
}

TEST_F(ActionMapTest, EditsInPlace) {
	ActionMap map;
	EXPECT_FALSE(map.setAction({}));
	EXPECT_TRUE(map.setAction(makeAction("a", ActionKind::Button, {})));
	EXPECT_TRUE(map.setAction(makeAction("b", ActionKind::Axis, {})));
	EXPECT_TRUE(map.setAction(makeAction("a", ActionKind::Vector, {})));
	ASSERT_EQ(map.size(), 2u);
	EXPECT_EQ(map.getActions().front().kind, ActionKind::Vector);
	EXPECT_TRUE(map.removeAction("a"));
	EXPECT_FALSE(map.removeAction("a"));
	EXPECT_EQ(map.size(), 1u);
}

TEST_F(ActionMapTest, DefaultsHoldTheEngineActions) {
	const auto defaults = ActionMap::createDefault();
	for (const auto* name: {"move", "look", "jump", "descend", "sprint", "boost", "interact", "primary", "secondary",
							"ui_up", "ui_down", "ui_left", "ui_right", "ui_accept", "ui_cancel", "ui_scroll"})
		EXPECT_NE(defaults.find(name), nullptr) << name;
	EXPECT_EQ(defaults.find("move")->kind, ActionKind::Vector);
	EXPECT_EQ(ActionMap::getDefault()->size(), defaults.size());
}

TEST_F(ActionMapTest, WithDefaultsKeepsTheProjectActionsFirst) {
	ActionMap project;
	ASSERT_TRUE(project.setAction(
			makeAction("jump", ActionKind::Button, {{.source = BindingSource::Key, .code = key::J}})));
	ASSERT_TRUE(project.setAction(makeAction("fire", ActionKind::Button, {})));
	const auto completed = project.withDefaults();
	EXPECT_EQ(completed.size(), ActionMap::createDefault().size() + 1);
	EXPECT_EQ(completed.getActions().front().name, "jump");
	EXPECT_EQ(completed.find("jump")->bindings.front().code, key::J);
	EXPECT_NE(completed.find("interact"), nullptr);
	EXPECT_NE(completed.find("fire"), nullptr);
}

TEST_F(ActionMapTest, SavesAndLoadsAFile) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_action_map_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	const auto parsed = ActionMap::fromYaml(g_sample);
	ASSERT_TRUE(parsed.has_value());
	const auto file = dir / std::string{ActionMap::fileName};
	ASSERT_TRUE(parsed->saveFile(file));
	const auto loaded = ActionMap::loadFile(file);
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(*parsed, *loaded);
	EXPECT_FALSE(ActionMap::loadFile(dir / "missing.yml").has_value());
	EXPECT_FALSE(parsed->saveFile(dir / "no_such_folder" / "file.yml"));
	std::filesystem::remove_all(dir);
}

TEST_F(ActionMapTest, ButtonFollowsAnyBinding) {
	const auto action = makeAction("jump", ActionKind::Button,
								   {{.source = BindingSource::Key, .code = key::Space},
									{.source = BindingSource::GamepadButton, .code = gamepad::A, .pad = 1},
									{.source = BindingSource::GamepadAxis, .code = gamepad::RightTrigger}});
	EXPECT_FALSE(sampleAction(action, {}).held);
	EXPECT_TRUE(sampleAction(action, withKeys({key::Space})).held);
	EXPECT_FLOAT_EQ(sampleAction(action, withKeys({key::Space})).value, 1.f);

	InputState pad;
	pad.gamepads[0].connected = true;
	pad.gamepads[0].setButton(gamepad::A, true);
	EXPECT_FALSE(sampleAction(action, pad).held);// bound to slot 1 only
	pad.gamepads[1].connected = true;
	pad.gamepads[1].setButton(gamepad::A, true);
	EXPECT_TRUE(sampleAction(action, pad).held);

	InputState trigger;
	trigger.gamepads[3].connected = true;
	trigger.gamepads[3].setAxis(gamepad::RightTrigger, 0.4f);
	EXPECT_FALSE(sampleAction(action, trigger).held);
	trigger.gamepads[3].setAxis(gamepad::RightTrigger, 1.f);
	EXPECT_TRUE(sampleAction(action, trigger).held);// any pad
}

TEST_F(ActionMapTest, AxisAddsAndClamps) {
	const auto action = makeAction("throttle", ActionKind::Axis,
								   {{.source = BindingSource::Key, .code = key::W},
									{.source = BindingSource::Key, .code = key::S, .scale = -1.f},
									{.source = BindingSource::GamepadAxis, .code = gamepad::LeftY, .scale = -1.f}});
	EXPECT_FLOAT_EQ(sampleAction(action, withKeys({key::W})).value, 1.f);
	EXPECT_FLOAT_EQ(sampleAction(action, withKeys({key::S})).value, -1.f);
	EXPECT_FLOAT_EQ(sampleAction(action, withKeys({key::W, key::S})).value, 0.f);
	EXPECT_FALSE(sampleAction(action, withKeys({key::W, key::S})).held);
	auto state = withKeys({key::W});
	state.gamepads[0].connected = true;
	state.gamepads[0].setAxis(gamepad::LeftY, -1.f);
	EXPECT_FLOAT_EQ(sampleAction(action, state).value, 1.f);// clamped, not 2
	InputState half;
	half.gamepads[0].connected = true;
	half.gamepads[0].setAxis(gamepad::LeftY, 0.5f);
	const auto reading = sampleAction(action, half);
	EXPECT_LT(reading.value, 0.f);
	EXPECT_GT(reading.value, -0.5f);// dead zone and rescale
}

TEST_F(ActionMapTest, VectorNormalisesTheDiagonalAndShapesTheStick) {
	const auto parsed = ActionMap::fromYaml(g_sample);
	ASSERT_TRUE(parsed.has_value());
	const auto& move = *parsed->find("move");
	const auto diagonal = sampleAction(move, withKeys({key::D, key::S}));
	EXPECT_NEAR(diagonal.vector.x(), 0.70710678f, 1e-5f);
	EXPECT_NEAR(diagonal.vector.y(), 0.70710678f, 1e-5f);
	EXPECT_NEAR(diagonal.value, 1.f, 1e-5f);
	const auto left = sampleAction(move, withKeys({key::A}));
	EXPECT_FLOAT_EQ(left.vector.x(), -1.f);
	EXPECT_FLOAT_EQ(left.vector.y(), 0.f);
	EXPECT_FALSE(sampleAction(move, withKeys({key::A, key::D})).held);

	InputState stick;
	stick.gamepads[0].connected = true;
	stick.gamepads[0].setAxis(gamepad::LeftX, 0.2f);// inside the 0.25 dead zone
	EXPECT_FLOAT_EQ(sampleAction(move, stick).vector.x(), 0.f);
	stick.gamepads[0].setAxis(gamepad::LeftX, 1.f);
	EXPECT_NEAR(sampleAction(move, stick).vector.x(), 1.f, 1e-5f);
	stick.gamepads[0].setAxis(gamepad::LeftX, 0.f);
	stick.gamepads[0].setAxis(gamepad::LeftY, -0.6f);
	const auto up = sampleAction(move, stick);
	EXPECT_LT(up.vector.y(), 0.f);
	EXPECT_NEAR(up.vector.x(), 0.f, 1e-6f);
}

TEST_F(ActionMapTest, VectorComponentOfAStickOnly) {
	const auto action = makeAction("aim", ActionKind::Vector,
								   {{.source = BindingSource::GamepadStick,
									 .code = 1,
									 .component = BindingComponent::Y,
									 .response = {.deadZone = 0.f}}});
	InputState stick;
	stick.gamepads[0].connected = true;
	stick.gamepads[0].setAxis(gamepad::RightX, 0.5f);
	stick.gamepads[0].setAxis(gamepad::RightY, 0.5f);
	const auto reading = sampleAction(action, stick);
	EXPECT_FLOAT_EQ(reading.vector.x(), 0.f);
	EXPECT_NEAR(reading.vector.y(), 0.5f, 1e-5f);
}

TEST_F(ActionMapTest, EdgesBetweenTwoSamples) {
	const auto action = makeAction("jump", ActionKind::Button, {{.source = BindingSource::Key, .code = key::Space}});
	InputFrame frame;
	frame.reset({});
	auto state = evaluateAction(action, frame);
	EXPECT_FALSE(state.held || state.pressed || state.released);
	frame.advance(withKeys({key::Space}));
	state = evaluateAction(action, frame);
	EXPECT_TRUE(state.held);
	EXPECT_TRUE(state.pressed);
	EXPECT_FALSE(state.released);
	frame.advance(withKeys({key::Space}));
	state = evaluateAction(action, frame);
	EXPECT_TRUE(state.held);
	EXPECT_FALSE(state.pressed);
	frame.advance({});
	state = evaluateAction(action, frame);
	EXPECT_FALSE(state.held);
	EXPECT_TRUE(state.released);
}

TEST_F(ActionMapTest, AxisEdgesCrossTheThreshold) {
	const auto action = makeAction("throttle", ActionKind::Axis, {{.source = BindingSource::Key, .code = key::W}});
	InputFrame frame;
	frame.reset({});
	frame.advance(withKeys({key::W}));
	EXPECT_TRUE(evaluateAction(action, frame).pressed);
	EXPECT_FLOAT_EQ(evaluateAction(action, frame).value, 1.f);
}

TEST_F(ActionMapTest, FrameLooksActionsUpByName) {
	auto map = mkShared<ActionMap>();
	EXPECT_TRUE(map->setAction(
			makeAction("jump", ActionKind::Button, {{.source = BindingSource::Key, .code = key::Space}})));
	EXPECT_TRUE(
			map->setAction(makeAction("move", ActionKind::Vector, {{.source = BindingSource::Key, .code = key::D}})));
	ActionFrame actions;
	EXPECT_FALSE(actions.isHeld("jump"));
	EXPECT_FALSE(actions.has("jump"));
	InputFrame frame;
	frame.reset({});
	frame.advance(withKeys({key::Space, key::D}));
	actions.update(map, frame);
	EXPECT_TRUE(actions.has("jump"));
	EXPECT_TRUE(actions.isHeld("jump"));
	EXPECT_TRUE(actions.isJustPressed("jump"));
	EXPECT_FALSE(actions.isJustReleased("jump"));
	EXPECT_FLOAT_EQ(actions.getValue("jump"), 1.f);
	EXPECT_FLOAT_EQ(actions.getVector("move").x(), 1.f);
	EXPECT_FALSE(actions.has("fly"));
	EXPECT_FALSE(actions.isHeld("fly"));
	EXPECT_FLOAT_EQ(actions.getValue("fly"), 0.f);
	EXPECT_EQ(actions.getStates().size(), 2u);
	EXPECT_EQ(actions.getMap(), map);
	frame.advance({});
	actions.update(map, frame);
	EXPECT_TRUE(actions.isJustReleased("jump"));
	actions.update(nullptr, frame);
	EXPECT_FALSE(actions.isHeld("jump"));
}
