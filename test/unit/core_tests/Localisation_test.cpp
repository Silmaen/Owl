/**
 * @file Localisation_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Log.h>
#include <data/localisation/Localisation.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::data::localisation;

namespace {

auto makeLocalisation() -> Localisation {
	return {.languages = {{.code = "en", .name = "English", .font = {}, .exported = true},
						  {.code = "fr", .name = "Français", .font = "NotoSans", .exported = true},
						  {.code = "ja", .name = "日本語", .font = "NotoSansJP", .exported = false}},
			.fallback = "en",
			.strings = {{.key = "menu.start",
						 .table = "menu",
						 .values = {{"en", "Start"}, {"fr", "Démarrer"}, {"ja", "開始"}}},
						{.key = "menu.quit", .table = "menu", .values = {{"en", "Quit"}}},
						{.key = "npc.hello", .table = "dialogue", .values = {{"fr", "Bonjour"}}}}};
}

}// namespace

TEST(Localisation, RoundTripsThroughYamlAndFiles) {
	core::Log::init(core::Log::Level::Off);
	const auto localisation = makeLocalisation();
	const auto text = localisation.serializeToString();
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);
	const auto read = Localisation::deserializeFromString(text);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, localisation);
	EXPECT_FALSE(Localisation::deserializeFromString("[unclosed").has_value());
	EXPECT_FALSE(Localisation::deserializeFromString("Other: {}").has_value());
	EXPECT_FALSE(Localisation::deserializeFromString("Localisation: ''\nFormatVersion: 99").has_value());

	const auto twice = Localisation::deserializeFromString(
			"Localisation: ''\nlanguages: [{code: en}, {code: en}, {name: nameless}]\n"
			"strings: [{key: a, values: {en: A}}, {key: a, values: {en: B}}, {table: t}]");
	ASSERT_TRUE(twice.has_value());
	EXPECT_EQ(twice->languages.size(), 1u);
	EXPECT_EQ(twice->languages[0].name, "en");
	ASSERT_EQ(twice->strings.size(), 1u);
	EXPECT_EQ(twice->strings[0].values.at("en"), "A");

	const auto path = std::filesystem::temp_directory_path() / "owl_localisation.yml";
	ASSERT_TRUE(localisation.saveToFile(path));
	const auto loaded = Localisation::loadFile(path);
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(*loaded, localisation);
	std::filesystem::remove(path);
	EXPECT_FALSE(Localisation::loadFile(path).has_value());
	core::Log::invalidate();
}

TEST(Localisation, TranslatesWithTheFallback) {
	const auto localisation = makeLocalisation();
	EXPECT_EQ(localisation.translate("menu.start", "fr"), "Démarrer");
	EXPECT_EQ(localisation.translate("menu.quit", "fr"), "Quit");
	EXPECT_EQ(localisation.translate("npc.hello", "en"), "npc.hello");
	EXPECT_EQ(localisation.translate("missing", "fr"), "missing");
	EXPECT_EQ(localisation.resolveLanguage("fr"), "fr");
	EXPECT_EQ(localisation.resolveLanguage("de"), "en");
	EXPECT_EQ(localisation.resolveLanguage(""), "en");
	EXPECT_TRUE(Localisation{}.resolveLanguage("fr").empty());
	EXPECT_EQ(localisation.getTables(), (std::vector<std::string>{"dialogue", "menu"}));
	EXPECT_EQ(localisation.getMissing("en"), (std::vector<std::string>{"npc.hello"}));
	EXPECT_EQ(localisation.getMissing("fr"), (std::vector<std::string>{"menu.quit"}));
	EXPECT_EQ(localisation.findLanguage("ja")->font, "NotoSansJP");
	EXPECT_EQ(localisation.findLanguage("de"), nullptr);
}

TEST(Localisation, ExportKeepsTheExportedLanguagesOnly) {
	const auto exported = makeLocalisation().withExportedLanguages();
	ASSERT_EQ(exported.languages.size(), 2u);
	EXPECT_EQ(exported.findLanguage("ja"), nullptr);
	EXPECT_FALSE(exported.findString("menu.start")->values.contains("ja"));
	EXPECT_EQ(exported.fallback, "en");

	auto frenchOnly = makeLocalisation();
	frenchOnly.languages[0].exported = false;
	EXPECT_EQ(frenchOnly.withExportedLanguages().fallback, "fr");
}

TEST(Localisation, TheEngineContextFollowsTheLanguage) {
	core::Log::init(core::Log::Level::Off);
	app::EngineContext context;
	EXPECT_EQ(context.translate("menu.start"), "menu.start");
	EXPECT_TRUE(context.getLanguage().empty());
	context.setLanguage("fr");
	const auto before = context.getLocalisationGeneration();
	context.setLocalisation(makeLocalisation());
	EXPECT_GT(context.getLocalisationGeneration(), before);
	EXPECT_EQ(context.getLanguage(), "fr");
	EXPECT_EQ(context.translate("menu.start"), "Démarrer");
	EXPECT_EQ(context.translate("menu.quit"), "Quit");
	EXPECT_EQ(context.getLanguageFont(), "NotoSans");
	context.setLanguage("de");
	EXPECT_EQ(context.getLanguage(), "en");
	EXPECT_EQ(context.translate("menu.start"), "Start");
	EXPECT_TRUE(context.getLanguageFont().empty());

	const auto dir = std::filesystem::temp_directory_path() / "owl_localisation_project";
	std::filesystem::create_directories(dir);
	ASSERT_TRUE(makeLocalisation().saveToFile(dir / Localisation::fileName));
	app::EngineContext loaded;
	EXPECT_TRUE(loaded.loadLocalisation(dir));
	EXPECT_EQ(loaded.getLocalisation(), makeLocalisation());
	std::filesystem::remove_all(dir);
	EXPECT_FALSE(loaded.loadLocalisation(dir));
	EXPECT_TRUE(loaded.getLocalisation().strings.empty());
	core::Log::invalidate();
}
