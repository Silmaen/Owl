/**
 * @file Conversation.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/dialogue/Conversation.h"

#include <cstdint>
#include <utility>

namespace owl::data::dialogue {

namespace {

auto holds(const ConversationHooks& iHooks, const std::string& iCondition) -> bool {
	return iCondition.empty() || !iHooks.condition || iHooks.condition(iCondition);
}

}// namespace

Conversation::Conversation(shared<const Dialogue> iDialogue) : m_dialogue{std::move(iDialogue)} {}

auto Conversation::start(const ConversationHooks& iHooks) -> bool {
	const auto* start = m_dialogue ? m_dialogue->findStart() : nullptr;
	if (start == nullptr) {
		OWL_CORE_WARN("Conversation: The dialogue has no Start node.")
		stop();
		return false;
	}
	return walk(start->id, iHooks);
}

auto Conversation::advance(const ConversationHooks& iHooks) -> bool {
	if (mp_current == nullptr || mp_current->kind != NodeKind::Line)
		return false;
	return walk(mp_current->next, iHooks);
}

auto Conversation::choose(const size_t iOffered, const ConversationHooks& iHooks) -> bool {
	if (mp_current == nullptr || mp_current->kind != NodeKind::Choice || iOffered >= m_offered.size())
		return false;
	return walk(mp_current->options[m_offered[iOffered]].next, iHooks);
}

void Conversation::stop() {
	mp_current = nullptr;
	m_offered.clear();
	++m_generation;
}

auto Conversation::walk(core::UUID iFrom, const ConversationHooks& iHooks) -> bool {
	stop();
	const uint64_t generation = m_generation;
	for (size_t step = 0; step < maxSteps; ++step) {
		const auto* node = m_dialogue->findNode(iFrom);
		if (node == nullptr)
			return false;
		switch (node->kind) {
			case NodeKind::Start:
				iFrom = node->next;
				break;
			case NodeKind::Branch:
				iFrom = holds(iHooks, node->function) ? node->next : node->nextFalse;
				break;
			case NodeKind::Event:
				if (!node->function.empty() && iHooks.action) {
					iHooks.action(node->function);
					// The action stopped or moved the conversation itself: its move wins.
					if (m_generation != generation)
						return isRunning();
				}
				iFrom = node->next;
				break;
			case NodeKind::End:
				return false;
			case NodeKind::Line:
				mp_current = node;
				return true;
			case NodeKind::Choice:
				for (size_t index = 0; index < node->options.size(); ++index)
					if (holds(iHooks, node->options[index].condition))
						m_offered.push_back(index);
				if (m_offered.empty()) {
					OWL_CORE_WARN("Conversation: No option of choice {} is offered, the conversation ends.",
								  static_cast<uint64_t>(node->id))
					return false;
				}
				mp_current = node;
				return true;
		}
	}
	OWL_CORE_WARN("Conversation: {} nodes passed without a line, the conversation is stopped.", maxSteps)
	return false;
}

}// namespace owl::data::dialogue
