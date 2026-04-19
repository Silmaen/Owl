/**
 * @file DocumentManager.cpp
 * @author Silmaen
 * @date 18/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "DocumentManager.h"

#include <algorithm>

namespace owl::nest {

auto DocumentManager::add(uniq<Document> iDocument) -> Document* {
	if (!iDocument)
		return nullptr;
	auto* raw = iDocument.get();
	m_documents.push_back(std::move(iDocument));
	mp_active = raw;
	return raw;
}

auto DocumentManager::remove(const core::UUID iId) -> bool {
	const auto it = std::ranges::find_if(
			m_documents, [iId](const uniq<Document>& iDoc) { return iDoc && iDoc->id() == iId; });
	if (it == m_documents.end())
		return false;
	const bool wasActive = (mp_active == it->get());
	(*it)->onDetach();
	m_documents.erase(it);
	if (wasActive)
		mp_active = m_documents.empty() ? nullptr : m_documents.back().get();
	return true;
}

void DocumentManager::clear() {
	for (const auto& doc: m_documents) {
		if (doc)
			doc->onDetach();
	}
	m_documents.clear();
	mp_active = nullptr;
}

void DocumentManager::setActive(const core::UUID iId) {
	if (auto* doc = find(iId); doc != nullptr)
		mp_active = doc;
}

void DocumentManager::setActive(Document* iDocument) {
	if (iDocument == nullptr) {
		mp_active = nullptr;
		return;
	}
	for (const auto& doc: m_documents) {
		if (doc.get() == iDocument) {
			mp_active = iDocument;
			return;
		}
	}
}

auto DocumentManager::find(const core::UUID iId) const -> Document* {
	const auto it = std::ranges::find_if(
			m_documents, [iId](const uniq<Document>& iDoc) { return iDoc && iDoc->id() == iId; });
	return (it == m_documents.end()) ? nullptr : it->get();
}

}// namespace owl::nest
