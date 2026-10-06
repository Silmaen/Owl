/**
 * @file PrefabCommands_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/PrefabCommands.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <scene/PrefabSerializer.h>
#include <scene/component/CircleRenderer.h>
#include <scene/component/PrefabLink.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <tuple>
#include <utility>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;
using namespace owl::nest::test;

namespace {

class PrefabCommandsTest : public NestTest {
protected:
	void SetUp() override {
		NestTest::SetUp();
		m_prefabPath = std::filesystem::temp_directory_path() /
					   std::format("owl_nest_prefab_{}.owlprefab", static_cast<uint64_t>(core::UUID{}));
		scene::Scene source;
		auto root = source.createEntity("Crate");
		auto lid = source.createEntity("Lid");
		lid.addComponent<scene::component::CircleRenderer>().thickness = 0.4f;
		source.setParent(lid, root);
		ASSERT_TRUE(scene::PrefabSerializer::serialize(root, source, m_prefabPath, "Crate"));
	}

	void TearDown() override {
		std::filesystem::remove(m_prefabPath);
		NestTest::TearDown();
	}

	auto instantiate() -> scene::Entity {
		return scene::PrefabSerializer::instantiate(
				m_prefabPath, shared<scene::Scene>(shared<scene::Scene>{}, &m_scene), "prefabs/crate.owlprefab");
	}

	std::filesystem::path m_prefabPath;
};

}// namespace

TEST_F(PrefabCommandsTest, InstantiateUndoRedo) {
	std::ignore = m_scene.createEntity("Existing");
	const auto initial = sceneState(m_scene);
	const auto root = instantiate();
	ASSERT_TRUE(root);
	const auto rootUuid = root.getUUID();
	const auto instantiated = sceneState(m_scene);
	ASSERT_EQ(m_scene.getEntityCount(), 3u);
	m_undo.push(mkUniq<InstantiatePrefabCommand>(root, m_scene, "Crate"));
	EXPECT_EQ(m_undo.undoDescription(), "Instantiate 'Crate'");

	m_undo.undo(m_scene);
	EXPECT_EQ(m_scene.getEntityCount(), 1u);
	EXPECT_EQ(sceneState(m_scene), initial);

	m_undo.redo(m_scene);
	EXPECT_EQ(m_scene.getEntityCount(), 3u);
	EXPECT_EQ(m_undo.lastSelectionHint(), rootUuid);
	EXPECT_EQ(sceneState(m_scene), instantiated);
	const auto restoredRoot = m_scene.findEntityByUUID(rootUuid);
	ASSERT_TRUE(restoredRoot);
	EXPECT_TRUE(restoredRoot.hasComponent<scene::component::PrefabLink>());
	EXPECT_EQ(childrenOf(restoredRoot).size(), 1u);
}

// The prefab merge itself (PrefabSerializer::applyToInstance) is PR-10 territory (C-02): the command is
// exercised here on a hand-made before / after pair.
TEST_F(PrefabCommandsTest, ApplyUndoRedo) {
	const auto root = instantiate();
	ASSERT_TRUE(root);
	const auto rootUuid = root.getUUID();
	const auto lidUuid = childrenOf(root).front();
	auto before = SubtreeSnapshot::capture(root, m_scene);
	const auto initial = sceneState(m_scene);
	m_scene.findEntityByUUID(lidUuid).getComponent<scene::component::CircleRenderer>().thickness = 0.9f;
	setLocalX(root, 4.f);
	auto after = SubtreeSnapshot::capture(root, m_scene);
	const auto applied = sceneState(m_scene);
	m_undo.push(mkUniq<ApplyPrefabCommand>(std::move(before), std::move(after), "Update from Prefab"));
	EXPECT_EQ(m_undo.undoDescription(), "Update from Prefab");

	m_undo.undo(m_scene);
	EXPECT_EQ(sceneState(m_scene), initial);
	EXPECT_EQ(parentOf(m_scene.findEntityByUUID(lidUuid)), rootUuid);
	EXPECT_EQ(m_undo.lastSelectionHint(), rootUuid);

	m_undo.redo(m_scene);
	EXPECT_EQ(sceneState(m_scene), applied);
	EXPECT_FLOAT_EQ(m_scene.findEntityByUUID(lidUuid).getComponent<scene::component::CircleRenderer>().thickness, 0.9f);
}
