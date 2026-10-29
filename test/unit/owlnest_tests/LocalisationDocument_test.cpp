/**
 * @file LocalisationDocument_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/LocalisationDocument.h"
#include "testHelper.h"

#include <core/Log.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::nest;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class LocalisationDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_localisation_document_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_file = m_root / std::string{data::localisation::Localisation::fileName};
		m_document.startEmpty(m_file);
	}

	void TearDown() override {
		m_document.onDetach();
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	[[nodiscard]] auto keys() const -> std::vector<std::string> {
		std::vector<std::string> names;
		for (const auto& entry: m_document.localisation().strings) names.push_back(entry.key);
		return names;
	}

	std::filesystem::path m_root;
	std::filesystem::path m_file;
	LocalisationDocument m_document;
};
OWL_DIAG_POP

}// namespace

TEST_F(LocalisationDocumentTest, StartsWithEnglishAndSavesTheFile) {
	EXPECT_EQ(m_document.type(), DocumentType::Localisation);
	EXPECT_TRUE(m_document.isDirty());
	ASSERT_EQ(m_document.localisation().languages.size(), 1u);
	EXPECT_EQ(m_document.localisation().fallback, "en");
	ASSERT_TRUE(m_document.save());
	EXPECT_FALSE(m_document.isDirty());
	LocalisationDocument other;
	ASSERT_TRUE(other.loadFromFile(m_file));
	EXPECT_EQ(other.localisation(), m_document.localisation());
	EXPECT_FALSE(other.loadFromFile(m_root / "missing.yml"));
}

TEST_F(LocalisationDocumentTest, LanguagesAreAddedEditedAndRemovedWithTheirTexts) {
	EXPECT_FALSE(m_document.addLanguage("en", "English"));
	EXPECT_FALSE(m_document.addLanguage("", "Nothing"));
	ASSERT_TRUE(m_document.addLanguage("fr", "Français"));
	const auto index = m_document.addString("menu.start", "menu");
	ASSERT_TRUE(m_document.setValue(index, "fr", "Démarrer"));
	EXPECT_FALSE(m_document.setValue(index, "de", "Starten"));
	ASSERT_TRUE(m_document.setFallback("fr"));
	EXPECT_FALSE(m_document.setFallback("de"));

	auto french = *m_document.localisation().findLanguage("fr");
	french.font = "NotoSans";
	french.exported = false;
	ASSERT_TRUE(m_document.setLanguage(french));
	EXPECT_EQ(m_document.localisation().findLanguage("fr")->font, "NotoSans");

	ASSERT_TRUE(m_document.removeLanguage("fr"));
	EXPECT_EQ(m_document.localisation().fallback, "en");
	EXPECT_TRUE(m_document.localisation().strings[0].values.empty());
	m_document.performUndo();
	EXPECT_EQ(m_document.localisation().strings[0].values.at("fr"), "Démarrer");
	EXPECT_FALSE(m_document.removeLanguage("de"));
}

TEST_F(LocalisationDocumentTest, StringsAreRenamedMovedDuplicatedAndRemovedAsGroups) {
	EXPECT_EQ(m_document.addString("menu.start", "menu"), 0u);
	EXPECT_EQ(m_document.addString("menu.start", "menu"), 1u);
	EXPECT_EQ(m_document.addString("npc.hello", ""), 2u);
	EXPECT_EQ(keys(), (std::vector<std::string>{"menu.start", "menu.start_2", "npc.hello"}));
	EXPECT_FALSE(m_document.setKey(1, "npc.hello"));
	EXPECT_FALSE(m_document.setKey(1, ""));
	ASSERT_TRUE(m_document.setKey(1, "menu.quit"));

	ASSERT_TRUE(m_document.setValue(0, "en", "Start"));
	ASSERT_TRUE(m_document.setValue(0, "en", ""));
	EXPECT_TRUE(m_document.localisation().strings[0].values.empty());

	const std::vector<size_t> menu{0, 1};
	ASSERT_TRUE(m_document.duplicateStrings(menu));
	EXPECT_EQ(keys(),
			  (std::vector<std::string>{"menu.start", "menu.quit", "menu.start_2", "menu.quit_2", "npc.hello"}));
	EXPECT_EQ(m_document.getSelection(), (std::vector<size_t>{2, 3}));
	ASSERT_TRUE(m_document.setTable(m_document.getSelection(), "copies"));
	EXPECT_EQ(m_document.localisation().getTables(), (std::vector<std::string>{"", "copies", "menu"}));

	const std::vector<size_t> doomed{2, 3, 7};
	ASSERT_TRUE(m_document.removeStrings(doomed));
	EXPECT_EQ(keys(), (std::vector<std::string>{"menu.start", "menu.quit", "npc.hello"}));
	m_document.performUndo();
	EXPECT_EQ(keys().size(), 5u);
	m_document.performRedo();
	EXPECT_EQ(keys().size(), 3u);

	const auto snapshot = m_document.recoverySnapshot();
	ASSERT_TRUE(snapshot.has_value());
	ASSERT_TRUE(m_document.restoreRecoverySnapshot(*snapshot));
	EXPECT_FALSE(m_document.restoreRecoverySnapshot("nope"));
	EXPECT_EQ(m_document.getUniqueKey("menu.start"), "menu.start_2");
	EXPECT_EQ(m_document.getUniqueKey(""), "text");
}
