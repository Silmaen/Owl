/**
 * @file Light_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/ComponentRegistry.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <cstddef>
#include <sstream>
#include <string>

using namespace owl;

namespace {

class LightTest : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
	static void TearDownTestSuite() { core::Log::invalidate(); }
};

/// The entity YAML with the `Light` block replaced by an empty map.
auto withEmptyLight(const std::string& iYaml) -> std::string {
	std::istringstream in{iYaml};
	std::string out;
	std::string line;
	size_t skipIndent = std::string::npos;
	while (std::getline(in, line)) {
		const auto indent = line.find_first_not_of(' ');
		if (skipIndent != std::string::npos) {
			if (indent != std::string::npos && indent > skipIndent)
				continue;
			skipIndent = std::string::npos;
		}
		if (const auto at = line.find("Light:"); at != std::string::npos && at == indent) {
			out += line.substr(0, at) + "Light: {}\n";
			skipIndent = at;
			continue;
		}
		out += line + "\n";
	}
	return out;
}

}// namespace

TEST_F(LightTest, serializationRoundTripsEveryField) {
	const auto sc = mkShared<scene::Scene>();
	auto lamp = sc->createEntity("Lamp");
	auto& light = lamp.addComponent<scene::component::Light>();
	light.type = scene::component::Light::Type::Spot;
	light.color = {0.25f, 0.5f, 0.75f};
	light.intensity = 3.5f;
	light.range = 42.f;
	light.innerAngle = 0.25f;
	light.outerAngle = 0.5f;
	light.castShadows = false;
	light.shadowDistance = 123.f;
	light.cascadeCount = 2;
	light.shadowResolution = 2048;
	light.shadowBias = 3.f;
	const auto yaml = scene::SceneSerializer::serializeEntityToString(lamp);
	EXPECT_NE(yaml.find("Light"), std::string::npos);

	const auto copy = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer::deserializeEntityFromString(copy, yaml));
	const auto restored = copy->findEntityByUUID(lamp.getUUID());
	ASSERT_TRUE(restored);
	ASSERT_TRUE(restored.hasComponent<scene::component::Light>());
	const auto& read = restored.getComponent<scene::component::Light>();
	EXPECT_EQ(read.type, scene::component::Light::Type::Spot);
	EXPECT_EQ(read.color, (math::vec3{0.25f, 0.5f, 0.75f}));
	EXPECT_FLOAT_EQ(read.intensity, 3.5f);
	EXPECT_FLOAT_EQ(read.range, 42.f);
	EXPECT_FLOAT_EQ(read.innerAngle, 0.25f);
	EXPECT_FLOAT_EQ(read.outerAngle, 0.5f);
	EXPECT_FALSE(read.castShadows);
	EXPECT_FLOAT_EQ(read.shadowDistance, 123.f);
	EXPECT_EQ(read.cascadeCount, 2u);
	EXPECT_EQ(read.shadowResolution, 2048u);
	EXPECT_FLOAT_EQ(read.shadowBias, 3.f);
}

TEST_F(LightTest, aBlockMissingEveryOptionalKeyLoadsTheDefaults) {
	const auto sc = mkShared<scene::Scene>();
	auto lamp = sc->createEntity("Lamp");
	lamp.addComponent<scene::component::Light>().intensity = 9.f;
	const auto yaml = withEmptyLight(scene::SceneSerializer::serializeEntityToString(lamp));
	ASSERT_NE(yaml.find("Light: {}"), std::string::npos);

	const auto copy = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer::deserializeEntityFromString(copy, yaml));
	const auto restored = copy->findEntityByUUID(lamp.getUUID());
	ASSERT_TRUE(restored);
	ASSERT_TRUE(restored.hasComponent<scene::component::Light>());
	const auto& read = restored.getComponent<scene::component::Light>();
	const scene::component::Light defaults;
	EXPECT_EQ(read.type, defaults.type);
	EXPECT_EQ(read.color, defaults.color);
	EXPECT_FLOAT_EQ(read.intensity, defaults.intensity);
	EXPECT_FLOAT_EQ(read.range, defaults.range);
	EXPECT_FLOAT_EQ(read.innerAngle, defaults.innerAngle);
	EXPECT_FLOAT_EQ(read.outerAngle, defaults.outerAngle);
	EXPECT_EQ(read.castShadows, defaults.castShadows);
	EXPECT_FLOAT_EQ(read.shadowDistance, defaults.shadowDistance);
	EXPECT_EQ(read.cascadeCount, defaults.cascadeCount);
	EXPECT_EQ(read.shadowResolution, defaults.shadowResolution);
	EXPECT_FLOAT_EQ(read.shadowBias, defaults.shadowBias);
}

TEST_F(LightTest, isAnOptionalComponentOfTheRegistry) {
	const auto* desc = scene::ComponentRegistry::find("Light");
	ASSERT_NE(desc, nullptr);
	EXPECT_TRUE(desc->optional);
}
