/**
 * @file LightCommands_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/Light.h>
#include <scene/component/Tag.h>
#include <scene/component/Transform.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class LightCommandTest : public ::testing::Test {
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

TEST_F(LightCommandTest, EachTypeCreatesALightEntityAndUndoes) {
	using Type = scene::component::Light::Type;
	for (const auto& [kind, wanted]:
		 {std::pair<std::string, Type>{"Directional", Type::Directional},
		  std::pair<std::string, Type>{"Point", Type::Point}, std::pair<std::string, Type>{"Spot", Type::Spot}}) {
		const auto result = run("entity.create_light", {{"type", kind}});
		ASSERT_TRUE(result) << result.error().message;
		auto entity = m_scene->findEntityByUUID(result->entity);
		ASSERT_TRUE(entity) << kind;
		EXPECT_EQ(entity.getName(), kind + " Light");
		ASSERT_TRUE(entity.hasComponent<scene::component::Light>());
		EXPECT_EQ(entity.getComponent<scene::component::Light>().type, wanted);

		ASSERT_TRUE(run("edit.undo", {}));
		EXPECT_FALSE(m_scene->findEntityByUUID(result->entity)) << kind;
		ASSERT_TRUE(run("edit.redo", {}));
		entity = m_scene->findEntityByUUID(result->entity);
		ASSERT_TRUE(entity) << kind;
		EXPECT_EQ(entity.getComponent<scene::component::Light>().type, wanted);
	}
}

TEST_F(LightCommandTest, ADirectionalLightPointsDown) {
	const auto result = run("entity.create_light", {{"type", std::string{"Directional"}}});
	ASSERT_TRUE(result) << result.error().message;
	const auto entity = m_scene->findEntityByUUID(result->entity);
	ASSERT_TRUE(entity);
	// -Z of the world transform, the direction the light shines along, goes down.
	const auto world = m_scene->getWorldTransform(entity)();
	EXPECT_LT((world * math::vec4{0.f, 0.f, -1.f, 0.f}).y(), -0.5f);
}

TEST_F(LightCommandTest, NameAndParentAreHonoured) {
	auto parent = m_scene->createEntity("Group");
	const auto result = run("entity.create_light", {{"type", std::string{"Point"}},
													{"name", std::string{"Lamp"}},
													{"translation", math::vec3{1.f, 2.f, 3.f}},
													{"parent", std::string{"Group"}}});
	ASSERT_TRUE(result) << result.error().message;
	const auto entity = m_scene->findEntityByUUID(result->entity);
	ASSERT_TRUE(entity);
	EXPECT_EQ(entity.getName(), "Lamp");
	EXPECT_EQ(entity.getComponent<scene::component::Hierarchy>().parentId, parent.getUUID());
}

TEST_F(LightCommandTest, AnUnknownTypeIsRefused) {
	const auto result = run("entity.create_light", {{"type", std::string{"Laser"}}});
	ASSERT_FALSE(result);
	EXPECT_EQ(m_scene->registry.view<scene::component::Tag>().size(), 0u);
}
