/**
 * @file IsometricObjects_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <renderer/IsometricObjects.h>

#include <string>

using namespace owl;
using owl::renderer::IsometricObjectError;
using owl::renderer::IsometricObjectSet;

namespace {

constexpr auto g_sheet = R"({
  "zoom": "2x",
  "buildings": {
    "warehouse": {
      "footprint": [2, 1],
      "slices": [
        {"cell": [0, 0], "rect": [0, 0, 128, 256], "anchor": [64, 256],
         "box": {"min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 2.4]}},
        {"cell": [1, 0], "rect": [128, 0, 128, 256], "anchor": [64, 256],
         "box": {"min": [1.0, 0.0, 0.0], "max": [2.0, 1.0, 3.1]}}
      ]
    },
    "cottage": {
      "footprint": [1, 1],
      "slices": [
        {"cell": [0, 0], "rect": [0, 256, 128, 256], "anchor": [64, 256],
         "box": {"min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 2.85]}}
      ]
    }
  }
})";

class IsometricObjectsTest : public testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }
};

}// namespace

TEST_F(IsometricObjectsTest, parsesTheGeneratorFormat) {
	const auto set = IsometricObjectSet::parse(g_sheet);
	ASSERT_TRUE(set.has_value());
	ASSERT_EQ(set->getObjects().size(), 2u);
	EXPECT_EQ(set->getObjects()[0].name, "cottage") << "objects are sorted by name";
	const auto* warehouse = set->find("warehouse");
	ASSERT_NE(warehouse, nullptr);
	EXPECT_EQ(warehouse->footprint.x(), 2u);
	EXPECT_EQ(warehouse->footprint.y(), 1u);
	ASSERT_EQ(warehouse->slices.size(), 2u);
	EXPECT_EQ(warehouse->slices[1].cell.x(), 1u);
	EXPECT_FLOAT_EQ(warehouse->slices[1].rect.x(), 128.f);
	EXPECT_FLOAT_EQ(warehouse->slices[1].rect.w(), 256.f);
	EXPECT_FLOAT_EQ(warehouse->slices[1].anchor.x(), 64.f);
	EXPECT_FLOAT_EQ(warehouse->slices[1].boxMax.z(), 3.1f);
	EXPECT_FLOAT_EQ(warehouse->slices[1].boxMin.x(), 1.f);
	EXPECT_FLOAT_EQ(warehouse->getHeight(), 3.1f);
	EXPECT_EQ(set->find("castle"), nullptr);
	EXPECT_EQ(set->getTexture(), nullptr);
}

TEST_F(IsometricObjectsTest, tileWidthFollowsTheZoomOfTheSheet) {
	const auto make = [](const std::string& iZoom) -> float {
		const std::string text = "{zoom: " + iZoom +
								 ", buildings: {a: {footprint: [1, 1], slices: [{cell: [0, 0], rect: [0, 0, 8, 8], "
								 "anchor: [4, 8], box: {min: [0, 0, 0], max: [1, 1, 1]}}]}}}";
		const auto set = IsometricObjectSet::parse(text);
		return set ? set->getTileWidth() : -1.f;
	};
	EXPECT_FLOAT_EQ(make("1x"), 64.f);
	EXPECT_FLOAT_EQ(make("2x"), 128.f);
	EXPECT_FLOAT_EQ(make("0.5x"), 32.f);
	EXPECT_FLOAT_EQ(make("half"), 32.f);
	EXPECT_FLOAT_EQ(make("nonsense"), 64.f);
	EXPECT_FLOAT_EQ(IsometricObjectSet::parse(g_sheet)->getTileWidth(), 128.f);
}

TEST_F(IsometricObjectsTest, refusesWhatIsNotASheet) {
	const auto notMap = IsometricObjectSet::parse("[1, 2]");
	ASSERT_FALSE(notMap.has_value());
	EXPECT_EQ(notMap.error(), IsometricObjectError::InvalidDocument);
	const auto broken = IsometricObjectSet::parse("{a: [");
	ASSERT_FALSE(broken.has_value());
	EXPECT_EQ(broken.error(), IsometricObjectError::InvalidDocument);
	const auto empty = IsometricObjectSet::parse("{zoom: 1x, buildings: {}}");
	ASSERT_FALSE(empty.has_value());
	EXPECT_EQ(empty.error(), IsometricObjectError::NoObjects);
	const auto missing = IsometricObjectSet::parse("{zoom: 1x}");
	ASSERT_FALSE(missing.has_value());
	EXPECT_EQ(missing.error(), IsometricObjectError::NoObjects);
}

TEST_F(IsometricObjectsTest, refusesMalformedObjects) {
	const std::string noSlices = "{buildings: {a: {footprint: [1, 1], slices: []}}}";
	const std::string zeroFootprint =
			"{buildings: {a: {footprint: [0, 1], slices: [{cell: [0, 0], rect: [0, 0, 8, 8], anchor: [4, 8], "
			"box: {min: [0, 0, 0], max: [1, 1, 1]}}]}}}";
	const std::string emptyRect =
			"{buildings: {a: {footprint: [1, 1], slices: [{cell: [0, 0], rect: [0, 0, 0, 8], anchor: [4, 8], "
			"box: {min: [0, 0, 0], max: [1, 1, 1]}}]}}}";
	const std::string noBox =
			"{buildings: {a: {footprint: [1, 1], slices: [{cell: [0, 0], rect: [0, 0, 8, 8], anchor: [4, 8]}]}}}";
	const std::string badNumber =
			"{buildings: {a: {footprint: [1, 1], slices: [{cell: [0, 0], rect: [x, 0, 8, 8], anchor: [4, 8], "
			"box: {min: [0, 0, 0], max: [1, 1, 1]}}]}}}";
	for (const auto& text: {noSlices, zeroFootprint, emptyRect, noBox, badNumber}) {
		const auto set = IsometricObjectSet::parse(text);
		ASSERT_FALSE(set.has_value()) << text;
		EXPECT_EQ(set.error(), IsometricObjectError::InvalidObject) << text;
	}
}
