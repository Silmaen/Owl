/**
 * @file BindingNodes.cpp
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "BindingNodes.h"

#include "script/LuaBindings.h"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::script::visual {

namespace {

auto pin(std::string iName, const PinType iType, std::string iDefault = {}, const bool iOptional = false) -> PinSpec {
	return {.name = std::move(iName),
			.type = iType,
			.defaultValue = std::move(iDefault),
			.optional = iOptional,
			.group = {}};
}

auto toPinType(const LuaType iType) -> PinType {
	switch (iType) {
		case LuaType::Boolean:
			return PinType::Boolean;
		case LuaType::Integer:
			return PinType::Integer;
		case LuaType::Number:
			return PinType::Number;
		case LuaType::String:
			return PinType::String;
		case LuaType::Table:
			return PinType::Table;
		case LuaType::Entity:
			return PinType::Entity;
		case LuaType::Any:
			return PinType::Any;
	}
	return PinType::Any;
}

// A binding that only reads engine state is pure: it has no execution pins and runs where its values are read.
auto isPure(const LuaBinding& iBinding) -> bool {
	constexpr std::array<std::string_view, 6> readers{"engine.version",  "time.delta",        "time.real_delta",
													  "time.tick_alpha", "scene.find_entity", "ui.find"};
	if (iBinding.returns.empty())
		return false;
	const auto& name = iBinding.name;
	if (name.starts_with("get_") || name.starts_with("is_") || name.starts_with("has_"))
		return true;
	const auto full = std::format("{}.{}", iBinding.table, iBinding.name);
	return std::ranges::find(readers, std::string_view{full}) != readers.end();
}

}// namespace

auto getBindingNodeTypes() -> std::vector<NodeType> {
	std::vector<NodeType> types;
	for (const auto& binding: getLuaBindings()) {
		NodeType type{.id = std::format("call.{}.{}", binding.table, binding.name),
					  .title = std::format("{}.{}", binding.table, binding.name),
					  .category = std::string{binding.table},
					  .description = std::string{binding.description},
					  .kind = NodeKind::Call,
					  .lua = std::format("{}.{}", binding.table, binding.name),
					  .inputs = {},
					  .outputs = {},
					  .pure = isPure(binding)};
		if (!type.pure) {
			type.inputs.push_back(pin("Exec", PinType::Exec));
			type.outputs.push_back(pin("Then", PinType::Exec));
		}
		for (const auto& param: binding.params)
			type.inputs.push_back(pin(std::string{param.name}, toPinType(param.type), {}, param.optional));
		for (const auto& ret: binding.returns) type.outputs.push_back(pin(std::string{ret.name}, toPinType(ret.type)));
		types.push_back(std::move(type));
	}
	return types;
}

}// namespace owl::script::visual
