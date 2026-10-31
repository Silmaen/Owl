/**
 * @file Accessibility_test.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Log.h>
#include <scene/Accessibility.h>
#include <scene/Scene.h>
#include <scene/SettingsManager.h>
#include <scene/Subtitles.h>

#include <cstdint>
#include <string>

using namespace owl;
using renderer::ColorDeficiency;

TEST(Accessibility, missingKeysKeepTheDefaults) {
	const scene::SettingsManager settings;
	const auto read = scene::Accessibility::read(settings);
	EXPECT_EQ(read.colorBlindMode, ColorDeficiency::None);
	EXPECT_FLOAT_EQ(read.colorBlindStrength, 1.f);
	EXPECT_FALSE(read.reducedMotion);
	EXPECT_TRUE(read.subtitles);
	EXPECT_FLOAT_EQ(read.subtitleScale, 1.f);
	EXPECT_FALSE(read.getColorVisionFilter().isActive());
}

TEST(Accessibility, readsAndClampsTheSettings) {
	scene::SettingsManager settings;
	settings.set(scene::Accessibility::KeyColorBlindMode, std::string{"tritanopia"});
	settings.set(scene::Accessibility::KeyColorBlindStrength, 3.f);
	settings.set(scene::Accessibility::KeyReducedMotion, true);
	settings.set(scene::Accessibility::KeySubtitles, false);
	settings.set(scene::Accessibility::KeySubtitleScale, int64_t{2});
	settings.set(scene::Accessibility::KeySubtitleBackground, -1.f);
	const auto read = scene::Accessibility::read(settings);
	EXPECT_EQ(read.colorBlindMode, ColorDeficiency::Tritanopia);
	EXPECT_FLOAT_EQ(read.colorBlindStrength, 1.f);
	EXPECT_TRUE(read.reducedMotion);
	EXPECT_FALSE(read.subtitles);
	EXPECT_FLOAT_EQ(read.subtitleScale, 2.f);
	EXPECT_FLOAT_EQ(read.subtitleBackground, 0.f);
	const auto filter = read.getColorVisionFilter();
	EXPECT_EQ(filter.deficiency, ColorDeficiency::Tritanopia);
	EXPECT_EQ(filter.mode, renderer::ColorVisionMode::Correct);
}

TEST(Accessibility, anUnknownColourModeIsNone) {
	scene::SettingsManager settings;
	settings.set(scene::Accessibility::KeyColorBlindMode, std::string{"sepia"});
	EXPECT_EQ(scene::Accessibility::read(settings).colorBlindMode, ColorDeficiency::None);
}

TEST(Accessibility, defaultsAreRegisteredForEveryKey) {
	scene::SettingsManager settings;
	scene::Accessibility::setDefaults(settings);
	for (const auto* key: {scene::Accessibility::KeyColorBlindMode, scene::Accessibility::KeyColorBlindStrength,
						   scene::Accessibility::KeyReducedMotion, scene::Accessibility::KeySubtitles,
						   scene::Accessibility::KeySubtitleScale, scene::Accessibility::KeySubtitleBackground})
		EXPECT_TRUE(settings.has(key)) << key;
	EXPECT_EQ(settings.getAs<std::string>(scene::Accessibility::KeyColorBlindMode), "none");
	settings.set(scene::Accessibility::KeyReducedMotion, true);
	settings.resetToDefault(scene::Accessibility::KeyReducedMotion);
	EXPECT_FALSE(scene::Accessibility::read(settings).reducedMotion);
}

TEST(Accessibility, theSceneCorrectsInPlayAndPreviewsEverywhere) {
	core::Log::init(core::Log::Level::Off);
	app::EngineContext context;
	const auto sc = mkShared<scene::Scene>();
	EXPECT_FALSE(sc->getColorVisionFilter().isActive());
	sc->setEngineContext(&context);
	context.getSettings().set(scene::Accessibility::KeyColorBlindMode, std::string{"deuteranopia"});
	EXPECT_TRUE(sc->getAccessibility().colorBlindMode == ColorDeficiency::Deuteranopia);
	// Edit mode shows the scene as authored.
	EXPECT_FALSE(sc->getColorVisionFilter().isActive());
	sc->status = scene::Scene::Status::Playing;
	EXPECT_EQ(sc->getColorVisionFilter().deficiency, ColorDeficiency::Deuteranopia);
	EXPECT_EQ(sc->getColorVisionFilter().mode, renderer::ColorVisionMode::Correct);
	const renderer::ColorVisionFilter preview{.deficiency = ColorDeficiency::Protanopia,
											  .mode = renderer::ColorVisionMode::Simulate};
	sc->setColorVisionPreview(preview);
	EXPECT_EQ(sc->getColorVisionFilter().deficiency, ColorDeficiency::Protanopia);
	EXPECT_EQ(sc->getColorVisionFilter().mode, renderer::ColorVisionMode::Simulate);
	sc->status = scene::Scene::Status::Editing;
	EXPECT_EQ(sc->getColorVisionFilter().deficiency, ColorDeficiency::Protanopia);
	sc->setColorVisionPreview({});
	EXPECT_FALSE(sc->getColorVisionFilter().isActive());
	core::Log::invalidate();
}

TEST(Subtitles, linesCountDownAndGo) {
	scene::Subtitles subtitles;
	subtitles.show("", 3.f);
	EXPECT_TRUE(subtitles.getLines().empty());
	subtitles.show("Hello", 1.f);
	ASSERT_EQ(subtitles.getLines().size(), 1u);
	subtitles.update(0.6f);
	EXPECT_EQ(subtitles.getLines().size(), 1u);
	subtitles.update(0.6f);
	EXPECT_TRUE(subtitles.getLines().empty());
}

TEST(Subtitles, theSameTextIsRenewedNotRepeated) {
	scene::Subtitles subtitles;
	subtitles.show("Rain", 1.f);
	subtitles.update(0.8f);
	subtitles.show("Rain", 1.f);
	ASSERT_EQ(subtitles.getLines().size(), 1u);
	EXPECT_FLOAT_EQ(subtitles.getLines().front().remaining, 1.f);
	// A shorter renewal never cuts a line short.
	subtitles.show("Rain", 0.2f);
	EXPECT_FLOAT_EQ(subtitles.getLines().front().remaining, 1.f);
}

TEST(Subtitles, aNewLinePushesTheOldestOut) {
	scene::Subtitles subtitles;
	for (const auto* text: {"one", "two", "three", "four"}) subtitles.show(text, 5.f);
	const auto lines = subtitles.getLines();
	ASSERT_EQ(lines.size(), scene::Subtitles::g_maxLines);
	EXPECT_EQ(lines.front().text, "two");
	EXPECT_EQ(lines.back().text, "four");
	subtitles.clear();
	EXPECT_TRUE(subtitles.getLines().empty());
}
