/**
 * @file LargeWorldCommands_test.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <renderer/RenderLayer.h>
#include <renderer/RenderLayerFactory.h>
#include <renderer/RenderStack.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/Planet.h>
#include <scene/component/Transform.h>

#include <gtest/gtest.h>

#include <string>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class LargeWorldCommandTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }

	auto run(const std::string& iName, const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		return m_registry.execute(iName, iArgs, m_scene, m_undo);
	}

	[[nodiscard]] auto partition() const -> const scene::WorldPartitionSettings& {
		return m_scene->getWorldPartitionSettings();
	}

	/// Registry under test.
	CommandRegistry m_registry;
	/// Scene changed by the commands.
	shared<scene::Scene> m_scene = mkShared<scene::Scene>();
	/// Undo history of the scene.
	SceneUndoManager m_undo;
};
OWL_DIAG_POP

}// namespace

TEST_F(LargeWorldCommandTest, CreatePlanetUndoesAndScalesItsRelief) {
	const auto result =
			run("entity.create_planet", {{"radius", 5000.0}, {"translation", math::vec3{0.f, -5000.f, 0.f}}});
	ASSERT_TRUE(result) << result.error().message;
	auto entity = m_scene->findEntityByUUID(result->entity);
	ASSERT_TRUE(entity.hasComponent<scene::component::Planet>());
	const auto& planet = entity.getComponent<scene::component::Planet>();
	EXPECT_DOUBLE_EQ(planet.settings.radius, 5000.0);
	EXPECT_FLOAT_EQ(planet.settings.heightScale, 150.f);
	EXPECT_FLOAT_EQ(entity.getComponent<scene::component::Transform>().transform.translation().y(), -5000.f);
	ASSERT_TRUE(run("edit.undo", {}));
	EXPECT_FALSE(m_scene->findEntityByUUID(result->entity));
	EXPECT_FALSE(run("entity.create_planet", {{"radius", 0.5}}));
}

TEST_F(LargeWorldCommandTest, ThePartitionSettingsAreEditedAndUndone) {
	ASSERT_TRUE(run("scene.set_world_partition", {{"enabled", true}, {"cell_size", 64.0}, {"load_radius", 600.0}}));
	EXPECT_TRUE(partition().enabled);
	EXPECT_DOUBLE_EQ(partition().cellSize, 64.0);
	EXPECT_DOUBLE_EQ(partition().unloadRadius, 600.0);
	EXPECT_FALSE(run("scene.set_world_partition", {{"cell_size", -1.0}}));
	EXPECT_FALSE(run("scene.set_world_partition", {{"enabled", true}}));
	ASSERT_TRUE(run("edit.undo", {}));
	EXPECT_EQ(partition(), scene::WorldPartitionSettings{});
}

TEST_F(LargeWorldCommandTest, CellsAreAddedMovedRetargetedAndRemovedInGroups) {
	for (const auto& cell: {math::vec3{0.f, 0.f, 0.f}, math::vec3{1.f, 0.f, 0.f}, math::vec3{0.f, 0.f, 1.f}})
		ASSERT_TRUE(run("scene.add_world_cell", {{"cell", cell}, {"scene", std::string{"scenes/cells/a.owl"}}}));
	ASSERT_EQ(partition().cells.size(), 3u);
	EXPECT_FALSE(run("scene.add_world_cell", {{"cell", math::vec3{1.f, 0.f, 0.f}}, {"scene", std::string{"b.owl"}}}));
	EXPECT_FALSE(run("scene.add_world_cell", {{"cell", math::vec3{5.f, 0.f, 0.f}}, {"scene", std::string{}}}));
	ASSERT_TRUE(
			run("scene.set_world_cell_scene", {{"cell", math::vec3{1.f, 0.f, 0.f}}, {"scene", std::string{"b.owl"}}}));
	EXPECT_EQ(partition().findCell({1, 0, 0})->scene, "b.owl");
	// Two cells moved together; one in the way of another refuses.
	ASSERT_TRUE(run("scene.move_world_cells",
					{{"cells", std::string{"0 0 0; 1,0,0"}}, {"offset", math::vec3{0.f, 2.f, 0.f}}}));
	EXPECT_NE(partition().findCell({0, 2, 0}), nullptr);
	EXPECT_EQ(partition().findCell({1, 2, 0})->scene, "b.owl");
	EXPECT_EQ(partition().findCell({0, 0, 0}), nullptr);
	EXPECT_FALSE(
			run("scene.move_world_cells", {{"cells", std::string{"0 2 0"}}, {"offset", math::vec3{0.f, -2.f, 1.f}}}));
	EXPECT_FALSE(run("scene.move_world_cells", {{"cells", std::string{"0 2"}}, {"offset", math::vec3{1.f, 0.f, 0.f}}}));
	EXPECT_FALSE(run("scene.remove_world_cells", {{"cells", std::string{"9 9 9"}}}));
	ASSERT_TRUE(run("scene.remove_world_cells", {{"cells", std::string{"0 2 0; 1 2 0"}}}));
	ASSERT_EQ(partition().cells.size(), 1u);
	ASSERT_TRUE(run("edit.undo", {}));
	EXPECT_EQ(partition().cells.size(), 3u);
	ASSERT_TRUE(run("edit.undo", {}));
	EXPECT_NE(partition().findCell({0, 0, 0}), nullptr);
}

TEST_F(LargeWorldCommandTest, TheFirstPlanetPutsAPlanetLayerFirstInTheProject) {
	if (!renderer::RenderLayerFactory::hasType("RendererPlanet"))
		renderer::RenderLayerFactory::registerType(
				"RendererPlanet",
				[](const std::string& /*iName*/) -> shared<renderer::RenderLayer> { return nullptr; });
	renderer::RendererStackConfig stack;
	CommandContext context{.scene = m_scene, .undo = &m_undo, .playing = false};
	context.projectStack = &stack;
	ASSERT_TRUE(m_registry.execute("entity.create_planet", {}, context));
	ASSERT_EQ(stack.entries.size(), 2u);
	EXPECT_EQ(stack.entries.front().typeKey, "RendererPlanet");
	ASSERT_TRUE(m_registry.execute("entity.create_planet", {}, context));
	EXPECT_EQ(stack.entries.size(), 2u);
}
