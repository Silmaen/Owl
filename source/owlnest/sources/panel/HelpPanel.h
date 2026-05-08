/**
 * @file HelpPanel.h
 * @author Silmaen
 * @date 28/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../document/codeEditor/MarkdownPreview.h"

#include <core/Timestep.h>

#include <filesystem>
#include <string>
#include <vector>

namespace owl::nest::panel {
/**
 * @brief
 *  Dockable in-editor help browser.
 *
 * Reads the bundled help index produced by `cmake/HelpAssets.cmake` (one
 * `engine_assets/help/index.yml` describing every page) and renders the
 * selected `.md` page through `codeEditor::MarkdownPreview`.
 *
 * - `open()` shows the panel without changing the current page.
 * - `open(id)` shows the panel and navigates to a specific page id (the
 *   basename of the source file without extension, e.g. `editor`).
 * - Internal links of the form `[text](other.md)` navigate within the
 *   panel; external links (http(s)://) are logged via `OWL_INFO` for now.
 */
class HelpPanel final {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	HelpPanel();

	~HelpPanel() = default;

	HelpPanel(const HelpPanel&) = delete;

	HelpPanel(HelpPanel&&) = delete;

	auto operator=(const HelpPanel&) -> HelpPanel& = delete;

	auto operator=(HelpPanel&&) -> HelpPanel& = delete;

	/**
	 * @brief
	 *  Show the panel; the current page (or the default landing page) is kept.
	 */
	void open();

	/**
	 * @brief
	 *  Show the panel and navigate to the given page id.
	 */
	void open(const std::string& iPageId);

	/**
	 * @brief
	 *  Render the panel. Call every frame.
	 * @param[in] iTimeStep Frame timestep (used by the embedded Markdown renderer).
	 */
	void onImGuiRender(const core::Timestep& iTimeStep);

private:
	struct PageEntry {
		std::string id;///< Basename without extension, used as canonical id.
		std::string title;///< First H1 line of the page (Doxygen anchor stripped).
		std::string category;///< "guides" or "reference".
		std::string path;///< File name relative to engine_assets/help/.
	};

	/**
	 * @brief
	 *  Locate engine_assets/help/ relative to the runtime CWD. Empty on failure.
	 * @return The std filesystem path.
	 */
	[[nodiscard]] static auto resolveHelpRoot() -> std::filesystem::path;

	/**
	 * @brief
	 *  Load (or reload) the index file.
	 */
	void loadIndex();

	/**
	 * @brief
	 *  Load a page by id; navigates to the default landing page if `iId` is unknown.
	 */
	void loadPage(const std::string& iId);

	/**
	 * @brief
	 *  React to a Markdown link click coming from `MarkdownPreview`.
	 */
	void onLinkClicked(const std::string& iHref);

	/**
	 * @brief
	 *  Push the current page onto the back-stack and switch to a new page.
	 */
	void navigateTo(const std::string& iId);

	bool m_visible = false;
	std::filesystem::path m_helpRoot;
	std::vector<PageEntry> m_pages;
	std::string m_currentPageId;
	std::string m_currentContent;
	std::vector<std::string> m_backStack;
	std::vector<std::string> m_forwardStack;
	std::string m_search;
	codeEditor::MarkdownPreview m_renderer;
	/**
	 * Horizontal split ratio between the page tree (left) and content (right). Clamped
	 * at render time to keep both sides at least `kMinSideW` pixels wide.
	 */
	float m_splitRatio = 0.30f;
};

}// namespace owl::nest::panel
