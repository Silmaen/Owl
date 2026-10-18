/**
 * @file SlangSyntax_test.cpp
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Tests of the Slang vocabulary the code editor colours and of its attachment to `.slang` files.
 */

#include "document/codeEditor/SlangSyntax.h"
#include "document/codeEditor/LanguageDefinitions.h"
#include "external/imgui_text_edit.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

using namespace owl::nest::codeEditor;

TEST(SlangSyntax, classifiesTheWords) {
	EXPECT_EQ(classifySlangWord("struct"), SlangWordKind::Keyword);
	EXPECT_EQ(classifySlangWord("import"), SlangWordKind::Keyword);
	EXPECT_EQ(classifySlangWord("uniform"), SlangWordKind::Keyword);
	EXPECT_EQ(classifySlangWord("float4"), SlangWordKind::Type);
	EXPECT_EQ(classifySlangWord("float4x4"), SlangWordKind::Type);
	EXPECT_EQ(classifySlangWord("uint2"), SlangWordKind::Type);
	EXPECT_EQ(classifySlangWord("Texture2D"), SlangWordKind::Type);
	EXPECT_EQ(classifySlangWord("SV_Position"), SlangWordKind::Semantic);
	EXPECT_EQ(classifySlangWord("SV_Target0"), SlangWordKind::Semantic);
	EXPECT_EQ(classifySlangWord("TEXCOORD3"), SlangWordKind::Semantic);
	EXPECT_EQ(classifySlangWord("mul"), SlangWordKind::Builtin);
	EXPECT_EQ(classifySlangWord("SampleLevel"), SlangWordKind::Builtin);
}

TEST(SlangSyntax, leavesOrdinaryIdentifiersAlone) {
	EXPECT_EQ(classifySlangWord("position"), SlangWordKind::None);
	EXPECT_EQ(classifySlangWord("float5"), SlangWordKind::None);
	EXPECT_EQ(classifySlangWord("float4x5"), SlangWordKind::None);
	EXPECT_EQ(classifySlangWord("Struct"), SlangWordKind::None);
	EXPECT_EQ(classifySlangWord(""), SlangWordKind::None);
}

TEST(SlangSyntax, everyListedWordClassifiesBackToItsKind) {
	for (const auto kind:
		 {SlangWordKind::Keyword, SlangWordKind::Type, SlangWordKind::Semantic, SlangWordKind::Builtin}) {
		const auto words = slangWords(kind);
		ASSERT_FALSE(words.empty());
		for (const auto& word: words) EXPECT_EQ(classifySlangWord(word), kind) << word;
	}
	EXPECT_TRUE(slangWords(SlangWordKind::None).empty());
}

TEST(SlangSyntax, detectsSlangFiles) {
	EXPECT_EQ(detectLanguage("engine_assets/shaders/opengl/text.slang"), Language::Slang);
	EXPECT_EQ(detectLanguage("A.SLANG"), Language::Slang);
	EXPECT_STREQ(languageName(Language::Slang), "Slang");
}

TEST(SlangSyntax, attachesTheSlangLanguageToTheEditor) {
	TextEditor editor;
	applyLanguage(editor, Language::Slang);
	EXPECT_EQ(editor.GetLanguageName(), "Slang");
	const auto* const language = editor.GetLanguage();
	ASSERT_NE(language, nullptr);
	EXPECT_TRUE(language->keywords.contains("struct"));
	EXPECT_TRUE(language->declarations.contains("float4"));
	EXPECT_TRUE(language->identifiers.contains("SV_Position"));
	EXPECT_TRUE(language->identifiers.contains("saturate"));
	EXPECT_EQ(language->preprocess, '#');
	EXPECT_EQ(language->singleLineComment, "//");
	EXPECT_EQ(language->commentStart, "/*");
	EXPECT_EQ(language->commentEnd, "*/");
}
