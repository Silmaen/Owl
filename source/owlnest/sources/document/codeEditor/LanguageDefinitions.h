/**
 * @file LanguageDefinitions.h
 * @author Silmaen
 * @date 19/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>
#include <filesystem>

class TextEditor;

namespace owl::nest::codeEditor {

/// @brief Known languages handled by the editor. Unknown extensions default to `PlainText`.
enum struct Language : uint8_t {
	PlainText,
	Lua,
	Python,
	C,
	Cpp,
	Yaml,
	Json,
	Markdown,
	Xml,
	Bash,///< POSIX shell / bash scripts. Custom highlighter (no built-in in `imgui_color_text_edit`).
};

/// @brief Best-effort language detection from a filesystem extension (e.g. `.lua`, `.py`).
[[nodiscard]] auto detectLanguage(const std::filesystem::path& iPath) -> Language;

/// @brief User-facing name (shown on the editor status line).
[[nodiscard]] auto languageName(Language iLanguage) -> const char*;

/// @brief Attach the chosen language to a `TextEditor`. Built-ins (Lua/C/Cpp/Python/Json/
/// Markdown) come from `TextEditor::Language::*`; YAML and XML/SVG use custom definitions.
void applyLanguage(TextEditor& ioEditor, Language iLanguage);

}// namespace owl::nest::codeEditor
