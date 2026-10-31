/**
 * @file DocumentCommands.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <data/localisation/Localisation.h>
#include <sound/SoundCue.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::commands {

class CommandRegistry;
class DataTableEditor;
class DialogueEditor;
class SequenceEditor;

/**
 * @brief
 *  A sound cue open in an editor tab: the `sound.*` commands edit it through its own undo history.
 */
class SoundCueEditor {
public:
	SoundCueEditor() = default;

	SoundCueEditor(const SoundCueEditor&) = default;

	SoundCueEditor(SoundCueEditor&&) = default;

	auto operator=(const SoundCueEditor&) -> SoundCueEditor& = default;

	auto operator=(SoundCueEditor&&) -> SoundCueEditor& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~SoundCueEditor();

	/**
	 * @brief
	 *  The cue as it is edited.
	 * @return The cue.
	 */
	[[nodiscard]] virtual auto cue() const -> const sound::SoundCue& = 0;

	/**
	 * @brief
	 *  Set the volume, the pitch, their random changes and the loop flag of the cue (the takes stay).
	 * @param[in] iCue The values.
	 * @param[in] iMerge Whether the edit merges with the previous one.
	 * @return True when something changed.
	 */
	virtual auto setProperties(const sound::SoundCue& iCue, bool iMerge = false) -> bool = 0;

	/**
	 * @brief
	 *  Append a take playing a whole audio file.
	 * @param[in] iFile Asset path of the audio file.
	 * @return False when it is not an audio file.
	 */
	virtual auto addVariant(const std::string& iFile) -> bool = 0;

	/**
	 * @brief
	 *  Replace a take.
	 * @param[in] iIndex Index of the take.
	 * @param[in] iVariant The take.
	 * @param[in] iMerge Whether the edit merges with the previous one.
	 * @return True when it changed.
	 */
	virtual auto setVariant(size_t iIndex, const sound::SoundVariant& iVariant, bool iMerge = false) -> bool = 0;

	/**
	 * @brief
	 *  Remove takes.
	 * @param[in] iIndices Indices of the takes.
	 * @return True when one was removed.
	 */
	virtual auto removeVariants(std::span<const size_t> iIndices) -> bool = 0;

	/**
	 * @brief
	 *  Copy takes after the last of them.
	 * @param[in] iIndices Indices of the takes.
	 * @return True when one was copied.
	 */
	virtual auto duplicateVariants(std::span<const size_t> iIndices) -> bool = 0;

	/**
	 * @brief
	 *  Move takes one row.
	 * @param[in] iIndices Indices of the takes.
	 * @param[in] iDown Whether they move down.
	 * @return True when they moved.
	 */
	virtual auto moveVariants(std::span<const size_t> iIndices, bool iDown) -> bool = 0;
};

/**
 * @brief
 *  A localisation open in an editor tab: the `locale.*` commands edit it through its own undo history.
 */
class LocalisationEditor {
public:
	LocalisationEditor() = default;

	LocalisationEditor(const LocalisationEditor&) = default;

	LocalisationEditor(LocalisationEditor&&) = default;

	auto operator=(const LocalisationEditor&) -> LocalisationEditor& = default;

	auto operator=(LocalisationEditor&&) -> LocalisationEditor& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~LocalisationEditor();

	/**
	 * @brief
	 *  The localisation as it is edited.
	 * @return The localisation.
	 */
	[[nodiscard]] virtual auto localisation() const -> const data::localisation::Localisation& = 0;

	/**
	 * @brief
	 *  Add a language.
	 * @param[in] iCode Its code, unique.
	 * @param[in] iName Its name.
	 * @return False when the code is empty or taken.
	 */
	virtual auto addLanguage(const std::string& iCode, const std::string& iName) -> bool = 0;

	/**
	 * @brief
	 *  Remove a language and its values.
	 * @param[in] iCode Its code.
	 * @return True when it was removed.
	 */
	virtual auto removeLanguage(std::string_view iCode) -> bool = 0;

	/**
	 * @brief
	 *  Change the name, the font or the export flag of a language (its code stays).
	 * @param[in] iLanguage The language, found by its code.
	 * @param[in] iMerge Whether the edit merges with the previous one.
	 * @return True when something changed.
	 */
	virtual auto setLanguage(const data::localisation::Language& iLanguage, bool iMerge = false) -> bool = 0;

	/**
	 * @brief
	 *  Choose the fallback language.
	 * @param[in] iCode A language code of the localisation.
	 * @return True when it changed.
	 */
	virtual auto setFallback(std::string_view iCode) -> bool = 0;

	/**
	 * @brief
	 *  Append a string.
	 * @param[in] iKey Its key; made unique when taken.
	 * @param[in] iTable Its string table.
	 * @return The index of the string.
	 */
	virtual auto addString(const std::string& iKey, const std::string& iTable) -> size_t = 0;

	/**
	 * @brief
	 *  Rename the key of a string.
	 * @param[in] iIndex Index of the string.
	 * @param[in] iKey The new key, not empty and not taken.
	 * @return True when it changed.
	 */
	virtual auto setKey(size_t iIndex, const std::string& iKey) -> bool = 0;

	/**
	 * @brief
	 *  Set the value of a string in a language.
	 * @param[in] iIndex Index of the string.
	 * @param[in] iCode The language code.
	 * @param[in] iValue The text.
	 * @param[in] iMerge Whether the edit merges with the previous one.
	 * @return True when it changed.
	 */
	virtual auto setValue(size_t iIndex, const std::string& iCode, const std::string& iValue, bool iMerge = false)
			-> bool = 0;

	/**
	 * @brief
	 *  Move strings to a string table.
	 * @param[in] iIndices Indices of the strings.
	 * @param[in] iTable The table.
	 * @return True when one moved.
	 */
	virtual auto setTable(std::span<const size_t> iIndices, const std::string& iTable) -> bool = 0;

	/**
	 * @brief
	 *  Remove strings.
	 * @param[in] iIndices Indices of the strings.
	 * @return True when one was removed.
	 */
	virtual auto removeStrings(std::span<const size_t> iIndices) -> bool = 0;

	/**
	 * @brief
	 *  Copy strings after the last of them, under unique keys.
	 * @param[in] iIndices Indices of the strings.
	 * @return True when one was copied.
	 */
	virtual auto duplicateStrings(std::span<const size_t> iIndices) -> bool = 0;
};

/**
 * @brief
 *  The asset documents of the editor, as the document commands reach them.
 */
class DocumentHost {
public:
	DocumentHost() = default;

	DocumentHost(const DocumentHost&) = default;

	DocumentHost(DocumentHost&&) = default;

	auto operator=(const DocumentHost&) -> DocumentHost& = default;

	auto operator=(DocumentHost&&) -> DocumentHost& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~DocumentHost();

	/**
	 * @brief
	 *  The tab of a sound cue, opened when it is not.
	 * @param[in] iPath The `.owlsound` file.
	 * @return The editor of the cue, or null when it cannot be opened.
	 */
	virtual auto openSoundCue(const std::filesystem::path& iPath) -> SoundCueEditor* = 0;

	/**
	 * @brief
	 *  The tab of a localisation, opened when it is not.
	 * @param[in] iPath The `localisation.yml` file.
	 * @return The editor of the localisation, or null when it cannot be opened.
	 */
	virtual auto openLocalisation(const std::filesystem::path& iPath) -> LocalisationEditor* = 0;

	/**
	 * @brief
	 *  The tab of a data table, opened when it is not.
	 * @param[in] iPath The `.owltable` file.
	 * @return The editor of the table, or null when it cannot be opened.
	 */
	virtual auto openDataTable(const std::filesystem::path& iPath) -> DataTableEditor* = 0;

	/**
	 * @brief
	 *  The tab of a dialogue, opened when it is not.
	 * @param[in] iPath The `.owldlg` file.
	 * @return The editor of the dialogue, or null when it cannot be opened.
	 */
	virtual auto openDialogue(const std::filesystem::path& iPath) -> DialogueEditor* = 0;

	/**
	 * @brief
	 *  The tab of a sequence, opened when it is not.
	 * @param[in] iPath The `.owlseq` file.
	 * @return The editor of the sequence, or null when it cannot be opened.
	 */
	virtual auto openSequence(const std::filesystem::path& iPath) -> SequenceEditor* = 0;
};

/**
 * @brief
 *  Parse a list of indices written `0, 2, 3`.
 * @param[in] iText The list.
 * @return The indices, or nothing when one is not a number.
 */
[[nodiscard]] auto parseIndices(std::string_view iText) -> std::optional<std::vector<size_t>>;

/**
 * @brief
 *  Format a list of indices as `parseIndices` reads it.
 * @param[in] iIndices The indices.
 * @return The list.
 */
[[nodiscard]] auto formatIndices(std::span<const size_t> iIndices) -> std::string;

/**
 * @brief
 *  Register the `sound.*` and `locale.*` commands that edit the open asset documents.
 * @param[in,out] ioRegistry The registry.
 */
void addDocumentCommands(CommandRegistry& ioRegistry);

}// namespace owl::nest::commands
