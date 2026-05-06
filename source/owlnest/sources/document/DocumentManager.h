/**
 * @file DocumentManager.h
 * @author Silmaen
 * @date 18/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"

namespace owl::nest {
/**
 * @brief
 *  Holds the list of open documents and tracks the active one.
 *
 * The active document is the one that global panels (scene hierarchy,
 * inspector, toolbar) target. It is updated from the viewport focus change
 * (Phase 2) or from the last `open()` call (Phase 1).
 */
class DocumentManager final {
public:
	DocumentManager(const DocumentManager&) = delete;

	DocumentManager(DocumentManager&&) = delete;

	auto operator=(const DocumentManager&) -> DocumentManager& = delete;

	auto operator=(DocumentManager&&) -> DocumentManager& = delete;

	DocumentManager() = default;

	~DocumentManager() = default;

	/**
	 * @brief
	 *  Add a document to the manager. Takes ownership. Returns a stable pointer.
	 */
	auto add(uniq<Document> iDocument) -> Document*;

	/**
	 * @brief
	 *  Remove the document with the given id. Returns true if it was removed.
	 */
	auto remove(core::UUID iId) -> bool;

	/**
	 * @brief
	 *  Remove and destroy every document (called on project close / editor detach).
	 */
	void clear();

	/**
	 * @brief
	 *  Make the document with this id the active one. No-op if not found.
	 */
	void setActive(core::UUID iId);

	/**
	 * @brief
	 *  Directly mark the given document as active (pointer must belong to this manager).
	 */
	void setActive(Document* iDocument);

	/**
	 * @brief
	 *  The currently active document, or nullptr if none.
	 */
	[[nodiscard]] auto getActive() const -> Document* { return mp_active; }

	/**
	 * @brief
	 *  Lookup by id. Returns nullptr if not found.
	 */
	[[nodiscard]] auto find(core::UUID iId) const -> Document*;

	/**
	 * @brief
	 *  Access the full list of open documents.
	 */
	[[nodiscard]] auto list() const -> const std::vector<uniq<Document>>& { return m_documents; }

	/**
	 * @brief
	 *  Number of open documents.
	 */
	[[nodiscard]] auto size() const -> size_t { return m_documents.size(); }

	/**
	 * @brief
	 *  True when no document is open.
	 */
	[[nodiscard]] auto empty() const -> bool { return m_documents.empty(); }

private:
	/// All open documents, owned by this manager.
	std::vector<uniq<Document>> m_documents;
	/// Currently active document (the one displayed in the front tab); may be null.
	Document* mp_active = nullptr;
};

}// namespace owl::nest
