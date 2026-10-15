/**
 * @file CommandRegistry_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/CommandRegistry.h"
#include "UndoManager.h"
#include "testHelper.h"

#include <core/SerializerImpl.h>
#include <scene/ComponentRegistry.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/Canvas.h>
#include <scene/component/CircleRenderer.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/Tag.h>
#include <scene/component/Transform.h>
#include <scene/component/UiRect.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <variant>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class CommandRegistryTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }

	auto run(const std::string& iName, const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		return m_registry.execute(iName, iArgs, m_scene, m_undo);
	}

	static auto idOf(const scene::Entity& iEntity) -> ArgValue {
		return static_cast<int64_t>(static_cast<uint64_t>(iEntity.getUUID()));
	}

	[[nodiscard]] auto count() const -> size_t { return m_scene->registry.view<scene::component::Tag>().size(); }

	/// Registry under test.
	CommandRegistry m_registry;
	/// Scene changed by the commands.
	shared<scene::Scene> m_scene = mkShared<scene::Scene>();
	/// Undo history of the scene.
	SceneUndoManager m_undo;
};
OWL_DIAG_POP

}// namespace

TEST_F(CommandRegistryTest, DeclaresTheBuiltinCommands) {
	for (const auto* name: {"entity.create", "entity.delete", "entity.duplicate", "entity.reparent", "entity.rename",
							"entity.set_transform", "component.add", "component.remove", "prefab.instantiate"}) {
		const auto* spec = m_registry.find(name);
		ASSERT_NE(spec, nullptr) << name;
		EXPECT_FALSE(spec->description.empty()) << name;
		EXPECT_TRUE(spec->factory) << name;
	}
	for (const auto* name: {"component.set", "scene.save", "edit.undo", "edit.redo", "macro.run"}) {
		const auto* spec = m_registry.find(name);
		ASSERT_NE(spec, nullptr) << name;
		EXPECT_TRUE(spec->factory || spec->run) << name;
		EXPECT_FALSE(spec->allowedInPlay) << name;
	}
	EXPECT_EQ(m_registry.find("entity.explode"), nullptr);
	EXPECT_EQ(m_registry.getCommands().size(), 14u);
}

TEST_F(CommandRegistryTest, CreateUnderAParentThenUndoRedo) {
	const auto parent = m_scene->createEntity("Parent");
	const auto result = run("entity.create", {{"name", std::string{"Child"}}, {"parent", std::string{"Parent"}}});
	ASSERT_TRUE(result.has_value());
	const auto child = m_scene->findEntityByUUID(result->entity);
	ASSERT_TRUE(child);
	EXPECT_EQ(child.getComponent<scene::component::Hierarchy>().parentId, parent.getUUID());
	EXPECT_EQ(m_undo.undoDescription(), "Create 'Child'");
	m_undo.undo(*m_scene);
	EXPECT_FALSE(m_scene->findEntityByUUID(result->entity));
	m_undo.redo(*m_scene);
	EXPECT_TRUE(m_scene->findEntityByUUID(result->entity));
}

TEST_F(CommandRegistryTest, DeleteRunsThroughTheUndoManager) {
	auto parent = m_scene->createEntity("Parent");
	auto child = m_scene->createEntity("Child");
	m_scene->setParent(child, parent);
	ASSERT_TRUE(run("entity.delete", {{"entity", idOf(parent)}, {"children", true}}).has_value());
	EXPECT_EQ(count(), 0u);
	m_undo.undo(*m_scene);
	EXPECT_EQ(count(), 2u);
	ASSERT_TRUE(run("entity.delete", {{"entity", std::string{"Parent"}}}).has_value());
	EXPECT_EQ(count(), 1u);
}

TEST_F(CommandRegistryTest, DuplicateReparentAndRename) {
	const auto a = m_scene->createEntity("A");
	const auto b = m_scene->createEntity("B");
	const auto copy = run("entity.duplicate", {{"entity", idOf(a)}});
	ASSERT_TRUE(copy.has_value());
	EXPECT_EQ(count(), 3u);
	ASSERT_TRUE(run("entity.reparent", {{"entity", idOf(a)}, {"parent", idOf(b)}}).has_value());
	EXPECT_EQ(a.getComponent<scene::component::Hierarchy>().parentId, b.getUUID());
	ASSERT_TRUE(run("entity.reparent", {{"entity", idOf(a)}}).has_value());
	EXPECT_EQ(a.getComponent<scene::component::Hierarchy>().parentId, core::UUID{0});
	ASSERT_TRUE(run("entity.rename", {{"entity", idOf(b)}, {"name", std::string{"Renamed"}}}).has_value());
	EXPECT_EQ(b.getName(), "Renamed");
	m_undo.undo(*m_scene);
	EXPECT_EQ(m_scene->findEntityByUUID(b.getUUID()).getName(), "B");
}

TEST_F(CommandRegistryTest, SetTransformAndComponents) {
	const auto entity = m_scene->createEntity("Box");
	ASSERT_TRUE(run("entity.set_transform", {{"entity", idOf(entity)}, {"translation", math::vec3{1.f, 2.f, 3.f}}})
						.has_value());
	EXPECT_FLOAT_EQ(entity.getComponent<scene::component::Transform>().transform.translation().y(), 2.f);
	const std::string circle = scene::component::CircleRenderer::name();
	ASSERT_TRUE(run("component.add", {{"entity", idOf(entity)}, {"component", circle}}).has_value());
	EXPECT_TRUE(entity.hasComponent<scene::component::CircleRenderer>());
	ASSERT_TRUE(run("component.remove", {{"entity", idOf(entity)}, {"component", circle}}).has_value());
	EXPECT_FALSE(m_scene->findEntityByUUID(entity.getUUID()).hasComponent<scene::component::CircleRenderer>());
	m_undo.undo(*m_scene);
	EXPECT_TRUE(m_scene->findEntityByUUID(entity.getUUID()).hasComponent<scene::component::CircleRenderer>());
}

namespace {
/// A component a game registers: the editor commands handle it like an engine one.
struct GameHealth {
	int64_t value = 100;
	static auto key() -> const char* { return "GameHealth"; }
	static auto name() -> const char* { return "Game Health"; }
	void serialize(const core::Serializer& iOut) const {
		iOut.getImpl()->emitter << YAML::Key << key() << YAML::Value << YAML::BeginMap;
		iOut.getImpl()->emitter << YAML::Key << "value" << YAML::Value << value;
		iOut.getImpl()->emitter << YAML::EndMap;
	}
	void deserialize(const core::Serializer& iNode) {
		if (const auto node = iNode.getImpl()->node["value"]; node)
			value = node.as<int64_t>();
	}
};
}// namespace

TEST_F(CommandRegistryTest, AddsAndRemovesARegisteredGameComponent) {
	ASSERT_TRUE(scene::ComponentRegistry::registerComponent<GameHealth>());
	const auto entity = m_scene->createEntity("Hero");
	ASSERT_TRUE(
			run("component.add", {{"entity", idOf(entity)}, {"component", std::string{"Game Health"}}}).has_value());
	EXPECT_TRUE(entity.hasComponent<GameHealth>());
	m_undo.undo(*m_scene);
	EXPECT_FALSE(m_scene->findEntityByUUID(entity.getUUID()).hasComponent<GameHealth>());
	m_undo.redo(*m_scene);
	const auto restored = m_scene->findEntityByUUID(entity.getUUID());
	ASSERT_TRUE(restored.hasComponent<GameHealth>());
	ASSERT_TRUE(run("component.remove", {{"entity", idOf(restored)}, {"component", std::string{"GameHealth"}}})
						.has_value());
	EXPECT_FALSE(m_scene->findEntityByUUID(entity.getUUID()).hasComponent<GameHealth>());
	std::ignore = scene::ComponentRegistry::unregisterComponent(GameHealth::key());
}

TEST_F(CommandRegistryTest, RefusesBadCallsWithoutChangingAnything) {
	const auto entity = m_scene->createEntity("Box");
	const auto expectError = [this](const std::string& iName, const CommandArgs& iArgs,
									const CommandError iError) -> void {
		const auto result = run(iName, iArgs);
		ASSERT_FALSE(result.has_value()) << iName;
		EXPECT_EQ(result.error().error, iError) << result.error().message;
		EXPECT_FALSE(result.error().message.empty());
	};
	expectError("entity.explode", {}, CommandError::UnknownCommand);
	expectError("entity.delete", {}, CommandError::MissingArgument);
	expectError("entity.delete", {{"entity", true}}, CommandError::WrongArgumentType);
	expectError("entity.delete", {{"entity", idOf(entity)}, {"force", true}}, CommandError::UnknownArgument);
	expectError("entity.delete", {{"entity", std::string{"Nobody"}}}, CommandError::UnknownEntity);
	expectError("component.add", {{"entity", idOf(entity)}, {"component", std::string{"Warp Drive"}}},
				CommandError::InvalidArgument);
	expectError("entity.set_transform", {{"entity", idOf(entity)}}, CommandError::MissingArgument);
	expectError("prefab.instantiate", {{"path", std::string{"/nowhere/none.owlprefab"}}},
				CommandError::InvalidArgument);
	EXPECT_FALSE(m_undo.canUndo());
	EXPECT_EQ(count(), 1u);
}

TEST_F(CommandRegistryTest, ParsesTextualArguments) {
	EXPECT_EQ(std::get<bool>(*CommandRegistry::parseArg(ArgType::Bool, "true")), true);
	EXPECT_FALSE(CommandRegistry::parseArg(ArgType::Bool, "yes").has_value());
	EXPECT_EQ(std::get<int64_t>(*CommandRegistry::parseArg(ArgType::Integer, "-12")), -12);
	EXPECT_FALSE(CommandRegistry::parseArg(ArgType::Integer, "1.5").has_value());
	EXPECT_DOUBLE_EQ(std::get<double>(*CommandRegistry::parseArg(ArgType::Number, "1.5")), 1.5);
	EXPECT_EQ(std::get<int64_t>(*CommandRegistry::parseArg(ArgType::Entity, "42")), 42);
	EXPECT_EQ(std::get<std::string>(*CommandRegistry::parseArg(ArgType::Entity, "Player")), "Player");
	const auto vec = CommandRegistry::parseArg(ArgType::Vec3, "1, 2 3");
	ASSERT_TRUE(vec.has_value());
	EXPECT_FLOAT_EQ(std::get<math::vec3>(*vec).z(), 3.f);
	EXPECT_FALSE(CommandRegistry::parseArg(ArgType::Vec3, "1 2").has_value());
}

TEST_F(CommandRegistryTest, IncompleteTargetRefusesCommands) {
	const CommandTarget target{.registry = &m_registry, .scene = m_scene, .undo = nullptr};
	EXPECT_FALSE(target.isValid());
	EXPECT_FALSE(target.execute("entity.create", {}).has_value());
	const CommandTarget complete{.registry = &m_registry, .scene = m_scene, .undo = &m_undo};
	ASSERT_TRUE(complete.execute("entity.create", {}).has_value());
	EXPECT_EQ(count(), 1u);
}

TEST_F(CommandRegistryTest, AddReplacesACommandOfTheSameName) {
	m_registry.add({.name = "entity.create", .description = "Replaced.", .args = {}, .factory = {}});
	EXPECT_EQ(m_registry.find("entity.create")->description, "Replaced.");
	EXPECT_EQ(m_registry.getCommands().size(), 14u);
}

TEST_F(CommandRegistryTest, ParsesVectorColourAndEnumArguments) {
	const auto vec2 = CommandRegistry::parseArg(ArgType::Vec2, "1.5, -2");
	ASSERT_TRUE(vec2.has_value());
	EXPECT_FLOAT_EQ(std::get<math::vec2>(*vec2).y(), -2.f);
	EXPECT_FALSE(CommandRegistry::parseArg(ArgType::Vec2, "1 2 3").has_value());
	const auto vec4 = CommandRegistry::parseArg(ArgType::Vec4, "1 2 3 4");
	ASSERT_TRUE(vec4.has_value());
	EXPECT_FLOAT_EQ(std::get<math::vec4>(*vec4).w(), 4.f);
	EXPECT_FALSE(CommandRegistry::parseArg(ArgType::Vec4, "1 2 3").has_value());
	EXPECT_FALSE(CommandRegistry::parseArg(ArgType::Vec3, "1 2 x").has_value());
	const auto rgb = CommandRegistry::parseArg(ArgType::Color, "1, 0.5, 0");
	ASSERT_TRUE(rgb.has_value());
	EXPECT_FLOAT_EQ(std::get<math::vec4>(*rgb).w(), 1.f);
	const auto hex = CommandRegistry::parseArg(ArgType::Color, "#ff000080");
	ASSERT_TRUE(hex.has_value());
	EXPECT_FLOAT_EQ(std::get<math::vec4>(*hex).x(), 1.f);
	EXPECT_NEAR(std::get<math::vec4>(*hex).w(), 0.5f, 0.01f);
	const auto opaque = CommandRegistry::parseArg(ArgType::Color, "#00ff00");
	ASSERT_TRUE(opaque.has_value());
	EXPECT_FLOAT_EQ(std::get<math::vec4>(*opaque).w(), 1.f);
	EXPECT_FALSE(CommandRegistry::parseArg(ArgType::Color, "#ff00").has_value());
	EXPECT_FALSE(CommandRegistry::parseArg(ArgType::Color, "#gg0000").has_value());
	EXPECT_EQ(std::get<std::string>(*CommandRegistry::parseArg(ArgType::Enum, "Linear")), "Linear");
}

TEST_F(CommandRegistryTest, DefaultsChoicesAndNewArgumentTypesReachTheCommand) {
	CommandArgs seen;
	m_registry.add({.name = "test.probe",
					.description = "Record its arguments.",
					.args = {{.name = "size", .type = ArgType::Vec2, .description = "Size."},
							 {.name = "tint",
							  .type = ArgType::Color,
							  .description = "Tint.",
							  .defaultValue = math::vec4{1.f, 1.f, 1.f, 1.f}},
							 {.name = "mode",
							  .type = ArgType::Enum,
							  .description = "Mode.",
							  .defaultValue = std::string{"Fast"},
							  .choices = {"Fast", "Slow"}}},
					.factory = {},
					.run = [&seen](const CommandContext& /*iContext*/,
								   const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
						seen = iArgs;
						return CommandResult{.entity = core::UUID{0}, .description = "Probe", .data = {{"ok", true}}};
					}});
	const auto result = run("test.probe", {{"size", math::vec2{2.f, 3.f}}});
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(std::get<bool>(result->data.at("ok")));
	EXPECT_EQ(seen.get<std::string>("mode"), "Fast");
	EXPECT_FLOAT_EQ(seen.get<math::vec4>("tint")->x(), 1.f);
	EXPECT_FLOAT_EQ(seen.get<math::vec2>("size")->y(), 3.f);
	EXPECT_FALSE(seen.get<double>("size").has_value());
	ASSERT_TRUE(run("test.probe", {{"mode", std::string{"Slow"}}}).has_value());
	EXPECT_EQ(seen.get<std::string>("mode"), "Slow");
	const auto badChoice = run("test.probe", {{"mode", std::string{"Warp"}}});
	ASSERT_FALSE(badChoice.has_value());
	EXPECT_EQ(badChoice.error().error, CommandError::InvalidArgument);
	EXPECT_NE(badChoice.error().message.find("Fast, Slow"), std::string::npos);
	EXPECT_EQ(run("test.probe", {{"tint", math::vec3{1.f, 0.f, 0.f}}}).error().error, CommandError::WrongArgumentType);
	EXPECT_EQ(run("test.probe", {{"size", math::vec4{}}}).error().error, CommandError::WrongArgumentType);
}

TEST_F(CommandRegistryTest, ResultsCarryStructuredData) {
	const auto created = run("entity.create", {});
	ASSERT_TRUE(created.has_value());
	EXPECT_EQ(std::get<int64_t>(created->data.at("entity")),
			  static_cast<int64_t>(static_cast<uint64_t>(created->entity)));
	EXPECT_EQ(m_scene->findEntityByUUID(created->entity).getName(), "Empty Entity");
}

TEST_F(CommandRegistryTest, SetComponentMergesFieldsAndUndoes) {
	auto entity = m_scene->createEntity("Ring");
	entity.addComponent<scene::component::CircleRenderer>();
	const auto result =
			run("component.set", {{"entity", idOf(entity)},
								  {"component", std::string{"Circle Renderer"}},
								  {"values", std::string{R"({"color": [1, 0, 0, 0.5], "thickness": 0.25})"}}});
	ASSERT_TRUE(result.has_value()) << result.error().message;
	EXPECT_EQ(std::get<std::string>(result->data.at("component")), "CircleRenderer");
	const auto& circle = entity.getComponent<scene::component::CircleRenderer>();
	EXPECT_FLOAT_EQ(circle.color.y(), 0.f);
	EXPECT_FLOAT_EQ(circle.color.w(), 0.5f);
	EXPECT_FLOAT_EQ(circle.thickness, 0.25f);
	EXPECT_FLOAT_EQ(circle.fade, 0.005f);
	ASSERT_TRUE(m_undo.canUndo());
	m_undo.undo(*m_scene);
	const auto restored = m_scene->findEntityByUUID(entity.getUUID());
	EXPECT_FLOAT_EQ(restored.getComponent<scene::component::CircleRenderer>().thickness, 1.f);
	m_undo.redo(*m_scene);
	const auto redone = m_scene->findEntityByUUID(entity.getUUID());
	EXPECT_FLOAT_EQ(redone.getComponent<scene::component::CircleRenderer>().thickness, 0.25f);
}

TEST_F(CommandRegistryTest, SetComponentEditsTheHudLayoutAndUndoes) {
	auto canvas = m_scene->createEntity("Hud");
	canvas.addComponent<scene::component::Canvas>();
	auto widget = m_scene->createEntity("Bar");
	widget.addComponent<scene::component::UiRect>();
	const auto setRect = run("component.set", {{"entity", idOf(widget)},
											   {"component", std::string{"UI Rect"}},
											   {"values", std::string{R"({"anchorMin": [0, 1], "anchorMax": [1, 1], )"
																	  R"("pivot": [0.5, 1], "size": [-20, 40]})"}}});
	ASSERT_TRUE(setRect.has_value()) << setRect.error().message;
	const auto setCanvas = run("component.set", {{"entity", idOf(canvas)},
												 {"component", std::string{"Canvas"}},
												 {"values", std::string{R"({"referenceResolution": [640, 360], )"
																		R"("scaleMode": "MatchSmallerSide"})"}}});
	ASSERT_TRUE(setCanvas.has_value()) << setCanvas.error().message;
	const auto& rect = widget.getComponent<scene::component::UiRect>();
	EXPECT_FLOAT_EQ(rect.anchorMax.x(), 1.f);
	EXPECT_FLOAT_EQ(rect.pivot.y(), 1.f);
	EXPECT_FLOAT_EQ(rect.size.x(), -20.f);
	EXPECT_FALSE(rect.getAnchor().has_value());
	EXPECT_EQ(canvas.getComponent<scene::component::Canvas>().scaleMode,
			  scene::component::Canvas::ScaleMode::MatchSmallerSide);
	m_undo.undo(*m_scene);
	EXPECT_EQ(m_scene->findEntityByUUID(canvas.getUUID()).getComponent<scene::component::Canvas>().scaleMode,
			  scene::component::Canvas::ScaleMode::ScaleWithHeight);
	m_undo.undo(*m_scene);
	EXPECT_EQ(m_scene->findEntityByUUID(widget.getUUID()).getComponent<scene::component::UiRect>().getAnchor(),
			  scene::component::UiRect::Anchor::Center);
}

TEST_F(CommandRegistryTest, SetComponentRefusesBadValues) {
	auto entity = m_scene->createEntity("Ring");
	entity.addComponent<scene::component::CircleRenderer>();
	const auto expectError = [this, &entity](const std::string& iComponent, const std::string& iValues,
											 const CommandError iError) -> void {
		const auto result =
				run("component.set", {{"entity", idOf(entity)}, {"component", iComponent}, {"values", iValues}});
		ASSERT_FALSE(result.has_value()) << iValues;
		EXPECT_EQ(result.error().error, iError) << result.error().message;
	};
	expectError("Sprite Renderer", "{}", CommandError::InvalidArgument);
	expectError("Circle Renderer", "[1, 2]", CommandError::InvalidArgument);
	expectError("Circle Renderer", "{color: [1, 0", CommandError::InvalidArgument);
	expectError("Circle Renderer", "{radius: 3}", CommandError::InvalidArgument);
	EXPECT_FALSE(m_undo.canUndo());
	EXPECT_FLOAT_EQ(entity.getComponent<scene::component::CircleRenderer>().thickness, 1.f);
}

TEST_F(CommandRegistryTest, UndoAndRedoCommands) {
	EXPECT_EQ(run("edit.undo", {}).error().error, CommandError::NotApplicable);
	EXPECT_EQ(run("edit.redo", {}).error().error, CommandError::NotApplicable);
	const auto created = run("entity.create", {{"name", std::string{"Crate"}}});
	ASSERT_TRUE(created.has_value());
	const auto undone = run("edit.undo", {});
	ASSERT_TRUE(undone.has_value());
	EXPECT_EQ(undone->description, created->description);
	EXPECT_FALSE(std::get<bool>(undone->data.at("can_undo")));
	EXPECT_TRUE(std::get<bool>(undone->data.at("can_redo")));
	EXPECT_EQ(count(), 0u);
	const auto redone = run("edit.redo", {});
	ASSERT_TRUE(redone.has_value());
	EXPECT_EQ(count(), 1u);
	EXPECT_FALSE(std::get<bool>(redone->data.at("can_redo")));
}

TEST_F(CommandRegistryTest, SaveWritesTheSceneAndMarksItSaved) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_command_save_test";
	std::filesystem::create_directories(dir);
	const auto path = dir / "saved.owl";
	std::filesystem::path notified;
	const CommandContext context{
			.scene = m_scene,
			.undo = &m_undo,
			.playing = false,
			.scenePath = {},
			.onSaved = [&notified](const std::filesystem::path& iPath) -> void { notified = iPath; }};
	ASSERT_TRUE(m_registry.execute("entity.create", {{"name", std::string{"Saved Crate"}}}, context).has_value());
	EXPECT_TRUE(m_undo.isDirty());
	EXPECT_EQ(m_registry.execute("scene.save", {}, context).error().error, CommandError::MissingArgument);
	const auto saved = m_registry.execute("scene.save", {{"path", path.string()}}, context);
	ASSERT_TRUE(saved.has_value()) << saved.error().message;
	EXPECT_EQ(std::get<std::string>(saved->data.at("path")), path.generic_string());
	EXPECT_EQ(notified, path);
	EXPECT_FALSE(m_undo.isDirty());
	{
		// Closed before the next save and the removal: Windows cannot delete an open file.
		const std::ifstream file{path};
		std::stringstream content;
		content << file.rdbuf();
		EXPECT_NE(content.str().find("Saved Crate"), std::string::npos);
	}
	CommandContext withPath = context;
	withPath.scenePath = path;
	EXPECT_TRUE(m_registry.execute("scene.save", {}, withPath).has_value());
	std::filesystem::remove_all(dir);
}

TEST_F(CommandRegistryTest, EditsAreRefusedDuringPlay) {
	std::ignore = m_scene->createEntity("Box");
	const CommandContext playing{.scene = m_scene, .undo = &m_undo, .playing = true, .scenePath = {}, .onSaved = {}};
	for (const auto* name: {"entity.create", "edit.undo", "scene.save", "component.set"}) {
		const auto result = m_registry.execute(name, {}, playing);
		ASSERT_FALSE(result.has_value()) << name;
		EXPECT_EQ(result.error().error, CommandError::RefusedDuringPlay) << name;
		EXPECT_NE(result.error().message.find("playing"), std::string::npos);
	}
	EXPECT_EQ(count(), 1u);
	m_registry.add({.name = "test.read",
					.description = "Read only.",
					.args = {},
					.factory = {},
					.run = [](const CommandContext& /*iContext*/, const CommandArgs& /*iArgs*/)
							-> expected<CommandResult, CommandFailure> { return CommandResult{}; },
					.allowedInPlay = true});
	EXPECT_TRUE(m_registry.execute("test.read", {}, playing).has_value());
	const CommandContext noUndo{.scene = m_scene, .undo = nullptr, .playing = false, .scenePath = {}, .onSaved = {}};
	EXPECT_EQ(m_registry.execute("test.read", {}, noUndo).error().error, CommandError::NotApplicable);
}
