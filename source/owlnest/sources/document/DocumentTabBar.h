/**
 * @file DocumentTabBar.h
 * @author Silmaen
 * @date 18/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "DocumentManager.h"

namespace owl::nest {

/**
 * @brief Top tab bar that lists every open document and tracks the active one.
 *
 * Renders an ImGui window containing a tab bar. Each tab shows the document
 * title (with a `*` when dirty), an optional play/pause badge for the scene
 * currently running, and a close button that pops a confirmation when the doc
 * is dirty. Clicking a tab makes it the active document. Pending close decisions
 * are stored internally and applied from the caller each frame.
 */
class DocumentTabBar final {
public:
	DocumentTabBar(const DocumentTabBar&) = delete;
	DocumentTabBar(DocumentTabBar&&) = delete;
	auto operator=(const DocumentTabBar&) -> DocumentTabBar& = delete;
	auto operator=(DocumentTabBar&&) -> DocumentTabBar& = delete;
	DocumentTabBar() = default;
	~DocumentTabBar() = default;

	/// @brief Render the tab bar and handle activation / close / prompt.
	/// @return The id of a document the user asked to close (and confirmed), or 0 if none.
	auto onImGuiRender(DocumentManager& ioManager) -> core::UUID;

private:
	/// Id of the document requested for close; 0 if no close in flight.
	core::UUID m_pendingClose{0};
	/// True when the confirmation popup must be opened on the next frame.
	bool m_showClosePrompt = false;
};

}// namespace owl::nest
