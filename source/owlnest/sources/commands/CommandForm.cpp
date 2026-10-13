/**
 * @file CommandForm.cpp
 * @author Silmaen
 * @date 13/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "CommandForm.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace owl::nest::commands {

namespace {

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto valueText(const ArgValue& iValue) -> std::string {
	return std::visit(
			[](const auto& iItem) -> std::string {
				using Type = std::decay_t<decltype(iItem)>;
				if constexpr (std::is_same_v<Type, bool>)
					return iItem ? "true" : "false";
				else if constexpr (std::is_same_v<Type, std::string>)
					return iItem;
				else if constexpr (std::is_same_v<Type, int64_t> || std::is_same_v<Type, double>)
					return std::format("{}", iItem);
				else if constexpr (std::is_same_v<Type, math::vec2>)
					return std::format("({}, {})", iItem.x(), iItem.y());
				else if constexpr (std::is_same_v<Type, math::vec3>)
					return std::format("({}, {}, {})", iItem.x(), iItem.y(), iItem.z());
				else
					return std::format("({}, {}, {}, {})", iItem.x(), iItem.y(), iItem.z(), iItem.w());
			},
			iValue);
}

}// namespace

CommandForm::CommandForm(const CommandSpec& iSpec) : m_command{iSpec.name} {
	for (const auto& arg: iSpec.args) {
		const bool hasDefault = arg.defaultValue.has_value();
		m_fields.push_back({.spec = arg,
							.set = arg.required || hasDefault,
							.value = hasDefault ? *arg.defaultValue : initialValue(arg)});
	}
}

void CommandForm::setDefaultEntity(const core::UUID iEntity) {
	if (iEntity == core::UUID{0})
		return;
	for (auto& field: m_fields) {
		if (field.spec.type != ArgType::Entity || !field.spec.required)
			continue;
		if (const auto* uuid = std::get_if<int64_t>(&field.value); uuid != nullptr && *uuid == 0)
			field.value = static_cast<int64_t>(static_cast<uint64_t>(iEntity));
	}
}

auto CommandForm::buildArgs() const -> expected<CommandArgs, CommandFailure> {
	CommandArgs args;
	for (const auto& field: m_fields) {
		if (!field.set && !field.spec.required)
			continue;
		const auto& name = field.spec.name;
		if (field.spec.type == ArgType::Entity) {
			if (const auto* uuid = std::get_if<int64_t>(&field.value); uuid != nullptr && *uuid == 0)
				return fail(CommandError::MissingArgument, std::format("pick an entity for `{}`", name));
		}
		if (const auto* text = std::get_if<std::string>(&field.value);
			text != nullptr && text->empty() && field.spec.required)
			return fail(CommandError::MissingArgument, std::format("`{}` is empty", name));
		if (field.spec.type == ArgType::Enum &&
			!std::ranges::contains(field.spec.choices, std::get<std::string>(field.value)))
			return fail(CommandError::InvalidArgument, std::format("`{}` is not one of the choices", name));
		args.set(name, field.value);
	}
	return args;
}

auto CommandForm::initialValue(const ArgSpec& iSpec) -> ArgValue {
	switch (iSpec.type) {
		case ArgType::Bool:
			return false;
		case ArgType::Integer:
		case ArgType::Entity:
			return int64_t{0};
		case ArgType::Number:
			return 0.0;
		case ArgType::Text:
			return std::string{};
		case ArgType::Enum:
			return iSpec.choices.empty() ? std::string{} : iSpec.choices.front();
		case ArgType::Vec2:
			return math::vec2{0.f, 0.f};
		case ArgType::Vec3:
			return math::vec3{0.f, 0.f, 0.f};
		case ArgType::Vec4:
			return math::vec4{0.f, 0.f, 0.f, 0.f};
		case ArgType::Color:
			return math::vec4{1.f, 1.f, 1.f, 1.f};
	}
	return std::string{};
}

auto CommandForm::describeData(const ResultData& iData) -> std::string {
	std::string text;
	for (const auto& [key, value]: iData)
		text += std::format("{}{}: {}", text.empty() ? "" : ", ", key, valueText(value));
	return text;
}

}// namespace owl::nest::commands
