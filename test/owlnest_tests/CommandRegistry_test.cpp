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
#include <scene/component/CircleRenderer.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/Tag.h>
#include <scene/component/Transform.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
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
	EXPECT_EQ(m_registry.find("entity.explode"), nullptr);
	EXPECT_EQ(m_registry.getCommands().size(), 9u);
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
	EXPECT_EQ(m_registry.getCommands().size(), 9u);
}
