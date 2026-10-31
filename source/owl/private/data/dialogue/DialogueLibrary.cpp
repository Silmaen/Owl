/**
 * @file DialogueLibrary.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/dialogue/DialogueLibrary.h"

#include "data/assets/AssetReference.h"

#include <cstdint>
#include <mutex>
#include <string>

namespace owl::data::dialogue {

auto DialogueLibrary::get(const std::string_view iAsset) -> shared<const Dialogue> {
	const std::string path = data::assets::toAssetPath(iAsset);
	const std::scoped_lock<std::mutex> lock{m_mutex};
	if (const auto found = m_dialogues.find(path); found != m_dialogues.end())
		return found->second;
	++m_loadCount;
	shared<const Dialogue> dialogue;
	if (auto loaded = Dialogue::load(path); loaded)
		dialogue = mkShared<const Dialogue>(std::move(*loaded));
	m_dialogues.emplace(path, dialogue);
	return dialogue;
}

void DialogueLibrary::invalidate(const std::string_view iAsset) {
	const std::string path = data::assets::toAssetPath(iAsset);
	const std::scoped_lock<std::mutex> lock{m_mutex};
	m_dialogues.erase(path);
}

void DialogueLibrary::invalidateAll() {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	m_dialogues.clear();
}

void DialogueLibrary::clear() {
	invalidateAll();
	for (auto& [id, conversation]: m_conversations) conversation->stop();
	collect();
}

auto DialogueLibrary::start(const std::string_view iAsset, const ConversationHooks& iHooks) -> std::optional<uint64_t> {
	auto dialogue = get(iAsset);
	if (!dialogue)
		return std::nullopt;
	const uint64_t id = m_nextId++;
	auto& conversation = m_conversations.emplace(id, mkUniq<Conversation>(std::move(dialogue))).first->second;
	++m_busy;
	const bool running = conversation->start(iHooks);
	--m_busy;
	collect();
	if (!running)
		return std::nullopt;
	return id;
}

auto DialogueLibrary::find(const uint64_t iId) -> Conversation* {
	const auto found = m_conversations.find(iId);
	return found == m_conversations.end() || !found->second->isRunning() ? nullptr : found->second.get();
}

auto DialogueLibrary::advance(const uint64_t iId, const ConversationHooks& iHooks) -> bool {
	auto* conversation = find(iId);
	if (conversation == nullptr)
		return false;
	++m_busy;
	conversation->advance(iHooks);
	--m_busy;
	collect();
	return find(iId) != nullptr;
}

auto DialogueLibrary::choose(const uint64_t iId, const size_t iOffered, const ConversationHooks& iHooks) -> bool {
	auto* conversation = find(iId);
	if (conversation == nullptr)
		return false;
	++m_busy;
	conversation->choose(iOffered, iHooks);
	--m_busy;
	collect();
	return find(iId) != nullptr;
}

void DialogueLibrary::stop(const uint64_t iId) {
	if (const auto found = m_conversations.find(iId); found != m_conversations.end())
		found->second->stop();
	collect();
}

auto DialogueLibrary::getConversations() const -> std::vector<uint64_t> {
	std::vector<uint64_t> ids;
	for (const auto& [id, conversation]: m_conversations)
		if (conversation->isRunning())
			ids.push_back(id);
	return ids;
}

void DialogueLibrary::collect() {
	if (m_busy > 0)
		return;
	std::erase_if(m_conversations, [](const auto& iEntry) -> bool { return !iEntry.second->isRunning(); });
}

}// namespace owl::data::dialogue
