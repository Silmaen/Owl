/**
 * @file MeshCommands_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "commands/ComponentCommands.h"
#include "commands/EntityCommands.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/MeshRenderer.h>
#include <scene/component/RendererTag.h>
#include <scene/component/Tag.h>
#include <scene/component/Transform.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;
using namespace owl::nest::test;

namespace {

auto cubePath() -> std::filesystem::path {
	return owl::test::getTestAssetsPath("render_tests") / "meshes" / "textured_cube.glb";
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class MeshCommandTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }

	auto run(const std::string& iName, const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		return m_registry.execute(iName, iArgs, m_scene, m_undo);
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

using MeshComponentCommandsTest = NestTest;

TEST_F(MeshComponentCommandsTest, AddMeshRendererUndoRedo) {
	auto entity = m_scene.createEntity("Crate");
	const auto uuid = entity.getUUID();
	auto before = EntitySnapshot::capture(entity);
	auto& mesh = entity.addComponent<scene::component::MeshRenderer>();
	mesh.setMeshPath("meshes/textured_cube.glb");
	mesh.color = {1.f, 0.5f, 0.25f, 0.8f};
	mesh.transparent = true;
	auto after = EntitySnapshot::capture(entity);
	m_undo.push(mkUniq<AddComponentCommand>(std::move(before), std::move(after), "Mesh Renderer"));
	EXPECT_EQ(m_undo.undoDescription(), "Add Mesh Renderer");

	m_undo.undo(m_scene);
	auto current = m_scene.findEntityByUUID(uuid);
	ASSERT_TRUE(current);
	EXPECT_FALSE(current.hasComponent<scene::component::MeshRenderer>());

	m_undo.redo(m_scene);
	current = m_scene.findEntityByUUID(uuid);
	ASSERT_TRUE(current);
	ASSERT_TRUE(current.hasComponent<scene::component::MeshRenderer>());
	const auto& restored = current.getComponent<scene::component::MeshRenderer>();
	EXPECT_EQ(restored.meshPath.generic_string(), "meshes/textured_cube.glb");
	EXPECT_FLOAT_EQ(restored.color.g(), 0.5f);
	EXPECT_TRUE(restored.transparent);
}

TEST_F(MeshComponentCommandsTest, RemoveMeshRendererUndoRestoresTheFields) {
	auto entity = m_scene.createEntity("Crate");
	entity.addComponent<scene::component::MeshRenderer>().setMeshPath("meshes/textured_cube.glb");
	const auto uuid = entity.getUUID();
	auto before = EntitySnapshot::capture(entity);
	entity.removeComponent<scene::component::MeshRenderer>();
	auto after = EntitySnapshot::capture(entity);
	m_undo.push(mkUniq<RemoveComponentCommand>(std::move(before), std::move(after), "Mesh Renderer"));

	m_undo.undo(m_scene);
	const auto current = m_scene.findEntityByUUID(uuid);
	ASSERT_TRUE(current);
	EXPECT_EQ(current.getComponent<scene::component::MeshRenderer>().meshPath.generic_string(),
			  "meshes/textured_cube.glb");
}

TEST_F(MeshCommandTest, CreateMeshPlacesATaggedEntityAndUndoes) {
	const auto result = run("mesh.create", {{"path", cubePath().string()},
											{"asset_path", std::string{"meshes/textured_cube.glb"}},
											{"translation", math::vec3{1.f, 0.f, -2.f}},
											{"layer", std::string{"mesh_world"}}});
	ASSERT_TRUE(result) << result.error().message;
	EXPECT_EQ(result->description, "Create 'textured_cube'");
	auto entity = m_scene->findEntityByUUID(result->entity);
	ASSERT_TRUE(entity);
	EXPECT_EQ(entity.getName(), "textured_cube");
	EXPECT_EQ(entity.getComponent<scene::component::Transform>().transform.translation(), (math::vec3{1.f, 0.f, -2.f}));
	ASSERT_TRUE(entity.hasComponent<scene::component::MeshRenderer>());
	EXPECT_EQ(entity.getComponent<scene::component::MeshRenderer>().meshPath.generic_string(),
			  "meshes/textured_cube.glb");
	ASSERT_TRUE(entity.hasComponent<scene::component::RendererTag>());
	EXPECT_EQ(entity.getComponent<scene::component::RendererTag>().rendererName, "mesh_world");

	ASSERT_TRUE(run("edit.undo", {}));
	EXPECT_FALSE(m_scene->findEntityByUUID(result->entity));
	ASSERT_TRUE(run("edit.redo", {}));
	entity = m_scene->findEntityByUUID(result->entity);
	ASSERT_TRUE(entity);
	EXPECT_TRUE(entity.hasComponent<scene::component::MeshRenderer>());
}

TEST_F(MeshCommandTest, CreateMeshUnderAParentKeepsTheWorldPosition) {
	auto parent = m_scene->createEntity("Group");
	parent.getComponent<scene::component::Transform>().transform.translation() = {10.f, 0.f, 0.f};
	const auto result = run("mesh.create", {{"path", cubePath().string()},
											{"name", std::string{"Crate"}},
											{"translation", math::vec3{12.f, 1.f, 0.f}},
											{"parent", std::string{"Group"}}});
	ASSERT_TRUE(result) << result.error().message;
	const auto entity = m_scene->findEntityByUUID(result->entity);
	ASSERT_TRUE(entity);
	EXPECT_EQ(entity.getName(), "Crate");
	EXPECT_FALSE(entity.hasComponent<scene::component::RendererTag>());
	EXPECT_EQ(entity.getComponent<scene::component::Hierarchy>().parentId, parent.getUUID());
	EXPECT_EQ(m_scene->getWorldTransform(entity).translation(), (math::vec3{12.f, 1.f, 0.f}));
	// Outside every asset folder, the component keeps the path as given.
	EXPECT_EQ(entity.getComponent<scene::component::MeshRenderer>().meshPath, cubePath());
}

TEST_F(MeshCommandTest, CreateMeshRefusesWhatIsNotAMesh) {
	const auto notMesh = run("mesh.create", {{"path", std::string{"scenes/level.owl"}}});
	ASSERT_FALSE(notMesh);
	EXPECT_EQ(notMesh.error().error, CommandError::InvalidArgument);
	const auto missing = run("mesh.create", {{"path", std::string{"/nowhere/crate.glb"}}});
	ASSERT_FALSE(missing);
	EXPECT_EQ(missing.error().error, CommandError::InvalidArgument);
	EXPECT_EQ(m_scene->registry.view<scene::component::Tag>().size(), 0u);
}
