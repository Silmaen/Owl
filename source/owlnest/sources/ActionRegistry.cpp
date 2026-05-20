/**
 * @file ActionRegistry.cpp
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ActionRegistry.h"

#include <imgui.h>

namespace owl::nest {

namespace {
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
auto buildKeyNameMap() -> const std::unordered_map<input::KeyCode, std::string>& {
	static const std::unordered_map<input::KeyCode, std::string> map = {
			{input::key::Space, "Space"},
			{input::key::Apostrophe, "'"},
			{input::key::Comma, ","},
			{input::key::Minus, "-"},
			{input::key::Period, "."},
			{input::key::Slash, "/"},
			{input::key::D0, "0"},
			{input::key::D1, "1"},
			{input::key::D2, "2"},
			{input::key::D3, "3"},
			{input::key::D4, "4"},
			{input::key::D5, "5"},
			{input::key::D6, "6"},
			{input::key::D7, "7"},
			{input::key::D8, "8"},
			{input::key::D9, "9"},
			{input::key::Semicolon, ";"},
			{input::key::Equal, "="},
			{input::key::A, "A"},
			{input::key::B, "B"},
			{input::key::C, "C"},
			{input::key::D, "D"},
			{input::key::E, "E"},
			{input::key::F, "F"},
			{input::key::G, "G"},
			{input::key::H, "H"},
			{input::key::I, "I"},
			{input::key::J, "J"},
			{input::key::K, "K"},
			{input::key::L, "L"},
			{input::key::M, "M"},
			{input::key::N, "N"},
			{input::key::O, "O"},
			{input::key::P, "P"},
			{input::key::Q, "Q"},
			{input::key::R, "R"},
			{input::key::S, "S"},
			{input::key::T, "T"},
			{input::key::U, "U"},
			{input::key::V, "V"},
			{input::key::W, "W"},
			{input::key::X, "X"},
			{input::key::Y, "Y"},
			{input::key::Z, "Z"},
			{input::key::LeftBracket, "["},
			{input::key::Backslash, "\\"},
			{input::key::RightBracket, "]"},
			{input::key::GraveAccent, "`"},
			{input::key::Escape, "Escape"},
			{input::key::Enter, "Enter"},
			{input::key::Tab, "Tab"},
			{input::key::Backspace, "Backspace"},
			{input::key::Insert, "Insert"},
			{input::key::Delete, "Delete"},
			{input::key::Right, "Right"},
			{input::key::Left, "Left"},
			{input::key::Down, "Down"},
			{input::key::Up, "Up"},
			{input::key::PageUp, "PageUp"},
			{input::key::PageDown, "PageDown"},
			{input::key::Home, "Home"},
			{input::key::End, "End"},
			{input::key::F1, "F1"},
			{input::key::F2, "F2"},
			{input::key::F3, "F3"},
			{input::key::F4, "F4"},
			{input::key::F5, "F5"},
			{input::key::F6, "F6"},
			{input::key::F7, "F7"},
			{input::key::F8, "F8"},
			{input::key::F9, "F9"},
			{input::key::F10, "F10"},
			{input::key::F11, "F11"},
			{input::key::F12, "F12"},
	};
	return map;
}

auto buildNameKeyMap() -> const std::unordered_map<std::string, input::KeyCode>& {
	static std::unordered_map<std::string, input::KeyCode> map;
	if (map.empty()) {
		for (const auto& [code, name]: buildKeyNameMap()) map[name] = code;
	}
	return map;
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

auto modifierCount(const Modifiers iMod) -> int {
	int count = 0;
	if (hasFlag(iMod, Modifiers::Ctrl))
		++count;
	if (hasFlag(iMod, Modifiers::Shift))
		++count;
	if (hasFlag(iMod, Modifiers::Alt))
		++count;
	return count;
}

auto isModifierKey(const input::KeyCode iKey) -> bool {
	return iKey == input::key::LeftControl || iKey == input::key::RightControl || iKey == input::key::LeftShift ||
		   iKey == input::key::RightShift || iKey == input::key::LeftAlt || iKey == input::key::RightAlt ||
		   iKey == input::key::LeftSuper || iKey == input::key::RightSuper;
}

}// namespace

auto Shortcut::keyName(const input::KeyCode iKey) -> std::string {
	const auto& map = buildKeyNameMap();
	if (const auto it = map.find(iKey); it != map.end())
		return it->second;
	return std::format("Key{}", iKey);
}

auto Shortcut::keyFromName(const std::string& iName) -> input::KeyCode {
	const auto& map = buildNameKeyMap();
	if (const auto it = map.find(iName); it != map.end())
		return it->second;
	return 0;
}

auto Shortcut::toString() const -> std::string {
	if (isEmpty())
		return "";
	std::string result;
	if (hasFlag(modifiers, Modifiers::Ctrl))
		result += "Ctrl+";
	if (hasFlag(modifiers, Modifiers::Shift))
		result += "Shift+";
	if (hasFlag(modifiers, Modifiers::Alt))
		result += "Alt+";
	result += keyName(key);
	return result;
}

auto Shortcut::fromString(const std::string& iStr) -> Shortcut {
	if (iStr.empty())
		return {};
	Shortcut result;
	std::string remaining = iStr;
	// Parse modifiers
	while (true) {
		if (remaining.starts_with("Ctrl+")) {
			result.modifiers = result.modifiers | Modifiers::Ctrl;
			remaining = remaining.substr(5);
		} else if (remaining.starts_with("Shift+")) {
			result.modifiers = result.modifiers | Modifiers::Shift;
			remaining = remaining.substr(6);
		} else if (remaining.starts_with("Alt+")) {
			result.modifiers = result.modifiers | Modifiers::Alt;
			remaining = remaining.substr(4);
		} else {
			break;
		}
	}
	result.key = keyFromName(remaining);
	return result;
}

auto Shortcut::modifiersMatch() const -> bool {
	const bool ctrlPressed =
			input::Input::isKeyPressed(input::key::LeftControl) || input::Input::isKeyPressed(input::key::RightControl);
	const bool shiftPressed =
			input::Input::isKeyPressed(input::key::LeftShift) || input::Input::isKeyPressed(input::key::RightShift);
	const bool altPressed =
			input::Input::isKeyPressed(input::key::LeftAlt) || input::Input::isKeyPressed(input::key::RightAlt);

	return ctrlPressed == hasFlag(modifiers, Modifiers::Ctrl) && shiftPressed == hasFlag(modifiers, Modifiers::Shift) &&
		   altPressed == hasFlag(modifiers, Modifiers::Alt);
}

void ActionRegistry::registerAction(std::string iId, std::string iDisplayName, Shortcut iDefaultShortcut,
									std::function<void()> iCallback) {
	m_actions.push_back({
			.id = std::move(iId),
			.displayName = std::move(iDisplayName),
			.defaultShortcut = iDefaultShortcut,
			.currentShortcut = iDefaultShortcut,
			.callback = std::move(iCallback),
	});
}

auto ActionRegistry::dispatch(const event::KeyPressedEvent& iEvent) -> bool {
	if (m_suspended)
		return false;
	if (static_cast<int>(iEvent.getRepeatCount()) > 0)
		return false;

	const auto keyCode = iEvent.getKeyCode();
	if (isModifierKey(keyCode))
		return false;

	// Try more-specific shortcuts first (more modifiers = higher priority).
	const ActionEntry* bestMatch = nullptr;
	int bestModCount = -1;
	for (const auto& action: m_actions) {
		if (action.currentShortcut.isEmpty())
			continue;
		if (action.currentShortcut.key == keyCode && action.currentShortcut.modifiersMatch()) {
			const int mc = modifierCount(action.currentShortcut.modifiers);
			if (mc > bestModCount) {
				bestMatch = &action;
				bestModCount = mc;
			}
		}
	}
	if (bestMatch == nullptr)
		return false;
	if (bestMatch->currentShortcut.modifiers == Modifiers::None && ImGui::GetIO().WantCaptureKeyboard) {
		OWL_TRACE("ActionRegistry: shortcut '{}' suppressed by ImGui keyboard capture.", bestMatch->id)
		return false;
	}
	bestMatch->callback();
	return true;
}

auto ActionRegistry::getShortcutString(const std::string& iId) const -> std::string {
	for (const auto& action: m_actions) {
		if (action.id == iId)
			return action.currentShortcut.toString();
	}
	return "";
}

void ActionRegistry::rebind(const std::string& iId, const Shortcut iNewShortcut) {
	for (auto& action: m_actions) {
		if (action.id == iId) {
			action.currentShortcut = iNewShortcut;
			return;
		}
	}
}

void ActionRegistry::resetToDefaults() {
	for (auto& action: m_actions) action.currentShortcut = action.defaultShortcut;
}

auto ActionRegistry::findConflict(const Shortcut& iShortcut, const std::string& iExcludeId) const -> std::string {
	if (iShortcut.isEmpty())
		return "";
	for (const auto& action: m_actions) {
		if (action.id == iExcludeId)
			continue;
		if (action.currentShortcut == iShortcut)
			return action.displayName;
	}
	return "";
}

void ActionRegistry::loadOverrides(const std::unordered_map<std::string, std::string>& iOverrides) {
	for (const auto& [id, shortcutStr]: iOverrides) {
		const auto shortcut = Shortcut::fromString(shortcutStr);
		if (!shortcut.isEmpty())
			rebind(id, shortcut);
	}
}

auto ActionRegistry::getOverrides() const -> std::unordered_map<std::string, std::string> {
	std::unordered_map<std::string, std::string> overrides;
	for (const auto& action: m_actions) {
		if (action.currentShortcut != action.defaultShortcut)
			overrides[action.id] = action.currentShortcut.toString();
	}
	return overrides;
}

}// namespace owl::nest
