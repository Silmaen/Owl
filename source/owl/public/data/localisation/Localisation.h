/**
 * @file Localisation.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/FormatVersion.h"

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::data::localisation {

/**
 * @brief
 *  A language a game ships in.
 */
struct OWL_API Language {
	/// Code the game, the settings and the string tables name it by (`en`, `fr`, `ja`).
	std::string code;
	/// Name shown to the player, in the language itself (`Français`).
	std::string name;
	/// Name of the font of its localised HUD texts (`FontLibrary`), chosen for its glyphs; empty keeps each text's font.
	std::string font;
	/// Whether the game export packs it.
	bool exported{true};

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other language.
	 * @return True when both are the same.
	 */
	auto operator==(const Language& iOther) const -> bool = default;
};

/**
 * @brief
 *  A translated text: its key and its value in each language.
 */
struct OWL_API LocalisedString {
	/// Key the scripts and the HUD name it by (`menu.start`).
	std::string key;
	/// String table it belongs to, a group of the editor (`menu`, `dialogue`).
	std::string table;
	/// Value by language code; a language without one falls back.
	std::map<std::string, std::string, std::less<>> values;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other string.
	 * @return True when both are the same.
	 */
	auto operator==(const LocalisedString& iOther) const -> bool = default;
};

/**
 * @brief
 *  Localisation of a game (`localisation.yml` at the project root): its languages, the fallback one and the string
 *  tables.
 */
struct OWL_API Localisation {
	/// Name of the project file.
	static constexpr std::string_view fileName{"localisation.yml"};

	/// Languages of the game, in the order the settings list them.
	std::vector<Language> languages;
	/// Code of the language a missing value falls back to.
	std::string fallback;
	/// Translated texts.
	std::vector<LocalisedString> strings;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other localisation.
	 * @return True when both are the same.
	 */
	auto operator==(const Localisation& iOther) const -> bool = default;

	/**
	 * @brief
	 *  Format descriptor of `localisation.yml`, with its migration chain.
	 * @return The Localisation format.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  Serialize to a YAML string.
	 * @return The YAML document.
	 */
	[[nodiscard]] auto serializeToString() const -> std::string;

	/**
	 * @brief
	 *  Read from a YAML string.
	 * @param[in] iYaml The YAML document.
	 * @return The localisation, or nothing when the document is not one.
	 */
	[[nodiscard]] static auto deserializeFromString(std::string_view iYaml) -> std::optional<Localisation>;

	/**
	 * @brief
	 *  Save to a file. Atomic: a failed write keeps the previous file.
	 * @param[in] iPath Destination file.
	 * @return True on success.
	 */
	[[nodiscard]] auto saveToFile(const std::filesystem::path& iPath) const -> bool;

	/**
	 * @brief
	 *  Read a file.
	 * @param[in] iPath The file on disk.
	 * @return The localisation, or nothing when it cannot be read.
	 */
	[[nodiscard]] static auto loadFile(const std::filesystem::path& iPath) -> std::optional<Localisation>;

	/**
	 * @brief
	 *  Look up a language.
	 * @param[in] iCode Its code.
	 * @return The language, or null.
	 */
	[[nodiscard]] auto findLanguage(std::string_view iCode) const -> const Language*;

	/**
	 * @brief
	 *  Look up a string.
	 * @param[in] iKey Its key.
	 * @return The string, or null.
	 */
	[[nodiscard]] auto findString(std::string_view iKey) const -> const LocalisedString*;

	/**
	 * @brief
	 *  Text of a key: its value in the language, else in the fallback language, else the key itself.
	 * @param[in] iKey The key.
	 * @param[in] iLanguage The language code.
	 * @return The text, a view into this localisation or into `iKey`.
	 */
	[[nodiscard]] auto translate(std::string_view iKey, std::string_view iLanguage) const -> std::string_view;

	/**
	 * @brief
	 *  Language a game plays in when asked for one: that one if the game has it, else the fallback, else the first.
	 * @param[in] iCode The language asked for (the `language` setting), possibly empty.
	 * @return The code, empty when the game has no language.
	 */
	[[nodiscard]] auto resolveLanguage(std::string_view iCode) const -> std::string;

	/**
	 * @brief
	 *  Names of the string tables, sorted.
	 * @return One entry per distinct `table`.
	 */
	[[nodiscard]] auto getTables() const -> std::vector<std::string>;

	/**
	 * @brief
	 *  Keys missing a value in a language.
	 * @param[in] iCode The language code.
	 * @return The keys, in table order.
	 */
	[[nodiscard]] auto getMissing(std::string_view iCode) const -> std::vector<std::string>;

	/**
	 * @brief
	 *  The localisation a game export packs: the exported languages only, and their values.
	 * @return A copy without the other languages; the fallback stays when it is exported.
	 */
	[[nodiscard]] auto withExportedLanguages() const -> Localisation;
};

/**
 * @brief
 *  Value of a string in a language, else in the fallback language, else its key.
 * @param[in] iEntry The string.
 * @param[in] iLanguage The language code.
 * @param[in] iFallback The fallback language code.
 * @return A view into `iEntry`.
 */
[[nodiscard]] OWL_API auto getValue(const LocalisedString& iEntry, std::string_view iLanguage,
									std::string_view iFallback) -> std::string_view;

}// namespace owl::data::localisation
