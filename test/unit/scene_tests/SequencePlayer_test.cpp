/**
 * @file SequencePlayer_test.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Log.h>
#include <core/Timestep.h>
#include <data/sequence/Sequence.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/SequencePlayback.h>
#include <scene/component/components.h>

#include <chrono>
#include <filesystem>
#include <string>

using namespace owl;
using namespace owl::scene;
using data::sequence::Easing;
using data::sequence::Sequence;

namespace {

class SequencePlayerTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_path = std::filesystem::temp_directory_path() / "player.owlseq";
		Sequence sequence;
		sequence.duration = 2.f;
		sequence.cameraTracks.push_back(
				{.name = "Dolly",
				 .keys = {{.time = 0.f, .position = {0.f, 0.f, 0.f}, .fov = 0.5f, .easing = Easing::Linear},
						  {.time = 2.f, .position = {4.f, 0.f, 0.f}, .fov = 1.f, .easing = Easing::Linear}},
				 .muted = false});
		ASSERT_TRUE(sequence.saveToFile(m_path));
	}

	void TearDown() override {
		std::filesystem::remove(m_path);
		core::Log::invalidate();
	}

	static void run(Scene& ioScene, const int iFrames, const int iMilliseconds = 100) {
		core::Timestep step;
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(iMilliseconds));
			ioScene.onUpdateRuntime(step, false);
		}
	}

	static auto addCamera(Scene& ioScene, const std::string& iName, const bool iPrimary) -> Entity {
		auto entity = ioScene.createEntity(iName);
		auto& camera = entity.addComponent<component::Camera>();
		camera.primary = iPrimary;
		camera.camera.setProjectionType(SceneCamera::ProjectionType::Perspective);
		return entity;
	}

	auto addPlayer(Scene& ioScene, const Entity& iCamera) const -> Entity {
		auto entity = ioScene.createEntity("Cutscene");
		auto& player = entity.addComponent<component::SequencePlayer>();
		player.sequence = m_path.generic_string();
		player.camera = iCamera.getUUID();
		return entity;
	}

	static auto xOf(const Entity& iEntity) -> float {
		return iEntity.getComponent<component::Transform>().transform.translation().x();
	}

	static auto isPrimary(const Entity& iEntity) -> bool { return iEntity.getComponent<component::Camera>().primary; }

	std::filesystem::path m_path;
};

}// namespace

TEST_F(SequencePlayerTest, RoundTripsThroughTheSceneFile) {
	auto scene = mkShared<Scene>();
	const auto camera = addCamera(*scene, "Cut", false);
	auto entity = addPlayer(*scene, camera);
	auto& player = entity.getComponent<component::SequencePlayer>();
	player.playOnStart = false;
	player.loop = true;
	player.speed = 0.5f;
	player.onFinished = "back_to_game";
	const auto text = SceneSerializer(scene).serializeToString();
	auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer({text.begin(), text.end()}));
	const auto read = loaded->findEntityByUUID(entity.getUUID());
	ASSERT_TRUE(read && read.hasComponent<component::SequencePlayer>());
	const auto& readPlayer = read.getComponent<component::SequencePlayer>();
	EXPECT_EQ(readPlayer.sequence, m_path.generic_string());
	EXPECT_EQ(readPlayer.camera, camera.getUUID());
	EXPECT_FALSE(readPlayer.playOnStart);
	EXPECT_TRUE(readPlayer.loop);
	EXPECT_FLOAT_EQ(readPlayer.speed, 0.5f);
	EXPECT_EQ(readPlayer.onFinished, "back_to_game");
}

TEST_F(SequencePlayerTest, DrivesItsCameraThenGivesThePrimaryBack) {
	auto scene = mkShared<Scene>();
	app::EngineContext context;
	scene->setEngineContext(&context);
	const auto main = addCamera(*scene, "Main", true);
	const auto cut = addCamera(*scene, "Cut", false);
	auto player = addPlayer(*scene, cut);
	player.getComponent<component::Transform>().transform.translation() = {0.f, 10.f, 0.f};
	scene->onStartRuntime();
	run(*scene, 1);
	EXPECT_TRUE(isPrimary(cut));
	EXPECT_FALSE(isPrimary(main));
	EXPECT_EQ(scene->getPrimaryCamera(), cut);
	EXPECT_NEAR(cut.getComponent<component::Transform>().transform.translation().y(), 10.f, 1e-4f);
	run(*scene, 10);
	EXPECT_NEAR(xOf(cut), 2.f, 1e-3f);
	EXPECT_NEAR(cut.getComponent<component::Camera>().camera.getPerspectiveVerticalFov(), 0.75f, 1e-3f);
	run(*scene, 10);
	EXPECT_NEAR(xOf(cut), 4.f, 1e-3f);
	EXPECT_FALSE(player.getComponent<component::SequencePlayer>().m_playing);
	EXPECT_TRUE(isPrimary(main));
	EXPECT_FALSE(isPrimary(cut));
	EXPECT_EQ(context.getSequences().getLoadCount(), 1u);
	scene->onEndRuntime();
	scene->setEngineContext(nullptr);
}

TEST_F(SequencePlayerTest, LoopsSeeksAndStops) {
	auto scene = mkShared<Scene>();
	const auto main = addCamera(*scene, "Main", true);
	const auto cut = addCamera(*scene, "Cut", false);
	auto player = addPlayer(*scene, cut);
	auto& component = player.getComponent<component::SequencePlayer>();
	component.playOnStart = false;
	component.loop = true;
	scene->onStartRuntime();
	run(*scene, 2);
	EXPECT_FALSE(component.m_playing);
	EXPECT_TRUE(isPrimary(main));
	ASSERT_TRUE(playSequence(player, 1.f));
	EXPECT_NEAR(xOf(cut), 2.f, 1e-3f);
	run(*scene, 15);
	EXPECT_TRUE(player.getComponent<component::SequencePlayer>().m_playing);
	EXPECT_NEAR(player.getComponent<component::SequencePlayer>().m_time, 0.5f, 1e-3f);
	EXPECT_TRUE(seekSequence(player, 1.5f));
	EXPECT_NEAR(xOf(cut), 3.f, 1e-3f);
	EXPECT_TRUE(stopSequence(player));
	EXPECT_FALSE(stopSequence(player));
	EXPECT_FALSE(seekSequence(player, 1.f));
	EXPECT_TRUE(isPrimary(main));
	scene->onEndRuntime();
}

TEST_F(SequencePlayerTest, ItsOwnCameraAndBadPlayers) {
	auto scene = mkShared<Scene>();
	auto self = addCamera(*scene, "Self", true);
	auto& player = self.addComponent<component::SequencePlayer>();
	player.sequence = m_path.generic_string();
	player.playOnStart = false;
	auto empty = scene->createEntity("Empty");
	empty.addComponent<component::SequencePlayer>().playOnStart = false;
	const auto plain = scene->createEntity("Plain");
	scene->onStartRuntime();
	ASSERT_TRUE(playSequence(self, 0.5f));
	EXPECT_NEAR(xOf(self), 1.f, 1e-3f);
	EXPECT_TRUE(isPrimary(self));
	EXPECT_FALSE(playSequence(empty));
	EXPECT_FALSE(playSequence(plain));
	EXPECT_FALSE(stopSequence(plain));
	const auto pose = getSequenceCameraPose(self, Sequence::makeDefault(), 0.f);
	ASSERT_TRUE(pose.has_value());
	EXPECT_EQ(pose->track, 0u);
	EXPECT_FALSE(getSequenceCameraPose(plain, Sequence{}, 0.f).has_value());
	scene->onEndRuntime();
}
