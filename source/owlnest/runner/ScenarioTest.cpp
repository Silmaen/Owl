/**
 * @file ScenarioTest.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ScenarioTest.h"
#include "FrameBench.h"
#include "commands/Macro.h"
#include "session/HeadlessSession.h"
#include "session/HeldInput.h"

#include <app/EngineContext.h>
#include <input/ActionMap.h>
#include <input/Gamepad.h>
#include <input/Input.h>
#include <input/InputNames.h>
#include <input/KeyCodes.h>
#include <scene/Entity.h>
#include <scene/GameState.h>
#include <scene/SettingsManager.h>
#include <scene/component/components.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

namespace owl::nest::runner {

namespace {

auto parseGamepad(const YAML::Node& iNode, ScenarioStep& oStep) -> std::string {
	if (!iNode.IsMap())
		return "`gamepad` is a map (`pad`, `button`, `buttons`, axes)";
	ScenarioGamepad gamepad;
	gamepad.state.connected = true;
	const auto addButton = [&gamepad](const YAML::Node& iButton) -> bool {
		const auto code = input::names::parseGamepadButton(iButton.as<std::string>());
		if (code)
			gamepad.state.setButton(*code, true);
		return code.has_value();
	};
	for (const auto& item: iNode) {
		const auto key = item.first.as<std::string>();
		if (key == "pad") {
			gamepad.pad = item.second.as<std::size_t>();
			if (gamepad.pad >= input::g_GamepadCount)
				return std::format("gamepad slot {} out of range", gamepad.pad);
		} else if (key == "button") {
			if (!addButton(item.second))
				return std::format("unknown gamepad button '{}'", item.second.as<std::string>());
		} else if (key == "buttons") {
			for (const auto& button: item.second)
				if (!addButton(button))
					return std::format("unknown gamepad button '{}'", button.as<std::string>());
		} else if (const auto axis = input::names::parseGamepadAxis(key); axis) {
			gamepad.state.setAxis(*axis, item.second.as<float>());
		} else {
			return std::format("unknown gamepad entry '{}'", key);
		}
	}
	oStep.gamepad = gamepad;
	return "";
}

void parseExpected(const YAML::Node& iValue, ScenarioCheck& oCheck) {
	const auto text = iValue.as<std::string>();
	if (text == "true" || text == "false") {
		oCheck.number = text == "true" ? 1.0 : 0.0;
		return;
	}
	try {
		oCheck.number = iValue.as<double>();
	} catch (const YAML::Exception&) { oCheck.text = text; }
}

auto parseCheck(const std::string& iField, const YAML::Node& iNode, ScenarioCheck& oCheck) -> bool {
	oCheck.field = iField;
	if (iNode.IsScalar()) {
		oCheck.op = ScenarioCheck::Op::Equals;
		parseExpected(iNode, oCheck);
		return true;
	}
	if (const auto tol = iNode["tolerance"]; tol)
		oCheck.tolerance = tol.as<double>();
	for (const auto& [key, op]:
		 {std::pair{"equals", ScenarioCheck::Op::Equals}, std::pair{"greater", ScenarioCheck::Op::Greater},
		  std::pair{"less", ScenarioCheck::Op::Less}}) {
		if (const auto value = iNode[key]; value) {
			oCheck.op = op;
			parseExpected(value, oCheck);
			return true;
		}
	}
	return false;
}

auto parseArgValue(const commands::ArgSpec& iSpec, const YAML::Node& iValue) -> std::optional<commands::ArgValue> {
	if (iValue.IsSequence()) {
		std::string numbers;
		for (const auto& item: iValue) {
			if (!item.IsScalar())
				return std::nullopt;
			numbers += std::format("{} ", item.as<std::string>());
		}
		return commands::CommandRegistry::parseArg(iSpec.type, numbers);
	}
	if (iValue.IsMap() && iSpec.type == commands::ArgType::Text) {
		YAML::Emitter out;
		out << YAML::Flow << iValue;
		return commands::ArgValue{std::string{out.c_str()}};
	}
	if (!iValue.IsScalar())
		return std::nullopt;
	return commands::CommandRegistry::parseArg(iSpec.type, iValue.as<std::string>());
}

auto parseCommand(const YAML::Node& iNode, ScenarioStep& oStep) -> std::string {
	oStep.command = iNode["command"].as<std::string>();
	static const commands::CommandRegistry s_registry;
	const auto* spec = s_registry.find(oStep.command);
	if (spec == nullptr)
		return std::format("unknown command `{}`", oStep.command);
	const auto args = iNode["args"];
	if (!args)
		return "";
	if (!args.IsMap())
		return "`args` is a map of the command arguments";
	for (const auto& item: args) {
		const auto key = item.first.as<std::string>();
		const auto arg = std::ranges::find(spec->args, key, &commands::ArgSpec::name);
		if (arg == spec->args.end())
			return std::format("`{}` has no argument `{}`", oStep.command, key);
		const auto value = parseArgValue(*arg, item.second);
		if (!value)
			return std::format("`{}`: `{}` has the wrong type", oStep.command, key);
		oStep.args.set(key, *value);
	}
	return "";
}

auto parseActions(const YAML::Node& iNode, ScenarioStep& oStep) -> std::string {
	if (!iNode.IsMap())
		return "`actions` is a map: `frames` and the held actions (`jump: true`, `move: [1, 0]`, `zoom: 0.5`)";
	oStep.frames = 1;
	for (const auto& item: iNode) {
		const auto name = item.first.as<std::string>();
		if (name == "frames") {
			oStep.frames = item.second.as<uint32_t>();
			continue;
		}
		ScenarioActionHold hold{.name = name};
		if (item.second.IsSequence()) {
			if (item.second.size() != 2)
				return std::format("action `{}`: a vector is `[x, y]`", name);
			hold.value = math::vec2{item.second[0].as<float>(), item.second[1].as<float>()};
		} else if (const auto text = item.second.as<std::string>(); text == "true" || text == "false") {
			hold.value = math::vec2{text == "true" ? 1.f : 0.f, 0.f};
		} else {
			hold.value = math::vec2{item.second.as<float>(), 0.f};
		}
		oStep.actions.push_back(std::move(hold));
	}
	return oStep.frames == 0 ? "actions frames must be positive" : "";
}

auto parseStep(const YAML::Node& iNode, ScenarioStep& oStep) -> std::string {
	oStep.line = iNode.Mark().line + 1;
	if (const auto frames = iNode["frames"]; frames && !iNode["input"]) {
		oStep.frames = frames.as<uint32_t>();
		return oStep.frames == 0 ? "frames must be positive" : "";
	}
	if (const auto actions = iNode["actions"]; actions)
		return parseActions(actions, oStep);
	if (const auto input = iNode["input"]; input) {
		oStep.frames = input["frames"] ? input["frames"].as<uint32_t>() : 1;
		const auto addKey = [&oStep](const YAML::Node& iKey) -> bool {
			const auto code = ScenarioTest::parseKey(iKey.as<std::string>());
			if (code)
				oStep.keys.push_back(*code);
			return code.has_value();
		};
		if (const auto key = input["key"]; key && !addKey(key))
			return std::format("unknown key '{}'", key.as<std::string>());
		if (const auto keys = input["keys"]; keys)
			for (const auto& key: keys)
				if (!addKey(key))
					return std::format("unknown key '{}'", key.as<std::string>());
		if (const auto mouse = input["mouse"]; mouse) {
			const auto button = input::names::parseMouseButton(mouse.as<std::string>());
			if (!button)
				return std::format("unknown mouse button '{}'", mouse.as<std::string>());
			oStep.mouseButtons.push_back(*button);
		}
		if (const auto pos = input["mouse_pos"]; pos && pos.IsSequence() && pos.size() == 2)
			oStep.mousePos = math::vec2{pos[0].as<float>(), pos[1].as<float>()};
		if (const auto gamepad = input["gamepad"]; gamepad)
			if (auto error = parseGamepad(gamepad, oStep); !error.empty())
				return error;
		return oStep.frames == 0 ? "input frames must be positive" : "";
	}
	if (const auto undo = iNode["undo"]; undo) {
		oStep.undo = undo.as<uint32_t>();
		return oStep.undo == 0 ? "undo count must be positive" : "";
	}
	if (const auto redo = iNode["redo"]; redo) {
		oStep.redo = redo.as<uint32_t>();
		return oStep.redo == 0 ? "redo count must be positive" : "";
	}
	if (iNode["command"])
		return parseCommand(iNode, oStep);
	if (const auto macro = iNode["macro"]; macro) {
		oStep.macro = macro.as<std::string>();
		return oStep.macro.empty() ? "`macro` names a .owlmacro file" : "";
	}
	const auto expect = iNode["expect"];
	if (!expect || !expect.IsMap())
		return "a step is `frames`, `input`, `expect`, `command`, `macro`, `undo` or `redo`";
	if (const auto gameState = expect["gamestate"]; gameState) {
		oStep.gameState = gameState.as<std::string>();
		ScenarioCheck check;
		if (!parseCheck("value", expect, check))
			return "a game-state expectation needs `equals`, `greater` or `less`";
		oStep.checks.push_back(std::move(check));
		return "";
	}
	if (const auto setting = expect["setting"]; setting) {
		oStep.setting = setting.as<std::string>();
		ScenarioCheck check;
		if (!parseCheck("value", expect, check))
			return "a setting expectation needs `equals`, `greater` or `less`";
		oStep.checks.push_back(std::move(check));
		return "";
	}
	if (const auto entity = expect["entity"]; entity)
		oStep.entity = entity.as<std::string>();
	else if (const auto player = expect["player"]; player && player.as<bool>())
		oStep.player = true;
	else
		return "an expectation names an `entity`, the `player`, a `gamestate` key or a `setting`";
	for (const auto& item: expect) {
		const auto field = item.first.as<std::string>();
		if (field == "entity" || field == "player")
			continue;
		ScenarioCheck check;
		if (!parseCheck(field, item.second, check))
			return std::format("`{}` needs `equals`, `greater` or `less`", field);
		oStep.checks.push_back(std::move(check));
	}
	return oStep.checks.empty() ? "an expectation without any check" : "";
}

auto findEntity(const ScenarioStep& iStep, const scene::Scene& iScene) -> scene::Entity {
	if (iStep.player)
		return iScene.getPrimaryPlayer();
	for (const auto view = iScene.registry.view<scene::component::Tag>(); const auto entity: view)
		if (view.get<scene::component::Tag>(entity).tag == iStep.entity)
			return {entity, const_cast<scene::Scene*>(&iScene)};
	return {};
}

auto entityField(const scene::Scene& iScene, const scene::Entity& iEntity, const std::string& iField)
		-> std::optional<double> {
	if (iField == "exists")
		return static_cast<bool>(iEntity) ? 1.0 : 0.0;
	if (!iEntity)
		return std::nullopt;
	const auto world = iScene.getWorldTransform(iEntity);
	const auto axis = [&iField]() -> int {
		if (iField.ends_with(".x"))
			return 0;
		if (iField.ends_with(".y"))
			return 1;
		return iField.ends_with(".z") ? 2 : -1;
	}();
	if (axis < 0)
		return std::nullopt;
	if (iField.starts_with("translation."))
		return static_cast<double>(world.translation()[static_cast<size_t>(axis)]);
	if (iField.starts_with("rotation."))
		return static_cast<double>(world.rotation()[static_cast<size_t>(axis)]);
	if (iField.starts_with("scale."))
		return static_cast<double>(world.scale()[static_cast<size_t>(axis)]);
	return std::nullopt;
}

auto compare(const ScenarioCheck& iCheck, const double iValue) -> bool {
	switch (iCheck.op) {
		case ScenarioCheck::Op::Equals:
			return std::abs(iValue - iCheck.number) <= iCheck.tolerance;
		case ScenarioCheck::Op::Greater:
			return iValue > iCheck.number;
		case ScenarioCheck::Op::Less:
			return iValue < iCheck.number;
	}
	return false;
}

auto opName(const ScenarioCheck::Op iOp) -> std::string_view {
	switch (iOp) {
		case ScenarioCheck::Op::Equals:
			return "equal to";
		case ScenarioCheck::Op::Greater:
			return "greater than";
		case ScenarioCheck::Op::Less:
			return "less than";
	}
	return "";
}


// A game-state or settings value against the one check of the step.
auto checkValue(const ScenarioStep& iStep, const std::string_view iKind, const std::string& iKey,
				const std::optional<scene::GameState::Value>& iValue) -> std::string {
	const auto& check = iStep.checks.front();
	if (!iValue)
		return std::format("line {}: {} `{}` is not set", iStep.line, iKind, iKey);
	if (const auto* text = std::get_if<std::string>(&*iValue); text != nullptr)
		return check.text == *text ? ""
								   : std::format("line {}: {} `{}` is '{}', expected '{}'", iStep.line, iKind, iKey,
												 *text, check.text.value_or(""));
	const double number = std::visit(
			[](const auto& iItem) -> double {
				if constexpr (std::is_same_v<std::decay_t<decltype(iItem)>, std::string>)
					return 0.0;
				else
					return static_cast<double>(iItem);
			},
			*iValue);
	return compare(check, number) ? ""
								  : std::format("line {}: {} `{}` is {}, expected {} {}", iStep.line, iKind, iKey,
												number, opName(check.op), check.number);
}

auto resolveActions(ScenarioStep& ioStep, const input::ActionMap& iMap) -> std::string {
	session::HeldInput held{.keys = std::move(ioStep.keys),
							.mouseButtons = std::move(ioStep.mouseButtons),
							.gamepad = ioStep.gamepad};
	auto error = session::resolveActionHolds(iMap, ioStep.actions, held);
	ioStep.keys = std::move(held.keys);
	ioStep.mouseButtons = std::move(held.mouseButtons);
	ioStep.gamepad = held.gamepad;
	return error;
}
}// namespace

auto ScenarioTest::parseKey(const std::string& iName) -> std::optional<uint16_t> {
	return input::names::parseKey(iName);
}

auto ScenarioTest::load(const std::filesystem::path& iFile) -> expected<ScenarioTest, std::string> {
	ScenarioTest scenario;
	try {
		const auto root = YAML::LoadFile(iFile.string());
		const auto sceneNode = root["scene"];
		if (!sceneNode)
			return unexpected<std::string>{"no `scene`"};
		scenario.m_scene = weakly_canonical(absolute(iFile).parent_path() / sceneNode.as<std::string>());
		if (!exists(scenario.m_scene))
			return unexpected<std::string>{std::format("scene {} not found", scenario.m_scene.string())};
		const auto folder = absolute(iFile).parent_path();
		if (const auto projectNode = root["project"]; projectNode) {
			scenario.m_project = weakly_canonical(folder / projectNode.as<std::string>());
			if (!exists(scenario.m_project / "owl_project.yml"))
				return unexpected<std::string>{std::format("project {} not found", scenario.m_project.string())};
		} else {
			scenario.m_project = session::findProjectDirectory(scenario.m_scene);
		}
		if (const auto assetsNode = root["assets"]; assetsNode) {
			const auto addFolder = [&](const std::string& iFolder) -> bool {
				scenario.m_assets.push_back(weakly_canonical(folder / iFolder));
				return exists(scenario.m_assets.back());
			};
			const bool found = assetsNode.IsSequence() ? std::all_of(assetsNode.begin(), assetsNode.end(),
																	 [&](const YAML::Node& iNode) -> bool {
																		 return addFolder(iNode.as<std::string>());
																	 })
													   : addFolder(assetsNode.as<std::string>());
			if (!found)
				return unexpected<std::string>{
						std::format("assets folder {} not found", scenario.m_assets.back().string())};
		}
		const double stepMs = root["timestep_ms"] ? root["timestep_ms"].as<double>() : 1000.0 / 60.0;
		scenario.m_step.forceUpdate(std::chrono::duration_cast<core::Timestep::duration>(
				std::chrono::duration<double, std::milli>{stepMs}));
		const auto steps = root["steps"];
		auto actionMap = input::ActionMap{}.withDefaults();
		if (!scenario.m_project.empty() && exists(scenario.m_project / input::ActionMap::fileName)) {
			auto projectMap = input::ActionMap::loadFile(scenario.m_project / input::ActionMap::fileName);
			if (!projectMap)
				return unexpected<std::string>{projectMap.error()};
			actionMap = projectMap->withDefaults();
		}
		if (!steps || !steps.IsSequence() || steps.size() == 0)
			return unexpected<std::string>{"no `steps`"};
		for (const auto& node: steps) {
			ScenarioStep step;
			if (const auto error = parseStep(node, step); !error.empty())
				return unexpected<std::string>{std::format("line {}: {}", step.line, error)};
			if (const auto error = resolveActions(step, actionMap); !error.empty())
				return unexpected<std::string>{std::format("line {}: {}", step.line, error)};
			if (!step.macro.empty()) {
				step.macro = weakly_canonical(absolute(iFile).parent_path() / step.macro);
				if (!exists(step.macro))
					return unexpected<std::string>{
							std::format("line {}: macro {} not found", step.line, step.macro.string())};
			}
			scenario.m_steps.push_back(std::move(step));
		}
	} catch (const std::exception& iEx) { return unexpected<std::string>{iEx.what()}; }
	return scenario;
}

auto ScenarioTest::runStep(const ScenarioStep& iStep, const shared<scene::Scene>& iScene) -> std::string {
	for (uint32_t i = 0; i < iStep.undo; ++i) {
		if (!m_undo->canUndo())
			return std::format("line {}: nothing left to undo", iStep.line);
		m_undo->undo(*iScene);
	}
	for (uint32_t i = 0; i < iStep.redo; ++i) {
		if (!m_undo->canRedo())
			return std::format("line {}: nothing left to redo", iStep.line);
		m_undo->redo(*iScene);
	}
	if (iStep.undo > 0 || iStep.redo > 0)
		return "";
	if (!iStep.macro.empty()) {
		const auto macro = commands::Macro::load(iStep.macro);
		if (!macro)
			return std::format("line {}: {}", iStep.line, macro.error().message);
		const commands::CommandContext context{.scene = iScene,
											   .undo = m_undo.get(),
											   .playing = false,
											   .source = commands::CommandSource::Scenario};
		if (const auto report =
					macro->replay(m_commands, context, std::format("Macro {}", iStep.macro.filename().string()));
			!report)
			return std::format("line {}: macro {}: {}", iStep.line, iStep.macro.filename().string(),
							   report.error().message);
		return "";
	}
	if (!iStep.command.empty()) {
		if (const auto result = m_commands.execute(iStep.command, iStep.args, iScene, *m_undo); !result)
			return std::format("line {}: {}", iStep.line, result.error().message);
		return "";
	}
	return check(iStep, *iScene);
}

auto ScenarioTest::check(const ScenarioStep& iStep, const scene::Scene& iScene) -> std::string {
	if (!iStep.gameState.empty())
		return checkValue(iStep, "game-state", iStep.gameState, iScene.getGameState().get(iStep.gameState));
	if (!iStep.setting.empty()) {
		auto* context = iScene.getEngineContext();
		return checkValue(iStep, "setting", iStep.setting,
						  context != nullptr ? context->getSettings().get(iStep.setting) : std::nullopt);
	}
	const auto entity = findEntity(iStep, iScene);
	const std::string who = iStep.player ? std::string{"the player"} : std::format("`{}`", iStep.entity);
	for (const auto& check: iStep.checks) {
		const auto value = entityField(iScene, entity, check.field);
		if (!value)
			return entity || check.field == "exists"
						   ? std::format("line {}: unknown field `{}`", iStep.line, check.field)
						   : std::format("line {}: {} not found", iStep.line, who);
		if (!compare(check, *value))
			return std::format("line {}: {} {} is {}, expected {} {}", iStep.line, who, check.field, *value,
							   opName(check.op), check.number);
	}
	return "";
}

auto ScenarioTest::beginFrame(const shared<scene::Scene>& iScene) -> bool {
	if (m_framesLeft > 0) {
		--m_framesLeft;
		if (m_framesLeft > 0)
			return false;
		++m_index;
	}
	while (m_index < m_steps.size() && m_steps[m_index].frames == 0) {
		const auto& step = m_steps[m_index];
		++m_checks;
		if (const auto failure = runStep(step, iScene); !failure.empty()) {
			++m_failures;
			OWL_ERROR("Scenario: {}.", failure)
		}
		++m_index;
	}
	input::Input::resetInjection();
	if (m_index >= m_steps.size())
		return true;
	const auto& step = m_steps[m_index];
	session::injectHeldInput({.keys = step.keys, .mouseButtons = step.mouseButtons, .gamepad = step.gamepad});
	if (step.mousePos)
		input::Input::injectMousePos(*step.mousePos);
	m_framesLeft = step.frames;
	return false;
}

}// namespace owl::nest::runner
