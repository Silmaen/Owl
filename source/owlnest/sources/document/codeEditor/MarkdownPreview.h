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

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

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
 * - headings get scaled fonts (`PushFont(font, baseSize × ratio)`) and a separator
 *   under H1/H2;
 * - paragraphs are wrapped text with inline emphasis, strong, strikethrough, code,
 *   and links (internal links go through `LinkCallback`, external `http(s)://` links
 *   open in the user's default browser);
 * - code blocks are rendered as a read-only `TextEditor` with syntax highlighting
 *   (one cached `TextEditor` per code block);
 * - tables are rendered with `ImGui::BeginTable` (borders + row stripes);
 * - local images (SVG / PNG / JPG, paths relative to `setBaseDirectory()`) are
 *   loaded once via `lunasvg` / `stb_image` and cached as ImGui textures. Remote
 *   `http(s)://` URLs are normally pre-fetched at configure time by
 *   `cmake/HelpAssets.cmake` and rewritten to local paths; if a remote URL still
 *   reaches the renderer (download failure), it is rendered as a clickable button
 *   that opens the URL in the user's default browser.
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
	 *  Render a block image (paragraph reduced to a single image span).
	 */
	void renderImage(const std::string& iSrc, const std::string& iAlt);

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

	/// Debounce window (seconds).
	static constexpr float kDebounceSeconds = 0.25f;
};

}// namespace owl::nest::codeEditor
