/**
 * @file MarkdownPreview.h
 * @author Silmaen
 * @date 28/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "MarkdownDocument.h"

#include <core/Timestep.h>
#include <math/vectors.h>
#include <renderer/gpu/Texture.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class TextEditor;
struct ImVec2;

namespace owl::nest::codeEditor {
/**
 * @brief
 *  Live Markdown renderer used by the code-editor preview pane and the help panel.
 *
 * Parsing is delegated to `MarkdownDocument` (md4c-backed CommonMark + GFM tables /
 * strikethrough / autolinks). Rendering walks the parsed block list and emits ImGui
 * widgets:
 *
 * - headings use the strong font in clear size steps (H1 to H4), with space above them, a rule under H1 (theme
 *   accent) and H2, H3 in the accent colour, H5 / H6 muted; each records the position of its anchor (`{#id}` or GitHub slug) so
 *   `scrollToAnchor()` can reach it;
 * - paragraphs are wrapped text with inline emphasis, strong, strikethrough, code,
 *   and links (internal links, in the accent colour, go through `LinkCallback`; external `http(s)://` links, in
 *   the info colour with an external-link glyph, open in the user's default browser); a hovered link is underlined,
 *   brighter, and shows the hand cursor;
 * - `![label](badge:rrggbb)` images (the status badges of the help bundle) are drawn as coloured tags;
 * - code blocks are rendered as a read-only `TextEditor` with syntax highlighting
 *   (one cached `TextEditor` per code block);
 * - tables are rendered with `ImGui::BeginTable` (borders + row stripes); a cell holding only images shows them at
 *   the column width (a head row of images is drawn as a body row);
 * - local images (SVG / PNG / JPG, paths relative to `setBaseDirectory()`) are
 *   loaded once via `lunasvg` / `stb_image` and cached as ImGui textures. Remote
 *   `http(s)://` URLs are normally pre-fetched at configure time by
 *   `cmake/HelpAssets.cmake` and rewritten to local paths; if a remote URL still
 *   reaches the renderer (download failure), it is rendered as a clickable button
 *   that opens the URL in the user's default browser. An image shown smaller than its size opens, on click, at up
 *   to the size of the main window in a popup.
 *
 * Re-parsing is debounced (~250 ms) so heavy content does not re-build the block
 * list on every keystroke.
 */
class MarkdownPreview final {
public:
	/**
	 * @brief
	 *  Callback invoked when the user clicks a Markdown link.
	 */
	using LinkCallback = std::function<void(const std::string& iHref)>;

	/**
	 * @brief
	 *  Default constructor.
	 */
	MarkdownPreview();

	/**
	 * @brief
	 *  Destructor.
	 */
	~MarkdownPreview();

	MarkdownPreview(const MarkdownPreview&) = delete;

	MarkdownPreview(MarkdownPreview&&) = delete;

	auto operator=(const MarkdownPreview&) -> MarkdownPreview& = delete;

	auto operator=(MarkdownPreview&&) -> MarkdownPreview& = delete;

	/**
	 * @brief
	 *  Register a callback invoked when a Markdown link is clicked.
	 * @param[in] iCallback The link callback (href is the raw string between the parentheses).
	 */
	void setLinkCallback(LinkCallback iCallback) { m_linkCallback = std::move(iCallback); }

	/**
	 * @brief
	 *  Set the directory used to resolve relative image paths.
	 * @param[in] iDir Absolute base directory; pass an empty path to disable image loading.
	 */
	void setBaseDirectory(std::filesystem::path iDir) { m_baseDir = std::move(iDir); }

	/**
	 * @brief
	 *  Feed a new text frame. Inline by design — no md4c work happens here, so callers that
	 *        only need debounce bookkeeping (headless tests) do not pull the parser into their
	 *        link graph. The actual parse happens lazily in `render()` on the first frame after
	 *        the debounce expires.
	 * @param[in] iTimeStep The current frame timestep.
	 * @param[in] iText The Markdown source.
	 */
	void update(const core::Timestep& iTimeStep, const std::string& iText) {
		if (iText != m_pendingText) {
			m_pendingText = iText;
			m_debounceLeft = kDebounceSeconds;
			m_dirty = true;
			return;
		}
		if (!m_dirty)
			return;
		m_debounceLeft -= iTimeStep.getSeconds();
		if (m_debounceLeft > 0.0f)
			return;
		m_renderedText = m_pendingText;
		m_needsRebuild = true;
		m_dirty = false;
	}

	/**
	 * @brief
	 *  Show a text at once, without the debounce of `update()` (a page opened in the help).
	 * @param[in] iText The Markdown source.
	 */
	void setText(const std::string& iText) {
		m_pendingText = iText;
		m_renderedText = iText;
		m_needsRebuild = true;
		m_dirty = false;
	}

	/**
	 * @brief
	 *  Scroll to a heading on the next frames: its anchor (`{#id}` or GitHub slug of its text), the top when empty.
	 * @param[in] iAnchor The anchor, without `#`.
	 */
	void scrollToAnchor(const std::string& iAnchor) {
		m_pendingAnchor = iAnchor;
		m_anchorFrames = kAnchorFrames;
		m_scrollFrames = kAnchorFrames;
		m_pendingScrollY = iAnchor.empty() ? 0.0f : -1.0f;
	}

	/**
	 * @brief
	 *  Scroll to a vertical position on the next frame (a page left by Back, at its former position).
	 * @param[in] iScrollY The scroll position, in pixels.
	 */
	void scrollTo(const float iScrollY) {
		m_pendingAnchor.clear();
		m_scrollFrames = kAnchorFrames;
		m_pendingScrollY = iScrollY;
	}

	/**
	 * @brief
	 *  Vertical scroll position of the last rendered frame.
	 * @return The scroll position, in pixels.
	 */
	[[nodiscard]] auto getScrollY() const -> float { return m_scrollY; }

	/**
	 * @brief
	 *  Render the most recent stable text inside the current ImGui child / window.
	 *        Triggers a (re)parse if `update()` flagged the buffer as needing a rebuild.
	 * @param[in] iSize Target area; ignored if zero.
	 */
	void render(const ImVec2& iSize);

	/**
	 * @brief
	 *  Currently rendered text (after debounce). Test hook.
	 * @return The const std string.
	 */
	[[nodiscard]] auto renderedText() const -> const std::string& { return m_renderedText; }

	/**
	 * @brief
	 *  True while a pending change is waiting for the debounce window to expire.
	 * @return True when the object is debouncing.
	 */
	[[nodiscard]] auto isDebouncing() const -> bool { return m_dirty; }

	/**
	 * @brief
	 *  Read-only access to the parsed block list (test hook).
	 * @return The const std vector.
	 */
	[[nodiscard]] auto blocks() const -> const std::vector<MdBlock>& { return m_document.blocks(); }

private:
	/**
	 * @brief
	 *  Cached read-only `TextEditor` for one code block (built once per parse).
	 */
	struct CodeEditorEntry {
		uniq<TextEditor> editor;///< Heap-allocated to keep the header light.
	};
	/**
	 * @brief
	 *  Cached image texture for one image source (path or URL).
	 */
	struct ImageEntry {
		shared<renderer::gpu::Texture2D> texture;///< Loaded texture (null if load failed or remote).
		math::vec2ui size;///< Pixel size of the loaded image.
		math::vec2ui logicalSize;///< Size the image is shown at when it is a rasterised SVG (zero: `size`).
		bool isRemote = false;///< True if the source is an HTTP(S) URL.
		bool failed = false;///< True if the most recent load attempt failed.
		bool flippedY = false;///< True when the texture was loaded with stb_image
		///< (`stbi_set_flip_vertically_on_load = 1` in Owl), so
		///< the renderer must flip the V axis at display time.
	};

	/**
	 * @brief
	 *  Re-parse the Markdown buffer and rebuild per-block render caches.
	 */
	void rebuildCaches();

	/**
	 * @brief
	 *  Render one block, recursing into containers as needed.
	 * @param[in] iBlock Block to render.
	 * @param[in,out] ioCodeIndex Walking index into `m_codeEditors` (one entry per code block).
	 * @param[in] iCompact When `true`, suppress trailing inter-paragraph spacing — used while
	 *                     emitting list items so consecutive bullets stay tight.
	 */
	void renderBlock(const MdBlock& iBlock, size_t& ioCodeIndex, bool iCompact);

	/**
	 * @brief
	 *  Render a heading: strong font at the size of its level, space above, rule under H1 / H2; records its anchor.
	 * @param[in] iHeading The heading.
	 */
	void renderHeading(const MdHeading& iHeading);

	/**
	 * @brief
	 *  Render one inline-span sequence with emphasis / code / link handling.
	 */
	void renderInlineSpans(const std::vector<MdInline>& iSpans);

	/**
	 * @brief
	 *  Render a code block via the cached read-only `TextEditor`.
	 */
	void renderCodeBlock(const MdCodeBlock& iCb, size_t& ioCodeIndex);

	/**
	 * @brief
	 *  Render a table with `BeginTable` / `TableNextColumn`.
	 */
	void renderTable(const MdTable& iTable);

	/**
	 * @brief
	 *  Render a list (UL / OL).
	 */
	void renderList(const MdList& iList, size_t& ioCodeIndex);

	/**
	 * @brief
	 *  Render one table cell: its images at the column width when it holds only images, else its inline spans.
	 * @param[in] iSpans The cell content.
	 */
	void renderTableCell(const std::vector<MdInline>& iSpans);

	/**
	 * @brief
	 *  Draw a loaded image at a size; when it is shown smaller than its own size, a click opens it enlarged.
	 * @param[in] iSrc Image source (cache key).
	 * @param[in] iEntry The loaded image.
	 * @param[in] iWidth Width shown, in pixels.
	 */
	void drawImage(const std::string& iSrc, const ImageEntry& iEntry, float iWidth);

	/**
	 * @brief
	 *  Draw the popup of the enlarged image, when one is open.
	 */
	void renderZoom();

	/**
	 * @brief
	 *  Draw a status badge as a coloured tag on the current line.
	 * @param[in] iLabel The badge text.
	 * @param[in] iColour The colour, `rrggbb` or `rgb` hex (neutral when empty or malformed).
	 * @param[in,out] ioFirstOnLine `false` when a previous span already emitted on the line.
	 */
	void renderBadge(const std::string& iLabel, const std::string& iColour, bool& ioFirstOnLine);

	/**
	 * @brief
	 *  Render a block image (paragraph reduced to a single image span).
	 * @param[in] iSrc Image path or URL.
	 * @param[in] iAlt Alt text.
	 * @param[in] iWidth Requested width in pixels (0 = natural size); always capped to the available width.
	 */
	void renderImage(const std::string& iSrc, const std::string& iAlt, uint32_t iWidth);

	/**
	 * @brief
	 *  Render an inline image span (e.g. README badges) at body line height.
	 * @param[in] iSrc Image href (path or URL).
	 * @param[in] iAlt Alt text (used as fallback button label).
	 * @param[in,out] ioFirstOnLine `false` when a previous span already emitted on the line.
	 * @param[in] iLinkOpen `true` when wrapped inside a `[…](href)` span.
	 * @param[in] iLinkHref href to forward on click when `iLinkOpen` is `true`.
	 */
	void renderInlineImage(const std::string& iSrc, const std::string& iAlt, bool& ioFirstOnLine, bool iLinkOpen,
						   const std::string& iLinkHref);

	/**
	 * @brief
	 *  Resolve an image href into a cached `ImageEntry`. Loads on first use.
	 * @return The const ImageEntry.
	 */
	auto resolveImage(const std::string& iSrc) -> const ImageEntry&;

	/**
	 * @brief
	 *  Click test for a link / image — invokes callback or `openExternalUrl`.
	 */
	void handleLinkClick(const std::string& iHref) const;

	/// Last input text seen by `update`.
	std::string m_pendingText;
	/// Text effectively pushed to the renderer.
	std::string m_renderedText;
	/// Seconds remaining before the next re-parse.
	float m_debounceLeft = 0.0f;
	/// True while waiting for the debounce window to expire.
	bool m_dirty = false;
	/// True when `m_renderedText` has changed since the last `rebuildCaches()` call.
	bool m_needsRebuild = false;
	/// User-provided link callback, may be empty.
	LinkCallback m_linkCallback;
	/// Base directory for resolving relative image paths.
	std::filesystem::path m_baseDir;
	/// Parsed document — rebuilt when text changes pass debounce.
	MarkdownDocument m_document;
	/// One `TextEditor` per code block in the current parse, indexed by traversal order.
	std::vector<CodeEditorEntry> m_codeEditors;
	/// Cache of image textures keyed by raw source string.
	std::unordered_map<std::string, ImageEntry> m_imageCache;
	/// Vertical position of each heading anchor in the content, filled while rendering.
	std::unordered_map<std::string, float> m_anchorY;
	/// Anchor to scroll to once the content is laid out, empty when none.
	std::string m_pendingAnchor;
	/// Frames left during which the scroll follows `m_pendingAnchor` (the layout of a new text settles).
	int m_anchorFrames = 0;
	/// Frames left during which `m_pendingScrollY` is applied.
	int m_scrollFrames = 0;
	/// Scroll position to apply before the next frames, negative when none.
	float m_pendingScrollY = -1.0f;
	/// Scroll position of the last frame.
	float m_scrollY = 0.0f;
	/// Link under the mouse on the last frame (index in rendering order), -1 when none.
	int m_hoveredLink = -1;
	/// Link under the mouse on the frame being rendered.
	int m_nextHoveredLink = -1;
	/// Links rendered so far on the frame being rendered.
	int m_linkCount = 0;
	/// True until the first block of the frame is drawn (no space above a leading heading).
	bool m_firstBlock = true;
	/// Source of the image shown enlarged, empty when none.
	std::string m_zoomSrc;
	/// True when a click asked for the enlarged image popup this frame.
	bool m_zoomRequested = false;

	/// Debounce window (seconds).
	static constexpr float kDebounceSeconds = 0.25f;
	/// Frames the scroll follows an anchor or a position after `scrollToAnchor` / `scrollTo`.
	static constexpr int kAnchorFrames = 4;
};

}// namespace owl::nest::codeEditor
