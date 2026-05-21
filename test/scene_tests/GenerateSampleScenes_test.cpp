/**
 * @file GenerateSampleScenes_test.cpp
 * @author Silmaen
 * @date 11/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/Canvas.h>
#include <scene/component/CircleRenderer.h>
#include <scene/component/SpriteRenderer.h>
#include <scene/component/UiButton.h>
#include <scene/component/UiProgressBar.h>
#include <scene/component/UiRect.h>
#include <scene/component/UiText.h>
#include <scene/component/components.h>

using namespace owl;
using namespace owl::scene;

TEST(GenerateSampleScenes, mainMenu) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_test_scenes";
	std::filesystem::create_directories(dir);

	auto scn = mkShared<Scene>();
	scn->onViewportResize({1280, 720});

	// Camera
	auto camEntity = scn->createEntity("MainCamera");
	auto& cam = camEntity.addComponent<component::Camera>();
	cam.camera.setOrthographic(10.f, -1.f, 10.f);
	cam.primary = true;
	cam.fixedAspectRatio = false;
	camEntity.getComponent<component::Transform>().transform.translation() = {0.f, 0.f, 5.f};

	// Background
	auto bgEntity = scn->createEntity("Background");
	bgEntity.getComponent<component::Transform>().transform.translation() = {0.f, 0.f, -1.f};
	bgEntity.getComponent<component::Transform>().transform.scale() = {20.f, 15.f, 1.f};
	bgEntity.addComponent<component::SpriteRenderer>().color = {0.08f, 0.08f, 0.18f, 1.f};

	// Decorative circles
	auto circle1 = scn->createEntity("DecoCircle1");
	circle1.getComponent<component::Transform>().transform.translation() = {-3.f, -1.f, 0.f};
	circle1.getComponent<component::Transform>().transform.scale() = {3.f, 3.f, 1.f};
	auto& c1 = circle1.addComponent<component::CircleRenderer>();
	c1.color = {0.15f, 0.25f, 0.5f, 0.4f};
	c1.thickness = 0.2f;
	c1.fade = 0.05f;

	auto circle2 = scn->createEntity("DecoCircle2");
	circle2.getComponent<component::Transform>().transform.translation() = {4.f, 1.5f, 0.f};
	circle2.getComponent<component::Transform>().transform.scale() = {2.f, 2.f, 1.f};
	auto& c2 = circle2.addComponent<component::CircleRenderer>();
	c2.color = {0.4f, 0.2f, 0.5f, 0.3f};
	c2.thickness = 0.15f;
	c2.fade = 0.05f;

	// Canvas + script
	auto canvasEntity = scn->createEntity("MenuCanvas");
	canvasEntity.addComponent<component::Canvas>().sortOrder = 0;
	canvasEntity.addComponent<component::LuaScript>().scriptPath = "scripts/main_menu.lua";

	// Title
	auto titleEntity = scn->createEntity("TitleText");
	auto& titleRect = titleEntity.addComponent<component::UiRect>();
	titleRect.anchor = component::UiRect::Anchor::TopCenter;
	titleRect.pivot = {0.5f, 1.f};
	titleRect.size = {600.f, 80.f};
	titleRect.anchorOffset = {0.f, -80.f};
	auto& titleText = titleEntity.addComponent<component::UiText>();
	titleText.text = "Owl Feature Demo";
	titleText.color = {1.f, 0.85f, 0.2f, 1.f};
	titleText.fontSize = 42.f;
	scn->setParent(titleEntity, canvasEntity);

	// Play button background
	auto playEntity = scn->createEntity("PlayButton");
	auto& playRect = playEntity.addComponent<component::UiRect>();
	playRect.anchor = component::UiRect::Anchor::Center;
	playRect.size = {200.f, 50.f};
	playRect.anchorOffset = {0.f, 0.f};
	auto& playBtn = playEntity.addComponent<component::UiButton>();
	playBtn.normalColor = {0.15f, 0.35f, 0.65f, 1.f};
	playBtn.hoverColor = {0.25f, 0.45f, 0.75f, 1.f};
	playBtn.pressedColor = {0.1f, 0.25f, 0.5f, 1.f};
	playBtn.onClickCallback = "on_play_clicked";
	auto& playText = playEntity.addComponent<component::UiText>();
	playText.text = "Play";
	playText.color = {1.f, 1.f, 1.f, 1.f};
	playText.fontSize = 26.f;
	scn->setParent(playEntity, canvasEntity);

	// Version label
	auto versionEntity = scn->createEntity("VersionLabel");
	auto& verRect = versionEntity.addComponent<component::UiRect>();
	verRect.anchor = component::UiRect::Anchor::BottomRight;
	verRect.pivot = {1.f, 0.f};
	verRect.size = {180.f, 25.f};
	verRect.anchorOffset = {-15.f, 15.f};
	auto& verText = versionEntity.addComponent<component::UiText>();
	verText.text = "testing-version";
	verText.color = {0.5f, 0.5f, 0.5f, 0.7f};
	verText.fontSize = 12.f;
	scn->setParent(versionEntity, canvasEntity);

	const SceneSerializer serializer(scn);
	serializer.serialize(dir / "main_menu.owl");
	EXPECT_TRUE(std::filesystem::exists(dir / "main_menu.owl"));
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(GenerateSampleScenes, gameplay) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_test_scenes";
	std::filesystem::create_directories(dir);

	auto scn = mkShared<Scene>();
	scn->onViewportResize({1280, 720});

	// Camera
	auto camEntity = scn->createEntity("MainCamera");
	auto& cam = camEntity.addComponent<component::Camera>();
	cam.camera.setOrthographic(10.f, -1.f, 10.f);
	cam.primary = true;
	cam.fixedAspectRatio = false;
	camEntity.getComponent<component::Transform>().transform.translation() = {0.f, 0.f, 5.f};
	camEntity.addComponent<component::SoundListener>().primary = true;

	// Player (blue square)
	auto playerEntity = scn->createEntity("Player");
	playerEntity.getComponent<component::Transform>().transform.translation() = {-3.f, 2.f, 0.f};
	playerEntity.addComponent<component::SpriteRenderer>().color = {0.3f, 0.6f, 1.f, 1.f};
	auto& playerBody = playerEntity.addComponent<component::PhysicBody>();
	playerBody.body.type = SceneBody::BodyType::Dynamic;
	playerBody.body.fixedRotation = true;
	auto& player = playerEntity.addComponent<component::Player>();
	player.primary = true;
	player.player.linearImpulse = 8.f;
	player.player.jumpImpulse = 12.f;
	auto& playerScript = playerEntity.addComponent<component::LuaScript>();
	playerScript.scriptPath = "scripts/player.lua";
	playerScript.properties.push_back({.name = "speed", .type = script::ScriptPropertyType::Float, .value = 8.0f});
	playerScript.properties.push_back({.name = "score", .type = script::ScriptPropertyType::Int, .value = int64_t{0}});

	// Player hat (child — tests hierarchy)
	auto hatEntity = scn->createEntity("PlayerHat");
	hatEntity.getComponent<component::Transform>().transform.translation() = {0.f, 0.55f, 0.1f};
	hatEntity.getComponent<component::Transform>().transform.scale() = {0.5f, 0.25f, 1.f};
	hatEntity.addComponent<component::SpriteRenderer>().color = {1.f, 0.85f, 0.1f, 1.f};
	scn->setParent(hatEntity, playerEntity);

	// Ground
	auto groundEntity = scn->createEntity("Ground");
	groundEntity.getComponent<component::Transform>().transform.translation() = {0.f, -4.f, 0.f};
	groundEntity.getComponent<component::Transform>().transform.scale() = {20.f, 1.f, 1.f};
	groundEntity.addComponent<component::SpriteRenderer>().color = {0.25f, 0.45f, 0.2f, 1.f};
	groundEntity.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;

	// Walls
	auto leftWall = scn->createEntity("LeftWall");
	leftWall.getComponent<component::Transform>().transform.translation() = {-8.f, 0.f, 0.f};
	leftWall.getComponent<component::Transform>().transform.scale() = {1.f, 10.f, 1.f};
	leftWall.addComponent<component::SpriteRenderer>().color = {0.35f, 0.35f, 0.35f, 1.f};
	leftWall.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;

	auto rightWall = scn->createEntity("RightWall");
	rightWall.getComponent<component::Transform>().transform.translation() = {8.f, 0.f, 0.f};
	rightWall.getComponent<component::Transform>().transform.scale() = {1.f, 10.f, 1.f};
	rightWall.addComponent<component::SpriteRenderer>().color = {0.35f, 0.35f, 0.35f, 1.f};
	rightWall.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;

	// Coins (gold circles)
	const float coinPositions[][2] = {{3.f, -1.f}, {-2.f, 0.f}, {5.f, 1.f}};
	for (int i = 0; i < 3; ++i) {
		auto coin = scn->createEntity(std::format("Coin{}", i + 1));
		coin.getComponent<component::Transform>().transform.translation() = {coinPositions[i][0], coinPositions[i][1],
																			 0.f};
		coin.getComponent<component::Transform>().transform.scale() = {0.7f, 0.7f, 1.f};
		auto& cr = coin.addComponent<component::CircleRenderer>();
		cr.color = {1.f, 0.85f, 0.f, 1.f};
		cr.thickness = 1.0f;
		cr.fade = 0.02f;
	}

	// Moving platform
	auto platform = scn->createEntity("Platform");
	platform.getComponent<component::Transform>().transform.translation() = {2.f, -2.f, 0.f};
	platform.getComponent<component::Transform>().transform.scale() = {3.f, 0.4f, 1.f};
	platform.addComponent<component::SpriteRenderer>().color = {0.55f, 0.35f, 0.15f, 1.f};
	auto& platScript = platform.addComponent<component::LuaScript>();
	platScript.scriptPath = "scripts/moving_platform.lua";
	platScript.properties.push_back({.name = "speed", .type = script::ScriptPropertyType::Float, .value = 2.0f});
	platScript.properties.push_back({.name = "distance", .type = script::ScriptPropertyType::Float, .value = 4.0f});

	// HUD Canvas
	auto hudCanvas = scn->createEntity("HUDCanvas");
	hudCanvas.addComponent<component::Canvas>().sortOrder = 0;
	hudCanvas.addComponent<component::LuaScript>().scriptPath = "scripts/hud.lua";

	// Score text (top-left)
	auto scoreEntity = scn->createEntity("ScoreText");
	auto& scoreRect = scoreEntity.addComponent<component::UiRect>();
	scoreRect.anchor = component::UiRect::Anchor::TopLeft;
	scoreRect.pivot = {0.f, 1.f};
	scoreRect.size = {200.f, 35.f};
	scoreRect.anchorOffset = {15.f, -10.f};
	auto& scoreText = scoreEntity.addComponent<component::UiText>();
	scoreText.text = "Score: 0";
	scoreText.color = {1.f, 1.f, 0.3f, 1.f};
	scoreText.fontSize = 22.f;
	scn->setParent(scoreEntity, hudCanvas);

	// Health bar (top-right)
	auto healthEntity = scn->createEntity("HealthBar");
	auto& healthRect = healthEntity.addComponent<component::UiRect>();
	healthRect.anchor = component::UiRect::Anchor::TopRight;
	healthRect.pivot = {1.f, 1.f};
	healthRect.size = {180.f, 20.f};
	healthRect.anchorOffset = {-15.f, -12.f};
	auto& healthBar = healthEntity.addComponent<component::UiProgressBar>();
	healthBar.value = 1.f;
	healthBar.backgroundColor = {0.4f, 0.1f, 0.1f, 0.8f};
	healthBar.fillColor = {0.2f, 0.85f, 0.2f, 1.f};
	scn->setParent(healthEntity, hudCanvas);

	// Instructions (top-centre)
	auto instrEntity = scn->createEntity("Instructions");
	auto& instrRect = instrEntity.addComponent<component::UiRect>();
	instrRect.anchor = component::UiRect::Anchor::TopCenter;
	instrRect.pivot = {0.5f, 1.f};
	instrRect.size = {400.f, 30.f};
	instrRect.anchorOffset = {0.f, -10.f};
	auto& instrText = instrEntity.addComponent<component::UiText>();
	instrText.text = "WASD to move - Collect all coins!";
	instrText.color = {1.f, 1.f, 1.f, 0.6f};
	instrText.fontSize = 14.f;
	scn->setParent(instrEntity, hudCanvas);

	const SceneSerializer serializer(scn);
	serializer.serialize(dir / "gameplay.owl");
	EXPECT_TRUE(std::filesystem::exists(dir / "gameplay.owl"));
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
