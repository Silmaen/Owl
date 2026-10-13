/**
 * @file CommandPalette.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::panel {

/**
 * @brief
 *  One entry of the command palette: an editor action, an entity to select...
 */
struct PaletteItem {
	/// Group shown on the left ("Scene", "Entity", "View").
	std::string category;
	/// What the entry does or names.
	std::string label;
	/// Shortcut shown on the right, empty for none.
	std::string shortcut;
	/// Run when the entry is chosen.
	std::function<void()> run;
	/// False to grey the entry out (an edit command while playing): it cannot be chosen.
	bool enabled = true;
};

/**
 * @brief
 *  Searchable list of everything the editor can do, opened by pressing `Shift` twice: type a few letters, move with
 *  the arrows, run with `Enter`, leave with `Escape`.
 */
class CommandPalette final {
public:
	/// Entries shown at once.
	static constexpr size_t maxResults = 12;
	/// Longest delay between the two `Shift` presses that open the palette, in seconds.
	static constexpr double doubleTapDelay = 0.4;
	/// Most asset files listed by `collectAssets`.
	static constexpr size_t maxAssets = 4000;

	/**
	 * @brief
	 *  How well a label matches a query: higher is better, negative is no match.
	 *
	 * Every character of the query must appear in order (case-insensitive). A run of contiguous characters, a match
	 * at the start of a word and an early match all rank higher.
	 * @param[in] iLabel The label (category and label of an entry).
	 * @param[in] iQuery What the user typed; empty matches everything with the score 0.
	 * @return The score.
	 */
	[[nodiscard]] static auto score(std::string_view iLabel, std::string_view iQuery) -> int;

	/**
	 * @brief
	 *  Indices of the entries matching a query, best first, at most `maxResults`.
	 * @param[in] iItems The entries.
	 * @param[in] iQuery The query.
	 * @return The indices into `iItems`.
	 */
	[[nodiscard]] static auto filter(const std::vector<PaletteItem>& iItems, std::string_view iQuery)
			-> std::vector<size_t>;

	/**
	 * @brief
	 *  Category shown for an editor action, from the prefix of its identifier ("scene.save" is "Scene").
	 * @param[in] iActionId The action identifier.
	 * @return The category, "Command" for an unknown prefix.
	 */
	[[nodiscard]] static auto categoryOf(std::string_view iActionId) -> std::string;

	/**
	 * @brief
	 *  Every file under a folder, hidden folders and files skipped, at most `maxAssets`.
	 * @param[in] iRoot The folder.
	 * @return The files, sorted.
	 */
	[[nodiscard]] static auto collectAssets(const std::filesystem::path& iRoot) -> std::vector<std::filesystem::path>;

	/**
	 * @brief
	 *  Record a press of `Shift`.
	 * @param[in] iTime Time of the press, in seconds.
	 * @return True when it is the second press of a double tap: the palette should open.
	 */
	auto onShiftPressed(double iTime) -> bool;

	/**
	 * @brief
	 *  Record a press of any other key or button, which breaks a double tap.
	 */
	void onOtherInput() { m_lastShift.reset(); }

	/**
	 * @brief
	 *  Watch the keyboard of this frame and open the palette on a double `Shift` (not while typing in a field).
	 */
	void pollDoubleShift();

	/**
	 * @brief
	 *  Whether the palette was opened since the last call, to refresh what it lists.
	 * @return True once after each opening.
	 */
	auto consumeOpened() -> bool {
		const bool opened = m_justOpened;
		m_justOpened = false;
		return opened;
	}

	/**
	 * @brief
	 *  Show the palette with an empty query.
	 */
	void open();

	/**
	 * @brief
	 *  Hide the palette.
	 */
	void close() { m_open = false; }

	/**
	 * @brief
	 *  Whether the palette is shown.
	 * @return True when open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_open; }

	/**
	 * @brief
	 *  Draw the palette when open; choosing an entry runs it and closes the palette.
	 * @param[in] iItems The entries to search.
	 */
	void onImGuiRender(const std::vector<PaletteItem>& iItems);

private:
	/// The palette is shown.
	bool m_open = false;
	/// The query field takes the keyboard focus on the next frame.
	bool m_focusInput = false;
	/// What the user typed.
	std::string m_query;
	/// Index of the highlighted result.
	size_t m_highlighted = 0;
	/// Time of the last `Shift` press not yet part of a double tap.
	std::optional<double> m_lastShift;
	/// Opened since the last `consumeOpened`.
	bool m_justOpened = false;
};

}// namespace owl::nest::panel
