/**
 * @file ComponentCommands_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/ComponentCommands.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <scene/component/CircleRenderer.h>

#include <utility>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;
using namespace owl::nest::test;

namespace {

// Build a ModifyEntityCommand around an edit applied to the entity, as the inspector and the gizmo do.
auto modifyX(const scene::Entity& iEntity, const float iX) -> uniq<ModifyEntityCommand> {
	auto command = mkUniq<ModifyEntityCommand>(iEntity.getUUID(), EntitySnapshot::capture(iEntity), "Move");
	setLocalX(iEntity, iX);
	command->captureAfter(iEntity);
	return command;
}

}// namespace

using ComponentCommandsTest = NestTest;

TEST_F(ComponentCommandsTest, AddComponentUndoRedo) {
	auto entity = m_scene.createEntity("E");
	const auto uuid = entity.getUUID();
	auto before = EntitySnapshot::capture(entity);
	entity.addComponent<scene::component::CircleRenderer>();
	auto after = EntitySnapshot::capture(entity);
	m_undo.push(mkUniq<AddComponentCommand>(std::move(before), std::move(after), "Circle Renderer"));
	EXPECT_EQ(m_undo.undoDescription(), "Add Circle Renderer");

	m_undo.undo(m_scene);
	auto current = m_scene.findEntityByUUID(uuid);
	ASSERT_TRUE(current);
	EXPECT_FALSE(current.hasComponent<scene::component::CircleRenderer>());
	EXPECT_EQ(m_undo.lastSelectionHint(), uuid);

	m_undo.redo(m_scene);
	current = m_scene.findEntityByUUID(uuid);
	ASSERT_TRUE(current);
	EXPECT_TRUE(current.hasComponent<scene::component::CircleRenderer>());
}

TEST_F(ComponentCommandsTest, RemoveComponentUndoRedo) {
	auto entity = m_scene.createEntity("E");
	entity.addComponent<scene::component::CircleRenderer>().thickness = 0.3f;
	const auto uuid = entity.getUUID();
	const auto withComponent = sceneState(m_scene);
	auto before = EntitySnapshot::capture(entity);
	entity.removeComponent<scene::component::CircleRenderer>();
	auto after = EntitySnapshot::capture(entity);
	const auto withoutComponent = sceneState(m_scene);
	m_undo.push(mkUniq<RemoveComponentCommand>(std::move(before), std::move(after), "Circle Renderer"));
	EXPECT_EQ(m_undo.undoDescription(), "Remove Circle Renderer");

	m_undo.undo(m_scene);
	EXPECT_EQ(sceneState(m_scene), withComponent);
	const auto current = m_scene.findEntityByUUID(uuid);
	ASSERT_TRUE(current);
	EXPECT_FLOAT_EQ(current.getComponent<scene::component::CircleRenderer>().thickness, 0.3f);

	m_undo.redo(m_scene);
	EXPECT_EQ(sceneState(m_scene), withoutComponent);
}

TEST_F(ComponentCommandsTest, ModifyUndoRedo) {
	auto entity = m_scene.createEntity("E");
	setLocalX(entity, 1.f);
	const auto uuid = entity.getUUID();
	const auto before = sceneState(m_scene);
	auto command = modifyX(entity, 8.f);
	const auto after = sceneState(m_scene);
	m_undo.push(std::move(command));
	EXPECT_EQ(m_undo.undoDescription(), "Move");

	m_undo.undo(m_scene);
	EXPECT_EQ(sceneState(m_scene), before);
	EXPECT_FLOAT_EQ(localX(m_scene.findEntityByUUID(uuid)), 1.f);

	m_undo.redo(m_scene);
	EXPECT_EQ(sceneState(m_scene), after);
	EXPECT_FLOAT_EQ(localX(m_scene.findEntityByUUID(uuid)), 8.f);
}

TEST_F(ComponentCommandsTest, ModifyMergesEditsOfSameEntity) {
	auto entity = m_scene.createEntity("E");
	const auto uuid = entity.getUUID();
	m_undo.push(modifyX(entity, 1.f));
	m_undo.push(modifyX(entity, 2.f));
	m_undo.push(modifyX(entity, 3.f));

	m_undo.undo(m_scene);
	EXPECT_FLOAT_EQ(localX(m_scene.findEntityByUUID(uuid)), 0.f);
	EXPECT_FALSE(m_undo.canUndo());
	m_undo.redo(m_scene);
	EXPECT_FLOAT_EQ(localX(m_scene.findEntityByUUID(uuid)), 3.f);
}

TEST_F(ComponentCommandsTest, ModifyDoesNotMergeAcrossEntities) {
	auto first = m_scene.createEntity("A");
	auto second = m_scene.createEntity("B");
	const auto firstUuid = first.getUUID();
	m_undo.push(modifyX(first, 1.f));
	m_undo.push(modifyX(second, 2.f));

	m_undo.undo(m_scene);
	EXPECT_FLOAT_EQ(localX(m_scene.findEntityByUUID(firstUuid)), 1.f);
	EXPECT_TRUE(m_undo.canUndo());
}

TEST_F(ComponentCommandsTest, ModifyParentUndoKeepsChildren) {
	auto parent = m_scene.createEntity("Parent");
	auto child = m_scene.createEntity("Child");
	setLocalX(parent, 5.f);
	m_scene.setParent(child, parent);
	const auto parentUuid = parent.getUUID();
	const auto childUuid = child.getUUID();
	const auto before = sceneState(m_scene);
	m_undo.push(modifyX(parent, 8.f));

	m_undo.undo(m_scene);
	const auto restoredParent = m_scene.findEntityByUUID(parentUuid);
	const auto restoredChild = m_scene.findEntityByUUID(childUuid);
	ASSERT_TRUE(restoredParent);
	ASSERT_TRUE(restoredChild);
	EXPECT_EQ(parentOf(restoredChild), parentUuid);
	EXPECT_EQ(childrenOf(restoredParent), std::vector{childUuid});
	EXPECT_FLOAT_EQ(worldX(m_scene, restoredChild), 0.f);
	EXPECT_EQ(sceneState(m_scene), before);

	m_undo.redo(m_scene);
	EXPECT_EQ(parentOf(m_scene.findEntityByUUID(childUuid)), parentUuid);
	EXPECT_FLOAT_EQ(worldX(m_scene, m_scene.findEntityByUUID(childUuid)), 3.f);
}

TEST_F(ComponentCommandsTest, AddComponentOnParentUndoKeepsChildren) {
	auto parent = m_scene.createEntity("Parent");
	auto child = m_scene.createEntity("Child");
	m_scene.setParent(child, parent);
	const auto before = sceneState(m_scene);
	auto snapshotBefore = EntitySnapshot::capture(parent);
	parent.addComponent<scene::component::CircleRenderer>();
	auto snapshotAfter = EntitySnapshot::capture(parent);
	const auto after = sceneState(m_scene);
	m_undo.push(mkUniq<AddComponentCommand>(std::move(snapshotBefore), std::move(snapshotAfter), "Circle"));

	m_undo.undo(m_scene);
	EXPECT_EQ(sceneState(m_scene), before);
	m_undo.redo(m_scene);
	EXPECT_EQ(sceneState(m_scene), after);
}

TEST_F(ComponentCommandsTest, ModifyUndoKeepsEntityHandle) {
	auto entity = m_scene.createEntity("E");
	const auto handle = static_cast<entt::entity>(entity);
	m_undo.push(modifyX(entity, 8.f));
	m_undo.undo(m_scene);
	EXPECT_TRUE(entity);
	EXPECT_EQ(static_cast<entt::entity>(m_scene.findEntityByUUID(entity.getUUID())), handle);
	EXPECT_FLOAT_EQ(localX(entity), 0.f);
}

TEST_F(ComponentCommandsTest, ModifyUndoKeepsDeepHierarchy) {
	auto root = m_scene.createEntity("Root");
	auto middle = m_scene.createEntity("Middle");
	auto leaf = m_scene.createEntity("Leaf");
	m_scene.setParent(middle, root);
	m_scene.setParent(leaf, middle);
	setLocalX(middle, 2.f);
	const auto before = sceneState(m_scene);
	m_undo.push(modifyX(middle, 5.f));
	m_undo.undo(m_scene);
	EXPECT_EQ(sceneState(m_scene), before);
	m_undo.redo(m_scene);
	EXPECT_FLOAT_EQ(worldX(m_scene, leaf), 5.f);
	EXPECT_EQ(parentOf(leaf), middle.getUUID());
}
