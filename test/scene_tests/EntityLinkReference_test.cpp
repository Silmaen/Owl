/**
 * @file EntityLinkReference_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/YamlNode.h>
#include <scene/Entity.h>
#include <scene/EntityLinkMigration.h>
#include <scene/PrefabSerializer.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/EntityLink.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/Tag.h>
#include <scene/component/Transform.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

class EntityLinkReferenceTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

auto findByName(const Scene& iScene, const std::string& iName) -> Entity {
	for (const auto& entity: iScene.getAllEntities())
		if (entity.getName() == iName)
			return entity;
	return {};
}

auto load(const std::string& iYaml) -> shared<Scene> {
	auto scene = mkShared<Scene>();
	const std::vector<uint8_t> bytes(iYaml.begin(), iYaml.end());
	if (!SceneSerializer(scene).deserializeFromBuffer(bytes, "test"))
		return nullptr;
	return scene;
}

auto step() -> core::Timestep {
	core::Timestep timestep;
	timestep.forceUpdate(std::chrono::milliseconds(16));
	return timestep;
}

auto xOf(const Entity& iEntity) -> float {
	return iEntity.getComponent<component::Transform>().transform.translation().x();
}

}// namespace

TEST_F(EntityLinkReferenceTest, VersionOneLinksGetTheUuidOfTheirTarget) {
	const auto scene =
			load("Scene: legacy\nEntities:\n"
				 "  - Entity: 11\n    Tag:\n      tag: Target\n"
				 "  - Entity: 12\n    Tag:\n      tag: Host\n    EntityLink:\n      linkedEntityName: Target\n"
				 "  - Entity: 13\n    Tag:\n      tag: Lost\n    EntityLink:\n      linkedEntityName: Nobody\n");
	ASSERT_NE(scene, nullptr);
	EXPECT_EQ(findByName(*scene, "Host").getComponent<component::EntityLink>().linkedEntityId, core::UUID{11});
	EXPECT_EQ(findByName(*scene, "Lost").getComponent<component::EntityLink>().linkedEntityId, core::UUID{0});
	const auto text = SceneSerializer(scene).serializeToString();
	EXPECT_NE(text.find("linkedEntityId: 11"), std::string::npos);
	EXPECT_NE(text.find(std::format("FormatVersion: {}", SceneSerializer::format().currentVersion())),
			  std::string::npos);
	EXPECT_EQ(SceneSerializer::format().currentVersion(), 2u);
}

TEST_F(EntityLinkReferenceTest, OnlyNameOnlyLinksNeedTheMigration) {
	const auto canRead = [](const std::string& iYaml, const core::DocumentFormat& iFormat) -> bool {
		const core::YamlDocument document{iYaml};
		return canReadWithoutMigration(document.getRoot(), iFormat);
	};
	const auto& format = SceneSerializer::format();
	EXPECT_TRUE(canRead("Scene: a\nFormatVersion: 2\n", format));
	EXPECT_FALSE(canRead("Scene: a\nFormatVersion: 1\n", format));
	EXPECT_FALSE(canRead("Scene: a\nFormatVersion: 3\n", format));
	EXPECT_FALSE(canRead("Scene: a\nFormatVersion: x\n", format));
	EXPECT_TRUE(canRead("Scene: a\nEntities:\n  - Entity: 1\n    Tag:\n      tag: A\n", format));
	EXPECT_TRUE(canRead("Scene: a\nEntities:\n  - Entity: 1\n    EntityLink:\n      linkedEntityId: 4\n"
						"      linkedEntityName: B\n",
						format));
	EXPECT_FALSE(canRead("Scene: a\nEntities:\n  - Entity: 1\n    EntityLink:\n      linkedEntityName: B\n", format));
	EXPECT_TRUE(canRead("Prefab: p\nEntities:\n  - 5\n", PrefabSerializer::format()));
}

TEST_F(EntityLinkReferenceTest, SavingBindsNameOnlyLinks) {
	const auto scene = mkShared<Scene>();
	const auto target = scene->createEntity("Target");
	auto host = scene->createEntity("Host");
	host.addComponent<component::EntityLink>().linkedEntityName = "Target";
	std::ignore = SceneSerializer(scene).serializeToString();
	EXPECT_EQ(host.getComponent<component::EntityLink>().linkedEntityId, target.getUUID());
}

TEST_F(EntityLinkReferenceTest, RenamedTargetKeepsItsLink) {
	auto scene = mkShared<Scene>();
	auto target = scene->createEntity("Target");
	target.getComponent<component::Transform>().transform.translation().x() = 5.f;
	auto host = scene->createEntity("Host");
	host.addComponent<component::EntityLink>().linkedEntityId = target.getUUID();
	scene->onStartRuntime();
	target.getComponent<component::Tag>().tag = "Renamed";
	target.getComponent<component::Transform>().transform.translation().x() = 7.f;
	scene->onUpdateRuntime(step(), false);
	EXPECT_FLOAT_EQ(xOf(host), 7.f);
	scene->onEndRuntime();
	auto& link = host.getComponent<component::EntityLink>();
	ASSERT_TRUE(scene->resolveEntityLink(link));
	EXPECT_EQ(link.linkedEntityName, "Renamed");
}

TEST_F(EntityLinkReferenceTest, MissingUuidFallsBackToTheName) {
	auto scene = mkShared<Scene>();
	auto target = scene->createEntity("Target");
	target.getComponent<component::Transform>().transform.translation().x() = 3.f;
	auto host = scene->createEntity("Host");
	auto& link = host.addComponent<component::EntityLink>();
	link.linkedEntityId = core::UUID{123456};
	link.linkedEntityName = "Target";
	ASSERT_TRUE(scene->resolveEntityLink(link));
	EXPECT_EQ(link.linkedEntityId, target.getUUID());
	scene->onStartRuntime();
	scene->onUpdateRuntime(step(), false);
	EXPECT_FLOAT_EQ(xOf(host), 3.f);
	scene->onEndRuntime();
}

TEST_F(EntityLinkReferenceTest, DuplicatedGroupLinksToItsOwnCopies) {
	Scene scene;
	auto group = scene.createEntity("Group");
	auto target = scene.createEntity("Target");
	auto host = scene.createEntity("Host");
	auto outside = scene.createEntity("Outside");
	scene.setParent(target, group);
	scene.setParent(host, group);
	host.addComponent<component::EntityLink>().linkedEntityName = "Target";
	auto watcher = scene.createEntity("Watcher");
	scene.setParent(watcher, group);
	watcher.addComponent<component::EntityLink>().linkedEntityId = outside.getUUID();

	const auto copy = scene.duplicateSubtree(group);
	Entity copiedTarget;
	Entity copiedHost;
	Entity copiedWatcher;
	for (const auto& child: scene.getChildren(copy)) {
		if (child.getName() == "Target")
			copiedTarget = child;
		else if (child.getName() == "Host")
			copiedHost = child;
		else if (child.getName() == "Watcher")
			copiedWatcher = child;
	}
	ASSERT_TRUE(copiedTarget && copiedHost && copiedWatcher);
	EXPECT_EQ(host.getComponent<component::EntityLink>().linkedEntityId, target.getUUID());
	EXPECT_EQ(copiedHost.getComponent<component::EntityLink>().linkedEntityId, copiedTarget.getUUID());
	EXPECT_EQ(copiedWatcher.getComponent<component::EntityLink>().linkedEntityId, outside.getUUID());
}

TEST_F(EntityLinkReferenceTest, EachPrefabInstanceLinksInsideItself) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_entity_link_prefab";
	std::filesystem::create_directories(dir);
	const auto path = dir / "pair.owlprefab";
	{
		Scene source;
		auto root = source.createEntity("Pair");
		auto target = source.createEntity("Target");
		auto host = source.createEntity("Host");
		source.setParent(target, root);
		source.setParent(host, root);
		host.addComponent<component::EntityLink>().linkedEntityId = target.getUUID();
		ASSERT_TRUE(PrefabSerializer::serialize(root, source, path, "Pair"));
	}
	const auto scene = mkShared<Scene>();
	for (int i = 0; i < 2; ++i) {
		const auto instance = PrefabSerializer::instantiate(path, scene);
		ASSERT_TRUE(instance);
		Entity target;
		Entity host;
		for (const auto& child: scene->getChildren(instance)) (child.getName() == "Target" ? target : host) = child;
		ASSERT_TRUE(target && host);
		EXPECT_EQ(host.getComponent<component::EntityLink>().linkedEntityId, target.getUUID());
	}
	std::filesystem::remove_all(dir);
}
