/**
 * @file ScenarioTest.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ScenarioTest.h"
#include "FrameBench.h"

#include <input/Input.h>
#include <input/KeyCodes.h>
#include <scene/Entity.h>
#include <scene/component/components.h>

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

struct NamedKey {
	std::string_view name;
	uint16_t code;
};

constexpr std::array g_namedKeys{
		NamedKey{"Space", input::key::Space},
		NamedKey{"Enter", input::key::Enter},
		NamedKey{"Escape", input::key::Escape},
		NamedKey{"Tab", input::key::Tab},
		NamedKey{"Left", input::key::Left},
		NamedKey{"Right", input::key::Right},
		NamedKey{"Up", input::key::Up},
		NamedKey{"Down", input::key::Down},
		NamedKey{"LeftShift", input::key::LeftShift},
		NamedKey{"LeftControl", input::key::LeftControl},
};

auto parseMouseButton(const std::string& iName) -> std::optional<uint8_t> {
	if (iName == "Left")
		return static_cast<uint8_t>(input::mouse::ButtonLeft);
	if (iName == "Right")
		return static_cast<uint8_t>(input::mouse::ButtonRight);
	if (iName == "Middle")
		return static_cast<uint8_t>(input::mouse::ButtonMiddle);
	return std::nullopt;
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

auto parseStep(const YAML::Node& iNode, ScenarioStep& oStep) -> std::string {
	oStep.line = iNode.Mark().line + 1;
	if (const auto frames = iNode["frames"]; frames && !iNode["input"]) {
		oStep.frames = frames.as<uint32_t>();
		return oStep.frames == 0 ? "frames must be positive" : "";
	}
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
			const auto button = parseMouseButton(mouse.as<std::string>());
			if (!button)
				return std::format("unknown mouse button '{}'", mouse.as<std::string>());
			oStep.mouseButtons.push_back(*button);
		}
		if (const auto pos = input["mouse_pos"]; pos && pos.IsSequence() && pos.size() == 2)
			oStep.mousePos = math::vec2{pos[0].as<float>(), pos[1].as<float>()};
		return oStep.frames == 0 ? "input frames must be positive" : "";
	}
	const auto expect = iNode["expect"];
	if (!expect || !expect.IsMap())
		return "a step is `frames`, `input` or `expect`";
	if (const auto gameState = expect["gamestate"]; gameState) {
		oStep.gameState = gameState.as<std::string>();
		ScenarioCheck check;
		if (!parseCheck("value", expect, check))
			return "a game-state expectation needs `equals`, `greater` or `less`";
		oStep.checks.push_back(std::move(check));
		return "";
	}
	if (const auto entity = expect["entity"]; entity)
		oStep.entity = entity.as<std::string>();
	else if (const auto player = expect["player"]; player && player.as<bool>())
		oStep.player = true;
	else
		return "an expectation names an `entity`, the `player` or a `gamestate` key";
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

}// namespace

auto ScenarioTest::parseKey(const std::string& iName) -> std::optional<uint16_t> {
	if (iName.size() == 1 && iName.front() >= 'A' && iName.front() <= 'Z')
		return static_cast<uint16_t>(input::key::A + (iName.front() - 'A'));
	if (iName.size() == 2 && iName.front() == 'D' && iName.back() >= '0' && iName.back() <= '9')
		return static_cast<uint16_t>(input::key::D0 + (iName.back() - '0'));
	for (const auto& [name, code]: g_namedKeys)
		if (name == iName)
			return code;
	if (!iName.empty() && iName.find_first_not_of("0123456789") == std::string::npos && iName.size() <= 3)
		return static_cast<uint16_t>(std::stoul(iName));
	return std::nullopt;
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
		scenario.m_project = findProject(scenario.m_scene);
		const double stepMs = root["timestep_ms"] ? root["timestep_ms"].as<double>() : 1000.0 / 60.0;
		scenario.m_step.forceUpdate(std::chrono::duration_cast<core::Timestep::duration>(
				std::chrono::duration<double, std::milli>{stepMs}));
		const auto steps = root["steps"];
		if (!steps || !steps.IsSequence() || steps.size() == 0)
			return unexpected<std::string>{"no `steps`"};
		for (const auto& node: steps) {
			ScenarioStep step;
			if (const auto error = parseStep(node, step); !error.empty())
				return unexpected<std::string>{std::format("line {}: {}", step.line, error)};
			scenario.m_steps.push_back(std::move(step));
		}
	} catch (const std::exception& iEx) { return unexpected<std::string>{iEx.what()}; }
	return scenario;
}

auto ScenarioTest::check(const ScenarioStep& iStep, const scene::Scene& iScene) -> std::string {
	if (!iStep.gameState.empty()) {
		const auto& check = iStep.checks.front();
		const auto value = iScene.getGameState().get(iStep.gameState);
		if (!value)
			return std::format("line {}: game-state `{}` is not set", iStep.line, iStep.gameState);
		if (const auto* text = std::get_if<std::string>(&*value); text != nullptr)
			return check.text == *text ? ""
									   : std::format("line {}: game-state `{}` is '{}', expected '{}'", iStep.line,
													 iStep.gameState, *text, check.text.value_or(""));
		const double number = std::visit(
				[](const auto& iValue) -> double {
					if constexpr (std::is_same_v<std::decay_t<decltype(iValue)>, std::string>)
						return 0.0;
					else
						return static_cast<double>(iValue);
				},
				*value);
		return compare(check, number) ? ""
									  : std::format("line {}: game-state `{}` is {}, expected {} {}", iStep.line,
													iStep.gameState, number, opName(check.op), check.number);
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

auto ScenarioTest::beginFrame(const scene::Scene& iScene) -> bool {
	if (m_framesLeft > 0) {
		--m_framesLeft;
		if (m_framesLeft > 0)
			return false;
		++m_index;
	}
	while (m_index < m_steps.size() && m_steps[m_index].frames == 0) {
		++m_checks;
		if (const auto failure = check(m_steps[m_index], iScene); !failure.empty()) {
			++m_failures;
			OWL_ERROR("Scenario: {}.", failure)
		}
		++m_index;
	}
	input::Input::resetInjection();
	if (m_index >= m_steps.size())
		return true;
	const auto& step = m_steps[m_index];
	for (const auto key: step.keys) input::Input::injectKey(key);
	for (const auto button: step.mouseButtons) input::Input::injectMouseButton(button);
	if (step.mousePos)
		input::Input::injectMousePos(*step.mousePos);
	m_framesLeft = step.frames;
	return false;
}

}// namespace owl::nest::runner
