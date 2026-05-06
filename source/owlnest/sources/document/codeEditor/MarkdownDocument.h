/**
 * @file MarkdownDocument.h
 * @author Silmaen
 * @date 29/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace owl::nest::codeEditor {
/**
 * @brief
 *  Inline span kind emitted by the Markdown parser.
 */
enum struct InlineKind : uint8_t {
	Text,         ///< Plain text run.
	Code,         ///< Backtick-quoted inline code.
	StrongStart,  ///< `**bold**` opening marker.
	StrongEnd,    ///< `**bold**` closing marker.
	EmphasisStart,///< `*italic*` opening marker.
	EmphasisEnd,  ///< `*italic*` closing marker.
	StrikeStart,  ///< `~~strike~~` opening marker.
	StrikeEnd,    ///< `~~strike~~` closing marker.
	LinkStart,    ///< `[text](href)` opening marker. `text` payload carries the href.
	LinkEnd,      ///< Closing marker for a link span.
	ImageInline,  ///< `![alt](src)` inside a paragraph. `text`=src, `alt`=alt text.
	LineBreak,    ///< Hard line break.
};

/**
 * @brief
 *  One inline span emitted by the parser.
 */
struct MdInline {
	InlineKind kind = InlineKind::Text;///< Span kind discriminator.
	std::string text;                  ///< Text run, code body, link href, or image src.
	std::string alt;                   ///< Image alt text (only for `ImageInline`).
};

/**
 * @brief
 *  Per-column horizontal alignment in a GFM table.
 */
enum struct TableAlign : uint8_t {
	Default,///< No explicit alignment marker — left in practice.
	Left,   ///< `|:---|`
	Center, ///< `|:---:|`
	Right,  ///< `|---:|`
};

// --- Leaf (non-recursive) block types ---------------------------------------

/**
 * @brief
 *  ATX or Setext heading (level 1-6) with inline spans.
 */
struct MdHeading {
	uint8_t level = 1;          ///< Heading level (1 = `#`, …, 6 = `######`).
	std::vector<MdInline> spans;///< Inline content of the heading line.
};

/**
 * @brief
 *  Plain paragraph with inline spans.
 */
struct MdParagraph {
	std::vector<MdInline> spans;///< Inline content of the paragraph.
};

/**
 * @brief
 *  Fenced or indented code block.
 */
struct MdCodeBlock {
	std::string lang;///< Language hint after the opening fence (`'cpp'`, `'lua'`, …).
	std::string text;///< Raw code (no fence markers, may contain trailing newline).
};

/**
 * @brief
 *  Standalone block image (a paragraph that contains only an image).
 */
struct MdImage {
	std::string src;///< Image href / file path (left as-is, possibly an URL).
	std::string alt;///< Alt text.
};

/**
 * @brief
 *  Horizontal rule (`---` / `***`).
 */
struct MdHRule {};

/**
 * @brief
 *  GFM-style table (head row + N body rows). Cells are inline-span sequences.
 */
struct MdTable {
	uint32_t cols = 0;                                       ///< Number of columns.
	std::vector<TableAlign> align;                           ///< Alignment per column (size = `cols`).
	std::vector<std::vector<std::vector<MdInline>>> headRows;///< `[row][col][span]`, typically 1 row.
	std::vector<std::vector<std::vector<MdInline>>> bodyRows;///< `[row][col][span]`.
};

// --- Recursive block types --------------------------------------------------

struct MdBlock;// forward; defined below once all alternatives are complete.

/**
 * @brief
 *  Ordered or unordered list. Each item is itself a list of blocks.
 */
struct MdList {
	bool ordered = false;                    ///< `true` for `1. … 2. …`, `false` for `- … - …`.
	std::vector<std::vector<MdBlock>> items; ///< One vector of blocks per list item.
};

/**
 * @brief
 *  Block quote, body is the nested block sequence.
 */
struct MdBlockQuote {
	std::vector<MdBlock> body;///< Quoted blocks (typically paragraphs).
};

/**
 * @brief
 *  Top-level block variant produced by the parser.
 */
struct MdBlock {
	/**
	 * @brief
	 *  Active alternative. Inspect with `std::get_if<T>(&block.data)` or `std::visit`.
	 */
	std::variant<MdHeading, MdParagraph, MdCodeBlock, MdImage, MdHRule, MdTable, MdList, MdBlockQuote> data;
};

/**
 * @brief
 *  Sequence of top-level blocks parsed from a Markdown source.
 */
class MarkdownDocument final {
public:
	/**
	 * @brief
	 *  Construct an empty document. Use `parse()` to populate.
	 */
	MarkdownDocument() = default;

	/**
	 * @brief
	 *  Default destructor.
	 */
	~MarkdownDocument() = default;

	MarkdownDocument(const MarkdownDocument&) = delete;

	MarkdownDocument(MarkdownDocument&&) = default;

	auto operator=(const MarkdownDocument&) -> MarkdownDocument& = delete;

	auto operator=(MarkdownDocument&&) -> MarkdownDocument& = default;

	/**
	 * @brief
	 *  Parse the given Markdown source via md4c.
	 * @param[in] iSource Markdown text (CommonMark + GFM tables/strikethrough/autolinks).
	 * @return `true` on success. On failure the document is left in a defined-but-empty state.
	 */
	auto parse(std::string_view iSource) -> bool;

	/**
	 * @brief
	 *  Top-level blocks (read-only view).
	 */
	[[nodiscard]] auto blocks() const -> const std::vector<MdBlock>& { return m_blocks; }

	/**
	 * @brief
	 *  Take ownership of the parsed blocks (clears the document).
	 */
	[[nodiscard]] auto takeBlocks() -> std::vector<MdBlock>;

private:
	std::vector<MdBlock> m_blocks;///< Top-level block list.
};

}// namespace owl::nest::codeEditor
