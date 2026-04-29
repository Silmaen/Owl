/**
 * @file MarkdownDocument.cpp
 * @author Silmaen
 * @date 29/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "MarkdownDocument.h"

#include "external/md4c_wrapper.h"

#include <core/Log.h>
#include <core/Macros.h>

OWL_DIAG_PUSH
// md4c uses C-style enums; we cover only the constructs we model and rely on the default
// branch for ignored/extension types (LaTeX math, wikilinks, underline, raw HTML, …).
OWL_DIAG_DISABLE_CLANG("-Wswitch-enum")

namespace owl::nest::codeEditor {

namespace {

/// @brief Convert md4c alignment enum to our `TableAlign` enum.
[[nodiscard]] auto mapAlign(const MD_ALIGN iAlign) -> TableAlign {
	switch (iAlign) {
		case MD_ALIGN_LEFT:
			return TableAlign::Left;
		case MD_ALIGN_CENTER:
			return TableAlign::Center;
		case MD_ALIGN_RIGHT:
			return TableAlign::Right;
		default:
			return TableAlign::Default;
	}
}

/// @brief Internal builder state. `Parser` walks md4c SAX callbacks and assembles `MdBlock`s.
class Parser {
public:
	[[nodiscard]] auto run(const std::string_view iSource) -> std::vector<MdBlock> {
		m_blockStack.clear();
		m_blockStack.push_back(&m_root);
		m_inlineAccum = nullptr;
		m_codeAccum = nullptr;
		m_inLink = false;
		m_inImage = false;
		MD_PARSER parser{};
		parser.flags = MD_DIALECT_GITHUB | MD_FLAG_COLLAPSEWHITESPACE;
		parser.enter_block = &Parser::enterBlock;
		parser.leave_block = &Parser::leaveBlock;
		parser.enter_span = &Parser::enterSpan;
		parser.leave_span = &Parser::leaveSpan;
		parser.text = &Parser::text;
		const int rc = md_parse(iSource.data(), static_cast<MD_SIZE>(iSource.size()), &parser, this);
		if (rc != 0) {
			OWL_CORE_WARN("MarkdownDocument: md_parse aborted with code {}", rc)
		}
		return std::move(m_root);
	}

private:
	// ---- md4c trampolines (static) ------------------------------------------
	static auto enterBlock(MD_BLOCKTYPE iType, void* iDetail, void* ioUserData) -> int {
		return static_cast<Parser*>(ioUserData)->onEnterBlock(iType, iDetail);
	}
	static auto leaveBlock(MD_BLOCKTYPE iType, void* iDetail, void* ioUserData) -> int {
		return static_cast<Parser*>(ioUserData)->onLeaveBlock(iType, iDetail);
	}
	static auto enterSpan(MD_SPANTYPE iType, void* iDetail, void* ioUserData) -> int {
		return static_cast<Parser*>(ioUserData)->onEnterSpan(iType, iDetail);
	}
	static auto leaveSpan(MD_SPANTYPE iType, void* iDetail, void* ioUserData) -> int {
		return static_cast<Parser*>(ioUserData)->onLeaveSpan(iType, iDetail);
	}
	static auto text(MD_TEXTTYPE iType, const MD_CHAR* iText, MD_SIZE iSize, void* ioUserData) -> int {
		return static_cast<Parser*>(ioUserData)->onText(iType, iText, iSize);
	}

	// ---- accumulator helpers ------------------------------------------------

	/// @brief Lazy-create an implicit `MdParagraph` in the current container if no inline target is
	/// active. md4c skips `MD_BLOCK_P` inside `MD_BLOCK_LI` for *tight* lists (the most common
	/// CommonMark case — `- a\n- b\n- c\n`); without this fallback the text events fired directly
	/// inside the LI would have nowhere to land and the items would be silently empty.
	void ensureInlineAccum() {
		if (m_inlineAccum != nullptr || m_blockStack.empty())
			return;
		m_blockStack.back()->push_back(MdBlock{.data = MdParagraph{.spans = {}}});
		m_inlineAccum = &std::get<MdParagraph>(m_blockStack.back()->back().data).spans;
	}

	void appendInlineText(const char* iData, const size_t iSize) {
		if (iSize == 0)
			return;
		if (m_inImage) {
			m_imageAlt.append(iData, iSize);
			return;
		}
		if (m_inLink) {
			// We don't render styled link contents differently from plain link text.
			m_linkText.append(iData, iSize);
			return;
		}
		ensureInlineAccum();
		if (m_inlineAccum == nullptr)
			return;
		// Merge consecutive text runs to keep the inline list compact.
		if (!m_inlineAccum->empty() && m_inlineAccum->back().kind == InlineKind::Text)
			m_inlineAccum->back().text.append(iData, iSize);
		else
			m_inlineAccum->push_back(MdInline{.kind = InlineKind::Text, .text = std::string{iData, iSize}, .alt = {}});
	}

	void emitInlineCode(const char* iData, const size_t iSize) {
		ensureInlineAccum();
		if (m_inlineAccum == nullptr)
			return;
		m_inlineAccum->push_back(MdInline{.kind = InlineKind::Code, .text = std::string{iData, iSize}, .alt = {}});
	}

	// ---- block events -------------------------------------------------------
	auto onEnterBlock(const MD_BLOCKTYPE iType, void* iDetail) -> int {
		switch (iType) {
			case MD_BLOCK_DOC:
				return 0;
			case MD_BLOCK_H: {
				const auto* d = static_cast<const MD_BLOCK_H_DETAIL*>(iDetail);
				m_blockStack.back()->push_back(MdBlock{.data = MdHeading{.level = static_cast<uint8_t>(d->level), .spans = {}}});
				m_inlineAccum = &std::get<MdHeading>(m_blockStack.back()->back().data).spans;
				return 0;
			}
			case MD_BLOCK_P:
				m_blockStack.back()->push_back(MdBlock{.data = MdParagraph{.spans = {}}});
				m_inlineAccum = &std::get<MdParagraph>(m_blockStack.back()->back().data).spans;
				return 0;
			case MD_BLOCK_CODE: {
				const auto* d = static_cast<const MD_BLOCK_CODE_DETAIL*>(iDetail);
				MdCodeBlock cb{};
				if (d != nullptr && d->lang.text != nullptr && d->lang.size > 0)
					cb.lang.assign(d->lang.text, d->lang.size);
				m_blockStack.back()->push_back(MdBlock{.data = std::move(cb)});
				m_codeAccum = &std::get<MdCodeBlock>(m_blockStack.back()->back().data).text;
				return 0;
			}
			case MD_BLOCK_HR:
				m_blockStack.back()->push_back(MdBlock{.data = MdHRule{}});
				return 0;
			case MD_BLOCK_QUOTE: {
				m_blockStack.back()->push_back(MdBlock{.data = MdBlockQuote{.body = {}}});
				m_blockStack.push_back(&std::get<MdBlockQuote>(m_blockStack.back()->back().data).body);
				return 0;
			}
			case MD_BLOCK_UL:
			case MD_BLOCK_OL: {
				const bool ordered = iType == MD_BLOCK_OL;
				m_blockStack.back()->push_back(MdBlock{.data = MdList{.ordered = ordered, .items = {}}});
				// Note: the list is "open" but we do not push it onto the block stack — list items
				// are picked up by `MD_BLOCK_LI` via `lastList()`, which inspects the current
				// container's last block.
				return 0;
			}
			case MD_BLOCK_LI: {
				// Find the enclosing list (most recent MdList in current container).
				auto* list = lastList();
				if (list == nullptr)
					return 0;
				list->items.emplace_back();
				m_blockStack.push_back(&list->items.back());
				// Reset the inline accumulator: a previous LI may have left it pointing at its own
				// implicit paragraph (tight-list path). The next text event must lazy-create a fresh
				// paragraph in *this* item.
				m_inlineAccum = nullptr;
				return 0;
			}
			case MD_BLOCK_TABLE: {
				const auto* d = static_cast<const MD_BLOCK_TABLE_DETAIL*>(iDetail);
				MdTable t{};
				if (d != nullptr)
					t.cols = d->col_count;
				t.align.resize(t.cols, TableAlign::Default);
				m_blockStack.back()->push_back(MdBlock{.data = std::move(t)});
				m_currentTable = &std::get<MdTable>(m_blockStack.back()->back().data);
				return 0;
			}
			case MD_BLOCK_THEAD:
				m_inTableHead = true;
				return 0;
			case MD_BLOCK_TBODY:
				m_inTableHead = false;
				return 0;
			case MD_BLOCK_TR: {
				if (m_currentTable == nullptr)
					return 0;
				auto& dst = m_inTableHead ? m_currentTable->headRows : m_currentTable->bodyRows;
				dst.emplace_back();
				dst.back().resize(m_currentTable->cols);
				m_currentRowIndex = static_cast<int>(dst.size()) - 1;
				m_currentColIndex = 0;
				return 0;
			}
			case MD_BLOCK_TH:
			case MD_BLOCK_TD: {
				if (m_currentTable == nullptr)
					return 0;
				const auto* d = static_cast<const MD_BLOCK_TD_DETAIL*>(iDetail);
				if (d != nullptr && m_inTableHead && static_cast<size_t>(m_currentColIndex) < m_currentTable->align.size())
					m_currentTable->align[static_cast<size_t>(m_currentColIndex)] = mapAlign(d->align);
				auto& dst = m_inTableHead ? m_currentTable->headRows : m_currentTable->bodyRows;
				if (m_currentRowIndex < 0 || static_cast<size_t>(m_currentRowIndex) >= dst.size())
					return 0;
				if (static_cast<size_t>(m_currentColIndex) >= m_currentTable->cols)
					return 0;
				m_inlineAccum = &dst[static_cast<size_t>(m_currentRowIndex)][static_cast<size_t>(m_currentColIndex)];
				return 0;
			}
			default:
				return 0;
		}
	}

	auto onLeaveBlock(const MD_BLOCKTYPE iType, void* /*iDetail*/) -> int {
		switch (iType) {
			case MD_BLOCK_H:
			case MD_BLOCK_P:
				m_inlineAccum = nullptr;
				return 0;
			case MD_BLOCK_CODE:
				m_codeAccum = nullptr;
				return 0;
			case MD_BLOCK_QUOTE:
				m_blockStack.pop_back();
				return 0;
			case MD_BLOCK_UL:
			case MD_BLOCK_OL:
				return 0;
			case MD_BLOCK_LI:
				m_blockStack.pop_back();
				m_inlineAccum = nullptr;
				return 0;
			case MD_BLOCK_TABLE:
				m_currentTable = nullptr;
				return 0;
			case MD_BLOCK_TH:
			case MD_BLOCK_TD:
				m_inlineAccum = nullptr;
				++m_currentColIndex;
				return 0;
			default:
				return 0;
		}
	}

	// ---- span events --------------------------------------------------------
	auto onEnterSpan(const MD_SPANTYPE iType, void* iDetail) -> int {
		// In tight lists md4c skips `MD_BLOCK_P` and emits spans directly inside `MD_BLOCK_LI`.
		// Lazy-create a paragraph so subsequent text/leave events have a stable destination.
		ensureInlineAccum();
		if (m_inlineAccum == nullptr)
			return 0;
		switch (iType) {
			case MD_SPAN_EM:
				m_inlineAccum->push_back(MdInline{.kind = InlineKind::EmphasisStart, .text = {}, .alt = {}});
				return 0;
			case MD_SPAN_STRONG:
				m_inlineAccum->push_back(MdInline{.kind = InlineKind::StrongStart, .text = {}, .alt = {}});
				return 0;
			case MD_SPAN_DEL:
				m_inlineAccum->push_back(MdInline{.kind = InlineKind::StrikeStart, .text = {}, .alt = {}});
				return 0;
			case MD_SPAN_A: {
				const auto* d = static_cast<const MD_SPAN_A_DETAIL*>(iDetail);
				m_linkHref.assign(d != nullptr && d->href.text != nullptr ? std::string{d->href.text, d->href.size} : std::string{});
				m_linkText.clear();
				m_inLink = true;
				return 0;
			}
			case MD_SPAN_IMG: {
				const auto* d = static_cast<const MD_SPAN_IMG_DETAIL*>(iDetail);
				m_imageSrc.assign(d != nullptr && d->src.text != nullptr ? std::string{d->src.text, d->src.size} : std::string{});
				m_imageAlt.clear();
				m_inImage = true;
				return 0;
			}
			case MD_SPAN_CODE:
				// Inline code span — text inside is collected as Code via onText.
				m_inlineCode = true;
				return 0;
			default:
				return 0;
		}
	}

	auto onLeaveSpan(const MD_SPANTYPE iType, void* /*iDetail*/) -> int {
		if (m_inlineAccum == nullptr)
			return 0;
		switch (iType) {
			case MD_SPAN_EM:
				m_inlineAccum->push_back(MdInline{.kind = InlineKind::EmphasisEnd, .text = {}, .alt = {}});
				return 0;
			case MD_SPAN_STRONG:
				m_inlineAccum->push_back(MdInline{.kind = InlineKind::StrongEnd, .text = {}, .alt = {}});
				return 0;
			case MD_SPAN_DEL:
				m_inlineAccum->push_back(MdInline{.kind = InlineKind::StrikeEnd, .text = {}, .alt = {}});
				return 0;
			case MD_SPAN_A:
				m_inLink = false;
				m_inlineAccum->push_back(MdInline{.kind = InlineKind::LinkStart, .text = m_linkHref, .alt = {}});
				if (!m_linkText.empty())
					m_inlineAccum->push_back(MdInline{.kind = InlineKind::Text, .text = m_linkText, .alt = {}});
				m_inlineAccum->push_back(MdInline{.kind = InlineKind::LinkEnd, .text = {}, .alt = {}});
				return 0;
			case MD_SPAN_IMG:
				m_inImage = false;
				m_inlineAccum->push_back(
						MdInline{.kind = InlineKind::ImageInline, .text = m_imageSrc, .alt = m_imageAlt});
				return 0;
			case MD_SPAN_CODE:
				m_inlineCode = false;
				return 0;
			default:
				return 0;
		}
	}

	// ---- text events --------------------------------------------------------
	auto onText(const MD_TEXTTYPE iType, const MD_CHAR* iText, const MD_SIZE iSize) -> int {
		const auto sz = static_cast<size_t>(iSize);
		if (m_codeAccum != nullptr) {
			m_codeAccum->append(iText, sz);
			return 0;
		}
		if (iType == MD_TEXT_BR) {
			if (m_inlineAccum != nullptr)
				m_inlineAccum->push_back(MdInline{.kind = InlineKind::LineBreak, .text = {}, .alt = {}});
			return 0;
		}
		if (iType == MD_TEXT_SOFTBR) {
			// In CommonMark a soft break is rendered as a single space.
			appendInlineText(" ", 1);
			return 0;
		}
		if (m_inlineCode) {
			emitInlineCode(iText, sz);
			return 0;
		}
		appendInlineText(iText, sz);
		return 0;
	}

	[[nodiscard]] auto lastList() -> MdList* {
		auto* container = m_blockStack.back();
		if (container == nullptr || container->empty())
			return nullptr;
		return std::get_if<MdList>(&container->back().data);
	}

	// ---- state --------------------------------------------------------------
	std::vector<MdBlock> m_root;
	/// Stack of "current container" pointers. Top of stack is where new blocks land.
	std::vector<std::vector<MdBlock>*> m_blockStack;
	/// Accumulator for inline spans (paragraph/heading/table cell). May be null.
	std::vector<MdInline>* m_inlineAccum = nullptr;
	/// Accumulator for raw code text. May be null.
	std::string* m_codeAccum = nullptr;
	/// True between MD_SPAN_CODE enter/leave (text in this span is emitted as `Code`).
	bool m_inlineCode = false;
	/// True between MD_SPAN_A enter/leave (text in this span is buffered into `m_linkText`).
	bool m_inLink = false;
	/// True between MD_SPAN_IMG enter/leave (text in this span is buffered into `m_imageAlt`).
	bool m_inImage = false;
	std::string m_linkHref;
	std::string m_linkText;
	std::string m_imageSrc;
	std::string m_imageAlt;
	/// Currently-built table. Null outside a table.
	MdTable* m_currentTable = nullptr;
	bool m_inTableHead = false;
	int m_currentRowIndex = -1;
	int m_currentColIndex = 0;
};

}// namespace

auto MarkdownDocument::parse(const std::string_view iSource) -> bool {
	m_blocks.clear();
	if (iSource.empty())
		return true;
	Parser parser;
	m_blocks = parser.run(iSource);
	return true;
}

auto MarkdownDocument::takeBlocks() -> std::vector<MdBlock> {
	auto out = std::move(m_blocks);
	m_blocks.clear();
	return out;
}

}// namespace owl::nest::codeEditor

OWL_DIAG_POP
