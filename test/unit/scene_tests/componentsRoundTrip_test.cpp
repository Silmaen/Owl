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
#include <renderer/gpu/Framebuffer.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <cstdint>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

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
	EXPECT_TRUE(saver.serialize(fs));
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
			[](scene::Entity& iEnt) -> void {
				auto& rect = iEnt.addComponent<scene::component::UiRect>();
				rect.anchorMin = {0.1f, 0.2f};
				rect.anchorMax = {0.9f, 0.8f};
				rect.pivot = {0.25f, 0.75f};
				rect.size = {200.f, 50.f};
				rect.anchorOffset = {-10.f, 5.f};
			},
			[](const scene::Entity& iEnt) -> void {
				const auto& rect = iEnt.getComponent<scene::component::UiRect>();
				EXPECT_NEAR(rect.anchorMin.x(), 0.1f, 0.001f);
				EXPECT_NEAR(rect.anchorMin.y(), 0.2f, 0.001f);
				EXPECT_NEAR(rect.anchorMax.x(), 0.9f, 0.001f);
				EXPECT_NEAR(rect.anchorMax.y(), 0.8f, 0.001f);
				EXPECT_FALSE(rect.getAnchor().has_value());
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
			[](scene::Entity& iEnt) -> void {
				auto& uiText = iEnt.addComponent<scene::component::UiText>();
				uiText.text = "hello";
				uiText.color = {0.1f, 0.2f, 0.3f, 0.4f};
				uiText.fontSize = 24.f;
				uiText.horizontalAlignment = data::fonts::HorizontalAlignment::Right;
				uiText.verticalAlignment = data::fonts::VerticalAlignment::Baseline;
				uiText.wrap = true;
				uiText.fit = data::fonts::TextFit::Shrink;
				uiText.minFontSize = 10.f;
				uiText.kerning = 1.5f;
				uiText.lineSpacing = 2.f;
				uiText.outlineColor = {0.2f, 0.3f, 0.4f, 0.5f};
				uiText.outlineWidth = 2.5f;
				uiText.backgroundColor = {0.6f, 0.7f, 0.8f, 0.9f};
				uiText.backgroundPadding = 6.f;
			},
			[](const scene::Entity& iEnt) -> void {
				const auto& t = iEnt.getComponent<scene::component::UiText>();
				EXPECT_EQ(t.text, "hello");
				EXPECT_NEAR(t.color.x(), 0.1f, 0.01f);
				EXPECT_NEAR(t.fontSize, 24.f, 0.01f);
				EXPECT_EQ(t.horizontalAlignment, data::fonts::HorizontalAlignment::Right);
				EXPECT_EQ(t.verticalAlignment, data::fonts::VerticalAlignment::Baseline);
				EXPECT_TRUE(t.wrap);
				EXPECT_EQ(t.fit, data::fonts::TextFit::Shrink);
				EXPECT_NEAR(t.minFontSize, 10.f, 0.01f);
				EXPECT_NEAR(t.kerning, 1.5f, 0.01f);
				EXPECT_NEAR(t.lineSpacing, 2.f, 0.01f);
				EXPECT_NEAR(t.outlineColor.y(), 0.3f, 0.01f);
				EXPECT_NEAR(t.outlineWidth, 2.5f, 0.01f);
				EXPECT_NEAR(t.backgroundColor.w(), 0.9f, 0.01f);
				EXPECT_NEAR(t.backgroundPadding, 6.f, 0.01f);
			});
}

TEST(ComponentRoundTrip, UiTextWithoutOutlineOrBox) {
	roundTrip(
			"UiText", [](scene::Entity& iEnt) -> void { iEnt.addComponent<scene::component::UiText>().text = "plain"; },
			[](const scene::Entity& iEnt) -> void {
				const auto& t = iEnt.getComponent<scene::component::UiText>();
				EXPECT_FLOAT_EQ(t.outlineWidth, 0.f);
				EXPECT_FLOAT_EQ(t.backgroundColor.w(), 0.f);
			});
}

TEST(ComponentRoundTrip, TextRendererOutlineAndBox) {
	roundTrip(
			"TextRenderer",
			[](scene::Entity& iEnt) -> void {
				auto& text = iEnt.addComponent<scene::component::Text>();
				text.text = "world";
				text.outlineColor = {0.1f, 0.2f, 0.3f, 0.4f};
				text.outlineWidth = 0.07f;
				text.backgroundColor = {0.5f, 0.6f, 0.7f, 0.8f};
				text.backgroundPadding = 0.3f;
			},
			[](const scene::Entity& iEnt) -> void {
				const auto& t = iEnt.getComponent<scene::component::Text>();
				EXPECT_NEAR(t.outlineColor.z(), 0.3f, 0.01f);
				EXPECT_NEAR(t.outlineWidth, 0.07f, 0.001f);
				EXPECT_NEAR(t.backgroundColor.y(), 0.6f, 0.01f);
				EXPECT_NEAR(t.backgroundPadding, 0.3f, 0.001f);
			});
}

TEST(ComponentRoundTrip, UiButton) {
	roundTrip(
			"UiButton",
			[](scene::Entity& iEnt) -> void {
				auto& b = iEnt.addComponent<scene::component::UiButton>();
				b.normalColor = {0.1f, 0.2f, 0.3f, 1.f};
				b.hoverColor = {0.2f, 0.3f, 0.4f, 1.f};
				b.pressedColor = {0.3f, 0.4f, 0.5f, 1.f};
				b.disabledColor = {0.0f, 0.0f, 0.0f, 0.5f};
				b.onClickCallback = "do_thing";
			},
			[](const scene::Entity& iEnt) -> void {
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
			[](scene::Entity& iEnt) -> void {
				auto& s = iEnt.addComponent<scene::component::UiSlider>();
				s.value = 0.6f;
				s.minValue = -1.f;
				s.maxValue = 2.f;
				s.trackColor = {0.1f, 0.1f, 0.1f, 1.f};
				s.fillColor = {0.2f, 0.2f, 0.2f, 1.f};
				s.handleColor = {0.3f, 0.3f, 0.3f, 1.f};
				s.onValueChangedCallback = "value_cb";
			},
			[](const scene::Entity& iEnt) -> void {
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
			[](scene::Entity& iEnt) -> void {
				auto& p = iEnt.addComponent<scene::component::UiProgressBar>();
				p.value = 0.42f;
				p.backgroundColor = {0.05f, 0.05f, 0.05f, 1.f};
				p.fillColor = {0.5f, 0.7f, 0.5f, 1.f};
			},
			[](const scene::Entity& iEnt) -> void {
				const auto& p = iEnt.getComponent<scene::component::UiProgressBar>();
				EXPECT_NEAR(p.value, 0.42f, 0.001f);
				EXPECT_NEAR(p.backgroundColor.x(), 0.05f, 0.01f);
				EXPECT_NEAR(p.fillColor.y(), 0.7f, 0.01f);
			});
}

TEST(ComponentRoundTrip, UiImage) {
	roundTrip(
			"UiImage",
			[](scene::Entity& iEnt) -> void {
				auto& img = iEnt.addComponent<scene::component::UiImage>();
				img.tint = {0.5f, 0.6f, 0.7f, 1.f};
			},
			[](const scene::Entity& iEnt) -> void {
				const auto& img = iEnt.getComponent<scene::component::UiImage>();
				EXPECT_NEAR(img.tint.x(), 0.5f, 0.01f);
				EXPECT_NEAR(img.tint.y(), 0.6f, 0.01f);
			});
}

TEST(ComponentRoundTrip, UiPanel) {
	roundTrip(
			"UiPanel",
			[](scene::Entity& iEnt) -> void {
				auto& p = iEnt.addComponent<scene::component::UiPanel>();
				p.backgroundColor = {0.4f, 0.4f, 0.4f, 0.9f};
				p.borderColor = {1.f, 0.8f, 0.f, 1.f};
				p.borderWidth = 2.5f;
				p.clipChildren = true;
			},
			[](const scene::Entity& iEnt) -> void {
				const auto& p = iEnt.getComponent<scene::component::UiPanel>();
				EXPECT_NEAR(p.backgroundColor.w(), 0.9f, 0.01f);
				EXPECT_NEAR(p.borderColor.y(), 0.8f, 0.01f);
				EXPECT_NEAR(p.borderWidth, 2.5f, 0.01f);
				EXPECT_TRUE(p.clipChildren);
			});
}

TEST(ComponentRoundTrip, Trigger) {
	roundTrip(
			"Trigger",
			[](scene::Entity& iEnt) -> void {
				auto& trig = iEnt.addComponent<scene::component::Trigger>();
				trig.trigger.type = scene::SceneTrigger::TriggerType::Timer;
				trig.trigger.timerDuration = 1.5f;
				trig.trigger.timerRepeating = true;
				trig.trigger.callbackName = "on_tick";
			},
			[](const scene::Entity& iEnt) -> void {
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
			[](scene::Entity& iEnt) -> void {
				auto& trig = iEnt.addComponent<scene::component::Trigger>();
				trig.trigger.type = scene::SceneTrigger::TriggerType::Interaction;
				trig.trigger.interactionRange = 3.5f;
			},
			[](const scene::Entity& iEnt) -> void {
				const auto& t = iEnt.getComponent<scene::component::Trigger>();
				EXPECT_EQ(t.trigger.type, scene::SceneTrigger::TriggerType::Interaction);
				EXPECT_NEAR(t.trigger.interactionRange, 3.5f, 0.001f);
			});
}

TEST(ComponentRoundTrip, TriggerTeleport) {
	roundTrip(
			"TriggerTeleport",
			[](scene::Entity& iEnt) -> void {
				auto& trig = iEnt.addComponent<scene::component::Trigger>();
				trig.trigger.type = scene::SceneTrigger::TriggerType::Teleport;
				trig.trigger.levelName = "level_2";
				trig.trigger.targetName = "spawn_a";
			},
			[](const scene::Entity& iEnt) -> void {
				const auto& t = iEnt.getComponent<scene::component::Trigger>();
				EXPECT_EQ(t.trigger.levelName, "level_2");
				EXPECT_EQ(t.trigger.targetName, "spawn_a");
			});
}

TEST(ComponentRoundTrip, SoundSource) {
	roundTrip(
			"SoundSource",
			[](scene::Entity& iEnt) -> void {
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
			[](const scene::Entity& iEnt) -> void {
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
			[](scene::Entity& iEnt) -> void {
				auto& bg = iEnt.addComponent<scene::component::BackgroundTexture>();
				bg.mode = scene::component::BackgroundTexture::Mode::Skybox;
				bg.type = scene::component::BackgroundTexture::Type::Gradient;
				bg.color = {0.1f, 0.2f, 0.3f, 1.f};
				bg.topColor = {0.4f, 0.5f, 0.6f, 1.f};
			},
			[](const scene::Entity& iEnt) -> void {
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
			[](scene::Entity& iEnt) -> void {
				auto& link = iEnt.addComponent<scene::component::PrefabLink>();
				link.prefabAssetPath = "prefabs/enemy.owlprefab";
				link.syncedVersion = 5;
				link.uuidMapping.push_back({.instanceUuid = 1001, .canonicalUuid = 2001});
				link.uuidMapping.push_back({.instanceUuid = 1002, .canonicalUuid = 2002});
				link.overriddenComponents.emplace_back("2001:Transform");
				link.overriddenComponents.emplace_back("2002:Tag");
			},
			[](const scene::Entity& iEnt) -> void {
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
			[](scene::Entity& iEnt) -> void {
				auto& [body] = iEnt.addComponent<scene::component::PhysicBody>();
				body.type = scene::SceneBody::BodyType::Kinematic;
				body.density = 2.f;
				body.friction = 0.4f;
				body.restitution = 0.7f;
				body.fixedRotation = true;
				body.colliderSize = {2.f, 1.f, 1.f};
			},
			[](const scene::Entity& iEnt) -> void {
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

TEST(UiRect, ComputeBoundsAcrossAllAnchors) {
	using A = scene::component::UiRect::Anchor;
	const scene::component::UiRect::Bounds parent{.position = {10.f, 20.f}, .size = {200.f, 100.f}};
	scene::component::UiRect rect;
	rect.size = {0.f, 0.f};
	struct Expected {
		A anchor;
		math::vec2 point;
	};
	for (const auto& [anchor, point]:
		 {Expected{A::TopLeft, {10.f, 20.f}}, Expected{A::TopCenter, {110.f, 20.f}},
		  Expected{A::TopRight, {210.f, 20.f}}, Expected{A::MiddleLeft, {10.f, 70.f}},
		  Expected{A::Center, {110.f, 70.f}}, Expected{A::MiddleRight, {210.f, 70.f}},
		  Expected{A::BottomLeft, {10.f, 120.f}}, Expected{A::BottomCenter, {110.f, 120.f}},
		  Expected{A::BottomRight, {210.f, 120.f}}}) {
		rect.setAnchor(anchor);
		EXPECT_EQ(rect.getAnchor(), anchor);
		const auto bounds = rect.computeBounds(parent);
		EXPECT_NEAR(bounds.position.x(), point.x(), 0.001f);
		EXPECT_NEAR(bounds.position.y(), point.y(), 0.001f);
	}
}

TEST(UiRect, ComputeBoundsWithPivotAndOffset) {
	scene::component::UiRect rect;
	rect.setAnchor(scene::component::UiRect::Anchor::TopLeft);
	rect.pivot = {1.f, 1.f};
	rect.size = {40.f, 20.f};
	rect.anchorOffset = {50.f, 30.f};
	const auto bounds = rect.computeBounds({.position = {0.f, 0.f}, .size = {100.f, 100.f}});
	// The bottom-right corner (pivot 1, 1) sits on the anchor plus the offset.
	EXPECT_NEAR(bounds.position.x(), 10.f, 0.001f);
	EXPECT_NEAR(bounds.position.y(), 10.f, 0.001f);
	EXPECT_NEAR(bounds.size.x(), 40.f, 0.001f);
	EXPECT_NEAR(bounds.size.y(), 20.f, 0.001f);
}

TEST(UiRect, StretchedAnchorsFollowTheParent) {
	scene::component::UiRect rect;
	rect.anchorMin = {0.f, 0.f};
	rect.anchorMax = {1.f, 0.f};
	rect.pivot = {0.5f, 0.f};
	rect.size = {-20.f, 30.f};
	rect.anchorOffset = {0.f, 5.f};
	EXPECT_FALSE(rect.getAnchor().has_value());
	for (const float width: {200.f, 640.f}) {
		const auto bounds = rect.computeBounds({.position = {0.f, 0.f}, .size = {width, 100.f}});
		EXPECT_NEAR(bounds.position.x(), 10.f, 0.001f);
		EXPECT_NEAR(bounds.position.y(), 5.f, 0.001f);
		EXPECT_NEAR(bounds.size.x(), width - 20.f, 0.001f);
		EXPECT_NEAR(bounds.size.y(), 30.f, 0.001f);
	}
	const scene::component::UiRect::Bounds square{.position = {0.f, 0.f}, .size = {10.f, 10.f}};
	EXPECT_TRUE(square.contains({10.f, 0.f}));
	EXPECT_FALSE(square.contains({10.1f, 0.f}));
}

TEST(ComponentRoundTrip, VoxelWorld) {
	roundTrip(
			"VoxelWorld",
			[](scene::Entity& iEnt) -> void {
				auto& vw = iEnt.addComponent<scene::component::VoxelWorld>();
				data::voxel::BlockType stone;
				stone.name = "stone";
				stone.renderKind = data::voxel::BlockRenderKind::Opaque;
				stone.solid = true;
				stone.setAllFaces(1);
				stone.surface.roughness = 0.8f;
				stone.surface.emissive = 1.5f;
				const data::voxel::BlockId stoneId = vw.registry.registerBlock(stone);
				vw.world.setBlock(math::vec3i{1, 2, 3}, stoneId);
				vw.world.setBlock(math::vec3i{-1, 0, 0}, stoneId);
				vw.tilesetPath = "tilesets/world_platform.owltileset";
				vw.sunDirection = math::vec3{0.1f, -0.9f, 0.2f};
				vw.ambient = math::vec3{0.2f, 0.3f, 0.4f};
				vw.ambientOcclusion = false;
				vw.editorStreamRadius = 12;
				vw.editorStreamHeight = 5;
			},
			[](const scene::Entity& iEnt) -> void {
				ASSERT_TRUE(iEnt.hasComponent<scene::component::VoxelWorld>());
				const auto& vw = iEnt.getComponent<scene::component::VoxelWorld>();
				ASSERT_EQ(vw.registry.count(), 2u);// air + stone
				EXPECT_EQ(vw.registry.get(1).name, "stone");
				EXPECT_FLOAT_EQ(vw.registry.get(1).surface.roughness, 0.8f);
				EXPECT_FLOAT_EQ(vw.registry.get(1).surface.emissive, 1.5f);
				EXPECT_EQ(vw.world.getBlock(math::vec3i{1, 2, 3}), 1u);
				EXPECT_EQ(vw.world.getBlock(math::vec3i{-1, 0, 0}), 1u);
				EXPECT_EQ(vw.world.getBlock(math::vec3i{5, 5, 5}), data::voxel::g_AirBlock);
				EXPECT_EQ(vw.tilesetPath.generic_string(), "tilesets/world_platform.owltileset");
				EXPECT_FLOAT_EQ(vw.sunDirection.y(), -0.9f);
				EXPECT_FLOAT_EQ(vw.ambient.z(), 0.4f);
				EXPECT_FALSE(vw.ambientOcclusion);
				EXPECT_EQ(vw.editorStreamRadius, 12);
				EXPECT_EQ(vw.editorStreamHeight, 5);
			});
}

TEST(ComponentRoundTrip, CameraPostProcess) {
	roundTrip(
			"CameraPostProcess",
			[](scene::Entity& iEnt) -> void {
				auto& cam = iEnt.addComponent<scene::component::Camera>();
				renderer::PostProcessSettings settings;
				settings.exposure = 1.5f;
				settings.bloom = true;
				settings.bloomThreshold = 0.8f;
				settings.bloomKnee = 0.25f;
				settings.bloomIntensity = 0.4f;
				settings.bloomScatter = 0.9f;
				settings.vignette = true;
				settings.vignetteIntensity = 0.6f;
				settings.vignetteSmoothness = 0.2f;
				settings.vignetteColor = {0.1f, 0.2f, 0.3f};
				settings.lut = "luts/warm.cube";
				settings.lutContribution = 0.5f;
				settings.chromaticAberration = true;
				settings.chromaticAberrationIntensity = 0.7f;
				settings.filmGrain = true;
				settings.filmGrainIntensity = 0.4f;
				settings.filmGrainSize = 2.5f;
				settings.depthOfField = true;
				settings.focusDistance = 7.5f;
				settings.focusRange = 2.5f;
				settings.bokehRadius = 12.f;
				settings.motionBlur = true;
				settings.motionBlurIntensity = 0.75f;
				cam.camera.setPostProcess(settings);
			},
			[](const scene::Entity& iEnt) -> void {
				ASSERT_TRUE(iEnt.hasComponent<scene::component::Camera>());
				const auto& settings = iEnt.getComponent<scene::component::Camera>().camera.getPostProcess();
				EXPECT_FLOAT_EQ(settings.exposure, 1.5f);
				EXPECT_TRUE(settings.bloom);
				EXPECT_FLOAT_EQ(settings.bloomThreshold, 0.8f);
				EXPECT_FLOAT_EQ(settings.bloomKnee, 0.25f);
				EXPECT_FLOAT_EQ(settings.bloomIntensity, 0.4f);
				EXPECT_FLOAT_EQ(settings.bloomScatter, 0.9f);
				EXPECT_TRUE(settings.vignette);
				EXPECT_FLOAT_EQ(settings.vignetteIntensity, 0.6f);
				EXPECT_FLOAT_EQ(settings.vignetteSmoothness, 0.2f);
				EXPECT_FLOAT_EQ(settings.vignetteColor.y(), 0.2f);
				EXPECT_EQ(settings.lut, "luts/warm.cube");
				EXPECT_FLOAT_EQ(settings.lutContribution, 0.5f);
				EXPECT_TRUE(settings.chromaticAberration);
				EXPECT_FLOAT_EQ(settings.chromaticAberrationIntensity, 0.7f);
				EXPECT_TRUE(settings.filmGrain);
				EXPECT_FLOAT_EQ(settings.filmGrainIntensity, 0.4f);
				EXPECT_FLOAT_EQ(settings.filmGrainSize, 2.5f);
				EXPECT_TRUE(settings.depthOfField);
				EXPECT_FLOAT_EQ(settings.focusDistance, 7.5f);
				EXPECT_FLOAT_EQ(settings.focusRange, 2.5f);
				EXPECT_FLOAT_EQ(settings.bokehRadius, 12.f);
				EXPECT_TRUE(settings.motionBlur);
				EXPECT_FLOAT_EQ(settings.motionBlurIntensity, 0.75f);
			});
}

TEST(ComponentRoundTrip, CameraWithoutPostProcessKeepsTheDefaults) {
	roundTrip(
			"CameraDefaultPostProcess",
			[](scene::Entity& iEnt) -> void { iEnt.addComponent<scene::component::Camera>(); },
			[](const scene::Entity& iEnt) -> void {
				const auto& settings = iEnt.getComponent<scene::component::Camera>().camera.getPostProcess();
				const renderer::PostProcessSettings defaults;
				EXPECT_FLOAT_EQ(settings.exposure, defaults.exposure);
				EXPECT_EQ(settings.bloom, defaults.bloom);
				EXPECT_TRUE(settings.lut.empty());
			});
}

TEST(ComponentRoundTrip, Environment) {
	roundTrip(
			"Environment",
			[](scene::Entity& iEnt) -> void {
				auto& env = iEnt.addComponent<scene::component::Environment>();
				env.hdri = "hdri/studio.hdr";
				env.intensity = 1.75f;
				env.rotation = 0.5f;
				env.showSky = false;
				env.ambient = math::vec3{0.1f, 0.2f, 0.3f};
			},
			[](const scene::Entity& iEnt) -> void {
				ASSERT_TRUE(iEnt.hasComponent<scene::component::Environment>());
				const auto& env = iEnt.getComponent<scene::component::Environment>();
				EXPECT_EQ(env.hdri.generic_string(), "hdri/studio.hdr");
				EXPECT_FLOAT_EQ(env.intensity, 1.75f);
				EXPECT_FLOAT_EQ(env.rotation, 0.5f);
				EXPECT_FALSE(env.showSky);
				EXPECT_FLOAT_EQ(env.ambient.z(), 0.3f);
			});
}

TEST(ComponentRoundTrip, FlyCamera) {
	roundTrip(
			"FlyCamera",
			[](scene::Entity& iEnt) -> void {
				auto& fly = iEnt.addComponent<scene::component::FlyCamera>();
				fly.moveSpeed = 12.5f;
				fly.lookSpeed = 2.25f;
			},
			[](const scene::Entity& iEnt) -> void {
				ASSERT_TRUE(iEnt.hasComponent<scene::component::FlyCamera>());
				const auto& fly = iEnt.getComponent<scene::component::FlyCamera>();
				EXPECT_FLOAT_EQ(fly.moveSpeed, 12.5f);
				EXPECT_FLOAT_EQ(fly.lookSpeed, 2.25f);
			});
}

TEST(ComponentRoundTrip, LightFlicker) {
	roundTrip(
			"LightFlicker",
			[](scene::Entity& iEnt) -> void {
				auto& flicker = iEnt.addComponent<scene::component::LightFlicker>();
				flicker.style = scene::component::LightFlicker::Style::Candle;
				flicker.enabled = false;
				flicker.amplitude = 0.35f;
				flicker.speed = 1.5f;
				flicker.seed = 913.f;
				flicker.colorShift = {0.f, 0.06f, 0.04f};
				flicker.positionJitter = {0.004f, 0.f, 0.004f};
				flicker.dipRate = 0.55f;
				flicker.dipDepth = 0.4f;
				flicker.follow = core::UUID{77};
				flicker.scaleResponse = {0.1f, 0.35f, 0.1f};
			},
			[](const scene::Entity& iEnt) -> void {
				ASSERT_TRUE(iEnt.hasComponent<scene::component::LightFlicker>());
				const auto& flicker = iEnt.getComponent<scene::component::LightFlicker>();
				EXPECT_EQ(flicker.style, scene::component::LightFlicker::Style::Candle);
				EXPECT_FALSE(flicker.enabled);
				EXPECT_FLOAT_EQ(flicker.amplitude, 0.35f);
				EXPECT_FLOAT_EQ(flicker.speed, 1.5f);
				EXPECT_FLOAT_EQ(flicker.seed, 913.f);
				EXPECT_FLOAT_EQ(flicker.colorShift.y(), 0.06f);
				EXPECT_FLOAT_EQ(flicker.positionJitter.z(), 0.004f);
				EXPECT_FLOAT_EQ(flicker.dipRate, 0.55f);
				EXPECT_FLOAT_EQ(flicker.dipDepth, 0.4f);
				EXPECT_EQ(flicker.follow, core::UUID{77});
				EXPECT_FLOAT_EQ(flicker.scaleResponse.y(), 0.35f);
			});
}

TEST(ComponentRoundTrip, Oscillator) {
	roundTrip(
			"Oscillator",
			[](scene::Entity& iEnt) -> void {
				auto& oscillator = iEnt.addComponent<scene::component::Oscillator>();
				oscillator.enabled = false;
				oscillator.offset = {0.f, 3.f, 0.5f};
				oscillator.period = 6.f;
				oscillator.phase = 0.25f;
				oscillator.easing = scene::component::Oscillator::Easing::Smooth;
			},
			[](const scene::Entity& iEnt) -> void {
				ASSERT_TRUE(iEnt.hasComponent<scene::component::Oscillator>());
				const auto& oscillator = iEnt.getComponent<scene::component::Oscillator>();
				EXPECT_FALSE(oscillator.enabled);
				EXPECT_FLOAT_EQ(oscillator.offset.y(), 3.f);
				EXPECT_FLOAT_EQ(oscillator.offset.z(), 0.5f);
				EXPECT_FLOAT_EQ(oscillator.period, 6.f);
				EXPECT_FLOAT_EQ(oscillator.phase, 0.25f);
				EXPECT_EQ(oscillator.easing, scene::component::Oscillator::Easing::Smooth);
			});
}

TEST(ComponentRoundTrip, Patrol) {
	roundTrip(
			"Patrol",
			[](scene::Entity& iEnt) -> void {
				auto& patrol = iEnt.addComponent<scene::component::Patrol>();
				patrol.waypoints = {{1.f, 2.f, 0.f}, {3.f, 4.f, 0.f}, {5.f, 6.f, 0.f}};
				patrol.mode = scene::component::Patrol::Mode::Loop;
				patrol.speed = 2.5f;
				patrol.waitTime = 0.75f;
				patrol.wanderRadius = 3.f;
				patrol.seed = 9.f;
				patrol.faceDirection = true;
				patrol.hopHeight = 0.06f;
				patrol.hopRate = 7.f;
			},
			[](const scene::Entity& iEnt) -> void {
				ASSERT_TRUE(iEnt.hasComponent<scene::component::Patrol>());
				const auto& patrol = iEnt.getComponent<scene::component::Patrol>();
				ASSERT_EQ(patrol.waypoints.size(), 3u);
				EXPECT_FLOAT_EQ(patrol.waypoints[1].y(), 4.f);
				EXPECT_EQ(patrol.mode, scene::component::Patrol::Mode::Loop);
				EXPECT_FLOAT_EQ(patrol.speed, 2.5f);
				EXPECT_FLOAT_EQ(patrol.waitTime, 0.75f);
				EXPECT_FLOAT_EQ(patrol.wanderRadius, 3.f);
				EXPECT_FLOAT_EQ(patrol.seed, 9.f);
				EXPECT_TRUE(patrol.faceDirection);
				EXPECT_FLOAT_EQ(patrol.hopHeight, 0.06f);
				EXPECT_FLOAT_EQ(patrol.hopRate, 7.f);
			});
}

TEST(ComponentRoundTrip, WorldPosition) {
	roundTrip(
			"WorldPosition",
			[](scene::Entity& iEnt) -> void {
				iEnt.addComponent<scene::component::WorldPosition>().position = {1.5e11 + 0.25, -7.0e8, 3.0};
			},
			[](const scene::Entity& iEnt) -> void {
				ASSERT_TRUE(iEnt.hasComponent<scene::component::WorldPosition>());
				const auto& world = iEnt.getComponent<scene::component::WorldPosition>();
				EXPECT_DOUBLE_EQ(world.position.x(), 1.5e11 + 0.25);
				EXPECT_DOUBLE_EQ(world.position.y(), -7.0e8);
				EXPECT_DOUBLE_EQ(world.position.z(), 3.0);
				EXPECT_FALSE(world.m_synced);
			});
}

TEST(ComponentRoundTrip, Drift) {
	roundTrip(
			"Drift",
			[](scene::Entity& iEnt) -> void {
				auto& drift = iEnt.addComponent<scene::component::Drift>();
				drift.enabled = false;
				drift.velocity = {0.3f, 0.1f, 0.f};
				drift.range = {20.f, 5.f, 0.f};
				drift.cameraFollow = 0.6f;
			},
			[](const scene::Entity& iEnt) -> void {
				ASSERT_TRUE(iEnt.hasComponent<scene::component::Drift>());
				const auto& drift = iEnt.getComponent<scene::component::Drift>();
				EXPECT_FALSE(drift.enabled);
				EXPECT_FLOAT_EQ(drift.velocity.y(), 0.1f);
				EXPECT_FLOAT_EQ(drift.range.x(), 20.f);
				EXPECT_FLOAT_EQ(drift.cameraFollow, 0.6f);
			});
}

TEST(ComponentRoundTrip, VoxelPlayer) {
	roundTrip(
			"VoxelPlayer",
			[](scene::Entity& iEnt) -> void {
				auto& vp = iEnt.addComponent<scene::component::VoxelPlayer>();
				vp.walkSpeed = 6.5f;
				vp.runSpeed = 11.f;
				vp.jumpSpeed = 9.25f;
				vp.gravity = 22.f;
				vp.lookSpeed = 2.f;
				vp.halfExtents = {0.35f, 0.95f, 0.35f};
				vp.reach = 7.5f;
				vp.placeBlock = 4;
				vp.captureCursor = true;
			},
			[](const scene::Entity& iEnt) -> void {
				ASSERT_TRUE(iEnt.hasComponent<scene::component::VoxelPlayer>());
				const auto& vp = iEnt.getComponent<scene::component::VoxelPlayer>();
				EXPECT_FLOAT_EQ(vp.walkSpeed, 6.5f);
				EXPECT_FLOAT_EQ(vp.runSpeed, 11.f);
				EXPECT_FLOAT_EQ(vp.jumpSpeed, 9.25f);
				EXPECT_FLOAT_EQ(vp.gravity, 22.f);
				EXPECT_FLOAT_EQ(vp.lookSpeed, 2.f);
				EXPECT_NEAR(vp.halfExtents.y(), 0.95f, 0.001f);
				EXPECT_FLOAT_EQ(vp.reach, 7.5f);
				EXPECT_EQ(vp.placeBlock, 4u);
				EXPECT_TRUE(vp.captureCursor);
			});
}

TEST(ComponentRoundTrip, IsometricObject) {
	roundTrip(
			"IsometricObject",
			[](scene::Entity& iEnt) -> void {
				auto& object = iEnt.addComponent<scene::component::IsometricObject>();
				object.atlasPath = "textures/iso/buildings.json";
				object.building = "townhouse";
				object.color = {0.5f, 0.25f, 1.f, 1.f};
			},
			[](const scene::Entity& iEnt) -> void {
				const auto& object = iEnt.getComponent<scene::component::IsometricObject>();
				EXPECT_EQ(object.atlasPath.generic_string(), "textures/iso/buildings.json");
				EXPECT_EQ(object.building, "townhouse");
				EXPECT_NEAR(object.color.x(), 0.5f, 0.001f);
				EXPECT_NEAR(object.color.y(), 0.25f, 0.001f);
				EXPECT_EQ(object.atlas, nullptr);
			});
}

TEST(ComponentRoundTrip, FollowTarget) {
	roundTrip(
			"FollowTarget",
			[](scene::Entity& iEnt) -> void {
				auto& follow = iEnt.addComponent<scene::component::FollowTarget>();
				follow.enabled = false;
				follow.target = core::UUID{77};
				follow.offset = {1.f, 25.f, -3.f};
				follow.followYaw = false;
			},
			[](const scene::Entity& iEnt) -> void {
				const auto& follow = iEnt.getComponent<scene::component::FollowTarget>();
				EXPECT_FALSE(follow.enabled);
				EXPECT_EQ(static_cast<uint64_t>(follow.target), 77u);
				EXPECT_NEAR(follow.offset.y(), 25.f, 1e-6f);
				EXPECT_NEAR(follow.offset.z(), -3.f, 1e-6f);
				EXPECT_FALSE(follow.followYaw);
			});
}

TEST(ComponentRoundTrip, RenderTarget) {
	roundTrip(
			"RenderTarget",
			[](scene::Entity& iEnt) -> void {
				auto& target = iEnt.addComponent<scene::component::RenderTarget>();
				target.enabled = false;
				target.targetName = "mirror";
				target.size = {320, 180};
				target.clearColor = {0.1f, 0.2f, 0.3f, 1.f};
				target.hiddenLayers = {"marks", "overlay"};
				iEnt.addComponent<scene::component::UiImage>().renderTarget = "mirror";
			},
			[](const scene::Entity& iEnt) -> void {
				const auto& target = iEnt.getComponent<scene::component::RenderTarget>();
				EXPECT_FALSE(target.enabled);
				EXPECT_EQ(target.targetName, "mirror");
				EXPECT_EQ(target.size.x(), 320u);
				EXPECT_EQ(target.size.y(), 180u);
				EXPECT_NEAR(target.clearColor.z(), 0.3f, 1e-6f);
				EXPECT_EQ(target.hiddenLayers, (std::vector<std::string>{"marks", "overlay"}));
				EXPECT_FALSE(target.drawsLayer("marks"));
				EXPECT_TRUE(target.drawsLayer("world"));
				EXPECT_EQ(iEnt.getComponent<scene::component::UiImage>().renderTarget, "mirror");
			});
}

TEST(RenderTarget, TheSceneDrawsEachEnabledTargetIntoItsImage) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeApp("RenderTargetDraw");
	const auto sc = mkShared<scene::Scene>();
	auto main = sc->createEntity("Main");
	main.addComponent<scene::component::Camera>().primary = true;
	auto top = sc->createEntity("Top");
	top.addComponent<scene::component::Camera>().primary = false;
	auto& target = top.addComponent<scene::component::RenderTarget>();
	target.size = {128, 64};
	auto spare = sc->createEntity("Spare");
	spare.addComponent<scene::component::Camera>().primary = false;
	auto& off = spare.addComponent<scene::component::RenderTarget>();
	off.targetName = "off";
	off.enabled = false;
	EXPECT_EQ(sc->getRenderTarget("minimap"), nullptr);
	sc->onViewportResize({640, 360});
	sc->onStartRuntime();
	sc->onRenderRuntime();
	const auto image = sc->getRenderTarget("minimap");
	ASSERT_NE(image, nullptr);
	EXPECT_EQ(image->getSpecification().size, (math::vec2ui{128, 64}));
	EXPECT_EQ(sc->getRenderTarget("off"), nullptr);
	EXPECT_EQ(sc->getRenderTarget("unknown"), nullptr);
	target.size = {64, 64};
	sc->onRenderRuntime();
	EXPECT_EQ(sc->getRenderTarget("minimap")->getSpecification().size, (math::vec2ui{64, 64}));
	sc->onEndRuntime();
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

TEST(ComponentRoundTrip, ProceduralSkyAndDayNightCycle) {
	roundTrip(
			"ProceduralSky",
			[](scene::Entity& iEnt) -> void {
				auto& sky = iEnt.addComponent<scene::component::ProceduralSky>();
				sky.enabled = false;
				sky.turbidity = 6.f;
				sky.nightBrightness = 0.03f;
				sky.stars = 0.5f;
				sky.discSize = 3.f;
				sky.resolution = 256;
				auto& cycle = iEnt.addComponent<scene::component::DayNightCycle>();
				cycle.timeOfDay = 19.5f;
				cycle.dayLength = 90.f;
				cycle.azimuth = 45.f;
				cycle.maxElevation = 30.f;
				cycle.sun = core::UUID{99};
				cycle.moonIlluminance = 1200.f;
				cycle.nightExposure = 2.f;
			},
			[](const scene::Entity& iEnt) -> void {
				const auto& sky = iEnt.getComponent<scene::component::ProceduralSky>();
				EXPECT_FALSE(sky.enabled);
				EXPECT_NEAR(sky.turbidity, 6.f, 1e-6f);
				EXPECT_NEAR(sky.nightBrightness, 0.03f, 1e-6f);
				EXPECT_NEAR(sky.stars, 0.5f, 1e-6f);
				EXPECT_NEAR(sky.discSize, 3.f, 1e-6f);
				EXPECT_EQ(sky.resolution, 256u);
				const auto& cycle = iEnt.getComponent<scene::component::DayNightCycle>();
				EXPECT_NEAR(cycle.timeOfDay, 19.5f, 1e-5f);
				EXPECT_NEAR(cycle.dayLength, 90.f, 1e-5f);
				EXPECT_NEAR(cycle.azimuth, 45.f, 1e-5f);
				EXPECT_NEAR(cycle.maxElevation, 30.f, 1e-5f);
				EXPECT_EQ(static_cast<uint64_t>(cycle.sun), 99u);
				EXPECT_NEAR(cycle.moonIlluminance, 1200.f, 1e-3f);
				EXPECT_NEAR(cycle.nightExposure, 2.f, 1e-6f);
			});
}

TEST(Outline, ASubtreeHoldsItsRootAndItsDescendantsOnly) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeApp("OutlineSubtree");
	const auto sc = mkShared<scene::Scene>();
	auto root = sc->createEntity("Root");
	auto child = sc->createEntity("Child");
	auto grandChild = sc->createEntity("GrandChild");
	auto other = sc->createEntity("Other");
	sc->setParent(child, root);
	sc->setParent(grandChild, child);
	const auto rootHandle = static_cast<entt::entity>(root);
	EXPECT_TRUE(sc->isInSubtree(static_cast<entt::entity>(root), rootHandle));
	EXPECT_TRUE(sc->isInSubtree(static_cast<entt::entity>(grandChild), rootHandle));
	EXPECT_FALSE(sc->isInSubtree(static_cast<entt::entity>(other), rootHandle));
	EXPECT_FALSE(sc->isInSubtree(rootHandle, static_cast<entt::entity>(child)));
	EXPECT_FALSE(sc->getOutline().has_value());
	sc->setOutline(scene::Scene::Outline{.root = child.getUUID()});
	ASSERT_TRUE(sc->getOutline().has_value());
	EXPECT_EQ(sc->getOutline()->root, child.getUUID());
	auto main = sc->createEntity("Main");
	main.addComponent<scene::component::Camera>().primary = true;
	sc->onViewportResize({320, 180});
	sc->onStartRuntime();
	EXPECT_NO_THROW(sc->onRenderRuntime());
	sc->setOutline(std::nullopt);
	EXPECT_FALSE(sc->getOutline().has_value());
	sc->onEndRuntime();
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}
