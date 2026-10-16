/**
 * @file NavWorld.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/nav/NavWorld.h"

#include "scene/Scene.h"
#include "scene/TilemapAsset.h"
#include "scene/component/ID.h"
#include "scene/component/Tilemap.h"

#include <algorithm>
#include <cstdint>

namespace owl::scene::nav {

NavWorld::NavWorld() = default;

NavWorld::~NavWorld() = default;

auto NavWorld::acquire(const uint64_t iKey, const TilemapAsset& iAsset, const NavProfile& iProfile) -> Entry& {
	auto& entry = m_entries[iKey];
	if (!(entry.map.getProfile() == iProfile))
		entry.map.setProfile(iProfile);
	entry.map.sync(iAsset);
	return entry;
}

auto NavWorld::find(const uint64_t iKey) -> Entry* {
	const auto found = m_entries.find(iKey);
	return found == m_entries.end() ? nullptr : &found->second;
}

void NavWorld::setBudget(const uint32_t iBudget) { m_budget = std::max(iBudget, 1u); }

void NavWorld::clear() { m_entries.clear(); }

void NavWorld::update(Scene& ioScene) {
	OWL_PROFILE_FUNCTION()

	uint64_t left = m_budget;
	for (const auto view = ioScene.registry.view<component::ID, component::Tilemap>(); const auto entity: view) {
		if (left == 0)
			break;
		const auto& [id, tilemap] = view.get<component::ID, component::Tilemap>(entity);
		auto* entry = find(static_cast<uint64_t>(id.id));
		if (entry == nullptr || entry->queue.getPendingCount() == 0 || !tilemap.asset || tilemap.navigation.isEmpty())
			continue;
		acquire(static_cast<uint64_t>(id.id), *tilemap.asset, tilemap.navigation);
		left -= std::min<uint64_t>(left, entry->queue.update(entry->map, static_cast<uint32_t>(left)));
	}
}

}// namespace owl::scene::nav
