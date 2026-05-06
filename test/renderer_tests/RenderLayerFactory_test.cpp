/**
 * @file RenderLayerFactory_test.cpp
 * @author Silmaen
 * @date 30/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/RenderLayerFactory.h>

namespace {
class FakeLayer final : public owl::renderer::RenderLayer {
public:
	explicit FakeLayer(std::string iName) : m_name{std::move(iName)} {}
	~FakeLayer() override = default;
	[[nodiscard]] auto getName() const -> const std::string& override { return m_name; }
	[[nodiscard]] auto getTypeKey() const -> const char* override { return "Fake"; }
	void onBeginFrame(const owl::renderer::Camera&) override {}
	void onRender(owl::scene::Scene&) override {}
	void onEndFrame() override {}
	void applyConfig(const YAML::Node&) override {}

private:
	std::string m_name;
};

}// namespace

using owl::renderer::RenderLayerFactory;

TEST(RenderLayerFactory, registerCreateUnregister) {
	RenderLayerFactory::clear();
	EXPECT_FALSE(RenderLayerFactory::hasType("Fake"));
	RenderLayerFactory::registerType("Fake", [](const std::string& iName) -> owl::shared<owl::renderer::RenderLayer> {
		return owl::mkShared<FakeLayer>(iName);
	});
	EXPECT_TRUE(RenderLayerFactory::hasType("Fake"));

	const auto layer = RenderLayerFactory::create("Fake", "world");
	ASSERT_NE(layer, nullptr);
	EXPECT_EQ(layer->getName(), "world");
	EXPECT_STREQ(layer->getTypeKey(), "Fake");

	EXPECT_TRUE(RenderLayerFactory::unregisterType("Fake"));
	EXPECT_FALSE(RenderLayerFactory::hasType("Fake"));
	EXPECT_FALSE(RenderLayerFactory::unregisterType("Fake"));
}

TEST(RenderLayerFactory, unknownTypeReturnsNull) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderLayerFactory::clear();
	const auto layer = RenderLayerFactory::create("DoesNotExist", "x");
	EXPECT_EQ(layer, nullptr);
	owl::core::Log::invalidate();
}

TEST(RenderLayerFactory, multipleInstancesOfSameType) {
	RenderLayerFactory::clear();
	RenderLayerFactory::registerType("Fake", [](const std::string& iName) -> owl::shared<owl::renderer::RenderLayer> {
		return owl::mkShared<FakeLayer>(iName);
	});
	const auto a = RenderLayerFactory::create("Fake", "a");
	const auto b = RenderLayerFactory::create("Fake", "b");
	ASSERT_NE(a, nullptr);
	ASSERT_NE(b, nullptr);
	EXPECT_NE(a.get(), b.get());
	EXPECT_EQ(a->getName(), "a");
	EXPECT_EQ(b->getName(), "b");
	RenderLayerFactory::clear();
}

TEST(RenderLayerFactory, registeredTypesSorted) {
	RenderLayerFactory::clear();
	const auto factory = [](const std::string& iName) -> owl::shared<owl::renderer::RenderLayer> {
		return owl::mkShared<FakeLayer>(iName);
	};
	RenderLayerFactory::registerType("Beta", factory);
	RenderLayerFactory::registerType("Alpha", factory);
	RenderLayerFactory::registerType("Gamma", factory);
	const auto types = RenderLayerFactory::registeredTypes();
	ASSERT_EQ(types.size(), 3u);
	EXPECT_EQ(types[0], "Alpha");
	EXPECT_EQ(types[1], "Beta");
	EXPECT_EQ(types[2], "Gamma");
	RenderLayerFactory::clear();
}
