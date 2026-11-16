/**
 * @file InterestManager_test.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <network/InterestManager.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <vector>

using namespace owl;
using namespace owl::network;

TEST(InterestManager, AnEntityIsRelevantNearAViewpointOrEverywhere) {
	const std::array<math::vec3, 1> viewpoint{math::vec3{0.f, 0.f, 0.f}};
	EXPECT_TRUE(InterestManager::isRelevant({.entity = 1, .position = {3.f, 0.f, 0.f}, .relevance = 5.f}, viewpoint));
	EXPECT_FALSE(InterestManager::isRelevant({.entity = 1, .position = {6.f, 0.f, 0.f}, .relevance = 5.f}, viewpoint));
	EXPECT_TRUE(InterestManager::isRelevant({.entity = 1, .position = {600.f, 0.f, 0.f}, .relevance = 0.f}, viewpoint));
	EXPECT_TRUE(InterestManager::isRelevant({.entity = 1, .position = {600.f, 0.f, 0.f}, .relevance = 5.f}, {}));
}

TEST(InterestManager, TheBudgetKeepsTheMostOverdue) {
	InterestManager manager;
	const std::array<InterestCandidate, 3> candidates{InterestCandidate{.entity = 1, .priority = 1.f},
													  InterestCandidate{.entity = 2, .priority = 3.f},
													  InterestCandidate{.entity = 3, .priority = 1.f}};
	const auto first = manager.select(7, candidates, {}, 1);
	EXPECT_EQ(first, std::vector<uint64_t>{2});
	EXPECT_EQ(manager.getRelevant(7), (std::vector<uint64_t>{1, 2, 3}));
}

TEST(InterestManager, NoRelevantEntityStarves) {
	InterestManager manager;
	std::vector<InterestCandidate> candidates;
	for (uint64_t entity = 1; entity <= 10; ++entity)
		candidates.push_back({.entity = entity, .priority = entity == 1 ? 10.f : 1.f});
	std::map<uint64_t, int> sent;
	for (int round = 0; round < 40; ++round) {
		for (const auto entity: manager.select(1, candidates, {}, 2)) ++sent[entity];
	}
	EXPECT_EQ(sent.size(), 10U);
	EXPECT_GT(sent[1], sent[2]);
}

TEST(InterestManager, AnEntityOutOfRangeIsLeftOut) {
	InterestManager manager;
	const std::array<math::vec3, 1> viewpoint{math::vec3{0.f, 0.f, 0.f}};
	const std::array<InterestCandidate, 2> candidates{
			InterestCandidate{.entity = 1, .position = {1.f, 0.f, 0.f}, .relevance = 2.f},
			InterestCandidate{.entity = 2, .position = {50.f, 0.f, 0.f}, .relevance = 2.f}};
	EXPECT_EQ(manager.select(4, candidates, viewpoint, 8), std::vector<uint64_t>{1});
	manager.forget(4);
	EXPECT_TRUE(manager.getRelevant(4).empty());
}
