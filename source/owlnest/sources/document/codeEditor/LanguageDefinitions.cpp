/**
 * @file LanguageDefinitions.cpp
 * @author Silmaen
 * @date 19/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "LanguageDefinitions.h"

#include "external/imgui_text_edit.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace owl::nest::codeEditor {

namespace {

/// @brief Tokenize an identifier (letters / digits / `_` / `-`) starting at `iStart`.
/// Returns `iStart` when the current character is not a valid identifier head.
auto defaultIdentifierTokenizer(TextEditor::Iterator iStart, TextEditor::Iterator iEnd) -> TextEditor::Iterator {
	if (iStart == iEnd)
		return iStart;
	const auto first = static_cast<unsigned char>(*iStart);
	if (std::isalpha(first) == 0 && first != '_')
		return iStart;
	auto it = iStart;
	while (it != iEnd) {
		const auto c = static_cast<unsigned char>(*it);
		if (std::isalnum(c) == 0 && c != '_' && c != '-')
			break;
		++it;
	}
	return it;
}

/// @brief Tokenize an integer or floating-point number at `iStart`.
auto defaultNumberTokenizer(TextEditor::Iterator iStart, TextEditor::Iterator iEnd) -> TextEditor::Iterator {
	if (iStart == iEnd)
		return iStart;
	auto it = iStart;
	if (*it == '+' || *it == '-') {
		++it;
		if (it == iEnd || std::isdigit(static_cast<unsigned char>(*it)) == 0)
			return iStart;
	} else if (std::isdigit(static_cast<unsigned char>(*it)) == 0) {
		return iStart;
	}
	while (it != iEnd && std::isdigit(static_cast<unsigned char>(*it)) != 0) ++it;
	if (it != iEnd && *it == '.') {
		++it;
		while (it != iEnd && std::isdigit(static_cast<unsigned char>(*it)) != 0) ++it;
	}
	if (it != iEnd && (*it == 'e' || *it == 'E')) {
		++it;
		if (it != iEnd && (*it == '+' || *it == '-'))
			++it;
		while (it != iEnd && std::isdigit(static_cast<unsigned char>(*it)) != 0) ++it;
	}
	return it;
}

auto defaultIsPunctuation(ImWchar iCh) -> bool {
	if (iCh > 127)
		return false;
	return std::ispunct(static_cast<unsigned char>(iCh)) != 0;
}

auto yamlLanguage() -> const TextEditor::Language* {
	static bool inited = false;
	static TextEditor::Language lang;
	if (inited)
		return &lang;
	lang.name = "YAML";
	lang.caseSensitive = true;
	lang.singleLineComment = "#";
	lang.hasSingleQuotedStrings = true;
	lang.hasDoubleQuotedStrings = true;
	lang.stringEscape = '\\';
	lang.getIdentifier = defaultIdentifierTokenizer;
	lang.getNumber = defaultNumberTokenizer;
	lang.isPunctuation = defaultIsPunctuation;
	static constexpr std::array keywords = {"true", "false", "null", "yes", "no", "on", "off", "~",
											"True", "False", "Null", "TRUE", "FALSE", "NULL"};
	for (const auto* k: keywords) lang.keywords.insert(k);
	inited = true;
	return &lang;
}

auto xmlLanguage() -> const TextEditor::Language* {
	static bool inited = false;
	static TextEditor::Language lang;
	if (inited)
		return &lang;
	lang.name = "XML";
	lang.caseSensitive = false;
	lang.commentStart = "<!--";
	lang.commentEnd = "-->";
	lang.hasSingleQuotedStrings = true;
	lang.hasDoubleQuotedStrings = true;
	lang.getIdentifier = defaultIdentifierTokenizer;
	lang.getNumber = defaultNumberTokenizer;
	lang.isPunctuation = defaultIsPunctuation;
	inited = true;
	return &lang;
}

auto plainTextLanguage() -> const TextEditor::Language* {
	static bool inited = false;
	static TextEditor::Language lang;
	if (inited)
		return &lang;
	lang.name = "Plain Text";
	lang.caseSensitive = true;
	inited = true;
	return &lang;
}

}// namespace

auto detectLanguage(const std::filesystem::path& iPath) -> Language {
	auto ext = iPath.extension().string();
	std::ranges::transform(ext, ext.begin(),
						   [](const unsigned char iCh) -> char { return static_cast<char>(std::tolower(iCh)); });
	if (ext == ".lua")
		return Language::Lua;
	if (ext == ".py")
		return Language::Python;
	if (ext == ".c")
		return Language::C;
	if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".h" || ext == ".hpp" || ext == ".hxx")
		return Language::Cpp;
	if (ext == ".yml" || ext == ".yaml")
		return Language::Yaml;
	if (ext == ".json")
		return Language::Json;
	if (ext == ".md" || ext == ".markdown")
		return Language::Markdown;
	if (ext == ".svg" || ext == ".xml")
		return Language::Xml;
	return Language::PlainText;
}

auto languageName(const Language iLanguage) -> const char* {
	switch (iLanguage) {
		case Language::Lua:
			return "Lua";
		case Language::Python:
			return "Python";
		case Language::C:
			return "C";
		case Language::Cpp:
			return "C++";
		case Language::Yaml:
			return "YAML";
		case Language::Json:
			return "JSON";
		case Language::Markdown:
			return "Markdown";
		case Language::Xml:
			return "XML";
		case Language::PlainText:
			return "Plain Text";
	}
	return "Plain Text";
}

void applyLanguage(TextEditor& ioEditor, const Language iLanguage) {
	switch (iLanguage) {
		case Language::Lua:
			ioEditor.SetLanguage(TextEditor::Language::Lua());
			break;
		case Language::C:
			ioEditor.SetLanguage(TextEditor::Language::C());
			break;
		case Language::Cpp:
			ioEditor.SetLanguage(TextEditor::Language::Cpp());
			break;
		case Language::Python:
			ioEditor.SetLanguage(TextEditor::Language::Python());
			break;
		case Language::Json:
			ioEditor.SetLanguage(TextEditor::Language::Json());
			break;
		case Language::Markdown:
			ioEditor.SetLanguage(TextEditor::Language::Markdown());
			break;
		case Language::Yaml:
			ioEditor.SetLanguage(yamlLanguage());
			break;
		case Language::Xml:
			ioEditor.SetLanguage(xmlLanguage());
			break;
		case Language::PlainText:
			ioEditor.SetLanguage(plainTextLanguage());
			break;
	}
}

}// namespace owl::nest::codeEditor
