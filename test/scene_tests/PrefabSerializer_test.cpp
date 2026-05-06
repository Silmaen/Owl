/**
 * @file PrefabSerializer_test.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/PrefabSerializer.h>
#include <scene/Scene.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/PrefabLink.h>
#include <scene/component/SpriteRenderer.h>
#include <scene/component/Transform.h>
#include <scene/component/Visibility.h>

using namespace owl;
using namespace owl::scene;

// ---------------------------------------------------------------------------
// Basic serialize / instantiate round-trip
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, SerializeAndInstantiate) {
	core::Log::init(core::Log::Level::Off);

	// Build a simple prefab scene with one entity.
	auto srcScene = mkShared<Scene>();
	auto root = srcScene->createEntity("PrefabRoot");
	root.getComponent<component::Transform>().transform.translation() = {1.f, 2.f, 3.f};
	root.addComponent<component::SpriteRenderer>().color = {0.5f, 0.5f, 0.5f, 1.f};

	const auto prefabFile = std::filesystem::temp_directory_path() / "test_prefab.owlprefab";
	PrefabSerializer::serialize(root, *srcScene, prefabFile, "TestPrefab");
	ASSERT_TRUE(exists(prefabFile));

	// Instantiate into a fresh scene.
	auto dstScene = mkShared<Scene>();
	auto instance = PrefabSerializer::instantiate(prefabFile, dstScene, "prefabs/test_prefab.owlprefab");
	ASSERT_TRUE(static_cast<bool>(instance));

	// Instance should have PrefabLink.
	EXPECT_TRUE(instance.hasComponent<component::PrefabLink>());
	const auto& link = instance.getComponent<component::PrefabLink>();
	EXPECT_EQ(link.prefabAssetPath, "prefabs/test_prefab.owlprefab");
	EXPECT_EQ(link.uuidMapping.size(), 1u);

	// Transform should match source.
	EXPECT_NEAR(instance.getComponent<component::Transform>().transform.translation().x(), 1.f, 0.01f);
	EXPECT_NEAR(instance.getComponent<component::Transform>().transform.translation().y(), 2.f, 0.01f);

	// SpriteRenderer should be copied.
	ASSERT_TRUE(instance.hasComponent<component::SpriteRenderer>());
	EXPECT_NEAR(instance.getComponent<component::SpriteRenderer>().color.x(), 0.5f, 0.01f);

	std::filesystem::remove(prefabFile);
	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// serializeToString
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, SerializeToString) {
	core::Log::init(core::Log::Level::Off);

	auto srcScene = mkShared<Scene>();
	auto root = srcScene->createEntity("StringPrefab");
	root.addComponent<component::SpriteRenderer>();

	const auto yamlStr = PrefabSerializer::serializeToString(root, *srcScene, "StrTest");
	EXPECT_FALSE(yamlStr.empty());
	EXPECT_NE(yamlStr.find("Prefab:"), std::string::npos);
	EXPECT_NE(yamlStr.find("StrTest"), std::string::npos);
	EXPECT_NE(yamlStr.find("Entities:"), std::string::npos);

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// readInfo
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, ReadInfo) {
	core::Log::init(core::Log::Level::Off);

	auto srcScene = mkShared<Scene>();
	auto root = srcScene->createEntity("InfoRoot");
	auto child = srcScene->createEntity("InfoChild");
	srcScene->setParent(child, root);

	const auto prefabFile = std::filesystem::temp_directory_path() / "test_info.owlprefab";
	PrefabSerializer::serialize(root, *srcScene, prefabFile, "InfoPrefab");
	ASSERT_TRUE(exists(prefabFile));

	const auto info = PrefabSerializer::readInfo(prefabFile);
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->name, "InfoPrefab");
	EXPECT_EQ(info->version, 1u);
	EXPECT_EQ(info->entityCount, 2u);

	std::filesystem::remove(prefabFile);
	core::Log::invalidate();
}

TEST(PrefabSerializer, ReadInfoNonexistentFile) {
	core::Log::init(core::Log::Level::Off);

	const auto info = PrefabSerializer::readInfo("/tmp/nonexistent_prefab_9999.owlprefab");
	EXPECT_FALSE(info.has_value());

	core::Log::invalidate();
}

TEST(PrefabSerializer, ReadInfoBadFile) {
	core::Log::init(core::Log::Level::Off);

	const auto badFile = std::filesystem::temp_directory_path() / "bad_prefab.owlprefab";
	{
		std::ofstream f(badFile);
		f << "NotAPrefab: true\n";
	}

	const auto info = PrefabSerializer::readInfo(badFile);
	EXPECT_FALSE(info.has_value());

	std::filesystem::remove(badFile);
	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// Instantiate with subtree (parent + child)
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, InstantiateSubtree) {
	core::Log::init(core::Log::Level::Off);

	auto srcScene = mkShared<Scene>();
	auto root = srcScene->createEntity("SubRoot");
	root.getComponent<component::Transform>().transform.translation() = {10.f, 0.f, 0.f};
	auto child = srcScene->createEntity("SubChild");
	child.getComponent<component::Transform>().transform.translation() = {0.f, 5.f, 0.f};
	srcScene->setParent(child, root);

	const auto prefabFile = std::filesystem::temp_directory_path() / "test_subtree.owlprefab";
	PrefabSerializer::serialize(root, *srcScene, prefabFile, "SubtreePrefab");

	auto dstScene = mkShared<Scene>();
	auto instanceRoot = PrefabSerializer::instantiate(prefabFile, dstScene, "sub.owlprefab");
	ASSERT_TRUE(static_cast<bool>(instanceRoot));

	// Should have 2 entities total.
	const auto all = dstScene->getAllEntities();
	EXPECT_EQ(all.size(), 2u);

	// UUID mapping should have 2 entries.
	ASSERT_TRUE(instanceRoot.hasComponent<component::PrefabLink>());
	EXPECT_EQ(instanceRoot.getComponent<component::PrefabLink>().uuidMapping.size(), 2u);

	// The root should have one child.
	const auto children = dstScene->getChildren(instanceRoot);
	EXPECT_EQ(children.size(), 1u);

	std::filesystem::remove(prefabFile);
	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// Instantiate with empty asset relative path (falls back to filename)
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, InstantiateDefaultAssetPath) {
	core::Log::init(core::Log::Level::Off);

	auto srcScene = mkShared<Scene>();
	auto root = srcScene->createEntity("DefPath");
	const auto prefabFile = std::filesystem::temp_directory_path() / "default_path.owlprefab";
	PrefabSerializer::serialize(root, *srcScene, prefabFile, "DefPathPrefab");

	auto dstScene = mkShared<Scene>();
	auto instance = PrefabSerializer::instantiate(prefabFile, dstScene);
	ASSERT_TRUE(static_cast<bool>(instance));
	ASSERT_TRUE(instance.hasComponent<component::PrefabLink>());
	EXPECT_EQ(instance.getComponent<component::PrefabLink>().prefabAssetPath, "default_path.owlprefab");

	std::filesystem::remove(prefabFile);
	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// Instantiate from non-existent file
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, InstantiateNonexistentFile) {
	core::Log::init(core::Log::Level::Off);

	auto dstScene = mkShared<Scene>();
	auto instance = PrefabSerializer::instantiate("/tmp/does_not_exist_9999.owlprefab", dstScene);
	EXPECT_FALSE(static_cast<bool>(instance));

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// Instantiate from a file that is not a prefab
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, InstantiateBadFile) {
	core::Log::init(core::Log::Level::Off);

	const auto badFile = std::filesystem::temp_directory_path() / "not_prefab.owlprefab";
	{
		std::ofstream f(badFile);
		f << "Scene: untitled\n";
	}

	auto dstScene = mkShared<Scene>();
	auto instance = PrefabSerializer::instantiate(badFile, dstScene);
	EXPECT_FALSE(static_cast<bool>(instance));

	std::filesystem::remove(badFile);
	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// Instantiate from a prefab file with no entities
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, InstantiateNoEntities) {
	core::Log::init(core::Log::Level::Off);

	const auto noEntFile = std::filesystem::temp_directory_path() / "no_entities.owlprefab";
	{
		std::ofstream f(noEntFile);
		f << "Prefab: Empty\nVersion: 1\n";
	}

	auto dstScene = mkShared<Scene>();
	auto instance = PrefabSerializer::instantiate(noEntFile, dstScene);
	EXPECT_FALSE(static_cast<bool>(instance));

	std::filesystem::remove(noEntFile);
	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// applyToInstance — non-overridden components are refreshed from prefab
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, ApplyToInstanceRefreshesNonOverridden) {
	core::Log::init(core::Log::Level::Off);

	// 1. Create and save original prefab.
	auto srcScene = mkShared<Scene>();
	auto root = srcScene->createEntity("ApplyRoot");
	root.getComponent<component::Transform>().transform.translation() = {1.f, 0.f, 0.f};
	root.addComponent<component::SpriteRenderer>().color = {1.f, 0.f, 0.f, 1.f};

	const auto prefabFile = std::filesystem::temp_directory_path() / "apply_test.owlprefab";
	PrefabSerializer::serialize(root, *srcScene, prefabFile, "ApplyPrefab");

	// 2. Instantiate.
	auto dstScene = mkShared<Scene>();
	auto instance = PrefabSerializer::instantiate(prefabFile, dstScene, "apply_test.owlprefab");
	ASSERT_TRUE(static_cast<bool>(instance));

	// 3. Modify the instance's SpriteRenderer (simulate user change).
	instance.getComponent<component::SpriteRenderer>().color = {0.f, 1.f, 0.f, 1.f};

	// 4. Now update the prefab source: change transform and SpriteRenderer colour.
	root.getComponent<component::Transform>().transform.translation() = {99.f, 0.f, 0.f};
	root.getComponent<component::SpriteRenderer>().color = {0.f, 0.f, 1.f, 1.f};
	PrefabSerializer::serialize(root, *srcScene, prefabFile, "ApplyPrefab");

	// 5. Apply — no overrides, so everything should update to the new prefab.
	EXPECT_TRUE(PrefabSerializer::applyToInstance(prefabFile, instance, *dstScene));

	// Find the updated root entity (it was destroyed and recreated).
	const auto allEntities = dstScene->getAllEntities();
	ASSERT_EQ(allEntities.size(), 1u);
	const auto& updatedEntity = allEntities[0];

	// Transform should be updated from the prefab.
	EXPECT_NEAR(updatedEntity.getComponent<component::Transform>().transform.translation().x(), 99.f, 0.01f);
	// SpriteRenderer should be updated (not overridden).
	EXPECT_NEAR(updatedEntity.getComponent<component::SpriteRenderer>().color.z(), 1.f, 0.01f);

	std::filesystem::remove(prefabFile);
	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// applyToInstance — overridden components are preserved
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, ApplyToInstancePreservesOverrides) {
	core::Log::init(core::Log::Level::Off);

	// 1. Create and save prefab.
	auto srcScene = mkShared<Scene>();
	auto root = srcScene->createEntity("OverrideRoot");
	root.getComponent<component::Transform>().transform.translation() = {1.f, 0.f, 0.f};
	root.addComponent<component::SpriteRenderer>().color = {1.f, 0.f, 0.f, 1.f};

	const auto prefabFile = std::filesystem::temp_directory_path() / "override_test.owlprefab";
	PrefabSerializer::serialize(root, *srcScene, prefabFile, "OverridePrefab");

	// 2. Instantiate.
	auto dstScene = mkShared<Scene>();
	auto instance = PrefabSerializer::instantiate(prefabFile, dstScene, "override_test.owlprefab");
	ASSERT_TRUE(static_cast<bool>(instance));

	// 3. Modify instance SpriteRenderer and mark it as overridden.
	instance.getComponent<component::SpriteRenderer>().color = {0.f, 1.f, 0.f, 1.f};
	auto& link = instance.getComponent<component::PrefabLink>();
	const auto canonicalUuid = link.uuidMapping[0].canonicalUuid;
	link.overriddenComponents.push_back(std::format("{}:SpriteRenderer", canonicalUuid));

	// 4. Update the prefab (change both transform and sprite colour).
	root.getComponent<component::Transform>().transform.translation() = {50.f, 0.f, 0.f};
	root.getComponent<component::SpriteRenderer>().color = {0.f, 0.f, 1.f, 1.f};
	PrefabSerializer::serialize(root, *srcScene, prefabFile, "OverridePrefab");

	// 5. Apply — SpriteRenderer is overridden, so it should stay green.
	EXPECT_TRUE(PrefabSerializer::applyToInstance(prefabFile, instance, *dstScene));

	const auto allEntities = dstScene->getAllEntities();
	ASSERT_EQ(allEntities.size(), 1u);
	const auto& updated = allEntities[0];

	// Transform should be updated from prefab (not overridden).
	EXPECT_NEAR(updated.getComponent<component::Transform>().transform.translation().x(), 50.f, 0.01f);
	// SpriteRenderer should be preserved (overridden) — green colour.
	EXPECT_NEAR(updated.getComponent<component::SpriteRenderer>().color.y(), 1.f, 0.01f);
	EXPECT_NEAR(updated.getComponent<component::SpriteRenderer>().color.z(), 0.f, 0.01f);

	std::filesystem::remove(prefabFile);
	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// applyToInstance — entity without PrefabLink returns false
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, ApplyToInstanceNoPrefabLink) {
	core::Log::init(core::Log::Level::Off);

	auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("NoPrefabLink");
	// Entity has no PrefabLink component.

	EXPECT_FALSE(PrefabSerializer::applyToInstance("/tmp/whatever.owlprefab", entity, *scene));

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// applyToInstance — invalid entity returns false
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, ApplyToInstanceInvalidEntity) {
	core::Log::init(core::Log::Level::Off);

	auto scene = mkShared<Scene>();
	Entity invalid;
	EXPECT_FALSE(PrefabSerializer::applyToInstance("/tmp/whatever.owlprefab", invalid, *scene));

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// applyToInstance — missing prefab file returns false
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, ApplyToInstanceMissingFile) {
	core::Log::init(core::Log::Level::Off);

	// Create a minimal prefab to get a valid instance with PrefabLink.
	auto srcScene = mkShared<Scene>();
	auto root = srcScene->createEntity("MissingFileRoot");
	const auto prefabFile = std::filesystem::temp_directory_path() / "apply_missing.owlprefab";
	PrefabSerializer::serialize(root, *srcScene, prefabFile, "MissingTest");

	auto dstScene = mkShared<Scene>();
	auto instance = PrefabSerializer::instantiate(prefabFile, dstScene, "apply_missing.owlprefab");
	ASSERT_TRUE(static_cast<bool>(instance));
	ASSERT_TRUE(instance.hasComponent<component::PrefabLink>());

	// Delete the prefab file.
	std::filesystem::remove(prefabFile);

	// Apply should fail gracefully.
	EXPECT_FALSE(PrefabSerializer::applyToInstance(prefabFile, instance, *dstScene));

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// revertInstance — clears overrides and refreshes all from prefab
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, RevertInstanceClearsOverrides) {
	core::Log::init(core::Log::Level::Off);

	// 1. Create and save prefab with a specific colour.
	auto srcScene = mkShared<Scene>();
	auto root = srcScene->createEntity("RevertRoot");
	root.getComponent<component::Transform>().transform.translation() = {5.f, 5.f, 0.f};
	root.addComponent<component::SpriteRenderer>().color = {1.f, 0.f, 0.f, 1.f};

	const auto prefabFile = std::filesystem::temp_directory_path() / "revert_test.owlprefab";
	PrefabSerializer::serialize(root, *srcScene, prefabFile, "RevertPrefab");

	// 2. Instantiate.
	auto dstScene = mkShared<Scene>();
	auto instance = PrefabSerializer::instantiate(prefabFile, dstScene, "revert_test.owlprefab");
	ASSERT_TRUE(static_cast<bool>(instance));

	// 3. Modify instance and mark SpriteRenderer as overridden.
	instance.getComponent<component::SpriteRenderer>().color = {0.f, 1.f, 0.f, 1.f};
	instance.getComponent<component::Transform>().transform.translation() = {99.f, 99.f, 0.f};
	{
		auto& link = instance.getComponent<component::PrefabLink>();
		const auto canonicalUuid = link.uuidMapping[0].canonicalUuid;
		link.overriddenComponents.push_back(std::format("{}:SpriteRenderer", canonicalUuid));
		link.overriddenComponents.push_back(std::format("{}:Transform", canonicalUuid));
	}

	// 4. Revert — should clear overrides and restore everything from prefab.
	EXPECT_TRUE(PrefabSerializer::revertInstance(prefabFile, instance, *dstScene));

	const auto allEntities = dstScene->getAllEntities();
	ASSERT_EQ(allEntities.size(), 1u);
	const auto& reverted = allEntities[0];

	// Transform should be back to prefab values.
	EXPECT_NEAR(reverted.getComponent<component::Transform>().transform.translation().x(), 5.f, 0.01f);
	EXPECT_NEAR(reverted.getComponent<component::Transform>().transform.translation().y(), 5.f, 0.01f);
	// SpriteRenderer should be back to prefab values (red).
	EXPECT_NEAR(reverted.getComponent<component::SpriteRenderer>().color.x(), 1.f, 0.01f);
	EXPECT_NEAR(reverted.getComponent<component::SpriteRenderer>().color.y(), 0.f, 0.01f);

	std::filesystem::remove(prefabFile);
	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// revertInstance — entity without PrefabLink returns false
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, RevertInstanceNoPrefabLink) {
	core::Log::init(core::Log::Level::Off);

	auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("NoLink");
	EXPECT_FALSE(PrefabSerializer::revertInstance("/tmp/whatever.owlprefab", entity, *scene));

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// revertInstance — invalid entity returns false
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, RevertInstanceInvalidEntity) {
	core::Log::init(core::Log::Level::Off);

	auto scene = mkShared<Scene>();
	Entity invalid;
	EXPECT_FALSE(PrefabSerializer::revertInstance("/tmp/whatever.owlprefab", invalid, *scene));

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// revertInstance — missing prefab file returns false
// ---------------------------------------------------------------------------

TEST(PrefabSerializer, RevertInstanceMissingFile) {
	core::Log::init(core::Log::Level::Off);

	auto srcScene = mkShared<Scene>();
	auto root = srcScene->createEntity("RevertMissRoot");
	const auto prefabFile = std::filesystem::temp_directory_path() / "revert_missing.owlprefab";
	PrefabSerializer::serialize(root, *srcScene, prefabFile, "RevertMissTest");

	auto dstScene = mkShared<Scene>();
	auto instance = PrefabSerializer::instantiate(prefabFile, dstScene, "revert_missing.owlprefab");
	ASSERT_TRUE(static_cast<bool>(instance));
	ASSERT_TRUE(instance.hasComponent<component::PrefabLink>());

	// Delete the prefab file.
	std::filesystem::remove(prefabFile);

	// Revert should fail gracefully (the file is gone).
	EXPECT_FALSE(PrefabSerializer::revertInstance(prefabFile, instance, *dstScene));

	core::Log::invalidate();
}
