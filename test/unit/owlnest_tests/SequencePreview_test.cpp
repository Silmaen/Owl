/**
 * @file SequencePreview_test.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "document/SequenceDocument.h"
#include "document/SequencePreview.h"

#include <core/Log.h>
#include <data/sequence/Sequence.h>
#include <math/Transform.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/SequencePlayer.h>
#include <scene/component/Transform.h>

#include <gtest/gtest.h>

#include <filesystem>

using namespace owl;
using namespace owl::nest;

namespace {

class SequencePreviewTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_sequence_preview_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_path = (m_root / "intro.owlseq").lexically_normal();
		ASSERT_TRUE(data::sequence::Sequence::makeDefault().saveToFile(m_path));
	}

	void TearDown() override {
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	std::filesystem::path m_root;
	std::filesystem::path m_path;
};

}// namespace

TEST_F(SequencePreviewTest, FindsThePlayerOfASequenceFile) {
	scene::Scene scene;
	auto other = scene.createEntity("Other");
	auto& otherPlayer = other.addComponent<scene::component::SequencePlayer>();
	otherPlayer.sequence = (m_root / "missing.owlseq").generic_string();
	EXPECT_FALSE(isPlayerOf(other, m_path));
	EXPECT_FALSE(findSequencePlayer(scene, m_path));

	auto player = scene.createEntity("Intro");
	player.addComponent<scene::component::SequencePlayer>().sequence = m_path.generic_string();
	EXPECT_TRUE(isPlayerOf(player, m_path));
	EXPECT_EQ(findSequencePlayer(scene, m_path), player);
	EXPECT_FALSE(isPlayerOf(scene::Entity{}, m_path));
	EXPECT_EQ(getSequenceDiskPath(m_path.generic_string()), m_path);
	EXPECT_FALSE(getSequenceDiskPath("").has_value());
}

TEST_F(SequencePreviewTest, KeysLiveInThePlayerOrItsParentSpace) {
	scene::Scene scene;
	const auto parent = scene.createEntity("Rig");
	parent.getComponent<scene::component::Transform>().transform.translation() = {5.f, 0.f, 0.f};
	auto player = scene.createEntity("Intro");
	player.getComponent<scene::component::Transform>().transform.translation() = {0.f, 2.f, 0.f};
	auto& component = player.addComponent<scene::component::SequencePlayer>();
	scene.setParent(player, parent);

	const auto world = [](const math::mat4& iMatrix) -> math::vec3 { return math::Transform{iMatrix}.translation(); };
	component.camera = core::UUID{0};// the player drives itself: keys in its parent's space
	EXPECT_EQ(world(getSequenceAnchor(scene, player)), (math::vec3{5.f, 0.f, 0.f}));
	component.camera = parent.getUUID();// another camera: keys in the player's space
	EXPECT_EQ(world(getSequenceAnchor(scene, player)), (math::vec3{0.f, 2.f, 0.f}));// setParent keeps the world pose
}

TEST_F(SequencePreviewTest, DocumentPreviewStateWithoutEditor) {
	SequenceDocument document;
	ASSERT_TRUE(document.loadFromFile(m_path));
	EXPECT_EQ(document.getPreviewDocument(), nullptr);
	EXPECT_FALSE(document.getPreviewPlayer());
	EXPECT_FALSE(document.consumePreviewChanged());
	document.setPreview(core::UUID{7}, core::UUID{8});
	EXPECT_TRUE(document.consumePreviewChanged());
	EXPECT_FALSE(document.consumePreviewChanged());
	document.setPreview(core::UUID{7}, core::UUID{8});
	EXPECT_FALSE(document.consumePreviewChanged());
	document.setLookingThrough(true);
	EXPECT_TRUE(document.isLookingThrough());
	document.setPreview(core::UUID{0}, core::UUID{0});
	EXPECT_FALSE(document.isLookingThrough());// no player left to look through
	EXPECT_TRUE(document.consumePreviewChanged());
	EXPECT_FALSE(document.setKeyAtPlayhead({0.f, 1.f, 2.f}, {0.f, 0.f, 0.f}));// no editor to run the command
	EXPECT_FALSE(document.overridesHierarchyPanel());
	EXPECT_TRUE(document.overridesGlobalPanels());
}
