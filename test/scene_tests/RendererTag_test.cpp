/**
 * @file RendererTag_test.cpp
 * @author Silmaen
 * @date 30/04/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Application.h>
#include <renderer/RenderStack.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

TEST(RendererTagComponent, keyAndName) {
	EXPECT_STREQ(owl::scene::component::RendererTag::key(), "RendererTag");
	EXPECT_STREQ(owl::scene::component::RendererTag::name(), "Renderer Tag");
}

TEST(RendererTagComponent, addAndAccess) {
	const auto scene = owl::mkShared<owl::scene::Scene>();
	auto entity = scene->createEntityWithUUID(42, "Tagged");
	auto& tag = entity.addComponent<owl::scene::component::RendererTag>();
	tag.rendererName = "hud";
	EXPECT_TRUE(entity.hasComponent<owl::scene::component::RendererTag>());
	EXPECT_EQ(entity.getComponent<owl::scene::component::RendererTag>().rendererName, "hud");
}

TEST(RendererTagComponent, serializeRoundTrip) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	auto app = owl::mkShared<owl::core::Application>(owl::core::AppParams{.args = nullptr,
																		   .frameLogFrequency = 0,
																		   .name = "rendererTagRoundTrip",
																		   .assetsPattern = "",
																		   .icon = "",
																		   .width = 0,
																		   .height = 0,
																		   .argCount = 0,
																		   .renderer = owl::renderer::gpu::RenderAPI::Type::Null,
																		   .hasGui = false,
																		   .useDebugging = false,
																		   .isDummy = true});

	const auto sc = owl::mkShared<owl::scene::Scene>();
	auto entity = sc->createEntityWithUUID(101, "TaggedEntity");
	auto& tag = entity.addComponent<owl::scene::component::RendererTag>();
	tag.rendererName = "world";

	const owl::scene::SceneSerializer saver(sc);
	const auto fs = std::filesystem::temp_directory_path() / "rendererTagRoundTrip.yml";
	saver.serialize(fs);
	ASSERT_TRUE(exists(fs));

	const auto sc2 = owl::mkShared<owl::scene::Scene>();
	const owl::scene::SceneSerializer loader(sc2);
	EXPECT_TRUE(loader.deserialize(fs));

	const auto entities = sc2->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	ASSERT_TRUE(entities[0].hasComponent<owl::scene::component::RendererTag>());
	EXPECT_EQ(entities[0].getComponent<owl::scene::component::RendererTag>().rendererName, "world");

	std::filesystem::remove(fs);
	owl::core::Application::invalidate();
	app.reset();
	owl::core::Log::invalidate();
}

TEST(RendererTagComponent, sceneEnabledRenderersRoundTrip) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	auto app = owl::mkShared<owl::core::Application>(owl::core::AppParams{.args = nullptr,
																		   .frameLogFrequency = 0,
																		   .name = "enabledRenderersRoundTrip",
																		   .assetsPattern = "",
																		   .icon = "",
																		   .width = 0,
																		   .height = 0,
																		   .argCount = 0,
																		   .renderer = owl::renderer::gpu::RenderAPI::Type::Null,
																		   .hasGui = false,
																		   .useDebugging = false,
																		   .isDummy = true});

	const auto sc = owl::mkShared<owl::scene::Scene>();
	auto& enabled = sc->getEnabledRenderers();
	enabled.entries.push_back({.name = "world", .enabled = true, .overrides = {}});
	YAML::Node hudOverride;
	hudOverride["Hidden"] = false;
	enabled.entries.push_back({.name = "hud", .enabled = true, .overrides = hudOverride});

	const owl::scene::SceneSerializer saver(sc);
	const auto fs = std::filesystem::temp_directory_path() / "enabledRenderersRoundTrip.yml";
	saver.serialize(fs);

	const auto sc2 = owl::mkShared<owl::scene::Scene>();
	const owl::scene::SceneSerializer loader(sc2);
	ASSERT_TRUE(loader.deserialize(fs));

	const auto& cfg = sc2->getEnabledRenderers();
	ASSERT_EQ(cfg.entries.size(), 2u);
	EXPECT_EQ(cfg.entries[0].name, "world");
	EXPECT_TRUE(cfg.entries[0].enabled);
	EXPECT_EQ(cfg.entries[1].name, "hud");
	ASSERT_TRUE(cfg.entries[1].overrides);
	EXPECT_EQ(cfg.entries[1].overrides["Hidden"].as<bool>(), false);

	std::filesystem::remove(fs);
	owl::core::Application::invalidate();
	app.reset();
	owl::core::Log::invalidate();
}
