/**
 * @file DialogueLibrary.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/dialogue/Conversation.h"
#include "data/dialogue/Dialogue.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace owl::data::dialogue {

/**
 * @brief
 *  Dialogues of the running game, read once, and the conversations the scripts play, by number.
 *
 * The dialogues are guarded for several readers; the conversations belong to the thread running the scripts.
 */
class OWL_API DialogueLibrary final {
public:
	/**
	 * @brief
	 *  Build an empty library.
	 */
	DialogueLibrary() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~DialogueLibrary() = default;

	DialogueLibrary(const DialogueLibrary&) = delete;

	DialogueLibrary(DialogueLibrary&&) = delete;

	auto operator=(const DialogueLibrary&) -> DialogueLibrary& = delete;

	auto operator=(DialogueLibrary&&) -> DialogueLibrary& = delete;

	/**
	 * @brief
	 *  A dialogue, read on first use.
	 * @param[in] iAsset Asset path or `gid:` reference of the dialogue.
	 * @return The dialogue, or null when it cannot be read (the failure is kept until `invalidate`).
	 */
	[[nodiscard]] auto get(std::string_view iAsset) -> shared<const Dialogue>;

	/**
	 * @brief
	 *  Forget a dialogue: the next `get` reads its file again; conversations already playing keep their copy.
	 * @param[in] iAsset Asset path or `gid:` reference of the dialogue.
	 */
	void invalidate(std::string_view iAsset);

	/**
	 * @brief
	 *  Forget every dialogue, so the next `get` reads the files again; conversations playing keep their copy.
	 */
	void invalidateAll();

	/**
	 * @brief
	 *  Forget every dialogue and stop every conversation.
	 */
	void clear();

	/**
	 * @brief
	 *  Start a conversation over a dialogue.
	 * @param[in] iAsset Asset path or `gid:` reference of the dialogue.
	 * @param[in] iHooks Conditions and actions met on the way to its first line.
	 * @return The number of the conversation, or nothing when the dialogue cannot be read or ends at once.
	 */
	[[nodiscard]] auto start(std::string_view iAsset, const ConversationHooks& iHooks) -> std::optional<uint64_t>;

	/**
	 * @brief
	 *  A conversation by number.
	 * @param[in] iId The number `start` gave.
	 * @return The conversation, null once it ended or was stopped.
	 */
	[[nodiscard]] auto find(uint64_t iId) -> Conversation*;

	/**
	 * @brief
	 *  Show the next line of a conversation.
	 * @param[in] iId The number of the conversation.
	 * @param[in] iHooks Conditions and actions met on the way.
	 * @return True while it goes on; an ended conversation is forgotten.
	 */
	auto advance(uint64_t iId, const ConversationHooks& iHooks) -> bool;

	/**
	 * @brief
	 *  Pick an offered option of a conversation resting on a Choice.
	 * @param[in] iId The number of the conversation.
	 * @param[in] iOffered Index of the option among the offered ones.
	 * @param[in] iHooks Conditions and actions met on the way.
	 * @return True while it goes on; an ended conversation is forgotten.
	 */
	auto choose(uint64_t iId, size_t iOffered, const ConversationHooks& iHooks) -> bool;

	/**
	 * @brief
	 *  Stop a conversation and forget it.
	 * @param[in] iId The number of the conversation.
	 */
	void stop(uint64_t iId);

	/**
	 * @brief
	 *  Numbers of the conversations playing, oldest first.
	 * @return The numbers.
	 */
	[[nodiscard]] auto getConversations() const -> std::vector<uint64_t>;

	/**
	 * @brief
	 *  Number of files read.
	 * @return The count.
	 */
	[[nodiscard]] auto getLoadCount() const -> uint64_t { return m_loadCount; }

private:
	/**
	 * @brief
	 *  Forget the conversations that ended, once no step of a conversation is under way.
	 */
	void collect();

	/// Guards the dialogues.
	mutable std::mutex m_mutex;
	/// Dialogues by asset path (null for a file that failed to load).
	std::unordered_map<std::string, shared<const Dialogue>> m_dialogues;
	/// Conversations by number; boxed so a hook that starts another one keeps the current one in place.
	std::map<uint64_t, uniq<Conversation>> m_conversations;
	/// Number the next conversation gets.
	uint64_t m_nextId = 1;
	/// Steps under way: a hook may stop the conversation that runs it, which is then forgotten afterwards.
	uint32_t m_busy = 0;
	/// Number of files read.
	uint64_t m_loadCount = 0;
};

}// namespace owl::data::dialogue
