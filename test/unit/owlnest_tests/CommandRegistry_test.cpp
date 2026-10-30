/**
 * @file CommandRegistry_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/CommandRegistry.h"
#include "UndoManager.h"
#include "commands/HudCommands.h"
#include "testHelper.h"

#include <core/SerializerImpl.h>
#include <renderer/Material.h>
#include <renderer/RenderLayerFactory.h>
#include <renderer/RenderStack.h>
#include <scene/ComponentRegistry.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/Canvas.h>
#include <scene/component/CircleRenderer.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/MeshRenderer.h>
#include <scene/component/Tag.h>
#include <scene/component/Tilemap.h>
#include <scene/component/Transform.h>
#include <scene/component/UiButton.h>
#include <scene/component/UiLayout.h>
#include <scene/component/UiList.h>
#include <scene/component/UiRect.h>
#include <scene/component/UiScrollView.h>
#include <scene/component/UiText.h>
#include <scene/nav/NavProfile.h>
#include <sound/SoundCue.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

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
							"entity.set_transform", "entity.create_light", "component.add", "component.remove",
							"prefab.instantiate", "mesh.create", "material.assign"}) {
		const auto* spec = m_registry.find(name);
		ASSERT_NE(spec, nullptr) << name;
		EXPECT_FALSE(spec->description.empty()) << name;
		EXPECT_TRUE(spec->factory) << name;
	}
	for (const auto* name:
		 {"component.set", "scene.save", "project.add_render_layer", "edit.undo", "edit.redo", "macro.run"}) {
		const auto* spec = m_registry.find(name);
		ASSERT_NE(spec, nullptr) << name;
		EXPECT_TRUE(spec->factory || spec->run) << name;
		EXPECT_FALSE(spec->allowedInPlay) << name;
	}
	EXPECT_EQ(m_registry.find("entity.explode"), nullptr);
	EXPECT_EQ(m_registry.getCommands().size(), 65u);
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

TEST_F(CommandRegistryTest, AttachToASocketThenDetachAndUndo) {
	auto owner = m_scene->createEntity("Hero");
	auto& sockets = owner.addComponent<scene::component::MeshSockets>();
	sockets.sockets.push_back({.name = "hand", .bone = "", .offset = {}});
	sockets.sockets.back().offset.translation() = {0.5f, 1.f, 0.f};
	auto item = m_scene->createEntity("Lantern");
	item.getComponent<scene::component::Transform>().transform.translation() = {4.f, 0.f, 0.f};
	EXPECT_FALSE(run("entity.attach_socket",
					 {{"entity", idOf(item)}, {"owner", idOf(owner)}, {"socket", std::string{"foot"}}}));
	EXPECT_FALSE(run("entity.attach_socket",
					 {{"entity", idOf(owner)}, {"owner", idOf(owner)}, {"socket", std::string{"hand"}}}));
	EXPECT_FALSE(run("entity.detach_socket", {{"entity", idOf(item)}}));
	ASSERT_TRUE(run("entity.attach_socket",
					{{"entity", idOf(item)}, {"owner", idOf(owner)}, {"socket", std::string{"hand"}}}));
	EXPECT_EQ(item.getComponent<scene::component::Hierarchy>().parentId, owner.getUUID());
	EXPECT_EQ(item.getComponent<scene::component::SocketAttachment>().socket, "hand");
	EXPECT_FLOAT_EQ(item.getComponent<scene::component::Transform>().transform.translation().y(), 1.f);
	m_undo.undo(*m_scene);
	EXPECT_EQ(item.getComponent<scene::component::Hierarchy>().parentId, core::UUID{0});
	EXPECT_FALSE(item.hasComponent<scene::component::SocketAttachment>());
	EXPECT_FLOAT_EQ(item.getComponent<scene::component::Transform>().transform.translation().x(), 4.f);
	m_undo.redo(*m_scene);
	ASSERT_TRUE(run("entity.detach_socket", {{"entity", idOf(item)}}));
	EXPECT_FALSE(item.hasComponent<scene::component::SocketAttachment>());
	EXPECT_EQ(item.getComponent<scene::component::Hierarchy>().parentId, owner.getUUID());
	m_undo.undo(*m_scene);
	EXPECT_EQ(item.getComponent<scene::component::SocketAttachment>().socket, "hand");
}

TEST_F(CommandRegistryTest, SetSocketMovesItAndUndoes) {
	auto owner = m_scene->createEntity("Hero");
	owner.addComponent<scene::component::MeshSockets>().sockets.push_back({.name = "hand", .bone = "", .offset = {}});
	const auto socketOf = [&owner]() -> const scene::component::MeshSockets::Socket& {
		return owner.getComponent<scene::component::MeshSockets>().sockets.front();
	};
	EXPECT_FALSE(run("entity.set_socket", {{"entity", idOf(owner)}, {"socket", std::string{"foot"}}}));
	ASSERT_TRUE(run("entity.set_socket", {{"entity", idOf(owner)},
										  {"socket", std::string{"hand"}},
										  {"translation", math::vec3{0.f, 1.5f, 0.f}},
										  {"rotation", math::vec3{0.f, 0.5f, 0.f}},
										  {"bone", std::string{"hand_r"}}}));
	EXPECT_FLOAT_EQ(socketOf().offset.translation().y(), 1.5f);
	EXPECT_FLOAT_EQ(socketOf().offset.rotation().y(), 0.5f);
	EXPECT_EQ(socketOf().bone, "hand_r");
	m_undo.undo(*m_scene);
	EXPECT_FLOAT_EQ(socketOf().offset.translation().y(), 0.f);
	EXPECT_TRUE(socketOf().bone.empty());
	m_undo.redo(*m_scene);
	EXPECT_FLOAT_EQ(socketOf().offset.translation().y(), 1.5f);
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
	EXPECT_EQ(m_registry.getCommands().size(), 65u);
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

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class HudCommandsTest : public CommandRegistryTest {
protected:
	auto addWidget(const std::string& iKind, const CommandArgs& iExtra = {}) -> scene::Entity {
		CommandArgs args = iExtra;
		args.set("kind", iKind);
		const auto added = run("hud.add_widget", args);
		EXPECT_TRUE(added.has_value()) << (added ? "" : added.error().message);
		return added ? m_scene->findEntityByUUID(added->entity) : scene::Entity{};
	}

	static auto rectOf(const scene::Entity& iEntity) -> const scene::component::UiRect& {
		return iEntity.getComponent<scene::component::UiRect>();
	}

	static auto idList(const std::vector<scene::Entity>& iEntities) -> std::string {
		std::string text;
		for (const auto& entity: iEntities)
			text += (text.empty() ? "" : ",") + std::to_string(static_cast<uint64_t>(entity.getUUID()));
		return text;
	}
};
OWL_DIAG_POP

}// namespace

TEST_F(HudCommandsTest, DeclaresTheHudCommands) {
	for (const auto* name: {"hud.add_widget", "hud.set_rect", "hud.set_anchors", "hud.set_pivot", "hud.move",
							"hud.delete", "hud.reparent", "hud.extract_canvas"}) {
		const auto* spec = m_registry.find(name);
		ASSERT_NE(spec, nullptr) << name;
		EXPECT_FALSE(spec->description.empty()) << name;
		EXPECT_TRUE(spec->factory) << name;
		EXPECT_FALSE(spec->allowedInPlay) << name;
	}
	for (const auto* name: {"hud.add_widget", "hud.set_rect"}) {
		const auto* spec = m_registry.find(name);
		ASSERT_NE(spec, nullptr) << name;
		for (const auto* arg: {"size_unit", "offset_unit"}) {
			EXPECT_TRUE(std::ranges::any_of(spec->args, [&](const ArgSpec& iArg) -> bool { return iArg.name == arg; }))
					<< name << " " << arg;
		}
	}
}

TEST_F(HudCommandsTest, AddWidgetCreatesTheCanvasWhenTheSceneHasNone) {
	const auto button = addWidget("button", {{"name", std::string{"Play"}}});
	ASSERT_TRUE(button);
	EXPECT_EQ(m_scene->registry.view<scene::component::Canvas>().size(), 1u);
	EXPECT_TRUE(button.hasComponent<scene::component::UiButton>());
	EXPECT_TRUE(button.hasComponent<scene::component::UiText>());
	EXPECT_EQ(button.getName(), "Play");
	const auto parent = m_scene->findEntityByUUID(button.getComponent<scene::component::Hierarchy>().parentId);
	ASSERT_TRUE(parent);
	EXPECT_TRUE(parent.hasComponent<scene::component::Canvas>());

	const auto buttonId = button.getUUID();
	m_undo.undo(*m_scene);
	EXPECT_EQ(count(), 0u);
	m_undo.redo(*m_scene);
	EXPECT_EQ(count(), 2u);
	const auto again = m_scene->findEntityByUUID(buttonId);
	ASSERT_TRUE(again);
	EXPECT_TRUE(again.hasComponent<scene::component::UiButton>());
	EXPECT_TRUE(m_scene->findEntityByUUID(again.getComponent<scene::component::Hierarchy>().parentId)
						.hasComponent<scene::component::Canvas>());
}

TEST_F(HudCommandsTest, AddWidgetUsesTheGivenParentAnchorAndOffset) {
	const auto canvas = addWidget("canvas", {{"name", std::string{"Gameplay"}}});
	ASSERT_TRUE(canvas);
	const auto panel = addWidget("panel", {{"parent", idOf(canvas)}});
	const auto label = addWidget("label", {{"parent", idOf(panel)},
										   {"anchor", math::vec2{0.f, 0.f}},
										   {"offset", math::vec2{30.f, 12.f}},
										   {"size", math::vec2{50.f, 20.f}}});
	ASSERT_TRUE(label);
	EXPECT_EQ(label.getComponent<scene::component::Hierarchy>().parentId, panel.getUUID());
	EXPECT_EQ(rectOf(label).getAnchor(), scene::component::UiRect::Anchor::TopLeft);
	EXPECT_FLOAT_EQ(rectOf(label).anchorOffset.x(), 30.f);
	EXPECT_FLOAT_EQ(rectOf(label).size.y(), 20.f);
	EXPECT_EQ(m_scene->registry.view<scene::component::Canvas>().size(), 1u);
	EXPECT_EQ(panel.getComponent<scene::component::Hierarchy>().childrenIds.size(), 1u);
}

TEST_F(HudCommandsTest, AddWidgetMakesAScrollViewAndAListAndUndoes) {
	const auto before = count();
	const auto view = addWidget("scroll_view");
	ASSERT_TRUE(view);
	EXPECT_TRUE(view.hasComponent<scene::component::UiScrollView>());
	EXPECT_TRUE(view.hasComponent<scene::component::UiLayout>());
	EXPECT_TRUE(view.hasComponent<scene::component::UiRect>());
	const auto list = addWidget("list", {{"parent", idOf(view)}, {"name", std::string{"Vehicles"}}});
	ASSERT_TRUE(list);
	EXPECT_EQ(list.getName(), "Vehicles");
	ASSERT_TRUE(list.hasComponent<scene::component::UiList>());
	EXPECT_GT(list.getComponent<scene::component::UiList>().itemCount, 0u);
	EXPECT_EQ(list.getComponent<scene::component::Hierarchy>().parentId, view.getUUID());
	const auto listId = list.getUUID();
	m_undo.undo(*m_scene);
	EXPECT_FALSE(m_scene->findEntityByUUID(listId));
	m_undo.redo(*m_scene);
	const auto restored = m_scene->findEntityByUUID(listId);
	ASSERT_TRUE(restored);
	EXPECT_TRUE(restored.hasComponent<scene::component::UiList>());
	EXPECT_GT(count(), before);
	const auto& kinds = getHudWidgetKinds();
	EXPECT_NE(std::ranges::find(kinds, "scroll_view"), kinds.end());
	EXPECT_NE(std::ranges::find(kinds, "list"), kinds.end());
}

TEST_F(HudCommandsTest, SetComponentEditsAListAndAScrollViewAndUndoes) {
	const auto view = addWidget("scroll_view");
	const auto list = addWidget("list");
	const auto setList = run("component.set", {{"entity", idOf(list)},
											   {"component", std::string{"UI List"}},
											   {"values", std::string{R"({"itemCount": 40, "rowHeight": 32, )"
																	  R"("onBindCallback": "bind_rows"})"}}});
	ASSERT_TRUE(setList.has_value()) << setList.error().message;
	const auto setView = run("component.set", {{"entity", idOf(view)},
											   {"component", std::string{"UI Scroll View"}},
											   {"values", std::string{R"({"horizontal": true, "wheelStep": 20, )"
																	  R"("contentSize": [600, 0]})"}}});
	ASSERT_TRUE(setView.has_value()) << setView.error().message;
	const auto& data = list.getComponent<scene::component::UiList>();
	EXPECT_EQ(data.itemCount, 40u);
	EXPECT_FLOAT_EQ(data.rowHeight, 32.f);
	EXPECT_EQ(data.onBindCallback, "bind_rows");
	const auto& scroll = view.getComponent<scene::component::UiScrollView>();
	EXPECT_TRUE(scroll.horizontal);
	EXPECT_FLOAT_EQ(scroll.contentSize.x(), 600.f);
	const auto listId = list.getUUID();
	m_undo.undo(*m_scene);
	m_undo.undo(*m_scene);
	const auto reverted = m_scene->findEntityByUUID(listId);
	EXPECT_EQ(reverted.getComponent<scene::component::UiList>().itemCount, 12u);
	EXPECT_TRUE(reverted.getComponent<scene::component::UiList>().onBindCallback.empty());
}

TEST_F(HudCommandsTest, AddWidgetRefusesAParentOutsideTheHud) {
	auto plain = m_scene->createEntity("Plain");
	const auto result = run("hud.add_widget", {{"kind", std::string{"panel"}}, {"parent", idOf(plain)}});
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().error, CommandError::InvalidArgument);
	const auto bad = run("hud.add_widget", {{"kind", std::string{"sprite"}}});
	ASSERT_FALSE(bad.has_value());
	EXPECT_EQ(bad.error().error, CommandError::InvalidArgument);
}

TEST_F(HudCommandsTest, SetRectChangesOnlyTheGivenFieldsAndMergesDrags) {
	const auto panel = addWidget("panel");
	const auto sizeBefore = rectOf(panel).size;
	const auto first = run("hud.set_rect", {{"entity", idOf(panel)}, {"offset", math::vec2{10.f, 5.f}}});
	ASSERT_TRUE(first.has_value()) << first.error().message;
	const auto second = run("hud.set_rect", {{"entity", idOf(panel)}, {"offset", math::vec2{20.f, 9.f}}});
	ASSERT_TRUE(second.has_value());
	EXPECT_FLOAT_EQ(rectOf(panel).anchorOffset.x(), 20.f);
	EXPECT_EQ(rectOf(panel).size, sizeBefore);
	m_undo.undo(*m_scene);
	EXPECT_FLOAT_EQ(rectOf(panel).anchorOffset.x(), 0.f);
	m_undo.redo(*m_scene);
	EXPECT_FLOAT_EQ(rectOf(panel).anchorOffset.x(), 20.f);
}

TEST_F(HudCommandsTest, SetRectRefusesNothingToChangeAndNonWidgets) {
	const auto panel = addWidget("panel");
	const auto none = run("hud.set_rect", {{"entity", idOf(panel)}});
	ASSERT_FALSE(none.has_value());
	EXPECT_EQ(none.error().error, CommandError::MissingArgument);
	const auto same = run("hud.set_rect", {{"entity", idOf(panel)}, {"size", rectOf(panel).size}});
	ASSERT_FALSE(same.has_value());
	EXPECT_EQ(same.error().error, CommandError::NotApplicable);
	auto plain = m_scene->createEntity("Plain");
	const auto noRect = run("hud.set_rect", {{"entity", idOf(plain)}, {"size", math::vec2{1.f, 1.f}}});
	ASSERT_FALSE(noRect.has_value());
	EXPECT_EQ(noRect.error().error, CommandError::InvalidArgument);
}

TEST_F(HudCommandsTest, SetAnchorsKeepsTheWidgetWhereItIs) {
	const auto panel = addWidget("panel", {{"size", math::vec2{200.f, 100.f}}});
	const auto result = run("hud.set_anchors", {{"entity", idOf(panel)}, {"anchor_min", math::vec2{0.f, 0.f}}});
	ASSERT_TRUE(result.has_value()) << result.error().message;
	const auto& rect = rectOf(panel);
	EXPECT_EQ(rect.getAnchor(), scene::component::UiRect::Anchor::TopLeft);
	const auto bounds = rect.computeBounds({.position = {0.f, 0.f}, .size = {1280.f, 720.f}});
	EXPECT_FLOAT_EQ(bounds.position.x(), 540.f);
	EXPECT_FLOAT_EQ(bounds.position.y(), 310.f);
	EXPECT_FLOAT_EQ(bounds.size.x(), 200.f);
	m_undo.undo(*m_scene);
	EXPECT_EQ(rectOf(panel).getAnchor(), scene::component::UiRect::Anchor::Center);
}

TEST_F(HudCommandsTest, SetAnchorsStretchesWithTheParentAndKeepsTheBounds) {
	const auto panel = addWidget("panel", {{"size", math::vec2{200.f, 100.f}}});
	const auto result = run(
			"hud.set_anchors",
			{{"entity", idOf(panel)}, {"anchor_min", math::vec2{0.f, 0.5f}}, {"anchor_max", math::vec2{1.f, 0.5f}}});
	ASSERT_TRUE(result.has_value()) << result.error().message;
	const auto bounds = rectOf(panel).computeBounds({.position = {0.f, 0.f}, .size = {1280.f, 720.f}});
	EXPECT_FLOAT_EQ(bounds.position.x(), 540.f);
	EXPECT_FLOAT_EQ(bounds.size.x(), 200.f);
	const auto wider = rectOf(panel).computeBounds({.position = {0.f, 0.f}, .size = {1920.f, 720.f}});
	EXPECT_FLOAT_EQ(wider.size.x(), 840.f);
}

TEST_F(HudCommandsTest, SetAnchorsWithoutKeepingBoundsSetsThemAsGiven) {
	const auto panel = addWidget("panel", {{"size", math::vec2{200.f, 100.f}}});
	const auto result = run("hud.set_anchors",
							{{"entity", idOf(panel)}, {"anchor_min", math::vec2{0.f, 0.f}}, {"keep_bounds", false}});
	ASSERT_TRUE(result.has_value());
	EXPECT_FLOAT_EQ(rectOf(panel).anchorOffset.x(), 0.f);
	EXPECT_FLOAT_EQ(rectOf(panel).size.x(), 200.f);
}

TEST_F(HudCommandsTest, SetPivotKeepsTheBounds) {
	const auto panel = addWidget("panel", {{"size", math::vec2{200.f, 100.f}}});
	const auto result = run("hud.set_pivot", {{"entity", idOf(panel)}, {"pivot", math::vec2{0.f, 0.f}}});
	ASSERT_TRUE(result.has_value()) << result.error().message;
	const auto bounds = rectOf(panel).computeBounds({.position = {0.f, 0.f}, .size = {1280.f, 720.f}});
	EXPECT_FLOAT_EQ(bounds.position.x(), 540.f);
	EXPECT_FLOAT_EQ(bounds.position.y(), 310.f);
	EXPECT_FLOAT_EQ(rectOf(panel).pivot.x(), 0.f);
	const auto missing = run("hud.set_pivot", {{"entity", idOf(panel)}});
	ASSERT_FALSE(missing.has_value());
	EXPECT_EQ(missing.error().error, CommandError::MissingArgument);
}

TEST_F(HudCommandsTest, MoveShiftsTheGroupInOneStepAndSkipsFollowers) {
	const auto first = addWidget("panel");
	const auto second = addWidget("panel");
	const auto child = addWidget("label", {{"parent", idOf(first)}});
	const auto result =
			run("hud.move", {{"entities", idList({first, second, child})}, {"delta", math::vec2{5.f, -3.f}}});
	ASSERT_TRUE(result.has_value()) << result.error().message;
	EXPECT_EQ(std::get<int64_t>(result->data.at("count")), 2);
	EXPECT_FLOAT_EQ(rectOf(first).anchorOffset.x(), 5.f);
	EXPECT_FLOAT_EQ(rectOf(second).anchorOffset.y(), -3.f);
	EXPECT_FLOAT_EQ(rectOf(child).anchorOffset.x(), 0.f);
	m_undo.undo(*m_scene);
	EXPECT_FLOAT_EQ(rectOf(first).anchorOffset.x(), 0.f);
	EXPECT_FLOAT_EQ(rectOf(second).anchorOffset.y(), 0.f);
	m_undo.redo(*m_scene);
	EXPECT_FLOAT_EQ(rectOf(second).anchorOffset.x(), 5.f);
}

TEST_F(HudCommandsTest, MoveFindsWidgetsByTagAndRefusesUnknownOnes) {
	const auto panel = addWidget("panel", {{"name", std::string{"Bar"}}});
	ASSERT_TRUE(run("hud.move", {{"entities", std::string{"Bar"}}, {"delta", math::vec2{1.f, 0.f}}}).has_value());
	EXPECT_FLOAT_EQ(rectOf(panel).anchorOffset.x(), 1.f);
	const auto unknown = run("hud.move", {{"entities", std::string{"Nope"}}, {"delta", math::vec2{1.f, 0.f}}});
	ASSERT_FALSE(unknown.has_value());
	EXPECT_EQ(unknown.error().error, CommandError::UnknownEntity);
	const auto empty = run("hud.move", {{"entities", std::string{" , "}}, {"delta", math::vec2{1.f, 0.f}}});
	ASSERT_FALSE(empty.has_value());
	EXPECT_EQ(empty.error().error, CommandError::MissingArgument);
	const auto idle = run("hud.move", {{"entities", std::string{"Bar"}}, {"delta", math::vec2{0.f, 0.f}}});
	ASSERT_FALSE(idle.has_value());
	EXPECT_EQ(idle.error().error, CommandError::NotApplicable);
}

TEST_F(HudCommandsTest, DeleteRemovesTheGroupWithItsChildrenAndUndoes) {
	const auto window = addWidget("window");
	const auto inner = addWidget("label", {{"parent", idOf(window)}});
	const auto other = addWidget("panel");
	const auto kept = addWidget("panel");
	const auto before = count();
	const auto windowId = window.getUUID();
	const auto innerId = inner.getUUID();
	const auto result = run("hud.delete", {{"entities", idList({window, inner, other})}});
	ASSERT_TRUE(result.has_value()) << result.error().message;
	EXPECT_EQ(std::get<int64_t>(result->data.at("count")), 2);
	EXPECT_EQ(count(), before - 3);
	EXPECT_TRUE(m_scene->findEntityByUUID(kept.getUUID()));
	m_undo.undo(*m_scene);
	EXPECT_EQ(count(), before);
	const auto restoredInner = m_scene->findEntityByUUID(innerId);
	ASSERT_TRUE(restoredInner);
	EXPECT_EQ(restoredInner.getComponent<scene::component::Hierarchy>().parentId, windowId);
	m_undo.redo(*m_scene);
	EXPECT_EQ(count(), before - 3);
}

TEST_F(HudCommandsTest, ReparentKeepsTheWidgetOnScreenAndSetsTheIndex) {
	const auto panel = addWidget("panel", {{"size", math::vec2{400.f, 300.f}}});
	const auto label = addWidget("label", {{"size", math::vec2{100.f, 40.f}}});
	const auto sibling = addWidget("label", {{"parent", idOf(panel)}});
	const auto result = run("hud.reparent", {{"entity", idOf(label)}, {"parent", idOf(panel)}, {"index", int64_t{0}}});
	ASSERT_TRUE(result.has_value()) << result.error().message;
	const auto& children = panel.getComponent<scene::component::Hierarchy>().childrenIds;
	ASSERT_EQ(children.size(), 2u);
	EXPECT_EQ(children.front(), label.getUUID());
	EXPECT_EQ(children.back(), sibling.getUUID());
	// Centred in the canvas before, in the panel (centred as well) after: same screen rectangle.
	const auto bounds = rectOf(label).computeBounds({.position = {440.f, 210.f}, .size = {400.f, 300.f}});
	EXPECT_FLOAT_EQ(bounds.position.x(), 590.f);
	EXPECT_FLOAT_EQ(bounds.position.y(), 340.f);
	m_undo.undo(*m_scene);
	EXPECT_NE(label.getComponent<scene::component::Hierarchy>().parentId, panel.getUUID());
	EXPECT_EQ(panel.getComponent<scene::component::Hierarchy>().childrenIds.size(), 1u);
	EXPECT_FLOAT_EQ(rectOf(label).anchorOffset.x(), 0.f);
	m_undo.redo(*m_scene);
	EXPECT_EQ(label.getComponent<scene::component::Hierarchy>().parentId, panel.getUUID());
}

TEST_F(HudCommandsTest, ReparentRefusesCyclesAndDoesNothingTwice) {
	const auto panel = addWidget("panel");
	const auto child = addWidget("panel", {{"parent", idOf(panel)}});
	const auto cycle = run("hud.reparent", {{"entity", idOf(panel)}, {"parent", idOf(child)}});
	ASSERT_FALSE(cycle.has_value());
	EXPECT_EQ(cycle.error().error, CommandError::NotApplicable);
	const auto self = run("hud.reparent", {{"entity", idOf(panel)}, {"parent", idOf(panel)}});
	ASSERT_FALSE(self.has_value());
	const auto same = run("hud.reparent", {{"entity", idOf(child)}, {"parent", idOf(panel)}});
	ASSERT_FALSE(same.has_value());
	EXPECT_EQ(same.error().error, CommandError::NotApplicable);
	auto plain = m_scene->createEntity("Plain");
	const auto outside = run("hud.reparent", {{"entity", idOf(child)}, {"parent", idOf(plain)}});
	ASSERT_FALSE(outside.has_value());
	EXPECT_EQ(outside.error().error, CommandError::InvalidArgument);
}

TEST_F(HudCommandsTest, EveryHudCommandIsRefusedWhilePlaying) {
	const auto panel = addWidget("panel");
	const CommandContext playing{.scene = m_scene, .undo = &m_undo, .playing = true, .scenePath = {}, .onSaved = {}};
	const auto result =
			m_registry.execute("hud.set_rect", {{"entity", idOf(panel)}, {"size", math::vec2{1.f, 1.f}}}, playing);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().error, CommandError::RefusedDuringPlay);
}

TEST_F(HudCommandsTest, SetRectWritesTheUnitsAndKeepsThemUndoable) {
	const auto panel = addWidget("panel");
	const auto result = run("hud.set_rect", {{"entity", idOf(panel)},
											 {"size", math::vec2{0.5f, 0.25f}},
											 {"size_unit", std::string{"ScreenWidth, ParentHeight"}},
											 {"offset_unit", std::string{"ScreenHeight"}}});
	ASSERT_TRUE(result.has_value()) << result.error().message;
	using Unit = scene::component::UiRect::Unit;
	EXPECT_EQ(rectOf(panel).sizeUnit.x, Unit::ScreenWidth);
	EXPECT_EQ(rectOf(panel).sizeUnit.y, Unit::ParentHeight);
	EXPECT_EQ(rectOf(panel).offsetUnit.x, Unit::ScreenHeight);
	EXPECT_EQ(rectOf(panel).offsetUnit.y, Unit::ScreenHeight);
	EXPECT_FLOAT_EQ(rectOf(panel).size.x(), 0.5f);
	// A later set_rect without units keeps them.
	ASSERT_TRUE(run("hud.set_rect", {{"entity", idOf(panel)}, {"offset", math::vec2{0.1f, 0.f}}}).has_value());
	EXPECT_EQ(rectOf(panel).sizeUnit.x, Unit::ScreenWidth);
	// The two edits of the same widget merge in one step.
	m_undo.undo(*m_scene);
	EXPECT_TRUE(rectOf(panel).sizeUnit.isPixels());
	EXPECT_TRUE(rectOf(panel).offsetUnit.isPixels());
	m_undo.redo(*m_scene);
	EXPECT_EQ(rectOf(panel).sizeUnit.y, Unit::ParentHeight);
}

TEST_F(HudCommandsTest, SetRectRefusesAnUnknownUnit) {
	const auto panel = addWidget("panel");
	const auto bad = run("hud.set_rect", {{"entity", idOf(panel)}, {"size_unit", std::string{"Furlongs"}}});
	ASSERT_FALSE(bad.has_value());
	EXPECT_EQ(bad.error().error, CommandError::InvalidArgument);
	const auto many =
			run("hud.set_rect", {{"entity", idOf(panel)}, {"size_unit", std::string{"Pixels,Pixels,Pixels"}}});
	ASSERT_FALSE(many.has_value());
	EXPECT_EQ(many.error().error, CommandError::InvalidArgument);
}

TEST_F(HudCommandsTest, AddWidgetTakesUnits) {
	const auto panel = addWidget(
			"panel", {{"size", math::vec2{0.5f, 0.5f}}, {"size_unit", std::string{"ScreenWidth,ScreenHeight"}}});
	ASSERT_TRUE(panel);
	EXPECT_EQ(rectOf(panel).sizeUnit.x, scene::component::UiRect::Unit::ScreenWidth);
	EXPECT_EQ(rectOf(panel).sizeUnit.y, scene::component::UiRect::Unit::ScreenHeight);
	EXPECT_TRUE(rectOf(panel).offsetUnit.isPixels());
}

TEST_F(HudCommandsTest, MoveKeepsTheUnitOfARatioOffset) {
	using Unit = scene::component::UiRect::Unit;
	const auto panel = addWidget("panel");
	ASSERT_TRUE(run("hud.set_rect", {{"entity", idOf(panel)},
									 {"offset", math::vec2{0.1f, 5.f}},
									 {"offset_unit", std::string{"ParentWidth,Pixels"}}})
						.has_value());
	// The canvas is 1280 reference pixels wide: 128 pixels are 0.1 of it.
	const auto result = run("hud.move", {{"entities", idList({panel})}, {"delta", math::vec2{128.f, 4.f}}});
	ASSERT_TRUE(result.has_value()) << result.error().message;
	EXPECT_EQ(rectOf(panel).offsetUnit.x, Unit::ParentWidth);
	EXPECT_NEAR(rectOf(panel).anchorOffset.x(), 0.2f, 1e-4f);
	EXPECT_NEAR(rectOf(panel).anchorOffset.y(), 9.f, 1e-4f);
	// Merged with the set_rect just before: one undo goes back to the pixels.
	m_undo.undo(*m_scene);
	EXPECT_TRUE(rectOf(panel).offsetUnit.isPixels());
}

TEST_F(HudCommandsTest, SetAnchorsKeepsTheBoundsOfARatioSizedWidget) {
	const auto panel = addWidget("panel");
	ASSERT_TRUE(run("hud.set_rect", {{"entity", idOf(panel)},
									 {"size", math::vec2{0.25f, 0.25f}},
									 {"size_unit", std::string{"ScreenWidth,ScreenWidth"}}})
						.has_value());
	ASSERT_TRUE(run("hud.set_anchors", {{"entity", idOf(panel)}, {"anchor_min", math::vec2{0.f, 0.f}}}).has_value());
	const auto bounds = rectOf(panel).computeBounds({.position = {0.f, 0.f}, .size = {1280.f, 720.f}});
	EXPECT_NEAR(bounds.position.x(), 640.f - 160.f, 1e-3f);
	EXPECT_NEAR(bounds.size.x(), 320.f, 1e-3f);
	EXPECT_NEAR(bounds.size.y(), 320.f, 1e-3f);
	EXPECT_EQ(rectOf(panel).sizeUnit.x, scene::component::UiRect::Unit::ScreenWidth);
}

TEST_F(HudCommandsTest, SolveRectWritesBackInTheUnitsOfTheBase) {
	scene::component::UiRect base;
	base.sizeUnit = {.x = scene::component::UiRect::Unit::ParentWidth,
					 .y = scene::component::UiRect::Unit::ScreenHeight};
	base.offsetUnit = {.x = scene::component::UiRect::Unit::ScreenWidth, .y = scene::component::UiRect::Unit::Pixels};
	const scene::component::UiRect::Frame frame{.parent = {.position = {10.f, 20.f}, .size = {800.f, 400.f}},
												.screen = {1280.f, 720.f}};
	const scene::component::UiRect::Bounds target{.position = {55.f, 77.f}, .size = {200.f, 72.f}};
	const auto solved = solveHudRect(base, frame, target);
	EXPECT_EQ(solved.sizeUnit, base.sizeUnit);
	EXPECT_NEAR(solved.size.x(), 0.25f, 1e-5f);
	EXPECT_NEAR(solved.size.y(), 0.1f, 1e-5f);
	const auto bounds = solved.computeBounds(frame.parent, frame.screen);
	EXPECT_NEAR(bounds.position.x(), 55.f, 1e-3f);
	EXPECT_NEAR(bounds.position.y(), 77.f, 1e-3f);
	EXPECT_NEAR(bounds.size.x(), 200.f, 1e-3f);
}

TEST_F(HudCommandsTest, SolveRectCoversTheTargetWhateverTheAnchorsAndPivot) {
	scene::component::UiRect base;
	base.anchorMin = {0.f, 0.25f};
	base.anchorMax = {1.f, 0.75f};
	base.pivot = {0.2f, 0.9f};
	const scene::component::UiRect::Bounds parent{.position = {10.f, 20.f}, .size = {800.f, 400.f}};
	const scene::component::UiRect::Bounds target{.position = {55.f, 77.f}, .size = {123.f, 45.f}};
	const auto solved = solveHudRect(base, {.parent = parent, .screen = parent.size}, target);
	const auto bounds = solved.computeBounds(parent);
	EXPECT_NEAR(bounds.position.x(), target.position.x(), 1e-3f);
	EXPECT_NEAR(bounds.position.y(), target.position.y(), 1e-3f);
	EXPECT_NEAR(bounds.size.x(), target.size.x(), 1e-3f);
	EXPECT_NEAR(bounds.size.y(), target.size.y(), 1e-3f);
	EXPECT_EQ(solved.anchorMin, base.anchorMin);
	EXPECT_EQ(solved.pivot, base.pivot);
}

TEST_F(HudCommandsTest, ExtractCanvasWritesTheHudRemovesTheCanvasAndUndoesIt) {
	const auto button = addWidget("button", {{"name", std::string{"Play"}}});
	ASSERT_TRUE(button);
	const auto canvas = m_scene->findEntityByUUID(button.getComponent<scene::component::Hierarchy>().parentId);
	ASSERT_TRUE(canvas);
	const auto buttonId = button.getUUID();
	const auto otherId = m_scene->createEntity("World Thing").getUUID();
	const auto dir = std::filesystem::temp_directory_path() / "owl_extract_canvas_test";
	std::filesystem::create_directories(dir);
	const auto path = dir / "extracted.owlhud";
	const auto done = run("hud.extract_canvas", {{"entity", idOf(canvas)},
												 {"path", path.generic_string()},
												 {"asset_path", std::string{"huds/extracted.owlhud"}}});
	ASSERT_TRUE(done.has_value()) << done.error().message;
	EXPECT_EQ(std::get<std::string>(done->data.at("hud")), "huds/extracted.owlhud");
	EXPECT_EQ(m_scene->getHudPath(), std::optional<std::string>{"huds/extracted.owlhud"});
	EXPECT_EQ(m_scene->registry.view<scene::component::Canvas>().size(), 0u);
	EXPECT_FALSE(m_scene->findEntityByUUID(buttonId));
	EXPECT_TRUE(m_scene->findEntityByUUID(otherId));

	const auto hud = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer(hud, scene::DocumentKind::Hud).deserialize(path).has_value());
	const auto loadedButton = hud->findEntityByUUID(buttonId);
	ASSERT_TRUE(loadedButton);
	EXPECT_TRUE(loadedButton.hasComponent<scene::component::UiButton>());
	EXPECT_EQ(hud->registry.view<scene::component::Canvas>().size(), 1u);

	m_undo.undo(*m_scene);
	EXPECT_EQ(m_scene->registry.view<scene::component::Canvas>().size(), 1u);
	EXPECT_TRUE(m_scene->findEntityByUUID(buttonId));
	EXPECT_EQ(m_scene->getHudPath(), std::nullopt);
	m_undo.redo(*m_scene);
	EXPECT_EQ(m_scene->registry.view<scene::component::Canvas>().size(), 0u);
	EXPECT_EQ(m_scene->getHudPath(), std::optional<std::string>{"huds/extracted.owlhud"});
	std::filesystem::remove_all(dir);
}

TEST_F(HudCommandsTest, ExtractCanvasRefusesWhatIsNotAHudSource) {
	const auto button = addWidget("button");
	ASSERT_TRUE(button);
	const auto canvas = m_scene->findEntityByUUID(button.getComponent<scene::component::Hierarchy>().parentId);
	const auto dir = std::filesystem::temp_directory_path() / "owl_extract_canvas_refused_test";
	const auto path = (dir / "refused.owlhud").generic_string();
	const auto notCanvas = run("hud.extract_canvas",
							   {{"entity", idOf(button)}, {"path", path}, {"asset_path", std::string{"a.owlhud"}}});
	ASSERT_FALSE(notCanvas.has_value());
	EXPECT_EQ(notCanvas.error().error, CommandError::InvalidArgument);
	const auto badExtension =
			run("hud.extract_canvas", {{"entity", idOf(canvas)}, {"path", (dir / "x.owl").generic_string()}});
	ASSERT_FALSE(badExtension.has_value());
	EXPECT_EQ(badExtension.error().error, CommandError::InvalidArgument);
	const auto noAssetFolder = run("hud.extract_canvas", {{"entity", idOf(canvas)}, {"path", path}});
	ASSERT_FALSE(noAssetFolder.has_value());
	EXPECT_EQ(noAssetFolder.error().error, CommandError::InvalidArgument);
	EXPECT_FALSE(std::filesystem::exists(path));
	EXPECT_EQ(m_scene->registry.view<scene::component::Canvas>().size(), 1u);
}

TEST_F(CommandRegistryTest, NavigationExceptionsAreUndoableCommands) {
	auto map = m_scene->createEntity("Map");
	map.addComponent<scene::component::Tilemap>().navigation.mode = scene::nav::NavMode::Connections;
	const auto& navigation = [&]() -> const scene::nav::NavProfile& {
		return m_scene->findEntityByUUID(map.getUUID()).getComponent<scene::component::Tilemap>().navigation;
	};
	const auto edge = [&](const std::string& iSide, const std::string& iState) -> auto {
		return run("nav.set_edge",
				   {{"entity", idOf(map)}, {"x", int64_t{4}}, {"y", int64_t{5}}, {"side", iSide}, {"state", iState}});
	};
	ASSERT_TRUE(edge("east", "exit_only").has_value());
	EXPECT_EQ(navigation().getSideState(4, 5, scene::nav::Direction::East), scene::nav::SideState::ExitOnly);
	EXPECT_EQ(navigation().getSideState(5, 5, scene::nav::Direction::West), scene::nav::SideState::EnterOnly);
	ASSERT_TRUE(run("nav.set_cell_cost", {{"entity", idOf(map)}, {"x", int64_t{4}}, {"y", int64_t{5}}, {"cost", 2.5}})
						.has_value());
	ASSERT_NE(navigation().findOverride(4, 5), nullptr);
	EXPECT_FLOAT_EQ(navigation().findOverride(4, 5)->cost, 2.5f);
	// Edits of the same entity within a second merge into one history entry.
	m_undo.undo(*m_scene);
	EXPECT_TRUE(navigation().overrides.empty());
	m_undo.redo(*m_scene);
	EXPECT_EQ(navigation().getSideState(4, 5, scene::nav::Direction::East), scene::nav::SideState::ExitOnly);
	EXPECT_FLOAT_EQ(navigation().findOverride(4, 5)->cost, 2.5f);
	ASSERT_TRUE(run("nav.clear_cell", {{"entity", idOf(map)}, {"x", int64_t{4}}, {"y", int64_t{5}}}).has_value());
	EXPECT_TRUE(navigation().overrides.empty());
	m_undo.undo(*m_scene);
	EXPECT_TRUE(navigation().overrides.empty()) << "merged with the edits above, back to the start";
}

TEST_F(CommandRegistryTest, NavigationCommandsRefuseATilemapWithoutNavigation) {
	const auto plain = m_scene->createEntity("Plain");
	auto map = m_scene->createEntity("Map");
	map.addComponent<scene::component::Tilemap>();
	const auto expectError = [this](const std::string& iName, const CommandArgs& iArgs,
									const CommandError iError) -> void {
		const auto result = run(iName, iArgs);
		ASSERT_FALSE(result.has_value()) << iName;
		EXPECT_EQ(result.error().error, iError) << result.error().message;
	};
	const CommandArgs onPlain{{"entity", idOf(plain)}, {"x", int64_t{1}}, {"y", int64_t{1}}, {"cost", 2.0}};
	expectError("nav.set_cell_cost", onPlain, CommandError::InvalidArgument);
	expectError("nav.set_cell_cost", {{"entity", idOf(map)}, {"x", int64_t{1}}, {"y", int64_t{1}}, {"cost", 2.0}},
				CommandError::NotApplicable);
	expectError("nav.set_edge",
				{{"entity", idOf(map)},
				 {"x", int64_t{1}},
				 {"y", int64_t{1}},
				 {"side", std::string{"up"}},
				 {"state", std::string{"closed"}}},
				CommandError::InvalidArgument);
	EXPECT_FALSE(m_undo.canUndo());
}

TEST_F(CommandRegistryTest, AddRenderLayerEditsTheProjectStack) {
	renderer::RenderLayerFactory::registerType(
			"FakeMeshLayer", [](const std::string& /*iName*/) -> shared<renderer::RenderLayer> { return nullptr; });
	renderer::RendererStackConfig stack;
	int notified = 0;
	CommandContext context{.scene = m_scene, .undo = &m_undo, .playing = false};
	const CommandArgs args{{"type", std::string{"FakeMeshLayer"}}, {"name", std::string{"meshes"}}};
	EXPECT_EQ(m_registry.execute("project.add_render_layer", args, context).error().error, CommandError::NotApplicable);
	context.projectStack = &stack;
	context.onProjectStackChanged = [&notified]() -> void { ++notified; };
	EXPECT_EQ(m_registry.execute("project.add_render_layer", {{"type", std::string{"Unknown"}}}, context).error().error,
			  CommandError::InvalidArgument);
	// The empty stack stands for the default one: its layer stays first, the new one comes after it.
	const auto added = m_registry.execute("project.add_render_layer", args, context);
	ASSERT_TRUE(added.has_value()) << added.error().message;
	ASSERT_EQ(stack.entries.size(), 2u);
	EXPECT_EQ(stack.entries[0].name, renderer::RendererStackConfig::makeDefault().entries[0].name);
	EXPECT_EQ(stack.entries[1].name, "meshes");
	EXPECT_EQ(stack.entries[1].typeKey, "FakeMeshLayer");
	EXPECT_EQ(notified, 1);
	EXPECT_EQ(m_registry.execute("project.add_render_layer", args, context).error().error,
			  CommandError::InvalidArgument);
	const auto front = m_registry.execute(
			"project.add_render_layer",
			{{"type", std::string{"FakeMeshLayer"}}, {"name", std::string{"first"}}, {"position", int64_t{-3}}},
			context);
	ASSERT_TRUE(front.has_value()) << front.error().message;
	EXPECT_EQ(stack.entries.front().name, "first");
	EXPECT_EQ(notified, 2);
	EXPECT_FALSE(m_undo.canUndo());
	EXPECT_TRUE(renderer::RenderLayerFactory::unregisterType("FakeMeshLayer"));
}

TEST_F(CommandRegistryTest, MaterialCreateWritesAReadableFile) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_material_create_test";
	std::filesystem::remove_all(dir);
	const auto file = dir / "materials" / "gold.owlmat";
	const auto result = run("material.create", {{"path", file.string()}});
	ASSERT_TRUE(result.has_value());
	const auto root = renderer::MaterialAsset::load(file);
	ASSERT_TRUE(root.has_value());
	EXPECT_TRUE(root->parent.empty());
	EXPECT_FALSE(run("material.create", {{"path", file.string()}}).has_value());
	EXPECT_FALSE(run("material.create", {{"path", (dir / "gold.png").string()}}).has_value());

	const auto instance = dir / "gold_worn.owlmat";
	ASSERT_TRUE(run("material.create", {{"path", instance.string()}, {"parent", std::string{"materials/gold.owlmat"}}})
						.has_value());
	const auto child = renderer::MaterialAsset::load(instance);
	ASSERT_TRUE(child.has_value());
	EXPECT_EQ(child->parent.generic_string(), "materials/gold.owlmat");
	EXPECT_EQ(child->overrides, 0u);
	std::filesystem::remove_all(dir);
}

TEST_F(CommandRegistryTest, SoundCreateWritesACue) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_sound_create_test";
	std::filesystem::remove_all(dir);
	const auto file = dir / "sounds" / "steps.owlsound";
	ASSERT_TRUE(run("sound.create", {{"path", file.string()}, {"file", std::string{"sounds/step.wav"}}}).has_value());
	const auto cue = sound::SoundCue::load(file);
	ASSERT_TRUE(cue.has_value());
	ASSERT_EQ(cue->variants.size(), 1u);
	EXPECT_EQ(cue->variants[0].file, "sounds/step.wav");
	EXPECT_FALSE(run("sound.create", {{"path", file.string()}}).has_value());
	EXPECT_FALSE(run("sound.create", {{"path", (dir / "steps.wav").string()}}).has_value());
	EXPECT_FALSE(
			run("sound.create", {{"path", (dir / "other.owlsound").string()}, {"file", file.string()}}).has_value());
	ASSERT_TRUE(run("sound.create", {{"path", (dir / "empty.owlsound").string()}}).has_value());
	EXPECT_TRUE(sound::SoundCue::load(dir / "empty.owlsound")->variants.empty());
	std::filesystem::remove_all(dir);
}

TEST_F(CommandRegistryTest, MaterialAssignSetsTheSlotsAndUndoRestoresThem) {
	auto crate = m_scene->createEntity("Crate");
	auto& meshRenderer = crate.addComponent<scene::component::MeshRenderer>();
	meshRenderer.materials.resize(2);
	meshRenderer.materials[0].material = "materials/old.owlmat";
	const auto target = idOf(crate);

	ASSERT_TRUE(run("material.assign",
					{{"entity", target}, {"slot", int64_t{1}}, {"material", std::string{"materials/gold.owlmat"}}})
						.has_value());
	auto& slots = crate.getComponent<scene::component::MeshRenderer>().materials;
	ASSERT_EQ(slots.size(), 2u);
	EXPECT_EQ(slots[0].material.generic_string(), "materials/old.owlmat");
	EXPECT_EQ(slots[1].material.generic_string(), "materials/gold.owlmat");

	ASSERT_TRUE(run("material.assign", {{"entity", target}, {"material", std::string{"materials/all.owlmat"}}})
						.has_value());
	EXPECT_EQ(crate.getComponent<scene::component::MeshRenderer>().materials[0].material.generic_string(),
			  "materials/all.owlmat");
	ASSERT_TRUE(run("material.assign", {{"entity", target}, {"slot", int64_t{0}}, {"material", std::string{}}})
						.has_value());
	EXPECT_TRUE(crate.getComponent<scene::component::MeshRenderer>().materials[0].material.empty());

	ASSERT_TRUE(m_undo.canUndo());
	while (m_undo.canUndo()) m_undo.undo(*m_scene);
	const auto& restored = crate.getComponent<scene::component::MeshRenderer>().materials;
	EXPECT_EQ(restored[0].material.generic_string(), "materials/old.owlmat");
	EXPECT_TRUE(restored[1].material.empty());
}

TEST_F(CommandRegistryTest, MaterialAssignRefusesBadArguments) {
	auto crate = m_scene->createEntity("Crate");
	crate.addComponent<scene::component::MeshRenderer>().materials.resize(1);
	const auto bare = m_scene->createEntity("Bare");
	const auto material = std::string{"materials/gold.owlmat"};
	EXPECT_FALSE(run("material.assign", {{"entity", idOf(crate)}, {"slot", int64_t{-2}}, {"material", material}})
						 .has_value());
	EXPECT_FALSE(run("material.assign", {{"entity", idOf(crate)}, {"slot", int64_t{1000}}, {"material", material}})
						 .has_value());
	EXPECT_FALSE(run("material.assign", {{"entity", idOf(crate)}, {"material", std::string{"gold.png"}}}).has_value());
	EXPECT_FALSE(run("material.assign", {{"entity", idOf(bare)}, {"material", material}}).has_value());
	EXPECT_FALSE(m_undo.canUndo());
}
