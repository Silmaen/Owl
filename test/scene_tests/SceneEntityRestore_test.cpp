/**
 * @file SceneEntityRestore_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <vector>

using namespace owl;
using namespace owl::scene;
using namespace owl::scene::component;

namespace {

auto localX(const Entity& iEntity) -> float { return iEntity.getComponent<Transform>().transform.translation().x(); }

void setLocalX(const Entity& iEntity, const float iX) {
	iEntity.getComponent<Transform>().transform.translation().x() = iX;
}

auto sceneRef(Scene& iScene) -> shared<Scene> { return shared<Scene>(shared<Scene>{}, &iScene); }

}// namespace

TEST(SceneDestroyEntity, ChildrenKeepWorldPosition) {
	Scene sc;
	auto grandParent = sc.createEntity("grandParent");
	auto parent = sc.createEntity("parent");
	auto child = sc.createEntity("child");
	setLocalX(grandParent, 1.f);
	setLocalX(parent, 5.f);
	sc.setParent(parent, grandParent);
	sc.setParent(child, parent);
	setLocalX(child, 2.f);
	ASSERT_FLOAT_EQ(sc.getWorldTransform(child).translation().x(), 7.f);

	sc.destroyEntity(parent);
	EXPECT_EQ(child.getComponent<Hierarchy>().parentId, grandParent.getUUID());
	EXPECT_FLOAT_EQ(sc.getWorldTransform(child).translation().x(), 7.f);
	EXPECT_FLOAT_EQ(localX(child), 6.f);
}

TEST(SceneDestroyEntity, RootChildrenKeepWorldPosition) {
	Scene sc;
	auto parent = sc.createEntity("parent");
	auto child = sc.createEntity("child");
	setLocalX(parent, 5.f);
	sc.setParent(child, parent);
	sc.destroyEntity(parent);
	EXPECT_EQ(child.getComponent<Hierarchy>().parentId, core::UUID{0});
	EXPECT_FLOAT_EQ(localX(child), 0.f);
}

TEST(SceneDestroyEntity, ChildrenTakeTheDeletedSlot) {
	Scene sc;
	auto root = sc.createEntity("root");
	auto first = sc.createEntity("first");
	auto middle = sc.createEntity("middle");
	auto last = sc.createEntity("last");
	auto childA = sc.createEntity("a");
	auto childB = sc.createEntity("b");
	sc.setParent(first, root);
	sc.setParent(middle, root);
	sc.setParent(last, root);
	sc.setParent(childA, middle);
	sc.setParent(childB, middle);

	sc.destroyEntity(middle);
	EXPECT_EQ(root.getComponent<Hierarchy>().childrenIds,
			  (std::vector{first.getUUID(), childA.getUUID(), childB.getUUID(), last.getUUID()}));
}

TEST(SceneEntityCount, CountsLiveEntities) {
	Scene sc;
	EXPECT_EQ(sc.getEntityCount(), 0u);
	auto a = sc.createEntity("a");
	sc.createEntity("b");
	EXPECT_EQ(sc.getEntityCount(), 2u);
	sc.destroyEntity(a);
	EXPECT_EQ(sc.getEntityCount(), 1u);
}

TEST(SceneSerializerApply, UpdatesInPlaceAndKeepsHierarchy) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto parent = sc.createEntity("parent");
	auto child = sc.createEntity("child");
	sc.setParent(child, parent);
	setLocalX(parent, 3.f);
	const auto yaml = SceneSerializer::serializeEntityToString(parent);
	const auto handle = static_cast<entt::entity>(parent);

	setLocalX(parent, 9.f);
	parent.getComponent<Tag>().tag = "renamed";
	parent.addComponent<CircleRenderer>();
	ASSERT_TRUE(SceneSerializer::applyEntityFromString(parent, yaml));

	EXPECT_TRUE(parent);
	EXPECT_EQ(static_cast<entt::entity>(sc.findEntityByUUID(parent.getUUID())), handle);
	EXPECT_FLOAT_EQ(localX(parent), 3.f);
	EXPECT_EQ(parent.getName(), "parent");
	EXPECT_FALSE(parent.hasComponent<CircleRenderer>());
	EXPECT_EQ(parent.getComponent<Hierarchy>().childrenIds, std::vector{child.getUUID()});
	EXPECT_EQ(child.getComponent<Hierarchy>().parentId, parent.getUUID());
	core::Log::invalidate();
}

TEST(SceneSerializerApply, AddsMissingComponentsAndKeepsUnchangedOnes) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto entity = sc.createEntity("e");
	entity.addComponent<CircleRenderer>().thickness = 0.5f;
	entity.getComponent<Visibility>().editorVisible = false;
	const auto yaml = SceneSerializer::serializeEntityToString(entity);
	entity.removeComponent<CircleRenderer>();

	ASSERT_TRUE(SceneSerializer::applyEntityFromString(entity, yaml));
	ASSERT_TRUE(entity.hasComponent<CircleRenderer>());
	EXPECT_FLOAT_EQ(entity.getComponent<CircleRenderer>().thickness, 0.5f);
	// Visibility did not change in the YAML: its editor-only flag survives.
	EXPECT_FALSE(entity.getComponent<Visibility>().editorVisible);
	core::Log::invalidate();
}

TEST(SceneSerializerApply, RejectsAnotherEntity) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto first = sc.createEntity("first");
	auto second = sc.createEntity("second");
	EXPECT_FALSE(SceneSerializer::applyEntityFromString(second, SceneSerializer::serializeEntityToString(first)));
	EXPECT_FALSE(SceneSerializer::applyEntityFromString(Entity{}, SceneSerializer::serializeEntityToString(first)));
	EXPECT_FALSE(SceneSerializer::applyEntityFromString(first, "not: [valid"));
	EXPECT_EQ(second.getName(), "second");
	core::Log::invalidate();
}

TEST(SceneSerializerDeserializeEntity, LinksParentAndAdoptsChildren) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto parent = sc.createEntity("parent");
	auto middle = sc.createEntity("middle");
	auto child = sc.createEntity("child");
	sc.setParent(middle, parent);
	sc.setParent(child, middle);
	const auto yaml = SceneSerializer::serializeEntityToString(middle);
	const auto middleUuid = middle.getUUID();
	sc.registry.destroy(static_cast<entt::entity>(middle));
	std::erase(parent.getComponent<Hierarchy>().childrenIds, middleUuid);

	ASSERT_TRUE(SceneSerializer::deserializeEntityFromString(sceneRef(sc), yaml));
	const auto restored = sc.findEntityByUUID(middleUuid);
	ASSERT_TRUE(restored);
	EXPECT_EQ(parent.getComponent<Hierarchy>().childrenIds, std::vector{middleUuid});
	EXPECT_EQ(restored.getComponent<Hierarchy>().childrenIds, std::vector{child.getUUID()});
	core::Log::invalidate();
}

TEST(SceneSerializerDeserializeEntity, OrphansWhenParentIsMissing) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto parent = sc.createEntity("parent");
	auto child = sc.createEntity("child");
	sc.setParent(child, parent);
	const auto yaml = SceneSerializer::serializeEntityToString(child);
	const auto childUuid = child.getUUID();
	sc.destroyEntityWithChildren(parent);

	ASSERT_TRUE(SceneSerializer::deserializeEntityFromString(sceneRef(sc), yaml));
	const auto restored = sc.findEntityByUUID(childUuid);
	ASSERT_TRUE(restored);
	EXPECT_EQ(restored.getComponent<Hierarchy>().parentId, core::UUID{0});
	core::Log::invalidate();
}
