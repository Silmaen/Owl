/**
 * @file BehaviourTreeLibrary.cpp
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/ai/BehaviourTreeLibrary.h"

#include "data/assets/AssetReference.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace owl::scene::ai {

auto BehaviourTreeLibrary::get(const std::string_view iAsset) -> shared<const BehaviourTree> {
	const std::string path = data::assets::toAssetPath(iAsset);
	const std::scoped_lock<std::mutex> lock{m_mutex};
	if (const auto found = m_trees.find(path); found != m_trees.end())
		return found->second;
	++m_loadCount;
	shared<const BehaviourTree> tree;
	if (auto loaded = loadAsset(path); loaded)
		tree = mkShared<const BehaviourTree>(std::move(*loaded));
	m_trees.emplace(path, tree);
	return tree;
}

void BehaviourTreeLibrary::invalidate(const std::string_view iAsset) {
	const std::string path = data::assets::toAssetPath(iAsset);
	const std::scoped_lock<std::mutex> lock{m_mutex};
	m_trees.erase(path);
}

void BehaviourTreeLibrary::clear() {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	m_trees.clear();
}

auto BehaviourTreeLibrary::getLoadCount() const -> uint64_t {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	return m_loadCount;
}

}// namespace owl::scene::ai
