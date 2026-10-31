/**
 * @file SequenceLibrary.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/sequence/Sequence.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace owl::data::sequence {

/**
 * @brief
 *  The `.owlseq` sequences read so far, shared by the players of every scene.
 */
class OWL_API SequenceLibrary final {
public:
	/**
	 * @brief
	 *  Build an empty library.
	 */
	SequenceLibrary() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~SequenceLibrary() = default;

	SequenceLibrary(const SequenceLibrary&) = delete;

	SequenceLibrary(SequenceLibrary&&) = delete;

	auto operator=(const SequenceLibrary&) -> SequenceLibrary& = delete;

	auto operator=(SequenceLibrary&&) -> SequenceLibrary& = delete;

	/**
	 * @brief
	 *  A sequence, read on first use.
	 * @param[in] iAsset Asset path or `gid:` reference of the sequence.
	 * @return The sequence, or null when it cannot be read (the failure is kept until `invalidate`).
	 */
	[[nodiscard]] auto get(std::string_view iAsset) -> shared<const Sequence>;

	/**
	 * @brief
	 *  Forget a sequence: the next `get` reads its file again; players keep the copy they hold.
	 * @param[in] iAsset Asset path or `gid:` reference of the sequence.
	 */
	void invalidate(std::string_view iAsset);

	/**
	 * @brief
	 *  Forget every sequence.
	 */
	void clear();

	/**
	 * @brief
	 *  Number of files read since the library was built.
	 * @return The count.
	 */
	[[nodiscard]] auto getLoadCount() const -> uint64_t;

private:
	/// Guards the sequences.
	mutable std::mutex m_mutex;
	/// Sequences by asset path (null for a file that failed to load).
	std::unordered_map<std::string, shared<const Sequence>> m_sequences;
	/// Number of files read.
	uint64_t m_loadCount = 0;
};

}// namespace owl::data::sequence
