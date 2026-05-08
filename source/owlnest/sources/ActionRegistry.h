/**
 * @file ActionRegistry.h
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::nest {
/// Modifier flags for keyboard shortcuts.
enum struct Modifiers : uint8_t {
	None = 0,
	Ctrl = 1 << 0,
	Shift = 1 << 1,
	CtrlShift = Ctrl | Shift,
	Alt = 1 << 2,
	CtrlAlt = Ctrl | Alt,
	ShiftAlt = Shift | Alt,
	CtrlShiftAlt = Ctrl | Shift | Alt,
};

/// Bitwise OR for Modifiers.
constexpr auto operator|(const Modifiers iLhs, const Modifiers iRhs) -> Modifiers {
	return static_cast<Modifiers>(static_cast<uint8_t>(iLhs) | static_cast<uint8_t>(iRhs));
}

/// Bitwise AND for Modifiers.
constexpr auto operator&(const Modifiers iLhs, const Modifiers iRhs) -> Modifiers {
	return static_cast<Modifiers>(static_cast<uint8_t>(iLhs) & static_cast<uint8_t>(iRhs));
}

/**
 * @brief
 *  Check if a modifier flag is set.
 */
constexpr auto hasFlag(const Modifiers iValue, const Modifiers iFlag) -> bool {
	return (iValue & iFlag) == iFlag;
}

/// A keyboard shortcut: a key code plus modifier flags.
struct Shortcut {
	/// The key code (0 = unbound).
	input::KeyCode key = 0;
	/// Modifier flags.
	Modifiers modifiers = Modifiers::None;

	/**
	 * @brief
	 *  Equality comparison.
	 * @return True when the operands are equal.
	 */
	auto operator==(const Shortcut&) const -> bool = default;

	/**
	 * @brief
	 *  Return true if this shortcut is unbound.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return key == 0; }

	/**
	 * @brief
	 *  Generate display string like "Ctrl+Shift+S".
	 */
	[[nodiscard]] auto toString() const -> std::string;

	/**
	 * @brief
	 *  Parse a display string back into a Shortcut.
	 */
	static auto fromString(const std::string& iStr) -> Shortcut;

	/**
	 * @brief
	 *  Check if the currently pressed modifier keys match this shortcut exactly.
	 */
	[[nodiscard]] auto modifiersMatch() const -> bool;

	/**
	 * @brief
	 *  Get a human-readable name for a key code.
	 */
	static auto keyName(input::KeyCode iKey) -> std::string;

	/**
	 * @brief
	 *  Parse a key name back to a key code. Returns 0 if unknown.
	 */
	static auto keyFromName(const std::string& iName) -> input::KeyCode;
};

/// Registration entry for a single action.
struct ActionEntry {
	/// Unique action identifier (e.g. "scene.new").
	std::string id;
	/// Human-readable name (e.g. "New Scene").
	std::string displayName;
	/// Factory default shortcut.
	Shortcut defaultShortcut;
	/// Current (potentially rebound) shortcut.
	Shortcut currentShortcut;
	/// Callback to execute.
	std::function<void()> callback;
};
/**
 * @brief
 *  Central registry that binds keyboard shortcuts to named actions.
 */
class ActionRegistry {
public:
	/**
	 * @brief
	 *  Register an action with its default shortcut and callback.
	 * @param[in] iId Unique action identifier.
	 * @param[in] iDisplayName Human-readable name.
	 * @param[in] iDefaultShortcut Default keyboard shortcut.
	 * @param[in] iCallback Function to execute.
	 */
	void registerAction(std::string iId, std::string iDisplayName, Shortcut iDefaultShortcut,
						std::function<void()> iCallback);

	/**
	 * @brief
	 *  Dispatch a key event to the matching action.
	 * @param[in] iEvent The key pressed event.
	 * @return True if an action was triggered.
	 */
	auto dispatch(const event::KeyPressedEvent& iEvent) -> bool;

	/**
	 * @brief
	 *  Get the display shortcut string for a given action ID.
	 * @param[in] iId The action identifier.
	 * @return The shortcut display string, or empty if not found/unbound.
	 */
	[[nodiscard]] auto getShortcutString(const std::string& iId) const -> std::string;

	/**
	 * @brief
	 *  Get all registered actions (for the settings UI).
	 */
	[[nodiscard]] auto getActions() const -> const std::vector<ActionEntry>& { return m_actions; }

	/**
	 * @brief
	 *  Get all registered actions (mutable, for rebinding).
	 */
	[[nodiscard]] auto getActions() -> std::vector<ActionEntry>& { return m_actions; }

	/**
	 * @brief
	 *  Rebind an action to a new shortcut.
	 * @param[in] iId The action identifier.
	 * @param[in] iNewShortcut The new shortcut.
	 */
	void rebind(const std::string& iId, Shortcut iNewShortcut);

	/**
	 * @brief
	 *  Reset all shortcuts to their defaults.
	 */
	void resetToDefaults();

	/**
	 * @brief
	 *  Find a conflicting action for a given shortcut.
	 * @param[in] iShortcut The shortcut to check.
	 * @param[in] iExcludeId Action ID to exclude from the check.
	 * @return The conflicting action's display name, or empty if none.
	 */
	[[nodiscard]] auto findConflict(const Shortcut& iShortcut, const std::string& iExcludeId) const
			-> std::string;

	/**
	 * @brief
	 *  Suspend dispatching (used during key capture in settings UI).
	 */
	void setSuspended(const bool iSuspended) { m_suspended = iSuspended; }

	/**
	 * @brief
	 *  Load keybinding overrides from a saved map.
	 * @param[in] iOverrides Map of action ID to shortcut string.
	 */
	void loadOverrides(const std::unordered_map<std::string, std::string>& iOverrides);

	/**
	 * @brief
	 *  Get current keybinding overrides (only non-default bindings).
	 * @return Map of action ID to shortcut string.
	 */
	[[nodiscard]] auto getOverrides() const -> std::unordered_map<std::string, std::string>;

private:
	/// All registered actions.
	std::vector<ActionEntry> m_actions;
	/// Whether dispatch is suspended.
	bool m_suspended = false;
};

}// namespace owl::nest
