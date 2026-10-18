/**
 * @file CommandLine_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/CommandLine.h"
#include "testHelper.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::nest::commands;

namespace {

/**
 * @brief
 *  A registry with one command taking an argument of every type.
 */
auto typedRegistry() -> CommandRegistry {
	CommandRegistry registry;
	registry.add({.name = "test.all",
				  .description = "Every argument type.",
				  .args = {{.name = "flag", .type = ArgType::Bool, .description = "flag"},
						   {.name = "count", .type = ArgType::Integer, .description = "count"},
						   {.name = "ratio", .type = ArgType::Number, .description = "ratio"},
						   {.name = "label", .type = ArgType::Text, .description = "label"},
						   {.name = "target", .type = ArgType::Entity, .description = "target"},
						   {.name = "position", .type = ArgType::Vec3, .description = "position"},
						   {.name = "size", .type = ArgType::Vec2, .description = "size"},
						   {.name = "rect", .type = ArgType::Vec4, .description = "rect"},
						   {.name = "tint", .type = ArgType::Color, .description = "tint"},
						   {.name = "mode",
							.type = ArgType::Enum,
							.description = "mode",
							.choices = {"fast", "slow", "two words"}}},
				  .factory = {}});
	return registry;
}

auto parse(const std::string& iLine, const CommandRegistry& iRegistry, const ReferenceResolver& iResolver = {})
		-> CommandLine {
	auto parsed = parseCommandLine(iLine, iRegistry, iResolver);
	EXPECT_TRUE(parsed.has_value()) << iLine << ": " << (parsed ? "" : parsed.error().message);
	if (!parsed || !parsed->has_value())
		return {};
	return std::move(**parsed);
}

auto roundTrip(const CommandArgs& iArgs, const CommandRegistry& iRegistry) -> CommandArgs {
	const auto line = printCommandLine("test.all", iArgs, iRegistry.find("test.all"));
	return parse(line, iRegistry).args;
}

}// namespace

TEST(CommandLine, RoundTripsEveryArgumentType) {
	const auto registry = typedRegistry();
	const CommandArgs args{{"flag", true},
						   {"count", int64_t{-42}},
						   {"ratio", 0.1},
						   {"label", std::string{"Tree \"oak\" \\ 1\n2\t3 # not a comment"}},
						   {"target", int64_t{-7'123'456'789'012}},
						   {"position", math::vec3{1.5f, -2.f, 0.1f}},
						   {"size", math::vec2{3.f, 1e-7f}},
						   {"rect", math::vec4{0.f, 1.f, 2.f, 3.25f}},
						   {"tint", math::vec4{1.f, 0.f, 0.2f, 1.f}},
						   {"mode", std::string{"slow"}}};
	EXPECT_EQ(roundTrip(args, registry).getValues(), args.getValues());
}

TEST(CommandLine, RoundTripsEdgeValues) {
	const auto registry = typedRegistry();
	for (const auto& args:
		 {CommandArgs{{"flag", false}, {"ratio", 1e20}, {"target", std::string{"12"}}},
		  CommandArgs{{"label", std::string{}}, {"target", std::string{"My Tree"}}, {"mode", std::string{"two words"}}},
		  CommandArgs{{"tint", math::vec4{0.123f, 0.5f, 1.f, 0.25f}}, {"ratio", -0.0}},
		  CommandArgs{{"tint", math::vec4{0.2f, 0.4f, 0.6f, 0.8f}}, {"count", int64_t{0}}}})
		EXPECT_EQ(roundTrip(args, registry).getValues(), args.getValues());
}

TEST(CommandLine, PrintsColoursAsHexWhenExact) {
	const auto registry = typedRegistry();
	const auto* spec = registry.find("test.all");
	EXPECT_EQ(printCommandLine("test.all", {{"tint", math::vec4{1.f, 0.f, 0.2f, 1.f}}}, spec), "test.all tint=#ff0033");
	EXPECT_EQ(printCommandLine("test.all", {{"tint", math::vec4{1.f, 1.f, 1.f, 0.f}}}, spec),
			  "test.all tint=#ffffff00");
	EXPECT_EQ(printCommandLine("test.all", {{"mode", std::string{"fast"}}, {"flag", true}}, spec),
			  "test.all flag=true mode=fast");
}

TEST(CommandLine, ReadsTheDocumentedSyntax) {
	const auto registry = typedRegistry();
	const auto command =
			parse("  test.all label=Tree size=(1,2) tint=#00ff00 mode=\"two words\"   # trailing", registry);
	EXPECT_EQ(command.name, "test.all");
	EXPECT_EQ(command.args.getText("label"), "Tree");
	EXPECT_EQ(command.args.get<math::vec2>("size"), (math::vec2{1.f, 2.f}));
	EXPECT_EQ(command.args.get<math::vec4>("tint"), (math::vec4{0.f, 1.f, 0.f, 1.f}));
	EXPECT_EQ(command.args.getText("mode"), "two words");
	EXPECT_EQ(command.args.get<std::string>("target"), std::nullopt);
}

TEST(CommandLine, SkipsBlankAndCommentLines) {
	const auto registry = typedRegistry();
	for (const auto* line: {"", "   ", "# a comment", "\t# indented comment"}) {
		const auto parsed = parseCommandLine(line, registry);
		ASSERT_TRUE(parsed.has_value()) << line;
		EXPECT_FALSE(parsed->has_value()) << line;
	}
}

TEST(CommandLine, ReportsTheColumnOfAnError) {
	const auto registry = typedRegistry();
	const auto expectError = [&registry](const std::string& iLine, const size_t iColumn) -> void {
		const auto parsed = parseCommandLine(iLine, registry);
		ASSERT_FALSE(parsed.has_value()) << iLine;
		EXPECT_EQ(parsed.error().column, iColumn) << iLine << ": " << parsed.error().message;
		EXPECT_FALSE(parsed.error().message.empty());
	};
	expectError("nothing.here", 1);
	expectError("test.all count=abc", 16);
	expectError("test.all unknown=1", 10);
	expectError("test.all flag", 14);
	expectError("test.all label=\"open", 16);
	expectError("test.all mode=medium", 15);
	expectError("test.all size=(1, 2", 15);
	expectError("test.all count=1 count=2", 18);
	expectError("test.all position=(1, 2)", 19);
	expectError("test.all target=$1", 17);
}

TEST(CommandLine, ResolvesResultReferences) {
	const auto registry = typedRegistry();
	const std::vector<core::UUID> results{core::UUID{11}, core::UUID{0}, core::UUID{33}};
	const ReferenceResolver resolver =
			[&results](const std::optional<size_t> iIndex) -> expected<core::UUID, std::string> {
		const auto index = iIndex.value_or(results.size());
		if (index == 0 || index > results.size() || results[index - 1] == core::UUID{0})
			return unexpected<std::string>{"no entity"};
		return results[index - 1];
	};
	EXPECT_EQ(parse("test.all target=$1", registry, resolver).args.get<int64_t>("target"), int64_t{11});
	EXPECT_EQ(parse("test.all target=$_", registry, resolver).args.get<int64_t>("target"), int64_t{33});
	EXPECT_EQ(parse("test.all target=\"$1\"", registry, resolver).args.get<std::string>("target"), "$1");
	for (const auto* line: {"test.all target=$2", "test.all target=$4", "test.all target=$x", "test.all target=$0"}) {
		const auto parsed = parseCommandLine(line, registry, resolver);
		EXPECT_FALSE(parsed.has_value()) << line;
	}
	const ReferenceNamer namer = [](const int64_t iUuid) -> std::optional<std::string> {
		return iUuid == 11 ? std::optional<std::string>{"$1"} : std::nullopt;
	};
	EXPECT_EQ(printCommandLine("test.all", {{"target", int64_t{11}}}, registry.find("test.all"), namer),
			  "test.all target=$1");
	EXPECT_EQ(printCommandLine("test.all", {{"target", int64_t{12}}}, registry.find("test.all"), namer),
			  "test.all target=12");
}

TEST(CommandLine, RoundTripsTheBuiltInCommands) {
	const CommandRegistry registry;
	const CommandArgs args{{"entity", int64_t{1234}},
						   {"translation", math::vec3{1.f, 2.f, 3.f}},
						   {"scale", math::vec3{0.5f, 0.5f, 1.f}}};
	const auto line = printCommandLine("entity.set_transform", args, registry.find("entity.set_transform"));
	EXPECT_EQ(line, "entity.set_transform entity=1234 translation=(1, 2, 3) scale=(0.5, 0.5, 1)");
	EXPECT_EQ(parse(line, registry).args.getValues(), args.getValues());
	const CommandArgs set{{"entity", std::string{"Crate"}},
						  {"component", std::string{"Transform"}},
						  {"values", std::string{"{translation: [4, 3, 0]}"}},
						  {"replace", true}};
	EXPECT_EQ(parse(printCommandLine("component.set", set, registry.find("component.set")), registry).args.getValues(),
			  set.getValues());
}

TEST(CommandLine, DescribesACommand) {
	const CommandRegistry registry;
	const auto text = describeCommand(*registry.find("entity.create"));
	EXPECT_TRUE(text.starts_with("entity.create [name=<text>] [parent=<entity>]")) << text;
	EXPECT_TRUE(text.contains("Default: \"Empty Entity\".")) << text;
	EXPECT_TRUE(describeCommand(*typedRegistry().find("test.all")).contains("Choices: fast, slow, two words."));
}
