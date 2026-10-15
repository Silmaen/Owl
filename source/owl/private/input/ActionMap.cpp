/**
 * @file ActionMap.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "input/ActionMap.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "input/InputNames.h"
#include "input/KeyCodes.h"
#include "input/MouseCode.h"
#include "platform/AtomicFile.h"
#include "platform/FileUtils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <utility>

namespace owl::input {

namespace {

constexpr std::array<core::MigrationStep, 0> g_actionMapMigrations{};
constexpr core::DocumentFormat g_actionMapFormat{.name = "InputActions", .migrations = g_actionMapMigrations};

constexpr std::string_view g_kindButton = "button";
constexpr std::string_view g_kindAxis = "axis";
constexpr std::string_view g_kindVector = "vector";

constexpr std::array<std::pair<ResponseCurve, std::string_view>, 4> g_curveNames{{
		{ResponseCurve::Linear, "linear"},
		{ResponseCurve::Quadratic, "quadratic"},
		{ResponseCurve::Cubic, "cubic"},
		{ResponseCurve::Custom, "custom"},
}};

constexpr std::array<std::pair<BindingSource, std::string_view>, 5> g_sourceKeys{{
		{BindingSource::Key, "key"},
		{BindingSource::MouseButton, "mouse_button"},
		{BindingSource::GamepadButton, "gamepad_button"},
		{BindingSource::GamepadAxis, "gamepad_axis"},
		{BindingSource::GamepadStick, "gamepad_stick"},
}};

auto kindName(const ActionKind iKind) -> std::string_view {
	switch (iKind) {
		case ActionKind::Button:
			return g_kindButton;
		case ActionKind::Axis:
			return g_kindAxis;
		case ActionKind::Vector:
			return g_kindVector;
	}
	return g_kindButton;
}

auto differs(const float iA, const float iB) -> bool { return std::abs(iA - iB) > 1e-6f; }

auto isShaped(const BindingSource iSource) -> bool {
	return iSource == BindingSource::GamepadAxis || iSource == BindingSource::GamepadStick;
}

auto sourceCodeName(const ActionBinding& iBinding) -> std::string {
	switch (iBinding.source) {
		case BindingSource::Key:
			return names::keyName(iBinding.code);
		case BindingSource::MouseButton:
			return names::mouseButtonName(static_cast<MouseCode>(iBinding.code));
		case BindingSource::GamepadButton:
			return names::gamepadButtonName(static_cast<GamepadButton>(iBinding.code));
		case BindingSource::GamepadAxis:
			return names::gamepadAxisName(static_cast<GamepadAxis>(iBinding.code));
		case BindingSource::GamepadStick:
			return iBinding.code == 0 ? "left" : "right";
	}
	return {};
}

auto sourceKey(const BindingSource iSource) -> std::string_view {
	return std::ranges::find(g_sourceKeys, iSource, &std::pair<BindingSource, std::string_view>::first)->second;
}

void emitBinding(YAML::Emitter& ioOut, const ActionBinding& iBinding) {
	ioOut << YAML::Flow << YAML::BeginMap;
	ioOut << YAML::Key << std::string{sourceKey(iBinding.source)} << YAML::Value << sourceCodeName(iBinding);
	if (iBinding.pad >= 0)
		ioOut << YAML::Key << "pad" << YAML::Value << iBinding.pad;
	if (iBinding.component != BindingComponent::Both)
		ioOut << YAML::Key << "component" << YAML::Value << (iBinding.component == BindingComponent::X ? "x" : "y");
	if (differs(iBinding.scale, 1.f))
		ioOut << YAML::Key << "scale" << YAML::Value << iBinding.scale;
	if (isShaped(iBinding.source)) {
		const AxisResponse defaults;
		if (differs(iBinding.response.deadZone, defaults.deadZone))
			ioOut << YAML::Key << "dead_zone" << YAML::Value << iBinding.response.deadZone;
		if (iBinding.response.curve != defaults.curve) {
			const auto* const found = std::ranges::find(g_curveNames, iBinding.response.curve,
														&std::pair<ResponseCurve, std::string_view>::first);
			ioOut << YAML::Key << "curve" << YAML::Value << std::string{found->second};
		}
		if (iBinding.response.curve == ResponseCurve::Custom)
			ioOut << YAML::Key << "exponent" << YAML::Value << iBinding.response.exponent;
	}
	ioOut << YAML::EndMap;
}

auto parseSource(const YAML::Node& iNode, ActionBinding& oBinding) -> std::string {
	bool found = false;
	for (const auto& [source, key]: g_sourceKeys) {
		const auto value = iNode[std::string{key}];
		if (!value)
			continue;
		if (found)
			return "a binding has several sources";
		found = true;
		oBinding.source = source;
		const auto text = value.as<std::string>();
		std::optional<uint16_t> code;
		switch (source) {
			case BindingSource::Key:
				code = names::parseKey(text);
				break;
			case BindingSource::MouseButton:
				code = names::parseMouseButton(text);
				break;
			case BindingSource::GamepadButton:
				code = names::parseGamepadButton(text);
				break;
			case BindingSource::GamepadAxis:
				code = names::parseGamepadAxis(text);
				break;
			case BindingSource::GamepadStick:
				if (text == "left" || text == "0")
					code = 0;
				else if (text == "right" || text == "1")
					code = 1;
				break;
		}
		if (!code)
			return std::format("unknown {} '{}'", key, text);
		oBinding.code = *code;
	}
	return found ? std::string{}
				 : "a binding has no source (key, mouse_button, gamepad_button, gamepad_axis or "
				   "gamepad_stick)";
}

auto parseBinding(const YAML::Node& iNode, ActionBinding& oBinding) -> std::string {
	if (!iNode.IsMap())
		return "a binding is a map";
	constexpr std::array<std::string_view, 12> known{"key",           "mouse_button", "gamepad_button", "gamepad_axis",
													 "gamepad_stick", "pad",          "component",      "scale",
													 "dead_zone",     "curve",        "exponent",       "description"};
	for (const auto& item: iNode) {
		if (const auto key = item.first.as<std::string>(); std::ranges::find(known, key) == known.end())
			return std::format("unknown binding entry '{}'", key);
	}
	if (const auto error = parseSource(iNode, oBinding); !error.empty())
		return error;
	if (const auto pad = iNode["pad"]; pad) {
		oBinding.pad = pad.as<int32_t>();
		if (oBinding.pad < -1 || oBinding.pad >= static_cast<int32_t>(g_GamepadCount))
			return std::format("gamepad slot {} out of range", oBinding.pad);
	}
	if (const auto component = iNode["component"]; component) {
		const auto text = component.as<std::string>();
		if (text == "x")
			oBinding.component = BindingComponent::X;
		else if (text == "y")
			oBinding.component = BindingComponent::Y;
		else
			return std::format("unknown component '{}' (x or y)", text);
	}
	if (const auto scale = iNode["scale"]; scale)
		oBinding.scale = scale.as<float>();
	if (const auto deadZone = iNode["dead_zone"]; deadZone) {
		oBinding.response.deadZone = deadZone.as<float>();
		if (oBinding.response.deadZone < 0.f || oBinding.response.deadZone >= 1.f)
			return "dead_zone is in [0, 1)";
	}
	if (const auto curve = iNode["curve"]; curve) {
		const auto text = curve.as<std::string>();
		const auto* const found =
				std::ranges::find(g_curveNames, text, &std::pair<ResponseCurve, std::string_view>::second);
		if (found == g_curveNames.end())
			return std::format("unknown curve '{}'", text);
		oBinding.response.curve = found->first;
	}
	if (const auto exponent = iNode["exponent"]; exponent)
		oBinding.response.exponent = exponent.as<float>();
	return {};
}

auto parseAction(const YAML::Node& iNode, ActionDefinition& oAction) -> std::string {
	if (!iNode.IsMap())
		return "an action is a map (`name`, `type`, `bindings`)";
	if (!iNode["name"])
		return "an action has no `name`";
	oAction.name = iNode["name"].as<std::string>();
	if (oAction.name.empty())
		return "an action has an empty name";
	const auto type = iNode["type"] ? iNode["type"].as<std::string>() : std::string{g_kindButton};
	if (type == g_kindButton)
		oAction.kind = ActionKind::Button;
	else if (type == g_kindAxis)
		oAction.kind = ActionKind::Axis;
	else if (type == g_kindVector)
		oAction.kind = ActionKind::Vector;
	else
		return std::format("action '{}': unknown type '{}' (button, axis or vector)", oAction.name, type);
	if (const auto bindings = iNode["bindings"]; bindings) {
		if (!bindings.IsSequence())
			return std::format("action '{}': `bindings` is a list", oAction.name);
		for (const auto& node: bindings) {
			ActionBinding binding;
			if (const auto error = parseBinding(node, binding); !error.empty())
				return std::format("action '{}': {}", oAction.name, error);
			oAction.bindings.push_back(binding);
		}
	}
	return {};
}

auto keyBind(const KeyCode iKey, const BindingComponent iComponent = BindingComponent::Both, const float iScale = 1.f)
		-> ActionBinding {
	return {.source = BindingSource::Key, .code = iKey, .component = iComponent, .scale = iScale};
}

auto mouseBind(const MouseCode iButton) -> ActionBinding {
	return {.source = BindingSource::MouseButton, .code = iButton};
}

auto padButtonBind(const GamepadButton iButton) -> ActionBinding {
	return {.source = BindingSource::GamepadButton, .code = iButton};
}

auto padAxisBind(const GamepadAxis iAxis) -> ActionBinding {
	return {.source = BindingSource::GamepadAxis, .code = iAxis};
}

auto padStickBind(const gamepad::Stick iStick) -> ActionBinding {
	return {.source = BindingSource::GamepadStick, .code = static_cast<uint16_t>(iStick)};
}

}// namespace

auto ActionMap::format() -> const core::DocumentFormat& { return g_actionMapFormat; }

auto ActionMap::createDefault() -> ActionMap {
	using enum BindingComponent;
	ActionMap map;
	map.m_actions = {
			{"move",
			 ActionKind::Vector,
			 {keyBind(key::W, Y, -1.f), keyBind(key::S, Y), keyBind(key::A, X, -1.f), keyBind(key::D, X),
			  padStickBind(gamepad::Stick::Left)}},
			{"look",
			 ActionKind::Vector,
			 {keyBind(key::Up, Y, -1.f), keyBind(key::Down, Y), keyBind(key::Left, X, -1.f), keyBind(key::Right, X),
			  padStickBind(gamepad::Stick::Right)}},
			{"jump", ActionKind::Button, {keyBind(key::Space), padButtonBind(gamepad::A)}},
			{"descend", ActionKind::Button, {keyBind(key::LeftShift), padButtonBind(gamepad::LeftBumper)}},
			{"sprint", ActionKind::Button, {keyBind(key::LeftShift), padButtonBind(gamepad::LeftThumb)}},
			{"boost", ActionKind::Button, {keyBind(key::J), padButtonBind(gamepad::Y)}},
			{"interact", ActionKind::Button, {keyBind(key::E), padButtonBind(gamepad::X)}},
			{"primary", ActionKind::Button, {mouseBind(mouse::ButtonLeft), padAxisBind(gamepad::RightTrigger)}},
			{"secondary", ActionKind::Button, {mouseBind(mouse::ButtonRight), padAxisBind(gamepad::LeftTrigger)}},
			{"ui_up", ActionKind::Button, {keyBind(key::Up), padButtonBind(gamepad::DpadUp)}},
			{"ui_down", ActionKind::Button, {keyBind(key::Down), padButtonBind(gamepad::DpadDown)}},
			{"ui_left", ActionKind::Button, {keyBind(key::Left), padButtonBind(gamepad::DpadLeft)}},
			{"ui_right", ActionKind::Button, {keyBind(key::Right), padButtonBind(gamepad::DpadRight)}},
			{"ui_accept", ActionKind::Button, {keyBind(key::Enter), padButtonBind(gamepad::A)}},
			{"ui_cancel", ActionKind::Button, {keyBind(key::Escape), padButtonBind(gamepad::B)}},
	};
	return map;
}

auto ActionMap::getDefault() -> shared<const ActionMap> {
	static const shared<const ActionMap> defaultMap = mkShared<const ActionMap>(createDefault());
	return defaultMap;
}

auto ActionMap::fromYaml(const std::string_view iText, const std::string_view iSourceName)
		-> expected<ActionMap, std::string> {
	try {
		YAML::Node root = YAML::Load(std::string{iText});
		if (!root.IsMap()) {
			OWL_CORE_WARN("ActionMap: {} is not a YAML map.", iSourceName)
			return unexpected{std::format("{}: the document is not a map", iSourceName)};
		}
		if (const auto upgraded = core::upgradeYamlDocument(g_actionMapFormat, root, iSourceName); !upgraded) {
			OWL_CORE_WARN("ActionMap: Cannot read {}: {}.", iSourceName, describe(upgraded.error()))
			return unexpected{std::format("{}: {}", iSourceName, describe(upgraded.error()))};
		}
		ActionMap map;
		if (const auto actions = root["actions"]; actions) {
			if (!actions.IsSequence())
				return unexpected{std::format("{}: `actions` is a list", iSourceName)};
			for (const auto& node: actions) {
				ActionDefinition action;
				if (const auto error = parseAction(node, action); !error.empty()) {
					OWL_CORE_WARN("ActionMap: {}: {}.", iSourceName, error)
					return unexpected{std::format("{}: {}", iSourceName, error)};
				}
				if (map.find(action.name) != nullptr) {
					OWL_CORE_WARN("ActionMap: {}: action '{}' is defined twice.", iSourceName, action.name)
					return unexpected{std::format("{}: action '{}' is defined twice", iSourceName, action.name)};
				}
				map.m_actions.push_back(std::move(action));
			}
		}
		return map;
	} catch (const YAML::Exception& iEx) {
		OWL_CORE_WARN("ActionMap: Cannot parse {}: {}.", iSourceName, iEx.what())
		return unexpected{std::format("{}: {}", iSourceName, iEx.what())};
	}
}

auto ActionMap::loadFile(const std::filesystem::path& iFile) -> expected<ActionMap, std::string> {
	if (!exists(iFile)) {
		OWL_CORE_WARN("ActionMap: File {} not found.", iFile.string())
		return unexpected{std::format("{}: file not found", iFile.string())};
	}
	return fromYaml(platform::fileToString(iFile), iFile.string());
}

auto ActionMap::toYaml() const -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	core::emitFormatVersion(out, g_actionMapFormat);
	out << YAML::Key << "actions" << YAML::Value << YAML::BeginSeq;
	for (const auto& action: m_actions) {
		out << YAML::BeginMap;
		out << YAML::Key << "name" << YAML::Value << action.name;
		out << YAML::Key << "type" << YAML::Value << std::string{kindName(action.kind)};
		out << YAML::Key << "bindings" << YAML::Value << YAML::BeginSeq;
		for (const auto& binding: action.bindings) emitBinding(out, binding);
		out << YAML::EndSeq << YAML::EndMap;
	}
	out << YAML::EndSeq << YAML::EndMap;
	return std::string{out.c_str()} + "\n";
}

auto ActionMap::saveFile(const std::filesystem::path& iFile) const -> bool {
	if (const auto written = platform::writeFileAtomic(iFile, toYaml()); !written) {
		OWL_CORE_ERROR("ActionMap: Cannot save {}: {}.", iFile.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto ActionMap::indexOf(const std::string_view iName) const -> std::optional<std::size_t> {
	const auto found = std::ranges::find(m_actions, iName, &ActionDefinition::name);
	if (found == m_actions.end())
		return std::nullopt;
	return static_cast<std::size_t>(found - m_actions.begin());
}

auto ActionMap::find(const std::string_view iName) const -> const ActionDefinition* {
	const auto index = indexOf(iName);
	return index ? &m_actions[*index] : nullptr;
}

auto ActionMap::setAction(const ActionDefinition& iAction) -> bool {
	if (iAction.name.empty()) {
		OWL_CORE_WARN("ActionMap: An action needs a name.")
		return false;
	}
	if (const auto index = indexOf(iAction.name); index)
		m_actions[*index] = iAction;
	else
		m_actions.push_back(iAction);
	return true;
}

auto ActionMap::removeAction(const std::string_view iName) -> bool {
	const auto index = indexOf(iName);
	if (!index)
		return false;
	m_actions.erase(m_actions.begin() + static_cast<std::ptrdiff_t>(*index));
	return true;
}

auto ActionMap::withDefaults() const -> ActionMap {
	ActionMap result = *this;
	for (const auto& action: createDefault().m_actions)
		if (result.find(action.name) == nullptr)
			result.m_actions.push_back(action);
	return result;
}

}// namespace owl::input
