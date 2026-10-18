/**
 * @file CommandForm_test.cpp
 * @author Silmaen
 * @date 13/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/CommandForm.h"
#include "commands/CommandRegistry.h"
#include "testHelper.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <variant>

using namespace owl;
using namespace owl::nest::commands;

namespace {

auto probeSpec() -> CommandSpec {
	return {.name = "test.probe",
			.description = "Probe.",
			.args = {{.name = "entity", .type = ArgType::Entity, .required = true, .description = "Target."},
					 {.name = "label", .type = ArgType::Text, .description = "Label."},
					 {.name = "size",
					  .type = ArgType::Vec2,
					  .description = "Size.",
					  .defaultValue = math::vec2{1.f, 2.f}},
					 {.name = "tint", .type = ArgType::Color, .description = "Tint."},
					 {.name = "mode",
					  .type = ArgType::Enum,
					  .required = true,
					  .description = "Mode.",
					  .choices = {"Fast", "Slow"}}},
			.factory = {}};
}

auto argOf(const ArgType iType) -> ArgSpec { return {.name = "value", .type = iType, .description = "Value."}; }

}// namespace

TEST(CommandForm, PrefillsDefaultsAndNeutralValues) {
	const CommandForm form{probeSpec()};
	EXPECT_EQ(form.getCommandName(), "test.probe");
	const auto& fields = form.getFields();
	ASSERT_EQ(fields.size(), 5u);
	EXPECT_TRUE(fields[0].set);
	EXPECT_EQ(std::get<int64_t>(fields[0].value), 0);
	EXPECT_FALSE(fields[1].set);
	EXPECT_TRUE(fields[2].set);
	EXPECT_FLOAT_EQ(std::get<math::vec2>(fields[2].value).y(), 2.f);
	EXPECT_FALSE(fields[3].set);
	EXPECT_FLOAT_EQ(std::get<math::vec4>(fields[3].value).w(), 1.f);
	EXPECT_EQ(std::get<std::string>(fields[4].value), "Fast");
	EXPECT_TRUE(CommandForm{}.getFields().empty());
}

TEST(CommandForm, NeutralValueOfEveryType) {
	EXPECT_FALSE(std::get<bool>(CommandForm::initialValue(argOf(ArgType::Bool))));
	EXPECT_EQ(std::get<int64_t>(CommandForm::initialValue(argOf(ArgType::Integer))), 0);
	EXPECT_DOUBLE_EQ(std::get<double>(CommandForm::initialValue(argOf(ArgType::Number))), 0.0);
	EXPECT_TRUE(std::get<std::string>(CommandForm::initialValue(argOf(ArgType::Text))).empty());
	EXPECT_TRUE(std::get<std::string>(CommandForm::initialValue(argOf(ArgType::Enum))).empty());
	EXPECT_FLOAT_EQ(std::get<math::vec3>(CommandForm::initialValue(argOf(ArgType::Vec3))).z(), 0.f);
	EXPECT_FLOAT_EQ(std::get<math::vec4>(CommandForm::initialValue(argOf(ArgType::Vec4))).w(), 0.f);
}

TEST(CommandForm, BuildsTheGivenArguments) {
	CommandForm form{probeSpec()};
	const auto missing = form.buildArgs();
	ASSERT_FALSE(missing.has_value());
	EXPECT_EQ(missing.error().error, CommandError::MissingArgument);
	form.setDefaultEntity(core::UUID{42});
	auto args = form.buildArgs();
	ASSERT_TRUE(args.has_value()) << args.error().message;
	EXPECT_EQ(args->get<int64_t>("entity"), 42);
	EXPECT_FALSE(args->has("label"));
	EXPECT_FALSE(args->has("tint"));
	EXPECT_FLOAT_EQ(args->get<math::vec2>("size")->x(), 1.f);
	EXPECT_EQ(args->get<std::string>("mode"), "Fast");
	form.getFields()[1].set = true;
	form.getFields()[1].value = std::string{"Crate"};
	form.getFields()[2].set = false;
	args = form.buildArgs();
	ASSERT_TRUE(args.has_value());
	EXPECT_EQ(args->get<std::string>("label"), "Crate");
	EXPECT_FALSE(args->has("size"));
	form.setDefaultEntity(core::UUID{7});
	EXPECT_EQ(form.buildArgs()->get<int64_t>("entity"), 42);
}

TEST(CommandForm, RefusesAnInvalidForm) {
	CommandForm form{probeSpec()};
	form.setDefaultEntity(core::UUID{1});
	form.getFields()[4].value = std::string{"Warp"};
	const auto badChoice = form.buildArgs();
	ASSERT_FALSE(badChoice.has_value());
	EXPECT_EQ(badChoice.error().error, CommandError::InvalidArgument);
	const CommandRegistry registry;
	CommandForm rename{*registry.find("entity.rename")};
	rename.setDefaultEntity(core::UUID{1});
	const auto empty = rename.buildArgs();
	ASSERT_FALSE(empty.has_value());
	EXPECT_EQ(empty.error().error, CommandError::MissingArgument);
}

TEST(CommandForm, FormArgumentsRunThroughTheRegistry) {
	core::Log::init(core::Log::Level::Off);
	const CommandRegistry registry;
	auto scene = mkShared<scene::Scene>();
	nest::SceneUndoManager undo;
	const auto box = scene->createEntity("Box");
	CommandForm form{*registry.find("entity.rename")};
	form.setDefaultEntity(box.getUUID());
	form.getFields()[1].value = std::string{"Crate"};
	const auto args = form.buildArgs();
	ASSERT_TRUE(args.has_value());
	ASSERT_TRUE(registry.execute(form.getCommandName(), *args, scene, undo).has_value());
	EXPECT_EQ(scene->findEntityByUUID(box.getUUID()).getName(), "Crate");
	core::Log::invalidate();
}

TEST(CommandForm, DescribesResultData) {
	EXPECT_TRUE(CommandForm::describeData({}).empty());
	const ResultData data{{"can_undo", true},
						  {"entity", int64_t{12}},
						  {"path", std::string{"a.owl"}},
						  {"size", math::vec2{1.f, 2.f}},
						  {"tint", math::vec4{1.f, 0.f, 0.f, 1.f}},
						  {"pos", math::vec3{0.f, 1.f, 2.f}},
						  {"scale", 0.5}};
	EXPECT_EQ(CommandForm::describeData(data),
			  "can_undo: true, entity: 12, path: a.owl, pos: (0, 1, 2), scale: 0.5, size: (1, 2), tint: (1, 0, 0, 1)");
}

TEST(CommandForm, EditsAreUnavailableDuringPlay) {
	CommandSpec spec = probeSpec();
	EXPECT_TRUE(CommandForm::isAvailable(spec, false));
	EXPECT_FALSE(CommandForm::isAvailable(spec, true));
	spec.allowedInPlay = true;
	EXPECT_TRUE(CommandForm::isAvailable(spec, true));
}
