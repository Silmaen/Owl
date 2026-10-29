/**
 * @file SoundDocument_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/SoundDocument.h"
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
class SoundDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_sound_document_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_file = m_root / "steps.owlsound";
		ASSERT_TRUE(sound::SoundCue{}.saveToFile(m_file));
		ASSERT_TRUE(m_document.loadFromFile(m_file));
	}

	void TearDown() override {
		m_document.onDetach();
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	[[nodiscard]] auto files() const -> std::vector<std::string> {
		std::vector<std::string> names;
		for (const auto& variant: m_document.cue().variants) names.push_back(variant.file);
		return names;
	}

	std::filesystem::path m_root;
	std::filesystem::path m_file;
	SoundDocument m_document;
};
OWL_DIAG_POP

}// namespace

TEST_F(SoundDocumentTest, ReadsTheFileAndRefusesOtherDocuments) {
	EXPECT_EQ(m_document.type(), DocumentType::Sound);
	EXPECT_EQ(m_document.title(), "steps.owlsound");
	EXPECT_FALSE(m_document.isDirty());
	EXPECT_FALSE(m_document.loadFromFile(m_root / "missing.owlsound"));
}

TEST_F(SoundDocumentTest, TakesAreAddedMovedDuplicatedAndRemovedAsGroups) {
	EXPECT_FALSE(m_document.addVariant("sounds/other.owlsound"));
	EXPECT_FALSE(m_document.addVariant("readme.txt"));
	ASSERT_TRUE(m_document.addVariant("a.wav"));
	ASSERT_TRUE(m_document.addVariant("b.ogg"));
	ASSERT_TRUE(m_document.addVariant("c.flac"));
	EXPECT_EQ(m_document.getSelection(), std::vector<size_t>{2});
	EXPECT_TRUE(m_document.isDirty());

	const std::vector<size_t> pair{1, 2};
	EXPECT_FALSE(m_document.moveVariants(pair, true));
	ASSERT_TRUE(m_document.moveVariants(pair, false));
	EXPECT_EQ(files(), (std::vector<std::string>{"b.ogg", "c.flac", "a.wav"}));
	EXPECT_EQ(m_document.getSelection(), (std::vector<size_t>{0, 1}));

	ASSERT_TRUE(m_document.duplicateVariants(m_document.getSelection()));
	EXPECT_EQ(files(), (std::vector<std::string>{"b.ogg", "c.flac", "b.ogg", "c.flac", "a.wav"}));
	EXPECT_EQ(m_document.getSelection(), (std::vector<size_t>{2, 3}));

	const std::vector<size_t> doomed{0, 3, 3, 9};
	ASSERT_TRUE(m_document.removeVariants(doomed));
	EXPECT_EQ(files(), (std::vector<std::string>{"c.flac", "b.ogg", "a.wav"}));

	m_document.performUndo();
	EXPECT_EQ(m_document.cue().variants.size(), 5u);
	m_document.performRedo();
	EXPECT_EQ(m_document.cue().variants.size(), 3u);
}

TEST_F(SoundDocumentTest, TakeAndCueEditsAreUndoableAndSaved) {
	ASSERT_TRUE(m_document.addVariant("a.wav"));
	auto take = m_document.cue().variants[0];
	take.trimStart = 0.4f;
	take.trimEnd = 0.2f;
	take.loopStart = 0.1f;
	take.loopEnd = 0.3f;
	ASSERT_TRUE(m_document.setVariant(0, take));
	const auto& stored = m_document.cue().variants[0];
	EXPECT_GT(stored.trimEnd, stored.trimStart);
	EXPECT_GE(stored.loopStart, stored.trimStart);
	EXPECT_LE(stored.loopEnd, stored.trimEnd);
	EXPECT_FALSE(m_document.setVariant(4, take));
	take.file = "other.owlsound";
	EXPECT_FALSE(m_document.setVariant(0, take));

	auto properties = m_document.cue();
	properties.volume = 0.5f;
	properties.pitchJitter = 3.f;
	properties.loop = true;
	ASSERT_TRUE(m_document.setProperties(properties));
	EXPECT_FLOAT_EQ(m_document.cue().volume, 0.5f);
	EXPECT_LT(m_document.cue().pitchJitter, 1.f);
	EXPECT_FALSE(m_document.setProperties(m_document.cue()));

	ASSERT_TRUE(m_document.save());
	EXPECT_FALSE(m_document.isDirty());
	const auto saved = sound::SoundCue::load(m_file);
	ASSERT_TRUE(saved.has_value());
	EXPECT_EQ(*saved, m_document.cue());

	m_document.performUndo();
	EXPECT_TRUE(m_document.isDirty());
	EXPECT_FLOAT_EQ(m_document.cue().volume, 1.f);
	const auto snapshot = m_document.recoverySnapshot();
	ASSERT_TRUE(snapshot.has_value());
	ASSERT_TRUE(m_document.restoreRecoverySnapshot(*snapshot));
	EXPECT_FALSE(m_document.restoreRecoverySnapshot("not a cue"));
}

TEST(SoundDocument, MarkersAreOrderedInsideTheFile) {
	const sound::SoundVariant raw{.file = "a.wav",
								  .trimStart = 3.f,
								  .trimEnd = 1.f,
								  .loopStart = 0.5f,
								  .loopEnd = 9.f,
								  .weight = -2.f};
	const auto ordered = orderMarkers(raw, 2.f);
	EXPECT_FLOAT_EQ(ordered.trimStart, 2.f);
	EXPECT_FLOAT_EQ(ordered.trimEnd, 2.f);
	EXPECT_FLOAT_EQ(ordered.loopStart, 2.f);
	EXPECT_FLOAT_EQ(ordered.loopEnd, 2.f);
	EXPECT_FLOAT_EQ(ordered.weight, 0.f);
	const sound::SoundVariant whole{.file = "a.wav",
									.trimStart = 0.f,
									.trimEnd = 0.f,
									.loopStart = 0.f,
									.loopEnd = 0.f,
									.weight = 1.f};
	EXPECT_EQ(orderMarkers(whole, 2.f), whole);
}

TEST_F(SoundDocumentTest, AnAudioFileStartsACueBesideIt) {
	const auto audio = m_root / "bell.wav";
	SoundDocument document;
	document.startFromAudio(audio);
	EXPECT_EQ(document.filePath(), m_root / "bell.owlsound");
	EXPECT_TRUE(document.isDirty());
	ASSERT_EQ(document.cue().variants.size(), 1u);
	EXPECT_EQ(std::filesystem::path{document.cue().variants[0].file}.filename(), "bell.wav");
	EXPECT_EQ(document.getSelection(), std::vector<size_t>{0});
	ASSERT_TRUE(document.save());
	EXPECT_TRUE(exists(m_root / "bell.owlsound"));
	document.onDetach();
}
