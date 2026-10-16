/**
 * @file PathQueue.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/nav/PathQueue.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>

namespace owl::scene::nav {

PathQueue::PathQueue() : mp_finder{mkUniq<Pathfinder>()} {}

PathQueue::~PathQueue() = default;

PathQueue::PathQueue(PathQueue&&) noexcept = default;

auto PathQueue::operator=(PathQueue&&) noexcept -> PathQueue& = default;

auto PathQueue::request(const math::vec2ui& iFrom, const math::vec2ui& iTo, const PathMode iMode,
						const PathOptions& iOptions) -> PathRequestId {
	auto slot = std::ranges::find_if(m_slots, [](const Slot& iSlot) -> bool { return iSlot.id == 0; });
	if (slot == m_slots.end())
		slot = m_slots.insert(m_slots.end(), Slot{});
	if (++m_lastId == 0)
		++m_lastId;
	slot->id = m_lastId;
	slot->status = PathStatus::Pending;
	slot->mode = iMode;
	slot->options = iOptions;
	slot->from = iFrom;
	slot->to = iTo;
	slot->result.cells.clear();
	slot->result.cost = 0;
	return slot->id;
}

auto PathQueue::findSlot(const PathRequestId iId) const -> const Slot* {
	if (iId == 0)
		return nullptr;
	const auto found = std::ranges::find_if(m_slots, [iId](const Slot& iSlot) -> bool { return iSlot.id == iId; });
	return found == m_slots.end() ? nullptr : &*found;
}

auto PathQueue::getStatus(const PathRequestId iId) const -> PathStatus {
	const auto* slot = findSlot(iId);
	return slot == nullptr ? PathStatus::Unknown : slot->status;
}

auto PathQueue::getResult(const PathRequestId iId) const -> const PathResult* {
	const auto* slot = findSlot(iId);
	return slot != nullptr && slot->status == PathStatus::Found ? &slot->result : nullptr;
}

void PathQueue::release(const PathRequestId iId) {
	const auto* slot = findSlot(iId);
	if (slot == nullptr)
		return;
	auto& mutableSlot = m_slots[static_cast<size_t>(slot - m_slots.data())];
	if (m_current == iId)
		m_current = 0;
	mutableSlot.id = 0;
	mutableSlot.status = PathStatus::Unknown;
}

auto PathQueue::getPendingCount() const -> size_t {
	return static_cast<size_t>(std::ranges::count_if(
			m_slots, [](const Slot& iSlot) -> bool { return iSlot.id != 0 && iSlot.status == PathStatus::Pending; }));
}

void PathQueue::clear() {
	for (auto& slot: m_slots) {
		slot.id = 0;
		slot.status = PathStatus::Unknown;
	}
	m_current = 0;
}

auto PathQueue::update(const NavMap& iMap, const uint32_t iBudget) -> uint64_t {
	OWL_PROFILE_FUNCTION()

	uint64_t spent = 0;
	while (spent < iBudget) {
		Slot* active = nullptr;
		for (auto& slot: m_slots) {
			if (slot.id == 0 || slot.status != PathStatus::Pending)
				continue;
			if (active == nullptr || slot.id < active->id)
				active = &slot;
		}
		if (active == nullptr)
			break;
		const bool restart = m_current != active->id || mp_finder->getState() == PathState::Idle ||
							 mp_finder->getMapVersion() != iMap.getVersion();
		if (restart) {
			m_current = active->id;
			mp_finder->begin(iMap, active->from, active->to, active->mode, active->options);
		}
		const uint64_t before = mp_finder->getWorkDone();
		const auto state = mp_finder->step(static_cast<uint32_t>(iBudget - spent));
		spent += std::max<uint64_t>(mp_finder->getWorkDone() - before, 1);
		if (state == PathState::Found) {
			std::ignore = mp_finder->takeResult(active->result);
			active->status = PathStatus::Found;
			m_current = 0;
		} else if (state == PathState::NotFound) {
			active->status = PathStatus::NotFound;
			m_current = 0;
		}
	}
	return spent;
}

}// namespace owl::scene::nav
