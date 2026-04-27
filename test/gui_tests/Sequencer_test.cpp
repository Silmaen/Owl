/**
 * @file Sequencer_test.cpp
 * @author Silmaen
 * @date 27/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "testHelper.h"

#include "gui/widgets/Sequencer.h"

using namespace owl;
using namespace owl::gui::widgets;

// The widget itself needs a live ImGui context, so the rendering call cannot run inside a unit
// test. These tests instead validate the data layer (entries, options) and confirm the public
// interface keeps ImSequencer types out of the public surface (they would not link otherwise).

TEST(Sequencer, EntryDefaults) {
	const SequencerEntry e;
	EXPECT_TRUE(e.label.empty());
	EXPECT_EQ(e.startFrame, 0);
	EXPECT_EQ(e.endFrame, 0);
	EXPECT_FLOAT_EQ(e.color.w(), 1.f);
}

TEST(Sequencer, OptionsDefaults) {
	const SequencerOptions o;
	EXPECT_EQ(o.frameMin, 0);
	EXPECT_GT(o.frameMax, o.frameMin);
	EXPECT_TRUE(o.allowEditStartEnd);
	EXPECT_TRUE(o.showCurrentFrame);
}

TEST(Sequencer, EntriesAreMovableAndCopyable) {
	std::vector<SequencerEntry> entries;
	entries.push_back({"walk", 0, 7, {1.f, 0.f, 0.f, 1.f}});
	entries.push_back({"jump", 8, 15, {0.f, 1.f, 0.f, 1.f}});

	const auto copy = entries;
	ASSERT_EQ(copy.size(), 2u);
	EXPECT_EQ(copy[0].label, "walk");
	EXPECT_EQ(copy[1].endFrame, 15);

	const auto moved = std::move(entries);
	EXPECT_EQ(moved.size(), 2u);
}
