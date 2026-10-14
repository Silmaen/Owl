/**
 * @file CommandLine.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "CommandLine.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <charconv>
#include <cmath>
#include <format>
#include <set>
#include <type_traits>
#include <utility>
#include <variant>

namespace owl::nest::commands {

namespace {

// A value as written, before typing: a bare word, a quoted text (escapes resolved) or a tuple (its inner text).
struct RawValue {
	enum struct Kind : uint8_t { Bare, Quoted, Tuple };
	Kind kind{Kind::Bare};
	std::string text;
	size_t column{1};
};

auto lineFail(const size_t iColumn, std::string iMessage) -> unexpected<LineFailure> {
	return unexpected<LineFailure>{LineFailure{.column = iColumn, .message = std::move(iMessage)}};
}

auto isSpace(const char iChar) -> bool { return std::isspace(static_cast<unsigned char>(iChar)) != 0; }

auto isWordChar(const char iChar) -> bool {
	return std::isalnum(static_cast<unsigned char>(iChar)) != 0 || iChar == '_';
}

auto isIdentifier(const std::string_view iText) -> bool {
	return !iText.empty() && std::isalpha(static_cast<unsigned char>(iText.front())) != 0 &&
		   std::ranges::all_of(iText, [](const char iChar) -> bool { return isWordChar(iChar) || iChar == '-'; });
}

auto helpTypeName(const ArgType iType) -> std::string_view {
	switch (iType) {
		case ArgType::Bool:
			return "bool";
		case ArgType::Integer:
			return "integer";
		case ArgType::Number:
			return "number";
		case ArgType::Text:
			return "text";
		case ArgType::Entity:
			return "entity";
		case ArgType::Vec3:
			return "(x, y, z)";
		case ArgType::Vec2:
			return "(x, y)";
		case ArgType::Vec4:
			return "(x, y, z, w)";
		case ArgType::Color:
			return "#rrggbb";
		case ArgType::Enum:
			return "choice";
	}
	return "";
}

class LineReader final {
public:
	explicit LineReader(const std::string_view iLine) : m_line{iLine} {}

	void skipSpaces() {
		while (m_pos < m_line.size() && isSpace(m_line[m_pos])) ++m_pos;
	}

	[[nodiscard]] auto atEnd() const -> bool { return m_pos >= m_line.size(); }

	[[nodiscard]] auto peek() const -> char { return atEnd() ? '\0' : m_line[m_pos]; }

	[[nodiscard]] auto column() const -> size_t { return m_pos + 1; }

	auto readWhile(const auto& iPredicate) -> std::string {
		const size_t start = m_pos;
		while (m_pos < m_line.size() && iPredicate(m_line[m_pos])) ++m_pos;
		return std::string{m_line.substr(start, m_pos - start)};
	}

	void advance() { ++m_pos; }

	auto readValue() -> expected<RawValue, LineFailure> {
		RawValue value{.kind = RawValue::Kind::Bare, .text = {}, .column = column()};
		if (peek() == '"') {
			advance();
			value.kind = RawValue::Kind::Quoted;
			return readQuoted(value);
		}
		if (peek() == '(') {
			advance();
			value.kind = RawValue::Kind::Tuple;
			value.text = readWhile([](const char iChar) -> bool { return iChar != ')'; });
			if (atEnd())
				return lineFail(value.column, "missing `)`");
			advance();
			return value;
		}
		value.text = readWhile([](const char iChar) -> bool { return !isSpace(iChar); });
		return value;
	}

private:
	auto readQuoted(RawValue& ioValue) -> expected<RawValue, LineFailure> {
		while (!atEnd()) {
			const char current = peek();
			advance();
			if (current == '"')
				return ioValue;
			if (current != '\\') {
				ioValue.text += current;
				continue;
			}
			if (atEnd())
				break;
			const char escaped = peek();
			advance();
			switch (escaped) {
				case 'n':
					ioValue.text += '\n';
					break;
				case 't':
					ioValue.text += '\t';
					break;
				case '"':
				case '\\':
					ioValue.text += escaped;
					break;
				default:
					return lineFail(column() - 2, std::format("unknown escape `\\{}`", escaped));
			}
		}
		return lineFail(ioValue.column, "missing closing `\"`");
	}

	std::string_view m_line;
	size_t m_pos{0};
};

auto resolveReference(const RawValue& iRaw, const ReferenceResolver& iResolver) -> expected<ArgValue, std::string> {
	std::optional<size_t> index;
	if (iRaw.text != "$_") {
		size_t number = 0;
		const auto* const first = iRaw.text.data() + 1;
		const auto* const last = iRaw.text.data() + iRaw.text.size();
		if (const auto [ptr, ec] = std::from_chars(first, last, number);
			ec != std::errc{} || ptr != last || number == 0)
			return unexpected<std::string>{
					std::format("`{}` is not a result reference (`$1`, `$2`... or `$_`)", iRaw.text)};
		index = number;
	}
	if (!iResolver)
		return unexpected<std::string>{"result references are not available here"};
	const auto uuid = iResolver(index);
	if (!uuid)
		return unexpected<std::string>{uuid.error()};
	return ArgValue{static_cast<int64_t>(static_cast<uint64_t>(*uuid))};
}

auto typeValue(const ArgSpec& iSpec, const RawValue& iRaw, const ReferenceResolver& iResolver)
		-> expected<ArgValue, std::string> {
	const auto wrong = [&iSpec]() -> unexpected<std::string> {
		return unexpected<std::string>{std::format("`{}` is {}", iSpec.name, helpTypeName(iSpec.type))};
	};
	const bool bare = iRaw.kind == RawValue::Kind::Bare;
	const bool tuple = iRaw.kind == RawValue::Kind::Tuple;
	switch (iSpec.type) {
		case ArgType::Bool:
		case ArgType::Integer:
		case ArgType::Number:
			if (!bare)
				return wrong();
			break;
		case ArgType::Text:
			if (tuple)
				return wrong();
			return ArgValue{iRaw.text};
		case ArgType::Entity:
			if (tuple)
				return wrong();
			if (bare && iRaw.text.starts_with('$'))
				return resolveReference(iRaw, iResolver);
			if (!bare)
				return ArgValue{iRaw.text};
			break;
		case ArgType::Vec2:
		case ArgType::Vec3:
		case ArgType::Vec4:
			if (!tuple)
				return wrong();
			break;
		case ArgType::Color:
			if (!tuple && !(bare && iRaw.text.starts_with('#')))
				return wrong();
			break;
		case ArgType::Enum:
			if (tuple)
				return wrong();
			if (!std::ranges::contains(iSpec.choices, iRaw.text)) {
				std::string choices;
				for (const auto& choice: iSpec.choices)
					choices += choices.empty() ? choice : std::format(", {}", choice);
				return unexpected<std::string>{std::format("`{}` is one of: {}", iSpec.name, choices)};
			}
			return ArgValue{iRaw.text};
	}
	if (auto value = CommandRegistry::parseArg(iSpec.type, iRaw.text); value)
		return *value;
	return wrong();
}

auto quote(const std::string_view iText) -> std::string {
	std::string quoted{"\""};
	for (const char current: iText) {
		switch (current) {
			case '"':
				quoted += "\\\"";
				break;
			case '\\':
				quoted += "\\\\";
				break;
			case '\n':
				quoted += "\\n";
				break;
			case '\t':
				quoted += "\\t";
				break;
			default:
				quoted += current;
		}
	}
	return quoted + "\"";
}

auto sameFloat(const float iFirst, const float iSecond) -> bool {
	return std::bit_cast<uint32_t>(iFirst) == std::bit_cast<uint32_t>(iSecond);
}

auto exactChannel(const float iChannel) -> std::optional<uint32_t> {
	if (std::isnan(iChannel) || iChannel < 0.f || iChannel > 1.f)
		return std::nullopt;
	const auto channel = static_cast<uint32_t>(std::lround(iChannel * 255.f));
	if (!sameFloat(static_cast<float>(channel) / 255.f, iChannel))
		return std::nullopt;
	return channel;
}

auto hexColor(const math::vec4& iColor) -> std::optional<std::string> {
	std::string hex{"#"};
	const size_t count = sameFloat(iColor.w(), 1.f) ? 3 : 4;
	for (size_t i = 0; i < count; ++i) {
		const auto channel = exactChannel(iColor[i]);
		if (!channel)
			return std::nullopt;
		hex += std::format("{:02x}", *channel);
	}
	return hex;
}

template<typename BaseType, size_t Dim>
auto tuple(const math::Vector<BaseType, Dim>& iVector) -> std::string {
	std::string text{"("};
	for (size_t i = 0; i < Dim; ++i) text += std::format("{}{}", i == 0 ? "" : ", ", iVector[i]);
	return text + ")";
}

}// namespace

auto parseCommandLine(const std::string_view iLine, const CommandRegistry& iRegistry,
					  const ReferenceResolver& iResolver) -> expected<std::optional<CommandLine>, LineFailure> {
	LineReader reader{iLine};
	reader.skipSpaces();
	if (reader.atEnd() || reader.peek() == '#')
		return std::optional<CommandLine>{};
	const size_t nameColumn = reader.column();
	CommandLine command;
	command.name = reader.readWhile([](const char iChar) -> bool { return isWordChar(iChar) || iChar == '.'; });
	if (command.name.empty())
		return lineFail(nameColumn, "a line starts with a command name");
	if (!reader.atEnd() && !isSpace(reader.peek()))
		return lineFail(reader.column(), std::format("unexpected `{}` after the command name", reader.peek()));
	const auto* spec = iRegistry.find(command.name);
	if (spec == nullptr)
		return lineFail(nameColumn, std::format("unknown command `{}`", command.name));
	while (true) {
		reader.skipSpaces();
		if (reader.atEnd() || reader.peek() == '#')
			break;
		const size_t keyColumn = reader.column();
		const auto key = reader.readWhile(isWordChar);
		if (key.empty())
			return lineFail(keyColumn, "expected an argument `name=value`");
		if (reader.peek() != '=')
			return lineFail(reader.column(), std::format("`{}` needs `=value`", key));
		reader.advance();
		if (reader.atEnd() || isSpace(reader.peek()))
			return lineFail(reader.column(), std::format("`{}` has no value", key));
		auto raw = reader.readValue();
		if (!raw)
			return unexpected<LineFailure>{raw.error()};
		if (!reader.atEnd() && !isSpace(reader.peek()))
			return lineFail(reader.column(),
							std::format("unexpected `{}` after the value of `{}`", reader.peek(), key));
		const auto arg = std::ranges::find(spec->args, key, &ArgSpec::name);
		if (arg == spec->args.end())
			return lineFail(keyColumn, std::format("`{}` has no argument `{}`", command.name, key));
		if (command.args.has(key))
			return lineFail(keyColumn, std::format("`{}` is given twice", key));
		auto value = typeValue(*arg, *raw, iResolver);
		if (!value)
			return lineFail(raw->column, value.error());
		command.args.set(key, std::move(*value));
	}
	return std::optional<CommandLine>{std::move(command)};
}

auto formatArgValue(const ArgValue& iValue, const std::optional<ArgType> iType) -> std::string {
	return std::visit(
			[&iType]<typename T>(const T& iAlternative) -> std::string {
				if constexpr (std::is_same_v<T, bool>) {
					return iAlternative ? "true" : "false";
				} else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, double>) {
					return std::format("{}", iAlternative);
				} else if constexpr (std::is_same_v<T, std::string>) {
					return iType == ArgType::Enum && isIdentifier(iAlternative) ? iAlternative : quote(iAlternative);
				} else {
					if constexpr (std::is_same_v<T, math::vec4>) {
						if (iType == ArgType::Color) {
							if (auto hex = hexColor(iAlternative); hex)
								return *hex;
						}
					}
					return tuple(iAlternative);
				}
			},
			iValue);
}

auto printCommandLine(const std::string_view iName, const CommandArgs& iArgs, const CommandSpec* iSpec,
					  const ReferenceNamer& iNamer) -> std::string {
	std::string line{iName};
	std::set<std::string> written;
	const auto writeArg = [&](const std::string& iKey, const ArgValue& iValue,
							  const std::optional<ArgType> iType) -> void {
		written.insert(iKey);
		if (iNamer && iType == ArgType::Entity) {
			if (const auto* uuid = std::get_if<int64_t>(&iValue); uuid != nullptr) {
				if (const auto reference = iNamer(*uuid); reference) {
					line += std::format(" {}={}", iKey, *reference);
					return;
				}
			}
		}
		line += std::format(" {}={}", iKey, formatArgValue(iValue, iType));
	};
	const auto& values = iArgs.getValues();
	if (iSpec != nullptr) {
		for (const auto& arg: iSpec->args) {
			if (const auto found = values.find(arg.name); found != values.end())
				writeArg(arg.name, found->second, arg.type);
		}
	}
	for (const auto& [key, value]: values) {
		if (!written.contains(key))
			writeArg(key, value, std::nullopt);
	}
	return line;
}

auto describeCommand(const CommandSpec& iSpec) -> std::string {
	std::string usage = iSpec.name;
	for (const auto& arg: iSpec.args)
		usage += arg.required ? std::format(" {}=<{}>", arg.name, helpTypeName(arg.type))
							  : std::format(" [{}=<{}>]", arg.name, helpTypeName(arg.type));
	std::string text = std::format("{}\n  {}", usage, iSpec.description);
	if (iSpec.allowedInPlay)
		text += " Runs during Play.";
	for (const auto& arg: iSpec.args) {
		text += std::format("\n  {} ({}{}): {}", arg.name, helpTypeName(arg.type), arg.required ? ", required" : "",
							arg.description);
		if (arg.defaultValue)
			text += std::format(" Default: {}.", formatArgValue(*arg.defaultValue, arg.type));
		if (!arg.choices.empty()) {
			std::string choices;
			for (const auto& choice: arg.choices) choices += choices.empty() ? choice : std::format(", {}", choice);
			text += std::format(" Choices: {}.", choices);
		}
	}
	return text;
}

}// namespace owl::nest::commands
