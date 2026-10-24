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
 * Reads the bundled help index produced by `cmake/HelpAssets.cmake` (`index.yml`: every page in sidebar order, each
 * with its group) and renders the selected `.md` page through `codeEditor::MarkdownPreview`.
 *
 * - The sidebar shows the groups in the order of the index, the pages in their order inside each group.
 * - `open()` shows the panel without changing the current page; the start page is the first page of the index.
 * - `open(id)` shows the panel and navigates to a specific page id (the
 *   basename of the source file without extension, e.g. `editor`).
 * - Internal links of the form `[text](other.md#anchor)` navigate within the panel, to the top of the page or to the
 *   heading of the anchor; external links (http(s)://) open in the browser.
 * - Back and Forward work like a web browser, the page coming back at its former scroll position: the toolbar
 *   buttons, the mouse side buttons (X1 back, X2 forward) and Alt+Left / Alt+Right while the panel is hovered or
 *   focused.
 */
class HelpPanel final {
public:
	/**
	 * @brief
	 *  One page of the help index.
	 */
	struct HelpPage {
		std::string id;///< Basename without extension, used as canonical id.
		std::string title;///< First H1 line of the page (Doxygen anchor stripped).
		std::string group;///< Sidebar group of the page.
		std::string path;///< File name relative to the help root.
	};

	/**
	 * @brief
	 *  A link of a help page split into the page it leads to and the anchor in it.
	 */
	struct LinkTarget {
		std::string pageId;///< Page id (file name without folder and extension), empty for the current page.
		std::string anchor;///< Heading anchor, empty for the top of the page.
	};

	/**
	 * @brief
	 *  Split a link of a help page: `scene.md#fixed-tick`, `../roadmap.md`, `#components`.
	 * @param[in] iHref The link target.
	 * @return The page id and the anchor.
	 */
	[[nodiscard]] static auto parseLink(const std::string& iHref) -> LinkTarget;

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

	/**
	 * @brief
	 *  Locate the help bundle of this editor: the bundle of its build tree (`OWL_HELP_BUILD_DIR`), else `help/` in an
	 *  asset directory (`assets/help` of a package).
	 * @return The help root, empty when none holds an `index.yml`.
	 */
	[[nodiscard]] static auto resolveHelpRoot() -> std::filesystem::path;

	/**
	 * @brief
	 *  Pick the help bundle among candidates: the build bundle first, so a development build always reads the pages
	 *  it bundled, then the `help/` folder of the asset directories in their order.
	 * @param[in] iBuildBundle The bundle of the build tree, empty when unknown.
	 * @param[in] iAssetDirs The asset directories, highest priority first.
	 * @return The first candidate holding an `index.yml`, empty when none does.
	 */
	[[nodiscard]] static auto resolveHelpRoot(const std::filesystem::path& iBuildBundle,
											  const std::vector<std::filesystem::path>& iAssetDirs)
			-> std::filesystem::path;

	/**
	 * @brief
	 *  Read a help index.
	 * @param[in] iIndexFile The `index.yml` file.
	 * @return Its pages in file order (the sidebar order), empty when the file is missing or malformed.
	 */
	[[nodiscard]] static auto readIndex(const std::filesystem::path& iIndexFile) -> std::vector<HelpPage>;

private:
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
	 *  Push the current page onto the back-stack and switch to a page, at its top or at an anchor.
	 * @param[in] iId The page id, empty for the current page.
	 * @param[in] iAnchor The heading anchor, empty for the top of the page.
	 */
	void navigateTo(const std::string& iId, const std::string& iAnchor = {});

	/**
	 * @brief
	 *  Go back to the previous page of the history, at its former scroll position.
	 */
	void goBack();

	/**
	 * @brief
	 *  Go forward to the page left by `goBack`, at its former scroll position.
	 */
	void goForward();

	/**
	 * @brief
	 *  Draw the navigation bar: Back, Forward, Home, then the group and title of the page.
	 */
	void renderNavigationBar();

	/**
	 * @brief
	 *  One step of the history: a page and its scroll position.
	 */
	struct HistoryEntry {
		std::string pageId;///< Page id.
		float scrollY = 0.0f;///< Scroll position when the page was left.
	};

	bool m_visible = false;///< Whether the panel window is shown.
	std::filesystem::path m_helpRoot;///< Folder of the bundle in use.
	std::vector<HelpPage> m_pages;///< Pages of the index, in sidebar order.
	std::string m_currentPageId;///< Page shown, empty when none.
	std::string m_currentContent;///< Markdown of the page shown.
	std::vector<HistoryEntry> m_backStack;///< Pages left by navigation, most recent last.
	std::vector<HistoryEntry> m_forwardStack;///< Pages left by Back, most recent last.
	std::string m_search;///< Sidebar filter on page titles.
	codeEditor::MarkdownPreview m_renderer;///< Renderer of the page.
	/**
	 * Horizontal split ratio between the page tree (left) and content (right). Clamped
	 * at render time to keep both sides at least `kMinSideW` pixels wide.
	 */
	float m_splitRatio = 0.30f;
};

}// namespace owl::nest::panel
