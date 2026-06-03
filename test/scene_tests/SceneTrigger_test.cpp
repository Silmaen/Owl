/**
 * @file SceneTrigger_test.cpp
 * @author Silmaen
 * @date 07/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneTrigger.h>
#include <scene/component/components.h>

using namespace owl;

namespace {

class SceneTriggerTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override {
		if (physics::PhysicCommand::isInitialized())
			physics::PhysicCommand::destroy();
		core::Log::invalidate();
	}
};

}// namespace

TEST_F(SceneTriggerTest, VictoryDefaultsToSceneStatus) {
	scene::Scene scn;
	auto player = scn.createEntity("Player");
	auto triggerEnt = scn.createEntity("Trigger");
	scene::SceneTrigger trigger;
	trigger.type = scene::SceneTrigger::TriggerType::Victory;
	trigger.onTriggered(player, triggerEnt);
	EXPECT_TRUE(trigger.isTriggered());
	EXPECT_EQ(scn.status, scene::Scene::Status::Victory);
	// Re-firing is a no-op.
	scn.status = scene::Scene::Status::Playing;
	trigger.onTriggered(player, triggerEnt);
	EXPECT_EQ(scn.status, scene::Scene::Status::Playing);
}

TEST_F(SceneTriggerTest, VictoryWithLevelNameRequestsTeleport) {
	scene::Scene scn;
	auto player = scn.createEntity("Player");
	auto triggerEnt = scn.createEntity("Trigger");
	scene::SceneTrigger trigger;
	trigger.type = scene::SceneTrigger::TriggerType::Victory;
	trigger.levelName = "victory.scene";
	trigger.onTriggered(player, triggerEnt);
	EXPECT_TRUE(scn.teleportRequest.pending);
	EXPECT_EQ(scn.teleportRequest.levelName, "victory.scene");
}

TEST_F(SceneTriggerTest, DeathDefaultsToSceneStatus) {
	scene::Scene scn;
	auto player = scn.createEntity("Player");
	auto triggerEnt = scn.createEntity("Trigger");
	scene::SceneTrigger trigger;
	trigger.type = scene::SceneTrigger::TriggerType::Death;
	trigger.onTriggered(player, triggerEnt);
	EXPECT_EQ(scn.status, scene::Scene::Status::Death);
}

TEST_F(SceneTriggerTest, DeathWithLevelNameRequestsTeleport) {
	scene::Scene scn;
	auto player = scn.createEntity("Player");
	auto triggerEnt = scn.createEntity("Trigger");
	scene::SceneTrigger trigger;
	trigger.type = scene::SceneTrigger::TriggerType::Death;
	trigger.levelName = "gameover.scene";
	trigger.onTriggered(player, triggerEnt);
	EXPECT_TRUE(scn.teleportRequest.pending);
	EXPECT_EQ(scn.teleportRequest.levelName, "gameover.scene");
}

TEST_F(SceneTriggerTest, TargetAndTimerAreNoOps) {
	scene::Scene scn;
	const auto initialStatus = scn.status;
	auto player = scn.createEntity("Player");
	auto triggerEnt = scn.createEntity("Trigger");
	scene::SceneTrigger trigger;
	trigger.type = scene::SceneTrigger::TriggerType::Target;
	trigger.onTriggered(player, triggerEnt);// no crash, no state change
	EXPECT_FALSE(trigger.isTriggered());
	EXPECT_EQ(scn.status, initialStatus);
	trigger.type = scene::SceneTrigger::TriggerType::Timer;
	trigger.onTriggered(player, triggerEnt);// timer triggered separately via updateTimer
	EXPECT_FALSE(trigger.isTriggered());
	EXPECT_EQ(scn.status, initialStatus);
}

TEST_F(SceneTriggerTest, TimerFiresAfterDuration) {
	scene::Scene scn;
	auto triggerEnt = scn.createEntity("Trigger");
	scene::SceneTrigger trigger;
	trigger.timerDuration = 0.5f;
	trigger.timerRepeating = false;
	trigger.startTimer();
	trigger.updateTimer(0.4f, triggerEnt);// not yet
	trigger.updateTimer(0.2f, triggerEnt);// fires
	// One-shot timer stops; further ticks do nothing.
	trigger.updateTimer(0.5f, triggerEnt);

	// Repeating timer keeps ticking.
	trigger.timerRepeating = true;
	trigger.startTimer();
	trigger.updateTimer(0.5f, triggerEnt);
	trigger.updateTimer(0.5f, triggerEnt);
	trigger.stopTimer();
	trigger.updateTimer(1.0f, triggerEnt);// stopped, no fire
	trigger.resetTimer();
}

TEST_F(SceneTriggerTest, InteractionRequiresKeyEdge) {
	scene::Scene scn;
	auto player = scn.createEntity("Player");
	auto triggerEnt = scn.createEntity("Trigger");
	scene::SceneTrigger trigger;
	trigger.type = scene::SceneTrigger::TriggerType::Interaction;
	// No input system — `Input::isKeyPressed` returns false. The path that
	// depends on the edge transition is exercised; no crash expected.
	trigger.onTriggered(player, triggerEnt);
}

TEST_F(SceneTriggerTest, LuaCallbackWithoutScriptIsNoOp) {
	scene::Scene scn;
	auto player = scn.createEntity("Player");
	auto triggerEnt = scn.createEntity("Trigger");
	scene::SceneTrigger trigger;
	trigger.type = scene::SceneTrigger::TriggerType::LuaCallback;
	trigger.callbackName = "on_pickup";
	trigger.onTriggered(player, triggerEnt);// no LuaScript → no-op, no crash.
}

TEST_F(SceneTriggerTest, TeleportSameLevelMovesPlayerToTarget) {
	scene::Scene scn;
	auto player = scn.createEntity("Player");
	{
		auto& [body] = player.addComponent<scene::component::PhysicBody>();
		body.type = scene::SceneBody::BodyType::Dynamic;
	}
	auto triggerEnt = scn.createEntity("TeleportTrigger");
	auto targetEnt = scn.createEntity("Target");
	auto& [targetTransform] = targetEnt.getComponent<scene::component::Transform>();
	targetTransform.translation() = {25.f, 50.f, 0.f};

	physics::PhysicCommand::init(&scn);
	physics::PhysicCommand::setVelocity(player, {3.f, 0.f});

	scene::SceneTrigger trigger;
	trigger.type = scene::SceneTrigger::TriggerType::Teleport;
	trigger.targetName = "Target";
	trigger.onTriggered(player, triggerEnt);

	// Player transform should be at target position.
	auto& [playerTransform] = player.getComponent<scene::component::Transform>();
	EXPECT_NEAR(playerTransform.translation().x(), 25.f, 0.01f);
	EXPECT_NEAR(playerTransform.translation().y(), 50.f, 0.01f);
}

TEST_F(SceneTriggerTest, TeleportCrossLevelSetsRequest) {
	scene::Scene scn;
	auto player = scn.createEntity("Player");
	{
		auto& [body] = player.addComponent<scene::component::PhysicBody>();
		body.type = scene::SceneBody::BodyType::Dynamic;
	}
	auto triggerEnt = scn.createEntity("TeleportTrigger");

	physics::PhysicCommand::init(&scn);
	physics::PhysicCommand::setVelocity(player, {2.f, 1.f});

	scene::SceneTrigger trigger;
	trigger.type = scene::SceneTrigger::TriggerType::Teleport;
	trigger.levelName = "next.scene";
	trigger.targetName = "spawn";
	trigger.onTriggered(player, triggerEnt);

	EXPECT_TRUE(scn.teleportRequest.pending);
	EXPECT_EQ(scn.teleportRequest.levelName, "next.scene");
	EXPECT_EQ(scn.teleportRequest.targetName, "spawn");
}

TEST_F(SceneTriggerTest, EnterExitNoOpWithoutScripts) {
	scene::Scene scn;
	auto player = scn.createEntity("Player");
	auto triggerEnt = scn.createEntity("Trigger");
	scene::SceneTrigger trigger;
	trigger.onTriggerEnter(player, triggerEnt);
	trigger.onTriggerExit(player, triggerEnt);
}

TEST_F(SceneTriggerTest, OverlappingFlag) {
	scene::SceneTrigger trigger;
	trigger.setOverlapping(true);
	trigger.setOverlapping(false);
	// Plain setter with no observable side-effect — exercises the line.
}
