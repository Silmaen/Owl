/**
 * @file RenderStack_test.cpp
 * @author Silmaen
 * @date 30/04/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/CameraOrtho.h>
#include <renderer/RenderLayerFactory.h>
#include <renderer/RenderStack.h>

namespace {

class TrackingLayer final : public owl::renderer::RenderLayer {
public:
	explicit TrackingLayer(std::string iName) : m_name{std::move(iName)} {}
	~TrackingLayer() override = default;

	[[nodiscard]] auto getName() const -> const std::string& override { return m_name; }
	[[nodiscard]] auto getTypeKey() const -> const char* override { return "Tracking"; }

	void onBeginFrame(const owl::renderer::Camera&) override { ++beginCount; }
	void onRender(owl::scene::Scene&) override { ++renderCount; }
	void onEndFrame() override { ++endCount; }
	void applyConfig(const YAML::Node& iConfig) override { lastConfig = YAML::Clone(iConfig); }

	int beginCount = 0;
	int renderCount = 0;
	int endCount = 0;
	YAML::Node lastConfig;

private:
	std::string m_name;
};

void registerTrackingFactory() {
	owl::renderer::RenderLayerFactory::registerType(
			"Tracking", [](const std::string& iName) -> owl::shared<owl::renderer::RenderLayer> {
				return owl::mkShared<TrackingLayer>(iName);
			});
}

}// namespace

using owl::renderer::EnabledRenderersConfig;
using owl::renderer::RendererStackConfig;
using owl::renderer::RendererStackEntry;
using owl::renderer::RenderLayerFactory;
using owl::renderer::RenderStack;

TEST(RendererStackConfig, makeDefault) {
	const auto cfg = RendererStackConfig::makeDefault();
	ASSERT_EQ(cfg.entries.size(), 1u);
	EXPECT_EQ(cfg.entries[0].typeKey, "Renderer2D");
	EXPECT_EQ(cfg.entries[0].name, "default");
}

TEST(RendererStackConfig, yamlRoundTrip) {
	RendererStackConfig cfg;
	cfg.entries.push_back({.typeKey = "Renderer2D", .name = "world", .defaultConfig = YAML::Node{}});
	YAML::Node hudCfg;
	hudCfg["Order"] = 10;
	cfg.entries.push_back({.typeKey = "Renderer2D", .name = "hud", .defaultConfig = hudCfg});

	const auto yaml = cfg.toYaml();
	const auto round = RendererStackConfig::fromYaml(yaml);

	ASSERT_EQ(round.entries.size(), 2u);
	EXPECT_EQ(round.entries[0].typeKey, "Renderer2D");
	EXPECT_EQ(round.entries[0].name, "world");
	EXPECT_EQ(round.entries[1].name, "hud");
	ASSERT_TRUE(round.entries[1].defaultConfig);
	EXPECT_EQ(round.entries[1].defaultConfig["Order"].as<int>(), 10);
}

TEST(RendererStackConfig, fromYamlSkipsInvalid) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	YAML::Node node{YAML::NodeType::Sequence};
	YAML::Node ok;
	ok["Type"] = "Renderer2D";
	ok["Name"] = "world";
	node.push_back(ok);
	YAML::Node missingType;
	missingType["Name"] = "broken";
	node.push_back(missingType);
	YAML::Node duplicate;
	duplicate["Type"] = "Renderer2D";
	duplicate["Name"] = "world";
	node.push_back(duplicate);

	const auto cfg = RendererStackConfig::fromYaml(node);
	ASSERT_EQ(cfg.entries.size(), 1u);
	EXPECT_EQ(cfg.entries[0].name, "world");
	owl::core::Log::invalidate();
}

TEST(RendererStackConfig, find) {
	RendererStackConfig cfg;
	cfg.entries.push_back({.typeKey = "Renderer2D", .name = "world", .defaultConfig = YAML::Node{}});
	cfg.entries.push_back({.typeKey = "Renderer2D", .name = "hud", .defaultConfig = YAML::Node{}});

	const auto* hud = cfg.find("hud");
	ASSERT_NE(hud, nullptr);
	EXPECT_EQ(hud->name, "hud");
	EXPECT_EQ(cfg.find("missing"), nullptr);
}

TEST(EnabledRenderersConfig, yamlRoundTrip) {
	EnabledRenderersConfig cfg;
	YAML::Node overrides;
	overrides["Fov"] = 90;
	cfg.entries.push_back({.name = "world", .enabled = true, .overrides = overrides});
	cfg.entries.push_back({.name = "hud", .enabled = false, .overrides = YAML::Node{}});

	const auto yaml = cfg.toYaml();
	const auto round = EnabledRenderersConfig::fromYaml(yaml);

	ASSERT_EQ(round.entries.size(), 2u);
	EXPECT_EQ(round.entries[0].name, "world");
	EXPECT_TRUE(round.entries[0].enabled);
	ASSERT_TRUE(round.entries[0].overrides);
	EXPECT_EQ(round.entries[0].overrides["Fov"].as<int>(), 90);
	EXPECT_FALSE(round.entries[1].enabled);
}

TEST(EnabledRenderersConfig, isEmptyOnDefault) {
	const EnabledRenderersConfig cfg;
	EXPECT_TRUE(cfg.isEmpty());
}

TEST(RenderStack, buildFromEmptyProjectFallsBack) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderLayerFactory::clear();
	registerTrackingFactory();
	// Map default Renderer2D type to Tracking so we can observe construction.
	RenderLayerFactory::registerType("Renderer2D",
									 [](const std::string& iName) -> owl::shared<owl::renderer::RenderLayer> {
										 return owl::mkShared<TrackingLayer>(iName);
									 });

	const auto stack = RenderStack::buildFromConfig(RendererStackConfig{}, EnabledRenderersConfig{});
	ASSERT_FALSE(stack.isEmpty());
	ASSERT_EQ(stack.getLayers().size(), 1u);
	EXPECT_EQ(stack.getLayers()[0]->getName(), "default");

	RenderLayerFactory::clear();
	owl::core::Log::invalidate();
}

TEST(RenderStack, buildAppliesSceneOverrides) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderLayerFactory::clear();
	registerTrackingFactory();

	RendererStackConfig project;
	YAML::Node defaultCfg;
	defaultCfg["Fov"] = 60;
	defaultCfg["Range"] = 100;
	project.entries.push_back({.typeKey = "Tracking", .name = "world", .defaultConfig = defaultCfg});
	project.entries.push_back({.typeKey = "Tracking", .name = "hud", .defaultConfig = YAML::Node{}});

	EnabledRenderersConfig scene;
	YAML::Node overrides;
	overrides["Fov"] = 90;
	scene.entries.push_back({.name = "world", .enabled = true, .overrides = overrides});
	scene.entries.push_back({.name = "hud", .enabled = false, .overrides = YAML::Node{}});

	const auto stack = RenderStack::buildFromConfig(project, scene);
	ASSERT_EQ(stack.getLayers().size(), 1u);// hud disabled
	const auto worldLayer = std::dynamic_pointer_cast<TrackingLayer>(stack.getLayers()[0]);
	ASSERT_NE(worldLayer, nullptr);
	EXPECT_EQ(worldLayer->getName(), "world");
	ASSERT_TRUE(worldLayer->lastConfig);
	EXPECT_EQ(worldLayer->lastConfig["Fov"].as<int>(), 90);// override wins
	EXPECT_EQ(worldLayer->lastConfig["Range"].as<int>(), 100);// default preserved

	RenderLayerFactory::clear();
	owl::core::Log::invalidate();
}

TEST(RenderStack, sceneOverrideAppliedWhenProjectHasNoDefault) {
	// Regression: when a project layer has no `DefaultConfig`, the merged YAML
	// passed to `applyConfig` would arrive as an Undefined node and silently
	// drop every scene-level override. yaml-cpp's auto-vivify on
	// `node[key] = ...` updates the local node pointer but does not propagate
	// back through the caller's reference, so passing an Undefined `merged`
	// to `mergeYaml` lost the override. `buildFromConfig` now pre-vivifies
	// `merged` to an empty Map.
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderLayerFactory::clear();
	registerTrackingFactory();

	RendererStackConfig project;
	// Note: NO defaultConfig on `world`.
	project.entries.push_back({.typeKey = "Tracking", .name = "world", .defaultConfig = YAML::Node{}});

	EnabledRenderersConfig scene;
	YAML::Node overrides;
	overrides["Space"] = std::string{"Screen"};
	scene.entries.push_back({.name = "world", .enabled = true, .overrides = overrides});

	const auto stack = RenderStack::buildFromConfig(project, scene);
	ASSERT_EQ(stack.getLayers().size(), 1u);
	const auto worldLayer = std::dynamic_pointer_cast<TrackingLayer>(stack.getLayers()[0]);
	ASSERT_NE(worldLayer, nullptr);
	ASSERT_TRUE(worldLayer->lastConfig);
	ASSERT_TRUE(worldLayer->lastConfig.IsMap());
	EXPECT_EQ(worldLayer->lastConfig["Space"].as<std::string>(), "Screen");

	RenderLayerFactory::clear();
	owl::core::Log::invalidate();
}

TEST(RenderStack, frameCallbackOrder) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderLayerFactory::clear();
	registerTrackingFactory();

	RendererStackConfig project;
	project.entries.push_back({.typeKey = "Tracking", .name = "a", .defaultConfig = YAML::Node{}});
	project.entries.push_back({.typeKey = "Tracking", .name = "b", .defaultConfig = YAML::Node{}});

	auto stack = RenderStack::buildFromConfig(project, EnabledRenderersConfig{});
	ASSERT_EQ(stack.getLayers().size(), 2u);
	const owl::renderer::CameraOrtho cam(0, 0, 100, 100);
	stack.beginFrame(cam);
	stack.endFrame();

	const auto a = std::dynamic_pointer_cast<TrackingLayer>(stack.getLayers()[0]);
	const auto b = std::dynamic_pointer_cast<TrackingLayer>(stack.getLayers()[1]);
	ASSERT_NE(a, nullptr);
	ASSERT_NE(b, nullptr);
	EXPECT_EQ(a->beginCount, 1);
	EXPECT_EQ(b->beginCount, 1);
	EXPECT_EQ(a->endCount, 1);
	EXPECT_EQ(b->endCount, 1);

	RenderLayerFactory::clear();
	owl::core::Log::invalidate();
}

TEST(RenderStack, findByName) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderLayerFactory::clear();
	registerTrackingFactory();

	RendererStackConfig project;
	project.entries.push_back({.typeKey = "Tracking", .name = "world", .defaultConfig = YAML::Node{}});
	project.entries.push_back({.typeKey = "Tracking", .name = "hud", .defaultConfig = YAML::Node{}});

	const auto stack = RenderStack::buildFromConfig(project, EnabledRenderersConfig{});
	EXPECT_EQ(stack.getDefaultLayer()->getName(), "world");
	const auto hud = stack.findByName("hud");
	ASSERT_NE(hud, nullptr);
	EXPECT_EQ(hud->getName(), "hud");
	EXPECT_EQ(stack.findByName("missing"), nullptr);

	RenderLayerFactory::clear();
	owl::core::Log::invalidate();
}

TEST(RenderStack, sceneOverridesLayerOrder) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderLayerFactory::clear();
	registerTrackingFactory();

	RendererStackConfig project;
	project.entries.push_back({.typeKey = "Tracking", .name = "world", .defaultConfig = YAML::Node{}});
	project.entries.push_back({.typeKey = "Tracking", .name = "fx", .defaultConfig = YAML::Node{}});
	project.entries.push_back({.typeKey = "Tracking", .name = "ui", .defaultConfig = YAML::Node{}});

	// Scene reorders to ui, world (fx is not mentioned and must append at the end).
	EnabledRenderersConfig scene;
	scene.entries.push_back({.name = "ui", .enabled = true, .overrides = YAML::Node{}});
	scene.entries.push_back({.name = "world", .enabled = true, .overrides = YAML::Node{}});

	const auto stack = RenderStack::buildFromConfig(project, scene);
	ASSERT_EQ(stack.getLayers().size(), 3u);
	EXPECT_EQ(stack.getLayers()[0]->getName(), "ui");
	EXPECT_EQ(stack.getLayers()[1]->getName(), "world");
	EXPECT_EQ(stack.getLayers()[2]->getName(), "fx");

	RenderLayerFactory::clear();
	owl::core::Log::invalidate();
}

TEST(RenderStack, sceneSilenceKeepsProjectOrder) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderLayerFactory::clear();
	registerTrackingFactory();

	RendererStackConfig project;
	project.entries.push_back({.typeKey = "Tracking", .name = "world", .defaultConfig = YAML::Node{}});
	project.entries.push_back({.typeKey = "Tracking", .name = "ui", .defaultConfig = YAML::Node{}});

	const auto stack = RenderStack::buildFromConfig(project, EnabledRenderersConfig{});
	ASSERT_EQ(stack.getLayers().size(), 2u);
	EXPECT_EQ(stack.getLayers()[0]->getName(), "world");
	EXPECT_EQ(stack.getLayers()[1]->getName(), "ui");

	RenderLayerFactory::clear();
	owl::core::Log::invalidate();
}

TEST(RenderStack, sceneIgnoresUnknownLayerName) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderLayerFactory::clear();
	registerTrackingFactory();

	RendererStackConfig project;
	project.entries.push_back({.typeKey = "Tracking", .name = "world", .defaultConfig = YAML::Node{}});

	EnabledRenderersConfig scene;
	scene.entries.push_back({.name = "ghost", .enabled = true, .overrides = YAML::Node{}});
	scene.entries.push_back({.name = "world", .enabled = true, .overrides = YAML::Node{}});

	const auto stack = RenderStack::buildFromConfig(project, scene);
	ASSERT_EQ(stack.getLayers().size(), 1u);
	EXPECT_EQ(stack.getLayers()[0]->getName(), "world");

	RenderLayerFactory::clear();
	owl::core::Log::invalidate();
}
