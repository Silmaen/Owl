/**
 * @file Conversation.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/dialogue/Dialogue.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

namespace owl::data::dialogue {

/**
 * @brief
 *  The Lua side of a conversation: how conditions are evaluated and actions run.
 */
struct OWL_API ConversationHooks {
	/// Evaluates a condition function by name; unset: every condition holds.
	std::function<bool(std::string_view)> condition;
	/// Runs an action function by name; unset: actions do nothing.
	std::function<void(std::string_view)> action;
};

/**
 * @brief
 *  A dialogue being played: it rests on a Line or a Choice node, Start, Branch and Event nodes are passed through.
 */
class OWL_API Conversation final {
public:
	/// Nodes passed through in one step before the conversation is stopped as a loop without speech.
	static constexpr size_t maxSteps = 1024;

	/**
	 * @brief
	 *  A conversation over a dialogue, not started.
	 * @param[in] iDialogue The dialogue played.
	 */
	explicit Conversation(shared<const Dialogue> iDialogue);

	/**
	 * @brief
	 *  Go from the Start node to the first Line or Choice.
	 * @param[in] iHooks Conditions and actions met on the way.
	 * @return True when the conversation rests on a node to show, false when it ended at once.
	 */
	auto start(const ConversationHooks& iHooks) -> bool;

	/**
	 * @brief
	 *  Leave the current Line for the node after it.
	 * @param[in] iHooks Conditions and actions met on the way.
	 * @return True while the conversation goes on; false when it ended, or rests on a Choice (nothing done).
	 */
	auto advance(const ConversationHooks& iHooks) -> bool;

	/**
	 * @brief
	 *  Pick an offered option of the current Choice.
	 * @param[in] iOffered Index in `getOfferedOptions`.
	 * @param[in] iHooks Conditions and actions met on the way.
	 * @return True while the conversation goes on; false when it ended, or the index or the node is not a choice.
	 */
	auto choose(size_t iOffered, const ConversationHooks& iHooks) -> bool;

	/**
	 * @brief
	 *  End the conversation where it is.
	 */
	void stop();

	/**
	 * @brief
	 *  Whether the conversation rests on a node to show.
	 * @return True until it ends or is stopped.
	 */
	[[nodiscard]] auto isRunning() const -> bool { return mp_current != nullptr; }

	/**
	 * @brief
	 *  The Line or Choice node shown.
	 * @return The node, null when the conversation is over.
	 */
	[[nodiscard]] auto getCurrent() const -> const DialogueNode* { return mp_current; }

	/**
	 * @brief
	 *  Options of the current Choice whose condition held when it was reached.
	 * @return Indices into the options of the node, empty on a Line.
	 */
	[[nodiscard]] auto getOfferedOptions() const -> std::span<const size_t> { return m_offered; }

	/**
	 * @brief
	 *  The dialogue played.
	 * @return The dialogue.
	 */
	[[nodiscard]] auto getDialogue() const -> const shared<const Dialogue>& { return m_dialogue; }

private:
	/**
	 * @brief
	 *  Walk from a node to the next one to show.
	 * @param[in] iFrom The first node.
	 * @param[in] iHooks Conditions and actions met on the way.
	 * @return True when a node to show was reached.
	 */
	auto walk(core::UUID iFrom, const ConversationHooks& iHooks) -> bool;

	/// The dialogue played.
	shared<const Dialogue> m_dialogue;
	/// The node shown, null when over.
	const DialogueNode* mp_current = nullptr;
	/// Options offered by the current Choice.
	std::vector<size_t> m_offered;
	/// Raised by every stop, so a walk sees an action that moved the conversation.
	uint64_t m_generation = 0;
};

}// namespace owl::data::dialogue
