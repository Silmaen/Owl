/**
 * @file MarkdownPreview_test.cpp
 * @author Silmaen
 * @date 28/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Parser tests for `codeEditor::MarkdownDocument` (md4c-backed). The renderer
 * itself (`MarkdownPreview`) is exercised manually in the editor since ImGui
 * state is unavailable in headless tests, but the parser is pure logic and
 * easy to unit-test.
 *
 * The parser source is included directly so the test can run against
 * `owl_scene_tests_unit_test` (which links neither md4c nor the editor sources
 * — pulling them in via CMake would force every scene test to depend on the
 * editor's full link graph).
 */

// NOLINTBEGIN: include the parser TU directly so the test target stays self-contained.
#include "../../source/owlnest/sources/document/codeEditor/MarkdownDocument.cpp"
// NOLINTEND

#include "testHelper.h"

#include <gtest/gtest.h>

#include <string>

using owl::nest::codeEditor::InlineKind;
using owl::nest::codeEditor::MarkdownDocument;
using owl::nest::codeEditor::MdBlock;
using owl::nest::codeEditor::MdBlockQuote;
using owl::nest::codeEditor::MdCodeBlock;
using owl::nest::codeEditor::MdHeading;
using owl::nest::codeEditor::MdHRule;
using owl::nest::codeEditor::MdImage;
using owl::nest::codeEditor::MdList;
using owl::nest::codeEditor::MdParagraph;
using owl::nest::codeEditor::MdTable;

namespace {
template<typename T>
[[nodiscard]] auto firstAs(const std::vector<MdBlock>& iBlocks) -> const T* {
	for (const auto& b: iBlocks)
		if (const auto* p = std::get_if<T>(&b.data); p != nullptr)
			return p;
	return nullptr;
}

}// namespace

TEST(MarkdownDocument, ParsesEmptyToEmpty) {
	MarkdownDocument doc;
	EXPECT_TRUE(doc.parse(""));
	EXPECT_TRUE(doc.blocks().empty());
}

TEST(MarkdownDocument, ParsesHeadingAndParagraph) {
	MarkdownDocument doc;
	ASSERT_TRUE(doc.parse("# Title\n\nBody text here.\n"));
	ASSERT_GE(doc.blocks().size(), 2u);
	const auto* h = firstAs<MdHeading>(doc.blocks());
	ASSERT_NE(h, nullptr);
	EXPECT_EQ(h->level, 1);
	ASSERT_FALSE(h->spans.empty());
	EXPECT_EQ(h->spans.front().kind, InlineKind::Text);
	EXPECT_EQ(h->spans.front().text, "Title");

	const auto* p = firstAs<MdParagraph>(doc.blocks());
	ASSERT_NE(p, nullptr);
	ASSERT_FALSE(p->spans.empty());
	EXPECT_NE(p->spans.front().text.find("Body text"), std::string::npos);
}

TEST(MarkdownDocument, ParsesGfmTable3by2) {
	MarkdownDocument doc;
	ASSERT_TRUE(doc.parse(R"(| A | B | C |
|---|---|---|
| 1 | 2 | 3 |
| 4 | 5 | 6 |
)"));
	const auto* t = firstAs<MdTable>(doc.blocks());
	ASSERT_NE(t, nullptr);
	EXPECT_EQ(t->cols, 3u);
	ASSERT_EQ(t->headRows.size(), 1u);
	ASSERT_EQ(t->headRows.front().size(), 3u);
	ASSERT_EQ(t->bodyRows.size(), 2u);
	ASSERT_EQ(t->bodyRows.front().size(), 3u);
	// Cell content is a vector of inline spans; the GFM cell "1" should produce one Text span.
	const auto& cell = t->bodyRows.front()[0];
	ASSERT_FALSE(cell.empty());
	EXPECT_EQ(cell.front().kind, InlineKind::Text);
	EXPECT_EQ(cell.front().text, "1");
}

TEST(MarkdownDocument, ParsesFencedCodeBlocksByLanguage) {
	MarkdownDocument doc;
	ASSERT_TRUE(doc.parse("```yaml\nkey: value\n```\n\n```lua\nlocal x = 1\n```\n\n```mermaid\nflowchart TD\nA-->B\n```\n"));
	std::vector<const MdCodeBlock*> codeBlocks;
	for (const auto& b: doc.blocks())
		if (const auto* cb = std::get_if<MdCodeBlock>(&b.data); cb != nullptr)
			codeBlocks.push_back(cb);
	ASSERT_EQ(codeBlocks.size(), 3u);
	EXPECT_EQ(codeBlocks[0]->lang, "yaml");
	EXPECT_NE(codeBlocks[0]->text.find("key: value"), std::string::npos);
	EXPECT_EQ(codeBlocks[1]->lang, "lua");
	EXPECT_EQ(codeBlocks[2]->lang, "mermaid");
	EXPECT_NE(codeBlocks[2]->text.find("flowchart TD"), std::string::npos);
}

TEST(MarkdownDocument, ParsesBlockImage) {
	MarkdownDocument doc;
	ASSERT_TRUE(doc.parse("![architecture](images/foo.svg)\n"));
	// md4c emits images as inline spans inside a paragraph.
	const auto* p = firstAs<MdParagraph>(doc.blocks());
	ASSERT_NE(p, nullptr);
	bool sawImage = false;
	for (const auto& span: p->spans) {
		if (span.kind == InlineKind::ImageInline) {
			sawImage = true;
			EXPECT_EQ(span.text, "images/foo.svg");
			EXPECT_EQ(span.alt, "architecture");
			break;
		}
	}
	EXPECT_TRUE(sawImage);
}

TEST(MarkdownDocument, ParsesBlockQuoteAndHRule) {
	MarkdownDocument doc;
	ASSERT_TRUE(doc.parse("> First quoted line\n> second\n\n---\n"));
	const auto* q = firstAs<MdBlockQuote>(doc.blocks());
	ASSERT_NE(q, nullptr);
	EXPECT_FALSE(q->body.empty());
	const auto* h = firstAs<MdHRule>(doc.blocks());
	EXPECT_NE(h, nullptr);
}

TEST(MarkdownDocument, ParsesUnorderedList) {
	MarkdownDocument doc;
	ASSERT_TRUE(doc.parse("- one\n- two\n- three\n"));
	const auto* l = firstAs<MdList>(doc.blocks());
	ASSERT_NE(l, nullptr);
	EXPECT_FALSE(l->ordered);
	EXPECT_EQ(l->items.size(), 3u);
}

TEST(MarkdownDocument, ListItemsContainParagraphsWithText) {
	// Each `- text` line should produce one item containing a Paragraph whose first inline span
	// is the text run. This verifies the parser does NOT silently drop list-item content (which
	// the renderer would then have nothing to render).
	MarkdownDocument doc;
	ASSERT_TRUE(doc.parse("- alpha\n- beta gamma\n- delta\n"));
	const auto* l = firstAs<MdList>(doc.blocks());
	ASSERT_NE(l, nullptr);
	ASSERT_EQ(l->items.size(), 3u);

	const auto extractFirstWord = [](const std::vector<MdBlock>& iItem) -> std::string {
		if (iItem.empty())
			return "<empty item>";
		const auto* p = std::get_if<MdParagraph>(&iItem.front().data);
		if (p == nullptr)
			return "<not a paragraph>";
		if (p->spans.empty())
			return "<empty paragraph>";
		return p->spans.front().text;
	};
	EXPECT_EQ(extractFirstWord(l->items[0]), "alpha");
	EXPECT_EQ(extractFirstWord(l->items[1]), "beta gamma");
	EXPECT_EQ(extractFirstWord(l->items[2]), "delta");
}

TEST(MarkdownDocument, TightListSkipsParagraphsAtMd4cLayer) {
	// Pin the md4c upstream behaviour our parser relies on: for a *tight* unordered list
	// (no blank lines between items), md4c does NOT emit `MD_BLOCK_P` inside `MD_BLOCK_LI` —
	// text spans are reported directly inside the LI. `MarkdownDocument` lazy-creates an
	// implicit paragraph in that case (`ensureInlineAccum`), so if md4c ever changes this and
	// starts emitting P unconditionally, this test will trip and we can drop the lazy path.
	struct Counts {
		int liEnters = 0;
		int pInsideLi = 0;
		int liDepth = 0;
	};
	Counts counts{};
	MD_PARSER parser{};
	parser.flags = MD_DIALECT_GITHUB | MD_FLAG_COLLAPSEWHITESPACE;
	parser.enter_block = [](MD_BLOCKTYPE type, void*, void* userdata) -> int {
		auto* c = static_cast<Counts*>(userdata);
		if (type == MD_BLOCK_LI) {
			++c->liEnters;
			++c->liDepth;
		} else if (type == MD_BLOCK_P && c->liDepth > 0) {
			++c->pInsideLi;
		}
		return 0;
	};
	parser.leave_block = [](MD_BLOCKTYPE type, void*, void* userdata) -> int {
		auto* c = static_cast<Counts*>(userdata);
		if (type == MD_BLOCK_LI)
			--c->liDepth;
		return 0;
	};
	// md4c dereferences span/text callbacks unconditionally — supply no-op stubs to keep the test
	// from crashing on the body of the LI items.
	parser.enter_span = [](MD_SPANTYPE, void*, void*) -> int { return 0; };
	parser.leave_span = [](MD_SPANTYPE, void*, void*) -> int { return 0; };
	parser.text = [](MD_TEXTTYPE, const MD_CHAR*, MD_SIZE, void*) -> int { return 0; };
	const std::string src = "- alpha\n- beta gamma\n- delta\n";
	md_parse(src.data(), static_cast<MD_SIZE>(src.size()), &parser, &counts);
	EXPECT_EQ(counts.liEnters, 3);
	EXPECT_EQ(counts.pInsideLi, 0) << "Tight-list LI no longer skips MD_BLOCK_P — the lazy "
									  "paragraph creation in MarkdownDocument can be simplified.";
}

TEST(MarkdownDocument, ListItemsKeepLinksInline) {
	// Reproduces the README pattern (`- [Title](url) -- description`). The item should contain a
	// Paragraph whose spans are: LinkStart, Text("Title"), LinkEnd, Text(" -- description").
	MarkdownDocument doc;
	ASSERT_TRUE(doc.parse("- [Title](url) -- description\n"));
	const auto* l = firstAs<MdList>(doc.blocks());
	ASSERT_NE(l, nullptr);
	ASSERT_EQ(l->items.size(), 1u);
	ASSERT_EQ(l->items[0].size(), 1u);
	const auto* p = std::get_if<MdParagraph>(&l->items[0].front().data);
	ASSERT_NE(p, nullptr);
	bool sawLinkStart = false;
	bool sawTitle = false;
	bool sawDescription = false;
	for (const auto& span: p->spans) {
		if (span.kind == InlineKind::LinkStart && span.text == "url")
			sawLinkStart = true;
		if (span.kind == InlineKind::Text && span.text == "Title")
			sawTitle = true;
		if (span.kind == InlineKind::Text && span.text.find("description") != std::string::npos)
			sawDescription = true;
	}
	EXPECT_TRUE(sawLinkStart);
	EXPECT_TRUE(sawTitle);
	EXPECT_TRUE(sawDescription);
}

TEST(MarkdownDocument, ParsesInlineCodeAndEmphasis) {
	MarkdownDocument doc;
	ASSERT_TRUE(doc.parse("Use `foo()` to **bar** the *baz*.\n"));
	const auto* p = firstAs<MdParagraph>(doc.blocks());
	ASSERT_NE(p, nullptr);
	bool sawCode = false;
	bool sawStrong = false;
	bool sawEm = false;
	for (const auto& span: p->spans) {
		if (span.kind == InlineKind::Code && span.text == "foo()")
			sawCode = true;
		if (span.kind == InlineKind::StrongStart)
			sawStrong = true;
		if (span.kind == InlineKind::EmphasisStart)
			sawEm = true;
	}
	EXPECT_TRUE(sawCode);
	EXPECT_TRUE(sawStrong);
	EXPECT_TRUE(sawEm);
}

TEST(MarkdownDocument, ParsesLinks) {
	MarkdownDocument doc;
	ASSERT_TRUE(doc.parse("See [docs](other.md) and [home](https://example.com).\n"));
	const auto* p = firstAs<MdParagraph>(doc.blocks());
	ASSERT_NE(p, nullptr);
	std::vector<std::string> hrefs;
	for (const auto& span: p->spans)
		if (span.kind == InlineKind::LinkStart)
			hrefs.push_back(span.text);
	ASSERT_EQ(hrefs.size(), 2u);
	EXPECT_EQ(hrefs[0], "other.md");
	EXPECT_EQ(hrefs[1], "https://example.com");
}
