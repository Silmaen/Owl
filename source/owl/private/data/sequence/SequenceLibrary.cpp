/**
 * @file SequenceLibrary.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/sequence/SequenceLibrary.h"

#include "data/assets/AssetReference.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace owl::data::sequence {

auto SequenceLibrary::get(const std::string_view iAsset) -> shared<const Sequence> {
	const std::string path = data::assets::toAssetPath(iAsset);
	const std::scoped_lock<std::mutex> lock{m_mutex};
	if (const auto found = m_sequences.find(path); found != m_sequences.end())
		return found->second;
	++m_loadCount;
	shared<const Sequence> sequence;
	if (auto loaded = Sequence::load(path); loaded)
		sequence = mkShared<const Sequence>(std::move(*loaded));
	m_sequences.emplace(path, sequence);
	return sequence;
}

void SequenceLibrary::invalidate(const std::string_view iAsset) {
	const std::string path = data::assets::toAssetPath(iAsset);
	const std::scoped_lock<std::mutex> lock{m_mutex};
	m_sequences.erase(path);
}

void SequenceLibrary::clear() {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	m_sequences.clear();
}

auto SequenceLibrary::getLoadCount() const -> uint64_t {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	return m_loadCount;
}

}// namespace owl::data::sequence
