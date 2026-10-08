/**
 * @file RecoveryPrompt.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../RecoveryManager.h"

#include <cstdint>
#include <vector>

namespace owl::nest::panel {

/**
 * @brief
 *  Modal offering to recover the documents autosaved by a session that ended with unsaved changes (crash or quit).
 */
class RecoveryPrompt final {
public:
	/**
	 * @brief
	 *  What the user chose in the modal.
	 */
	enum struct Choice : uint8_t {
		None,///< No choice this frame (modal closed or still open).
		Recover,///< Reopen the documents with their autosaved content.
		Discard,///< Delete the autosaved content.
	};

	/**
	 * @brief
	 *  Show the modal for these entries.
	 * @param[in] iEntries The autosaved documents.
	 */
	void open(const std::vector<RecoveryEntry>& iEntries);

	/**
	 * @brief
	 *  Check whether the modal waits for a choice.
	 * @return True while open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_open; }

	/**
	 * @brief
	 *  The entries shown by the modal.
	 * @return The entries.
	 */
	[[nodiscard]] auto getEntries() const -> const std::vector<RecoveryEntry>& { return m_entries; }

	/**
	 * @brief
	 *  Draw the modal.
	 * @return The choice made this frame.
	 */
	auto onImGuiRender() -> Choice;

private:
	/// The autosaved documents.
	std::vector<RecoveryEntry> m_entries;
	/// True while the modal waits for a choice.
	bool m_open = false;
	/// True until the ImGui popup is opened.
	bool m_openRequested = false;
};

}// namespace owl::nest::panel
