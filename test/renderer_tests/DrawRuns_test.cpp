/**
 * @file DrawRuns_test.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/utils/DrawRuns.h>

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <vector>

using owl::renderer::utils::DrawRuns;
using owl::renderer::utils::RunKind;

namespace {

// One draw as the renderer would issue it.
using Draw = std::tuple<RunKind, uint32_t, uint32_t>;

// Notes primitives as the renderer's draw calls do.
struct Submitter {
	DrawRuns runs;

	void add(const RunKind iKind, const float iDepth = 0.f) { runs.note(iKind, iDepth); }

	auto draws() -> std::vector<Draw> {
		std::vector<Draw> result;
		runs.forEachDraw([&result](const RunKind iKind, const uint32_t iBegin, const uint32_t iCount) -> void {
			result.emplace_back(iKind, iBegin, iCount);
		});
		return result;
	}
};

}// namespace

TEST(DrawRuns, emptyDrawsNothing) {
	Submitter sub;
	EXPECT_TRUE(sub.draws().empty());
	EXPECT_EQ(sub.runs.size(), 0u);
}

TEST(DrawRuns, oneKindIsOneDraw) {
	Submitter sub;
	for (int i = 0; i < 5; ++i) sub.add(RunKind::Quad);
	EXPECT_EQ(sub.runs.size(), 0u);
	EXPECT_EQ(sub.draws(), (std::vector<Draw>{{RunKind::Quad, 0, 5}}));
}

TEST(DrawRuns, sameDepthDrawsByTypeWhateverTheSubmissionOrder) {
	Submitter sub;
	sub.add(RunKind::Text);
	sub.add(RunKind::Line);
	sub.add(RunKind::Circle);
	sub.add(RunKind::Quad);
	sub.add(RunKind::Circle);
	sub.add(RunKind::Quad);
	// Quads, circles, lines, text: one draw per type, however interleaved.
	EXPECT_EQ(sub.draws(), (std::vector<Draw>{{RunKind::Quad, 0, 2},
											  {RunKind::Circle, 0, 2},
											  {RunKind::Line, 0, 1},
											  {RunKind::Text, 0, 1}}));
}

TEST(DrawRuns, lowerDepthDrawsFirstWhateverTheType) {
	Submitter sub;
	sub.add(RunKind::Text, 2.f);
	sub.add(RunKind::Quad, 1.f);
	sub.add(RunKind::Circle, 0.f);
	sub.add(RunKind::Line, -1.f);
	EXPECT_EQ(sub.draws(), (std::vector<Draw>{{RunKind::Line, 0, 1},
											  {RunKind::Circle, 0, 1},
											  {RunKind::Quad, 0, 1},
											  {RunKind::Text, 0, 1}}));
}

TEST(DrawRuns, equalDepthsOrderByTypeThenSubmission) {
	Submitter sub;
	sub.add(RunKind::Circle, 1.f);
	sub.add(RunKind::Quad, 0.f);
	sub.add(RunKind::Line, 1.f);
	sub.add(RunKind::Text, 0.f);
	sub.add(RunKind::Quad, 1.f);
	// Depth 0: quad, text. Depth 1: quad, circle, line (by type).
	EXPECT_EQ(sub.draws(), (std::vector<Draw>{{RunKind::Quad, 0, 1},
											  {RunKind::Text, 0, 1},
											  {RunKind::Quad, 1, 1},
											  {RunKind::Circle, 0, 1},
											  {RunKind::Line, 0, 1}}));
}

TEST(DrawRuns, sameTypeKeepsSubmissionOrderAndMergesWhenTouching) {
	Submitter sub;
	sub.add(RunKind::Quad, 0.f);
	sub.add(RunKind::Circle, 1.f);
	sub.add(RunKind::Quad, 0.f);
	sub.add(RunKind::Quad, 1.f);
	// The quads of depth 0 then the quad of depth 1 touch in the buffer: one draw; then the circle.
	EXPECT_EQ(sub.draws(), (std::vector<Draw>{{RunKind::Quad, 0, 3}, {RunKind::Circle, 0, 1}}));
}

TEST(DrawRuns, firstDepthSurvivesTheSwitchToRuns) {
	Submitter sub;
	sub.add(RunKind::Quad, 0.f);
	sub.add(RunKind::Circle, 0.f);
	sub.add(RunKind::Quad, 0.f);
	sub.add(RunKind::Quad, -1.f);
	// The quad of depth -1 first, then the two of depth 0 (touching), then the circle.
	EXPECT_EQ(sub.draws(), (std::vector<Draw>{{RunKind::Quad, 2, 1}, {RunKind::Quad, 0, 2}, {RunKind::Circle, 0, 1}}));
}

TEST(DrawRuns, clearForgetsThePrimitives) {
	Submitter sub;
	sub.add(RunKind::Quad, 1.f);
	sub.add(RunKind::Quad, 0.f);
	sub.runs.clear();
	sub.add(RunKind::Circle);
	EXPECT_EQ(sub.draws(), (std::vector<Draw>{{RunKind::Circle, 0, 1}}));
}

TEST(DrawRuns, drawingTwiceGivesTheSameDraws) {
	Submitter sub;
	sub.add(RunKind::Text, 1.f);
	sub.add(RunKind::Quad, 0.f);
	EXPECT_EQ(sub.draws(), sub.draws());
}
