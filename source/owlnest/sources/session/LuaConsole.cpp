/**
 * @file LuaConsole.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "LuaConsole.h"

#include "EditorSession.h"
#include "commands/CommandLine.h"
#include "commands/CommandRegistry.h"

#include <platform/FileUtils.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace owl::nest::session {

namespace {

auto numbers(const script::HostValue& iValue) -> std::optional<std::vector<double>> {
	if (const auto* list = std::get_if<std::vector<double>>(&iValue))
		return *list;
	return std::nullopt;
}

auto number(const script::HostValue& iValue) -> std::optional<double> {
	if (const auto* value = std::get_if<double>(&iValue))
		return *value;
	if (const auto* value = std::get_if<int64_t>(&iValue))
		return static_cast<double>(*value);
	return std::nullopt;
}

auto text(const script::HostValue& iValue) -> std::optional<std::string> {
	if (const auto* value = std::get_if<std::string>(&iValue))
		return *value;
	if (const auto* value = std::get_if<int64_t>(&iValue))
		return std::format("{}", *value);
	if (const auto* value = std::get_if<double>(&iValue))
		return std::format("{}", *value);
	if (const auto* value = std::get_if<bool>(&iValue))
		return *value ? "true" : "false";
	return std::nullopt;
}

// The value of a Lua argument as the command expects it, or why it does not fit.
auto toArgValue(const commands::ArgSpec& iSpec, const script::HostValue& iValue)
		-> expected<commands::ArgValue, std::string> {
	const auto wrong = [&iSpec](const char* iExpected) -> unexpected<std::string> {
		return unexpected<std::string>{std::format("`{}` takes {}", iSpec.name, iExpected)};
	};
	switch (iSpec.type) {
		case commands::ArgType::Bool:
			if (const auto* value = std::get_if<bool>(&iValue))
				return *value;
			return wrong("a boolean");
		case commands::ArgType::Integer:
			if (const auto value = number(iValue); value.has_value())
				return static_cast<int64_t>(std::llround(*value));
			return wrong("an integer");
		case commands::ArgType::Number:
			if (const auto value = number(iValue); value.has_value())
				return *value;
			return wrong("a number");
		case commands::ArgType::Text:
		case commands::ArgType::Enum:
			if (const auto value = text(iValue); value.has_value())
				return *value;
			return wrong("a string");
		case commands::ArgType::Entity:
			if (const auto* value = std::get_if<int64_t>(&iValue))
				return *value;
			if (const auto* value = std::get_if<std::string>(&iValue))
				return *value;
			return wrong("an entity id or tag");
		case commands::ArgType::Vec2:
			if (const auto list = numbers(iValue); list && list->size() == 2)
				return math::vec2{static_cast<float>((*list)[0]), static_cast<float>((*list)[1])};
			return wrong("{x, y}");
		case commands::ArgType::Vec3:
			if (const auto list = numbers(iValue); list && list->size() == 3)
				return math::vec3{static_cast<float>((*list)[0]), static_cast<float>((*list)[1]),
								  static_cast<float>((*list)[2])};
			return wrong("{x, y, z}");
		case commands::ArgType::Vec4:
		case commands::ArgType::Color:
			if (const auto list = numbers(iValue); list && (list->size() == 4 || list->size() == 3))
				return math::vec4{static_cast<float>((*list)[0]), static_cast<float>((*list)[1]),
								  static_cast<float>((*list)[2]),
								  list->size() == 4 ? static_cast<float>((*list)[3]) : 1.f};
			if (const auto* value = std::get_if<std::string>(&iValue);
				value != nullptr && iSpec.type == commands::ArgType::Color)
				return *value;
			return wrong("{r, g, b[, a]}");
	}
	return wrong("a value");
}

auto toDoubles(const std::initializer_list<float> iValues) -> std::vector<double> {
	std::vector<double> out;
	out.reserve(iValues.size());
	for (const float value: iValues) out.push_back(static_cast<double>(value));
	return out;
}

auto toHostValue(const commands::ArgValue& iValue) -> script::HostValue {
	return std::visit(
			[]<typename T>(const T& iItem) -> script::HostValue {
				if constexpr (std::is_same_v<T, math::vec2>)
					return toDoubles({iItem.x(), iItem.y()});
				else if constexpr (std::is_same_v<T, math::vec3>)
					return toDoubles({iItem.x(), iItem.y(), iItem.z()});
				else if constexpr (std::is_same_v<T, math::vec4>)
					return toDoubles({iItem.x(), iItem.y(), iItem.z(), iItem.w()});
				else
					return iItem;
			},
			iValue);
}

auto luaLiteral(const commands::ArgValue& iValue) -> std::string {
	return std::visit(
			[]<typename T>(const T& iItem) -> std::string {
				if constexpr (std::is_same_v<T, bool>)
					return iItem ? "true" : "false";
				else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, double>)
					return std::format("{}", iItem);
				else if constexpr (std::is_same_v<T, std::string>) {
					std::string out = "\"";
					for (const char character: iItem) {
						if (character == '"' || character == '\\')
							out += '\\';
						out += character == '\n' ? std::string{"\\n"} : std::string{character};
					}
					return out + "\"";
				} else if constexpr (std::is_same_v<T, math::vec2>)
					return std::format("{{{}, {}}}", iItem.x(), iItem.y());
				else if constexpr (std::is_same_v<T, math::vec3>)
					return std::format("{{{}, {}, {}}}", iItem.x(), iItem.y(), iItem.z());
				else
					return std::format("{{{}, {}, {}, {}}}", iItem.x(), iItem.y(), iItem.z(), iItem.w());
			},
			iValue);
}

auto toOutput(const script::ScriptConsole::Result& iResult) -> std::vector<ConsoleOutput> {
	std::vector<ConsoleOutput> output;
	output.reserve(iResult.output.size());
	for (const auto& line: iResult.output)
		output.push_back(
				{.kind = iResult.status == script::ScriptConsole::Status::Error && &line == &iResult.output.back()
								 ? ConsoleLineKind::Error
								 : ConsoleLineKind::Result,
				 .text = line,
				 .journalIndex = 0});
	return output;
}

}// namespace

LuaConsole::LuaConsole(EditorSession& ioSession, Console& ioCommandConsole)
	: m_session{ioSession}, m_commands{ioCommandConsole} {}

void LuaConsole::registerCommands() {
	if (m_registered)
		return;
	m_registered = true;
	for (const auto& spec: m_session.getRegistry().getCommands()) {
		m_lua.addFunction(
				std::format("editor.{}", spec.name), spec.description,
				[this, name = spec.name](const std::map<std::string, script::HostValue>& iArgs) -> script::HostResult {
					const auto* command = m_session.getRegistry().find(name);
					if (command == nullptr)
						return {.ok = false, .error = std::format("{}: no such command", name), .fields = {}};
					commands::CommandArgs args;
					for (const auto& [key, value]: iArgs) {
						const auto argSpec = std::ranges::find(command->args, key, &commands::ArgSpec::name);
						if (argSpec == command->args.end())
							return {.ok = false, .error = std::format("{}: no argument `{}`", name, key), .fields = {}};
						auto converted = toArgValue(*argSpec, value);
						if (!converted)
							return {.ok = false, .error = std::format("{}: {}", name, converted.error()), .fields = {}};
						args.set(key, std::move(*converted));
					}
					const auto result = m_session.execute(name, args, commands::CommandSource::Script);
					if (!result)
						return {.ok = false,
								.error = std::format("{}: {}", name, result.error().message),
								.fields = {}};
					script::HostResult out;
					for (const auto& [key, value]: result->data) out.fields.emplace(key, toHostValue(value));
					out.fields.emplace("description", result->description);
					if (result->entity != core::UUID{0})
						out.fields.emplace("entity", static_cast<int64_t>(static_cast<uint64_t>(result->entity)));
					return out;
				});
	}
}

auto LuaConsole::submit(const std::string_view iLine) -> std::vector<ConsoleOutput> {
	registerCommands();
	const auto firstSpace = iLine.find_first_of(" \t");
	const auto head = iLine.substr(0, firstSpace);
	if (!m_lua.hasPending() && !head.empty() && m_session.getRegistry().find(head) != nullptr &&
		(firstSpace == std::string_view::npos || iLine.substr(firstSpace).find('=') != std::string_view::npos ||
		 iLine.substr(firstSpace).find_first_not_of(" \t") == std::string_view::npos))
		return m_commands.submit(iLine);
	std::vector<ConsoleOutput> output{{.kind = ConsoleLineKind::Input,
									   .text = std::format("{} {}", m_lua.hasPending() ? ">>" : ">", iLine),
									   .journalIndex = 0}};
	if (!iLine.empty())
		m_commands.remember(std::string{iLine});
	const auto result = m_lua.evaluate(iLine);
	for (auto& line: toOutput(result)) output.push_back(std::move(line));
	return output;
}

auto LuaConsole::runScript(const std::filesystem::path& iPath) -> std::vector<ConsoleOutput> {
	registerCommands();
	std::vector<ConsoleOutput> output{{.kind = ConsoleLineKind::Input,
									   .text = std::format("> run {}", iPath.filename().string()),
									   .journalIndex = 0}};
	if (m_session.isScenePlaying()) {
		output.push_back({.kind = ConsoleLineKind::Refused,
						  .text = "refused during Play: editor scripts edit the scene, stop it first",
						  .journalIndex = 0});
		return output;
	}
	std::error_code error;
	if (!std::filesystem::is_regular_file(iPath, error)) {
		output.push_back({.kind = ConsoleLineKind::Error,
						  .text = std::format("{}: no such script", iPath.string()),
						  .journalIndex = 0});
		return output;
	}
	auto context = m_session.makeCommandContext();
	if (context.undo != nullptr)
		context.undo->beginGroup(std::format("Run {}", iPath.filename().string()));
	const auto result = m_lua.run(platform::fileToString(iPath), iPath.string());
	if (context.undo != nullptr && context.scene) {
		if (result.status == script::ScriptConsole::Status::Error)
			context.undo->cancelGroup(*context.scene);
		else
			context.undo->endGroup();
	}
	for (auto& line: toOutput(result)) output.push_back(std::move(line));
	return output;
}

auto LuaConsole::macroToLua(const std::vector<std::string>& iLines, const commands::CommandRegistry& iRegistry)
		-> std::string {
	std::string script = "-- Editor script exported from a macro: one editor call per command.\n";
	const commands::ReferenceResolver noReference =
			[](const std::optional<size_t>) -> expected<core::UUID, std::string> {
		return unexpected<std::string>{"a macro reference ($n) has no Lua equivalent"};
	};
	for (const auto& line: iLines) {
		const auto trimmed = std::string_view{line}.substr(0, line.find_last_not_of(" \t\r") + 1);
		if (trimmed.empty()) {
			script += '\n';
			continue;
		}
		if (trimmed.starts_with('#')) {
			script += std::format("--{}\n", trimmed.substr(1));
			continue;
		}
		const auto parsed = commands::parseCommandLine(trimmed, iRegistry, noReference);
		if (!parsed || !parsed->has_value()) {
			script += std::format("-- not translated: {}\n", trimmed);
			continue;
		}
		const auto& command = **parsed;
		std::string fields;
		for (const auto& [key, value]: command.args.getValues())
			fields += std::format("{}{} = {}", fields.empty() ? "" : ", ", key, luaLiteral(value));
		script += std::format("editor.{}{{{}}}\n", command.name, fields);
	}
	return script;
}

auto LuaConsole::listEditorScripts(const std::vector<std::filesystem::path>& iAssetDirectories)
		-> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> scripts;
	std::error_code error;
	for (const auto& directory: iAssetDirectories) {
		const auto folder = directory / "editor";
		if (!std::filesystem::is_directory(folder, error))
			continue;
		for (const auto& entry: std::filesystem::recursive_directory_iterator(folder, error))
			if (entry.is_regular_file() && entry.path().extension() == ".lua")
				scripts.push_back(entry.path());
	}
	std::ranges::sort(scripts);
	return scripts;
}

}// namespace owl::nest::session
