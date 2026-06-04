/**
 * @file componentsRoundTrip_test.cpp
 * @author Silmaen
 * @date 07/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

using namespace owl;

namespace {

auto makeApp(const char* iName) -> shared<app::Application> {
	return mkShared<app::Application>(app::AppParams{.args = nullptr,
													 .frameLogFrequency = 0,
													 .name = iName,
													 .assetsPattern = "",
													 .icon = "",
													 .width = 0,
													 .height = 0,
													 .argCount = 0,
													 .renderer = renderer::gpu::RenderAPI::Type::Null,
													 .hasGui = false,
													 .useDebugging = false,
													 .isDummy = true});
}

template<typename Mutate, typename Verify>
void roundTrip(const char* iCaseName, Mutate&& iMutate, Verify&& iVerify) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeApp(iCaseName);
	const auto sc = mkShared<scene::Scene>();
	auto ent = sc->createEntityWithUUID(42, "TestEntity");
	iMutate(ent);
	const scene::SceneSerializer saver(sc);
	const auto fs = std::filesystem::temp_directory_path() / (std::string("owl_rt_") + iCaseName + ".yml");
	saver.serialize(fs);
	ASSERT_TRUE(exists(fs));
	const auto sc2 = mkShared<scene::Scene>();
	const scene::SceneSerializer loader(sc2);
	EXPECT_TRUE(loader.deserialize(fs));
	const auto entities = sc2->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	iVerify(entities[0]);
	std::filesystem::remove(fs);
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

}// namespace

TEST(ComponentRoundTrip, UiRect) {
	roundTrip(
			"UiRect",
			[](scene::Entity& iEnt) {
				auto& rect = iEnt.addComponent<scene::component::UiRect>();
				rect.anchor = scene::component::UiRect::Anchor::BottomRight;
				rect.pivot = {0.25f, 0.75f};
				rect.size = {200.f, 50.f};
				rect.anchorOffset = {-10.f, 5.f};
			},
			[](const scene::Entity& iEnt) {
				const auto& rect = iEnt.getComponent<scene::component::UiRect>();
				EXPECT_EQ(rect.anchor, scene::component::UiRect::Anchor::BottomRight);
				EXPECT_NEAR(rect.pivot.x(), 0.25f, 0.001f);
				EXPECT_NEAR(rect.pivot.y(), 0.75f, 0.001f);
				EXPECT_NEAR(rect.size.x(), 200.f, 0.001f);
				EXPECT_NEAR(rect.size.y(), 50.f, 0.001f);
				EXPECT_NEAR(rect.anchorOffset.x(), -10.f, 0.001f);
				EXPECT_NEAR(rect.anchorOffset.y(), 5.f, 0.001f);
			});
}

TEST(ComponentRoundTrip, UiText) {
	roundTrip(
			"UiText",
			[](scene::Entity& iEnt) {
				auto& uiText = iEnt.addComponent<scene::component::UiText>();
				uiText.text = "hello";
				uiText.color = {0.1f, 0.2f, 0.3f, 0.4f};
				uiText.fontSize = 24.f;
				uiText.alignment = scene::component::UiText::Alignment::Right;
				uiText.kerning = 1.5f;
				uiText.lineSpacing = 2.f;
			},
			[](const scene::Entity& iEnt) {
				const auto& t = iEnt.getComponent<scene::component::UiText>();
				EXPECT_EQ(t.text, "hello");
				EXPECT_NEAR(t.color.x(), 0.1f, 0.01f);
				EXPECT_NEAR(t.fontSize, 24.f, 0.01f);
				EXPECT_EQ(t.alignment, scene::component::UiText::Alignment::Right);
				EXPECT_NEAR(t.kerning, 1.5f, 0.01f);
				EXPECT_NEAR(t.lineSpacing, 2.f, 0.01f);
			});
}

TEST(ComponentRoundTrip, UiButton) {
	roundTrip(
			"UiButton",
			[](scene::Entity& iEnt) {
				auto& b = iEnt.addComponent<scene::component::UiButton>();
				b.normalColor = {0.1f, 0.2f, 0.3f, 1.f};
				b.hoverColor = {0.2f, 0.3f, 0.4f, 1.f};
				b.pressedColor = {0.3f, 0.4f, 0.5f, 1.f};
				b.disabledColor = {0.0f, 0.0f, 0.0f, 0.5f};
				b.onClickCallback = "do_thing";
			},
			[](const scene::Entity& iEnt) {
				const auto& b = iEnt.getComponent<scene::component::UiButton>();
				EXPECT_NEAR(b.normalColor.x(), 0.1f, 0.01f);
				EXPECT_NEAR(b.hoverColor.x(), 0.2f, 0.01f);
				EXPECT_NEAR(b.pressedColor.x(), 0.3f, 0.01f);
				EXPECT_NEAR(b.disabledColor.w(), 0.5f, 0.01f);
				EXPECT_EQ(b.onClickCallback, "do_thing");
				EXPECT_EQ(b.state, scene::component::UiButton::State::Normal);
			});
}

TEST(ComponentRoundTrip, UiSlider) {
	roundTrip(
			"UiSlider",
			[](scene::Entity& iEnt) {
				auto& s = iEnt.addComponent<scene::component::UiSlider>();
				s.value = 0.6f;
				s.minValue = -1.f;
				s.maxValue = 2.f;
				s.trackColor = {0.1f, 0.1f, 0.1f, 1.f};
				s.fillColor = {0.2f, 0.2f, 0.2f, 1.f};
				s.handleColor = {0.3f, 0.3f, 0.3f, 1.f};
				s.onValueChangedCallback = "value_cb";
			},
			[](const scene::Entity& iEnt) {
				const auto& s = iEnt.getComponent<scene::component::UiSlider>();
				EXPECT_NEAR(s.value, 0.6f, 0.001f);
				EXPECT_NEAR(s.minValue, -1.f, 0.001f);
				EXPECT_NEAR(s.maxValue, 2.f, 0.001f);
				EXPECT_NEAR(s.trackColor.x(), 0.1f, 0.01f);
				EXPECT_EQ(s.onValueChangedCallback, "value_cb");
			});
}

TEST(ComponentRoundTrip, UiProgressBar) {
	roundTrip(
			"UiProgressBar",
			[](scene::Entity& iEnt) {
				auto& p = iEnt.addComponent<scene::component::UiProgressBar>();
				p.value = 0.42f;
				p.backgroundColor = {0.05f, 0.05f, 0.05f, 1.f};
				p.fillColor = {0.5f, 0.7f, 0.5f, 1.f};
			},
			[](const scene::Entity& iEnt) {
				const auto& p = iEnt.getComponent<scene::component::UiProgressBar>();
				EXPECT_NEAR(p.value, 0.42f, 0.001f);
				EXPECT_NEAR(p.backgroundColor.x(), 0.05f, 0.01f);
				EXPECT_NEAR(p.fillColor.y(), 0.7f, 0.01f);
			});
}

TEST(ComponentRoundTrip, UiImage) {
	roundTrip(
			"UiImage",
			[](scene::Entity& iEnt) {
				auto& img = iEnt.addComponent<scene::component::UiImage>();
				img.tint = {0.5f, 0.6f, 0.7f, 1.f};
			},
			[](const scene::Entity& iEnt) {
				const auto& img = iEnt.getComponent<scene::component::UiImage>();
				EXPECT_NEAR(img.tint.x(), 0.5f, 0.01f);
				EXPECT_NEAR(img.tint.y(), 0.6f, 0.01f);
			});
}

TEST(ComponentRoundTrip, UiPanel) {
	roundTrip(
			"UiPanel",
			[](scene::Entity& iEnt) {
				auto& p = iEnt.addComponent<scene::component::UiPanel>();
				p.backgroundColor = {0.4f, 0.4f, 0.4f, 0.9f};
				p.borderColor = {1.f, 0.8f, 0.f, 1.f};
				p.borderWidth = 2.5f;
				p.spacing = 4.f;
				p.padding = 6.f;
			},
			[](const scene::Entity& iEnt) {
				const auto& p = iEnt.getComponent<scene::component::UiPanel>();
				EXPECT_NEAR(p.backgroundColor.w(), 0.9f, 0.01f);
				EXPECT_NEAR(p.borderColor.y(), 0.8f, 0.01f);
				EXPECT_NEAR(p.borderWidth, 2.5f, 0.01f);
				EXPECT_NEAR(p.spacing, 4.f, 0.01f);
				EXPECT_NEAR(p.padding, 6.f, 0.01f);
			});
}

TEST(ComponentRoundTrip, Trigger) {
	roundTrip(
			"Trigger",
			[](scene::Entity& iEnt) {
				auto& trig = iEnt.addComponent<scene::component::Trigger>();
				trig.trigger.type = scene::SceneTrigger::TriggerType::Timer;
				trig.trigger.timerDuration = 1.5f;
				trig.trigger.timerRepeating = true;
				trig.trigger.callbackName = "on_tick";
			},
			[](const scene::Entity& iEnt) {
				const auto& t = iEnt.getComponent<scene::component::Trigger>();
				EXPECT_EQ(t.trigger.type, scene::SceneTrigger::TriggerType::Timer);
				EXPECT_NEAR(t.trigger.timerDuration, 1.5f, 0.001f);
				EXPECT_TRUE(t.trigger.timerRepeating);
				EXPECT_EQ(t.trigger.callbackName, "on_tick");
			});
}

TEST(ComponentRoundTrip, TriggerInteraction) {
	roundTrip(
			"TriggerInteraction",
			[](scene::Entity& iEnt) {
				auto& trig = iEnt.addComponent<scene::component::Trigger>();
				trig.trigger.type = scene::SceneTrigger::TriggerType::Interaction;
				trig.trigger.interactionRange = 3.5f;
			},
			[](const scene::Entity& iEnt) {
				const auto& t = iEnt.getComponent<scene::component::Trigger>();
				EXPECT_EQ(t.trigger.type, scene::SceneTrigger::TriggerType::Interaction);
				EXPECT_NEAR(t.trigger.interactionRange, 3.5f, 0.001f);
			});
}

TEST(ComponentRoundTrip, TriggerTeleport) {
	roundTrip(
			"TriggerTeleport",
			[](scene::Entity& iEnt) {
				auto& trig = iEnt.addComponent<scene::component::Trigger>();
				trig.trigger.type = scene::SceneTrigger::TriggerType::Teleport;
				trig.trigger.levelName = "level_2";
				trig.trigger.targetName = "spawn_a";
			},
			[](const scene::Entity& iEnt) {
				const auto& t = iEnt.getComponent<scene::component::Trigger>();
				EXPECT_EQ(t.trigger.levelName, "level_2");
				EXPECT_EQ(t.trigger.targetName, "spawn_a");
			});
}

TEST(ComponentRoundTrip, SoundSource) {
	roundTrip(
			"SoundSource",
			[](scene::Entity& iEnt) {
				auto& [sound] = iEnt.addComponent<scene::component::SoundSource>();
				sound.soundAsset = "music/intro.wav";
				sound.category = scene::SceneSound::Category::Music;
				sound.volume = 0.7f;
				sound.pitch = 1.2f;
				sound.loop = true;
				sound.spatial = true;
				sound.playOnStart = true;
				sound.maxDistance = 25.f;
				sound.rolloff = 0.8f;
			},
			[](const scene::Entity& iEnt) {
				const auto& [sound] = iEnt.getComponent<scene::component::SoundSource>();
				EXPECT_EQ(sound.soundAsset, "music/intro.wav");
				EXPECT_EQ(sound.category, scene::SceneSound::Category::Music);
				EXPECT_NEAR(sound.volume, 0.7f, 0.01f);
				EXPECT_NEAR(sound.pitch, 1.2f, 0.01f);
				EXPECT_TRUE(sound.loop);
				EXPECT_TRUE(sound.spatial);
				EXPECT_TRUE(sound.playOnStart);
				EXPECT_NEAR(sound.maxDistance, 25.f, 0.01f);
				EXPECT_NEAR(sound.rolloff, 0.8f, 0.01f);
			});
}

TEST(ComponentRoundTrip, BackgroundTexture) {
	roundTrip(
			"BackgroundTexture",
			[](scene::Entity& iEnt) {
				auto& bg = iEnt.addComponent<scene::component::BackgroundTexture>();
				bg.mode = scene::component::BackgroundTexture::Mode::Skybox;
				bg.type = scene::component::BackgroundTexture::Type::Gradient;
				bg.color = {0.1f, 0.2f, 0.3f, 1.f};
				bg.topColor = {0.4f, 0.5f, 0.6f, 1.f};
			},
			[](const scene::Entity& iEnt) {
				const auto& bg = iEnt.getComponent<scene::component::BackgroundTexture>();
				EXPECT_EQ(bg.mode, scene::component::BackgroundTexture::Mode::Skybox);
				EXPECT_EQ(bg.type, scene::component::BackgroundTexture::Type::Gradient);
				EXPECT_NEAR(bg.color.x(), 0.1f, 0.01f);
				EXPECT_NEAR(bg.topColor.z(), 0.6f, 0.01f);
			});
}

TEST(ComponentRoundTrip, PrefabLink) {
	roundTrip(
			"PrefabLink",
			[](scene::Entity& iEnt) {
				auto& link = iEnt.addComponent<scene::component::PrefabLink>();
				link.prefabAssetPath = "prefabs/enemy.owlprefab";
				link.syncedVersion = 5;
				link.uuidMapping.push_back({.instanceUuid = 1001, .canonicalUuid = 2001});
				link.uuidMapping.push_back({.instanceUuid = 1002, .canonicalUuid = 2002});
				link.overriddenComponents.emplace_back("2001:Transform");
				link.overriddenComponents.emplace_back("2002:Tag");
			},
			[](const scene::Entity& iEnt) {
				const auto& link = iEnt.getComponent<scene::component::PrefabLink>();
				EXPECT_EQ(link.prefabAssetPath, "prefabs/enemy.owlprefab");
				EXPECT_EQ(link.syncedVersion, 5u);
				ASSERT_EQ(link.uuidMapping.size(), 2u);
				EXPECT_EQ(link.uuidMapping[0].instanceUuid, 1001u);
				EXPECT_EQ(link.uuidMapping[0].canonicalUuid, 2001u);
				ASSERT_EQ(link.overriddenComponents.size(), 2u);
				EXPECT_EQ(link.overriddenComponents[0], "2001:Transform");
			});
}

TEST(ComponentRoundTrip, PhysicBody) {
	roundTrip(
			"PhysicBody",
			[](scene::Entity& iEnt) {
				auto& [body] = iEnt.addComponent<scene::component::PhysicBody>();
				body.type = scene::SceneBody::BodyType::Kinematic;
				body.density = 2.f;
				body.friction = 0.4f;
				body.restitution = 0.7f;
				body.fixedRotation = true;
				body.colliderSize = {2.f, 1.f, 1.f};
			},
			[](const scene::Entity& iEnt) {
				const auto& [body] = iEnt.getComponent<scene::component::PhysicBody>();
				EXPECT_EQ(body.type, scene::SceneBody::BodyType::Kinematic);
				EXPECT_NEAR(body.density, 2.f, 0.01f);
				EXPECT_NEAR(body.friction, 0.4f, 0.01f);
				EXPECT_NEAR(body.restitution, 0.7f, 0.01f);
				EXPECT_TRUE(body.fixedRotation);
				EXPECT_NEAR(body.colliderSize.x(), 2.f, 0.01f);
				EXPECT_NEAR(body.colliderSize.y(), 1.f, 0.01f);
			});
}

TEST(UiRect, ComputePositionAcrossAllAnchors) {
	using A = scene::component::UiRect::Anchor;
	const math::vec2 parent{200.f, 100.f};
	scene::component::UiRect rect;
	rect.size = {0.f, 0.f};
	rect.pivot = {0.5f, 0.5f};
	rect.anchorOffset = {0.f, 0.f};

	rect.anchor = A::TopLeft;
	auto p = rect.computePosition(parent);
	EXPECT_NEAR(p.x(), 0.f, 0.001f);
	EXPECT_NEAR(p.y(), 100.f, 0.001f);

	rect.anchor = A::TopCenter;
	p = rect.computePosition(parent);
	EXPECT_NEAR(p.x(), 100.f, 0.001f);
	EXPECT_NEAR(p.y(), 100.f, 0.001f);

	rect.anchor = A::TopRight;
	p = rect.computePosition(parent);
	EXPECT_NEAR(p.x(), 200.f, 0.001f);
	EXPECT_NEAR(p.y(), 100.f, 0.001f);

	rect.anchor = A::MiddleLeft;
	p = rect.computePosition(parent);
	EXPECT_NEAR(p.x(), 0.f, 0.001f);
	EXPECT_NEAR(p.y(), 50.f, 0.001f);

	rect.anchor = A::Center;
	p = rect.computePosition(parent);
	EXPECT_NEAR(p.x(), 100.f, 0.001f);
	EXPECT_NEAR(p.y(), 50.f, 0.001f);

	rect.anchor = A::MiddleRight;
	p = rect.computePosition(parent);
	EXPECT_NEAR(p.x(), 200.f, 0.001f);
	EXPECT_NEAR(p.y(), 50.f, 0.001f);

	rect.anchor = A::BottomLeft;
	p = rect.computePosition(parent);
	EXPECT_NEAR(p.x(), 0.f, 0.001f);
	EXPECT_NEAR(p.y(), 0.f, 0.001f);

	rect.anchor = A::BottomCenter;
	p = rect.computePosition(parent);
	EXPECT_NEAR(p.x(), 100.f, 0.001f);
	EXPECT_NEAR(p.y(), 0.f, 0.001f);

	rect.anchor = A::BottomRight;
	p = rect.computePosition(parent);
	EXPECT_NEAR(p.x(), 200.f, 0.001f);
	EXPECT_NEAR(p.y(), 0.f, 0.001f);
}

TEST(UiRect, ComputePositionWithPivotAndOffset) {
	scene::component::UiRect rect;
	rect.anchor = scene::component::UiRect::Anchor::TopLeft;
	rect.pivot = {0.f, 1.f};// top-left pivot
	rect.size = {40.f, 20.f};
	rect.anchorOffset = {3.f, -7.f};
	const auto p = rect.computePosition({100.f, 100.f});
	// TopLeft anchor in this engine = (0, parentY) = (0, 100).
	// posX = 0 + 3 + 40*(0.5 - 0) = 23
	// posY = 100 - 7 + 20*(0.5 - 1) = 83
	EXPECT_NEAR(p.x(), 23.f, 0.001f);
	EXPECT_NEAR(p.y(), 83.f, 0.001f);
}

TEST(ComponentRoundTrip, VoxelWorld) {
	roundTrip(
			"VoxelWorld",
			[](scene::Entity& iEnt) {
				auto& vw = iEnt.addComponent<scene::component::VoxelWorld>();
				data::voxel::BlockType stone;
				stone.name = "stone";
				stone.renderKind = data::voxel::BlockRenderKind::Opaque;
				stone.solid = true;
				stone.setAllFaces(1);
				const data::voxel::BlockId stoneId = vw.registry.registerBlock(stone);
				vw.world.setBlock(math::vec3i{1, 2, 3}, stoneId);
				vw.world.setBlock(math::vec3i{-1, 0, 0}, stoneId);
				vw.blockTextures = {"textures/air.png", "textures/stone.png"};
				vw.sunDirection = math::vec3{0.1f, -0.9f, 0.2f};
				vw.ambient = math::vec3{0.2f, 0.3f, 0.4f};
			},
			[](const scene::Entity& iEnt) {
				ASSERT_TRUE(iEnt.hasComponent<scene::component::VoxelWorld>());
				const auto& vw = iEnt.getComponent<scene::component::VoxelWorld>();
				ASSERT_EQ(vw.registry.count(), 2u);// air + stone
				EXPECT_EQ(vw.registry.get(1).name, "stone");
				EXPECT_EQ(vw.world.getBlock(math::vec3i{1, 2, 3}), 1u);
				EXPECT_EQ(vw.world.getBlock(math::vec3i{-1, 0, 0}), 1u);
				EXPECT_EQ(vw.world.getBlock(math::vec3i{5, 5, 5}), data::voxel::g_AirBlock);
				ASSERT_EQ(vw.blockTextures.size(), 2u);
				EXPECT_EQ(vw.blockTextures[1].generic_string(), "textures/stone.png");
				EXPECT_FLOAT_EQ(vw.sunDirection.y(), -0.9f);
				EXPECT_FLOAT_EQ(vw.ambient.z(), 0.4f);
			});
}
