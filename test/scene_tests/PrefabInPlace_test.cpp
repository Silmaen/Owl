/**
 * @file PrefabInPlace_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/PrefabSerializer.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::scene;
using namespace owl::scene::component;

namespace {

auto localX(const Entity& iEntity) -> float { return iEntity.getComponent<Transform>().transform.translation().x(); }

void setLocalX(const Entity& iEntity, const float iX) {
	iEntity.getComponent<Transform>().transform.translation().x() = iX;
}

auto parentOf(const Entity& iEntity) -> core::UUID { return iEntity.getComponent<Hierarchy>().parentId; }

auto childrenOf(const Entity& iEntity) -> std::vector<core::UUID> {
	return iEntity.getComponent<Hierarchy>().childrenIds;
}

class PrefabInPlace : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_prefabPath = std::filesystem::temp_directory_path() /
					   std::format("owl_prefab_in_place_{}.owlprefab", static_cast<uint64_t>(core::UUID{}));
		m_srcRoot = m_source.createEntity("Crate");
		m_srcRoot.addComponent<SpriteRenderer>().color = {1.f, 0.f, 0.f, 1.f};
		m_srcLid = m_source.createEntity("Lid");
		m_srcLid.addComponent<CircleRenderer>().thickness = 0.1f;
		m_source.setParent(m_srcLid, m_srcRoot);
		setLocalX(m_srcLid, 1.f);
		m_srcLock = m_source.createEntity("Lock");
		m_source.setParent(m_srcLock, m_srcLid);
		m_srcLabel = m_source.createEntity("Label");
		m_srcLabel.addComponent<SpriteRenderer>();
		m_source.setParent(m_srcLabel, m_srcRoot);
		save();
		m_root = PrefabSerializer::instantiate(m_prefabPath, sceneRef(), "prefabs/crate.owlprefab");
		ASSERT_TRUE(m_root);
		m_lid = instanceOf(m_srcLid);
		m_lock = instanceOf(m_srcLock);
		m_label = instanceOf(m_srcLabel);
		ASSERT_TRUE(m_lid && m_lock && m_label);
	}

	void TearDown() override {
		std::filesystem::remove(m_prefabPath);
		core::Log::invalidate();
	}

	void save() { ASSERT_TRUE(PrefabSerializer::serialize(m_srcRoot, m_source, m_prefabPath, "Crate")); }

	auto sceneRef() -> shared<Scene> { return shared<Scene>(shared<Scene>{}, &m_scene); }

	[[nodiscard]] auto link() const -> PrefabLink& { return m_root.getComponent<PrefabLink>(); }

	[[nodiscard]] auto instanceOf(const Entity& iCanonical) const -> Entity {
		for (const auto& [inst, canon]: link().uuidMapping) {
			if (canon == static_cast<uint64_t>(iCanonical.getUUID()))
				return m_scene.findEntityByUUID(core::UUID{inst});
		}
		return {};
	}

	template<typename Edit>
	void edit(const Entity& iEntity, Edit&& iEdit) {
		const auto before = SceneSerializer::serializeEntityToString(iEntity);
		std::forward<Edit>(iEdit)();
		PrefabSerializer::recordOverrides(iEntity, m_scene, before);
	}

	std::filesystem::path m_prefabPath;
	Scene m_source;
	Entity m_srcRoot;
	Entity m_srcLid;
	Entity m_srcLock;
	Entity m_srcLabel;
	Scene m_scene;
	Entity m_root;
	Entity m_lid;
	Entity m_lock;
	Entity m_label;
};

}// namespace

TEST_F(PrefabInPlace, UpdateKeepsHierarchyHandlesAndPlacement) {
	const auto rootChildren = childrenOf(m_root);
	ASSERT_EQ(rootChildren.size(), 2u);
	edit(m_root, [&]() -> void { setLocalX(m_root, 10.f); });
	m_srcRoot.getComponent<SpriteRenderer>().color = {0.f, 0.f, 1.f, 1.f};
	setLocalX(m_srcRoot, 3.f);
	save();

	ASSERT_TRUE(PrefabSerializer::applyToInstance(m_prefabPath, m_root, m_scene));
	EXPECT_EQ(m_scene.getEntityCount(), 4u);
	EXPECT_FLOAT_EQ(localX(m_root), 10.f);
	EXPECT_FLOAT_EQ(m_root.getComponent<SpriteRenderer>().color.z(), 1.f);
	EXPECT_TRUE(link().overriddenComponents.empty());
	EXPECT_EQ(parentOf(m_lid), m_root.getUUID());
	EXPECT_EQ(parentOf(m_lock), m_lid.getUUID());
	EXPECT_EQ(parentOf(m_label), m_root.getUUID());
	EXPECT_EQ(childrenOf(m_root), rootChildren);
	EXPECT_EQ(childrenOf(m_lid), std::vector{m_lock.getUUID()});
	EXPECT_FLOAT_EQ(m_scene.getWorldTransform(m_lid).translation().x(), 11.f);
}

TEST_F(PrefabInPlace, UpdatePreservesOverriddenAndRefreshesTheRest) {
	edit(m_lid, [&]() -> void { m_lid.getComponent<CircleRenderer>().thickness = 0.7f; });
	ASSERT_TRUE(link().isOverridden(static_cast<uint64_t>(m_srcLid.getUUID()), "CircleRenderer"));
	m_srcLid.getComponent<CircleRenderer>().thickness = 0.3f;
	m_srcLid.getComponent<CircleRenderer>().color = {0.f, 1.f, 0.f, 1.f};
	setLocalX(m_srcLid, 2.f);
	m_srcLock.addComponent<SpriteRenderer>();
	save();

	ASSERT_TRUE(PrefabSerializer::applyToInstance(m_prefabPath, m_root, m_scene));
	EXPECT_FLOAT_EQ(m_lid.getComponent<CircleRenderer>().thickness, 0.7f);
	EXPECT_FLOAT_EQ(m_lid.getComponent<CircleRenderer>().color.y(), 1.f) << "a component override keeps it whole";
	EXPECT_FLOAT_EQ(localX(m_lid), 2.f);
	EXPECT_TRUE(m_lock.hasComponent<SpriteRenderer>());
	EXPECT_EQ(link().overriddenComponents.size(), 1u);
}

TEST_F(PrefabInPlace, UpdateAddsNewEntitiesUnderTheirParent) {
	auto srcHinge = m_source.createEntity("Hinge");
	srcHinge.addComponent<CircleRenderer>().thickness = 0.5f;
	m_source.setParent(srcHinge, m_srcLid);
	save();

	ASSERT_TRUE(PrefabSerializer::applyToInstance(m_prefabPath, m_root, m_scene));
	EXPECT_EQ(m_scene.getEntityCount(), 5u);
	const auto hinge = instanceOf(srcHinge);
	ASSERT_TRUE(hinge);
	EXPECT_NE(hinge.getUUID(), srcHinge.getUUID());
	EXPECT_EQ(hinge.getName(), "Hinge");
	EXPECT_EQ(parentOf(hinge), m_lid.getUUID());
	EXPECT_EQ(childrenOf(m_lid), (std::vector{m_lock.getUUID(), hinge.getUUID()}));
	ASSERT_TRUE(hinge.hasComponent<CircleRenderer>());
	EXPECT_FLOAT_EQ(hinge.getComponent<CircleRenderer>().thickness, 0.5f);
	EXPECT_EQ(link().uuidMapping.size(), 5u);
}

TEST_F(PrefabInPlace, UpdateRemovesEntitiesGoneFromThePrefab) {
	const auto labelUuid = m_label.getUUID();
	auto expectedChildren = childrenOf(m_root);
	auto extra = m_scene.createEntity("Sticker");
	m_scene.setParent(extra, m_label);
	m_source.destroyEntity(m_srcLabel);
	save();

	ASSERT_TRUE(PrefabSerializer::applyToInstance(m_prefabPath, m_root, m_scene));
	EXPECT_FALSE(m_scene.findEntityByUUID(labelUuid));
	EXPECT_EQ(link().uuidMapping.size(), 3u);
	EXPECT_EQ(parentOf(extra), m_root.getUUID()) << "an entity added to the instance only is kept";
	std::ranges::replace(expectedChildren, labelUuid, extra.getUUID());
	EXPECT_EQ(childrenOf(m_root), expectedChildren);
}

TEST_F(PrefabInPlace, UpdateKeepsInstanceOnlyChildren) {
	auto extra = m_scene.createEntity("Sticker");
	m_scene.setParent(extra, m_lid);
	save();

	ASSERT_TRUE(PrefabSerializer::applyToInstance(m_prefabPath, m_root, m_scene));
	EXPECT_EQ(parentOf(extra), m_lid.getUUID());
	EXPECT_EQ(childrenOf(m_lid), (std::vector{m_lock.getUUID(), extra.getUUID()}));
}

TEST_F(PrefabInPlace, OverriddenRemovalAndAdditionSurviveUpdate) {
	edit(m_label, [&]() -> void { m_label.removeComponent<SpriteRenderer>(); });
	edit(m_lock, [&]() -> void { m_lock.addComponent<CircleRenderer>(); });
	ASSERT_TRUE(PrefabSerializer::applyToInstance(m_prefabPath, m_root, m_scene));
	EXPECT_FALSE(m_label.hasComponent<SpriteRenderer>());
	EXPECT_TRUE(m_lock.hasComponent<CircleRenderer>());
}

TEST_F(PrefabInPlace, UnmarkedLocalComponentFollowsThePrefab) {
	m_lock.addComponent<CircleRenderer>();
	ASSERT_TRUE(PrefabSerializer::applyToInstance(m_prefabPath, m_root, m_scene));
	EXPECT_FALSE(m_lock.hasComponent<CircleRenderer>());
}

TEST_F(PrefabInPlace, RevertRestoresEverythingButThePlacement) {
	edit(m_root, [&]() -> void { setLocalX(m_root, 10.f); });
	edit(m_lid, [&]() -> void { m_lid.getComponent<CircleRenderer>().thickness = 0.7f; });
	edit(m_lid, [&]() -> void { setLocalX(m_lid, 4.f); });
	edit(m_label, [&]() -> void { m_label.getComponent<Tag>().tag = "Renamed"; });
	ASSERT_EQ(link().overriddenComponents.size(), 3u);

	ASSERT_TRUE(PrefabSerializer::revertInstance(m_prefabPath, m_root, m_scene));
	EXPECT_TRUE(link().overriddenComponents.empty());
	EXPECT_FLOAT_EQ(localX(m_root), 10.f);
	EXPECT_FLOAT_EQ(m_lid.getComponent<CircleRenderer>().thickness, 0.1f);
	EXPECT_FLOAT_EQ(localX(m_lid), 1.f);
	EXPECT_EQ(m_label.getName(), "Label");
	EXPECT_EQ(parentOf(m_lock), m_lid.getUUID());
	EXPECT_EQ(m_scene.getEntityCount(), 4u);
}

TEST_F(PrefabInPlace, RevertFailureKeepsOverrides) {
	edit(m_lid, [&]() -> void { m_lid.getComponent<CircleRenderer>().thickness = 0.7f; });
	std::filesystem::remove(m_prefabPath);
	EXPECT_FALSE(PrefabSerializer::revertInstance(m_prefabPath, m_root, m_scene));
	EXPECT_EQ(link().overriddenComponents.size(), 1u);
}

TEST_F(PrefabInPlace, RecordOverridesIgnoresRootPlacementAndUnchangedComponents) {
	const auto before = SceneSerializer::serializeEntityToString(m_root);
	setLocalX(m_root, 5.f);
	EXPECT_FALSE(PrefabSerializer::recordOverrides(m_root, m_scene, before));
	EXPECT_FALSE(PrefabSerializer::recordOverrides(m_lid, m_scene, SceneSerializer::serializeEntityToString(m_lid)));
	EXPECT_TRUE(link().overriddenComponents.empty());

	edit(m_lid, [&]() -> void { setLocalX(m_lid, 4.f); });
	EXPECT_TRUE(link().isOverridden(static_cast<uint64_t>(m_srcLid.getUUID()), "Transform"));
	const auto lidYaml = SceneSerializer::serializeEntityToString(m_lid);
	EXPECT_FALSE(PrefabSerializer::recordOverrides(m_lid, m_scene, lidYaml));
}

TEST_F(PrefabInPlace, RecordOverridesSkipsEntitiesOutsideTheInstance) {
	auto extra = m_scene.createEntity("Sticker");
	m_scene.setParent(extra, m_lid);
	const auto before = SceneSerializer::serializeEntityToString(extra);
	extra.addComponent<CircleRenderer>();
	EXPECT_FALSE(PrefabSerializer::recordOverrides(extra, m_scene, before));
	EXPECT_FALSE(PrefabSerializer::recordOverrides(Entity{}, m_scene, before));
	EXPECT_TRUE(link().overriddenComponents.empty());
}

TEST_F(PrefabInPlace, FindInstanceRoot) {
	EXPECT_EQ(PrefabSerializer::findInstanceRoot(m_lock, m_scene), m_root);
	EXPECT_EQ(PrefabSerializer::findInstanceRoot(m_root, m_scene), m_root);
	auto extra = m_scene.createEntity("Sticker");
	m_scene.setParent(extra, m_lid);
	EXPECT_FALSE(PrefabSerializer::findInstanceRoot(extra, m_scene));
	EXPECT_FALSE(PrefabSerializer::findInstanceRoot(m_scene.createEntity("Loose"), m_scene));
}

TEST_F(PrefabInPlace, RevertComponent) {
	edit(m_lid, [&]() -> void { m_lid.getComponent<CircleRenderer>().thickness = 0.7f; });
	edit(m_lid, [&]() -> void { setLocalX(m_lid, 4.f); });
	edit(m_label, [&]() -> void { m_label.removeComponent<SpriteRenderer>(); });

	ASSERT_TRUE(PrefabSerializer::revertComponent(m_prefabPath, m_root, m_lid, "CircleRenderer"));
	EXPECT_FLOAT_EQ(m_lid.getComponent<CircleRenderer>().thickness, 0.1f);
	EXPECT_FLOAT_EQ(localX(m_lid), 4.f);
	EXPECT_FALSE(link().isOverridden(static_cast<uint64_t>(m_srcLid.getUUID()), "CircleRenderer"));
	EXPECT_TRUE(link().isOverridden(static_cast<uint64_t>(m_srcLid.getUUID()), "Transform"));

	ASSERT_TRUE(PrefabSerializer::revertComponent(m_prefabPath, m_root, m_label, "SpriteRenderer"));
	EXPECT_TRUE(m_label.hasComponent<SpriteRenderer>());
	EXPECT_EQ(parentOf(m_label), m_root.getUUID());

	EXPECT_FALSE(PrefabSerializer::revertComponent(m_prefabPath, m_root, m_root, "Transform"));
	EXPECT_FALSE(PrefabSerializer::revertComponent(m_prefabPath, m_root, m_lid, "Hierarchy"));
	auto extra = m_scene.createEntity("Sticker");
	EXPECT_FALSE(PrefabSerializer::revertComponent(m_prefabPath, m_root, extra, "Tag"));
	EXPECT_FALSE(PrefabSerializer::revertComponent(m_prefabPath, extra, m_lid, "Tag"));
}

TEST(PrefabSample, CoinPairInstancesUpdateInPlace) {
	core::Log::init(core::Log::Level::Off);
	const auto sample = owl::test::getRootPath() / "sample_project";
	auto scene = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(scene).deserialize(sample / "scenes" / "platformer_house.owl"));
	const auto count = scene->getEntityCount();
	for (const uint64_t uuid: {3000000000000000501ULL, 3000000000000000511ULL}) {
		auto root = scene->findEntityByUUID(core::UUID{uuid});
		ASSERT_TRUE(root) << uuid;
		ASSERT_TRUE(root.hasComponent<PrefabLink>());
		const auto placement = root.getComponent<Transform>().transform.translation();
		ASSERT_TRUE(PrefabSerializer::applyToInstance(sample / root.getComponent<PrefabLink>().prefabAssetPath, root,
													  *scene));
		EXPECT_EQ(root.getComponent<Transform>().transform.translation(), placement);
		EXPECT_EQ(childrenOf(root).size(), 2u);
	}
	EXPECT_EQ(scene->getEntityCount(), count);
	const auto tinted = scene->findEntityByUUID(core::UUID{3000000000000000513});
	ASSERT_TRUE(tinted);
	EXPECT_FLOAT_EQ(tinted.getComponent<AnimatedSpriteRenderer>().color.z(), 0.3f);
	core::Log::invalidate();
}

TEST_F(PrefabInPlace, PrefabLinkOverrideHelpers) {
	PrefabLink prefabLink;
	EXPECT_EQ(PrefabLink::overrideKey(42, "Tag"), "42:Tag");
	EXPECT_TRUE(prefabLink.setOverridden(42, "Tag"));
	EXPECT_FALSE(prefabLink.setOverridden(42, "Tag"));
	EXPECT_TRUE(prefabLink.isOverridden(42, "Tag"));
	EXPECT_FALSE(prefabLink.isOverridden(4, "Tag"));
	EXPECT_TRUE(prefabLink.clearOverride(42, "Tag"));
	EXPECT_FALSE(prefabLink.clearOverride(42, "Tag"));
	prefabLink.uuidMapping.push_back({.instanceUuid = 7, .canonicalUuid = 8});
	EXPECT_EQ(prefabLink.findCanonicalUuid(7), 8u);
	EXPECT_FALSE(prefabLink.findCanonicalUuid(8).has_value());
}
