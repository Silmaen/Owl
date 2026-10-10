/**
 * @file CodeEditorDocument_test.cpp
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Headless tests of the code editor document and of the `TextEditor` features it relies on (editing, selection,
 * search, highlighting language, undo, save). Rendering needs an ImGui frame and stays a manual check.
 */

#include "document/CodeEditorDocument.h"
#include "document/codeEditor/TextLayout.h"
#include "external/imgui_text_edit.h"
#include "testHelper.h"

#include <core/Log.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace owl;
using namespace owl::nest;

namespace {

class CodeEditorDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_code_editor_document_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_document.onAttach(nullptr);
	}

	void TearDown() override {
		m_document.onDetach();
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	[[nodiscard]] auto write(const std::string& iName, const std::string& iContent) const -> std::filesystem::path {
		const auto path = m_root / iName;
		std::ofstream out(path, std::ios::binary);
		out << iContent;
		return path;
	}

	[[nodiscard]] static auto read(const std::filesystem::path& iPath) -> std::string {
		const std::ifstream in(iPath, std::ios::binary);
		std::stringstream buffer;
		buffer << in.rdbuf();
		return buffer.str();
	}

	std::filesystem::path m_root;
	CodeEditorDocument m_document;
};

}// namespace

TEST(CodeEditorTextLayout, visualColumnExpandsTabsAndCountsCodePoints) {
	EXPECT_EQ(codeEditor::visualColumn("abc", 2, 4), 2u);
	EXPECT_EQ(codeEditor::visualColumn("\tx", 1, 4), 4u);
	EXPECT_EQ(codeEditor::visualColumn("ab\tx", 3, 4), 4u);
	EXPECT_EQ(codeEditor::visualColumn("\t\tx", 2, 2), 4u);
	EXPECT_EQ(codeEditor::visualColumn("\tx", 1, 0), 1u);
	EXPECT_EQ(codeEditor::visualColumn("\xc3\xa9t\xc3\xa9", 2, 4), 2u);
	EXPECT_EQ(codeEditor::visualColumn("ab", 10, 4), 2u);
}

TEST(CodeEditorDocumentDetached, hasNoEditorBeforeAttach) {
	CodeEditorDocument document;
	EXPECT_EQ(document.editor(), nullptr);
	EXPECT_FALSE(document.isDirty());
	EXPECT_FALSE(document.recoverySnapshot().has_value());
	EXPECT_EQ(document.cursorLocation().line, 0u);
	EXPECT_FALSE(document.save());
}

TEST_F(CodeEditorDocumentTest, loadDetectsTheLanguageAndStaysClean) {
	ASSERT_TRUE(m_document.loadFromFile(write("script.lua", "local a = 1\nreturn a\n")));
	EXPECT_EQ(m_document.language(), codeEditor::Language::Lua);
	EXPECT_EQ(m_document.title(), "script");
	EXPECT_FALSE(m_document.isDirty());
	ASSERT_NE(m_document.editor(), nullptr);
	EXPECT_EQ(m_document.editor()->GetLanguageName(), "Lua");
	EXPECT_EQ(m_document.editor()->GetLineCount(), 3u);
	EXPECT_FALSE(m_document.loadFromFile(m_root / "missing.lua"));
}

TEST_F(CodeEditorDocumentTest, customLanguagesAreAttached) {
	ASSERT_TRUE(m_document.loadFromFile(write("config.yml", "key: value\n")));
	EXPECT_EQ(m_document.editor()->GetLanguageName(), "YAML");
	ASSERT_TRUE(m_document.loadFromFile(write("icon.svg", "<svg/>")));
	EXPECT_EQ(m_document.editor()->GetLanguageName(), "XML");
	EXPECT_TRUE(m_document.canShowPreview());
	EXPECT_TRUE(m_document.isPreviewVisible());
}

TEST_F(CodeEditorDocumentTest, editingMakesTheDocumentDirtyAndUndoRestoresIt) {
	ASSERT_TRUE(m_document.loadFromFile(write("a.lua", "local a = 1\n")));
	auto* editor = m_document.editor();
	editor->ReplaceSectionText(TextEditor::DocPos{0, 10}, TextEditor::DocPos{0, 11}, "42");
	EXPECT_EQ(editor->GetText(), "local a = 42\n");
	EXPECT_TRUE(m_document.isDirty());
	ASSERT_TRUE(editor->CanUndo());
	editor->Undo();
	EXPECT_EQ(editor->GetText(), "local a = 1\n");
	EXPECT_FALSE(m_document.isDirty());
	ASSERT_TRUE(editor->CanRedo());
	editor->Redo();
	EXPECT_EQ(editor->GetText(), "local a = 42\n");
}

TEST_F(CodeEditorDocumentTest, searchSelectsTheOccurrencesAndMovesTheCursor) {
	ASSERT_TRUE(m_document.loadFromFile(write("b.lua", "x = 1\n\tfoo(x)\nfoo(x)\n")));
	auto* editor = m_document.editor();
	editor->SelectFirstOccurrenceOf("foo");
	EXPECT_EQ(editor->GetSectionText(editor->GetMainCursorSelection()), "foo");
	const auto first = m_document.cursorLocation();
	EXPECT_EQ(first.line, 1u);
	EXPECT_EQ(first.column, 7u);
	editor->SelectNextOccurrenceOf("foo");
	EXPECT_EQ(m_document.cursorLocation().line, 2u);
	EXPECT_EQ(m_document.cursorLocation().column, 3u);
	editor->SelectAll();
	EXPECT_EQ(editor->GetSectionText(editor->GetMainCursorSelection()), editor->GetText());
}

TEST_F(CodeEditorDocumentTest, saveWritesTheTextAndCleansTheDocument) {
	const auto path = write("c.py", "print(1)\n");
	ASSERT_TRUE(m_document.loadFromFile(path));
	m_document.editor()->ReplaceSectionText(TextEditor::DocPos{0, 6}, TextEditor::DocPos{0, 7}, "2");
	ASSERT_TRUE(m_document.isDirty());
	ASSERT_TRUE(m_document.save());
	EXPECT_FALSE(m_document.isDirty());
	EXPECT_EQ(read(path), "print(2)\n");
	const auto copy = m_root / "copy.json";
	ASSERT_TRUE(m_document.saveAs(copy));
	EXPECT_EQ(m_document.filePath(), copy);
	EXPECT_EQ(m_document.language(), codeEditor::Language::Json);
	EXPECT_EQ(read(copy), "print(2)\n");
}

TEST_F(CodeEditorDocumentTest, recoverySnapshotRoundTrips) {
	ASSERT_TRUE(m_document.loadFromFile(write("d.lua", "return 1\n")));
	ASSERT_TRUE(m_document.restoreRecoverySnapshot("return 2\n"));
	EXPECT_TRUE(m_document.isDirty());
	EXPECT_EQ(m_document.recoverySnapshot().value_or(""), "return 2\n");
}
