/**
 * @file SettingsPanel.cpp
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SettingsPanel.h"

#include <gui/IconBank.h>

#include <algorithm>

namespace owl::nest::panel {

void SettingsPanel::onImGuiRender(EditorSettings& ioSettings, ActionRegistry& ioRegistry) {
	if (!m_visible)
		return;
	ImGui::Begin("Editor Settings", &m_visible);
	renderGeneralSection(ioSettings);
	ImGui::Separator();
	renderThemeSection(ioSettings);
	ImGui::Separator();
	renderKeybindingsSection(ioSettings, ioRegistry);
	ImGui::End();
}

void SettingsPanel::renderGeneralSection(EditorSettings& ioSettings) {
	if (!ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
		return;
	ImGui::Checkbox("Show Stats Panel", &ioSettings.showStats);

	ImGui::Spacing();
	ImGui::TextUnformatted("Code editor font size");
	ImGui::SameLine();
	if (ImGui::SliderInt("##codeEditorFontSize", &ioSettings.codeEditorFontSize, 8, 32, "%d px"))
		ioSettings.codeEditorFontSize = std::clamp(ioSettings.codeEditorFontSize, 8, 48);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
		ImGui::SetTooltip("Pixel size used inside the code/text editor tabs. Takes effect on the next startup. Default is 13.");

	ImGui::Spacing();
	ImGui::TextUnformatted("UI font size");
	ImGui::SameLine();
	if (ImGui::SliderInt("##uiFontSize", &ioSettings.uiFontSize, 14, 24, "%d px"))
		ioSettings.uiFontSize = std::clamp(ioSettings.uiFontSize, 14, 24);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
		ImGui::SetTooltip("Pixel size of the main UI font. Takes effect on the next startup.");
}

void SettingsPanel::renderThemeSection(EditorSettings& ioSettings) {
	if (!ImGui::CollapsingHeader("Theme", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	const auto presets = gui::Theme::getPresetNames();
	// Build preview string
	std::string preview = ioSettings.themePreset;
	if (preview.empty())
		preview = "Dark";

	if (ImGui::BeginCombo("##ThemeCombo", preview.c_str())) {
		for (const auto& [preset, name]: presets) {
			const bool selected = ioSettings.themePreset == name;
			if (ImGui::Selectable(name.c_str(), selected)) {
				ioSettings.themePreset = name;
				gui::UiLayer::setTheme(gui::Theme::fromPreset(preset));
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::Separator();
		{
			const bool selected = ioSettings.themePreset == "Custom";
			if (ImGui::Selectable("Custom (theme.yml)", selected)) {
				ioSettings.themePreset = "Custom";
				if (const auto themePath = core::Application::get().getWorkingDirectory() / "theme.yml";
					exists(themePath)) {
					gui::Theme theme;
					theme.loadFromFile(themePath);
					gui::UiLayer::setTheme(theme);
				}
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

void SettingsPanel::renderKeybindingsSection(EditorSettings& ioSettings, ActionRegistry& ioRegistry) {
	if (!ImGui::CollapsingHeader("Keybindings", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	if (gui::IconBank::instance().iconButton("undo", "Reset All to Defaults")) {
		ioRegistry.resetToDefaults();
		ioSettings.keybindingOverrides.clear();
		m_conflictMessage.clear();
		m_capturing = false;
	}

	ImGui::Spacing();

	constexpr auto tableFlags =
			ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
	if (!ImGui::BeginTable("##keybindings", 3, tableFlags))
		return;

	ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 3.0f);
	ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthStretch, 2.0f);
	ImGui::TableSetupColumn("##reset", ImGuiTableColumnFlags_WidthFixed, 50.0f);
	ImGui::TableHeadersRow();

	for (auto& action: ioRegistry.getActions()) {
		ImGui::PushID(action.id.c_str());
		ImGui::TableNextRow();

		// Column 0: Action name
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(action.displayName.c_str());

		// Column 1: Shortcut button (clickable for capture)
		ImGui::TableSetColumnIndex(1);
		if (m_capturing && m_captureActionId == action.id) {
			handleKeyCapture(action, ioRegistry);
		} else {
			// Normal display: button with current shortcut
			const auto label =
					action.currentShortcut.isEmpty() ? std::string("(none)") : action.currentShortcut.toString();
			if (ImGui::Button(label.c_str(), {-FLT_MIN, 0})) {
				m_capturing = true;
				m_captureActionId = action.id;
				m_conflictMessage.clear();
				ioRegistry.setSuspended(true);
			}
		}

		// Column 2: Reset button
		ImGui::TableSetColumnIndex(2);
		if (action.currentShortcut != action.defaultShortcut) {
			if (ImGui::SmallButton("Reset")) {
				ioRegistry.rebind(action.id, action.defaultShortcut);
				if (m_capturing && m_captureActionId == action.id) {
					m_capturing = false;
					m_captureActionId.clear();
					m_conflictMessage.clear();
					ioRegistry.setSuspended(false);
				}
			}
		}

		ImGui::PopID();
	}
	ImGui::EndTable();
}

void SettingsPanel::handleKeyCapture(ActionEntry& ioAction, ActionRegistry& ioRegistry) {
	ImGui::Button("Press a key...", {-FLT_MIN, 0});

	// Check for Escape to cancel
	if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		m_capturing = false;
		m_captureActionId.clear();
		m_conflictMessage.clear();
		ioRegistry.setSuspended(false);
	} else {
		// Scan for a non-modifier key press
		for (auto imKey = ImGuiKey_NamedKey_BEGIN; imKey < ImGuiKey_NamedKey_END;
			 imKey = static_cast<ImGuiKey>(imKey + 1)) {
			if (!ImGui::IsKeyPressed(imKey))
				continue;
			const auto glfwKey = static_cast<input::KeyCode>(ImGuiKeyToGlfwKey(imKey));
			if (glfwKey == 0)
				continue;
			if (glfwKey >= input::key::LeftShift && glfwKey <= input::key::RightSuper)
				continue;

			// Build shortcut from current modifier state
			Modifiers mods = Modifiers::None;
			if (ImGui::GetIO().KeyCtrl)
				mods = mods | Modifiers::Ctrl;
			if (ImGui::GetIO().KeyShift)
				mods = mods | Modifiers::Shift;
			if (ImGui::GetIO().KeyAlt)
				mods = mods | Modifiers::Alt;

			const Shortcut newShortcut{glfwKey, mods};
			const auto conflict = ioRegistry.findConflict(newShortcut, ioAction.id);
			if (!conflict.empty()) {
				m_conflictMessage = std::format("Conflict with '{}'", conflict);
			} else {
				ioRegistry.rebind(ioAction.id, newShortcut);
				m_capturing = false;
				m_captureActionId.clear();
				m_conflictMessage.clear();
				ioRegistry.setSuspended(false);
			}
			break;
		}
	}

	// Show conflict warning
	if (!m_conflictMessage.empty()) {
		ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "%s", m_conflictMessage.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Cancel")) {
			m_capturing = false;
			m_captureActionId.clear();
			m_conflictMessage.clear();
			ioRegistry.setSuspended(false);
		}
	}
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wswitch-enum")
auto SettingsPanel::ImGuiKeyToGlfwKey(const ImGuiKey iKey) -> int {
	// Map ImGui named keys back to GLFW key codes
	switch (iKey) {
		case ImGuiKey_Tab:
			return 258;
		case ImGuiKey_LeftArrow:
			return 263;
		case ImGuiKey_RightArrow:
			return 262;
		case ImGuiKey_UpArrow:
			return 265;
		case ImGuiKey_DownArrow:
			return 264;
		case ImGuiKey_PageUp:
			return 266;
		case ImGuiKey_PageDown:
			return 267;
		case ImGuiKey_Home:
			return 268;
		case ImGuiKey_End:
			return 269;
		case ImGuiKey_Insert:
			return 260;
		case ImGuiKey_Delete:
			return 261;
		case ImGuiKey_Backspace:
			return 259;
		case ImGuiKey_Space:
			return 32;
		case ImGuiKey_Enter:
			return 257;
		case ImGuiKey_Escape:
			return 256;
		case ImGuiKey_Apostrophe:
			return 39;
		case ImGuiKey_Comma:
			return 44;
		case ImGuiKey_Minus:
			return 45;
		case ImGuiKey_Period:
			return 46;
		case ImGuiKey_Slash:
			return 47;
		case ImGuiKey_Semicolon:
			return 59;
		case ImGuiKey_Equal:
			return 61;
		case ImGuiKey_LeftBracket:
			return 91;
		case ImGuiKey_Backslash:
			return 92;
		case ImGuiKey_RightBracket:
			return 93;
		case ImGuiKey_GraveAccent:
			return 96;
		case ImGuiKey_CapsLock:
			return 280;
		case ImGuiKey_ScrollLock:
			return 281;
		case ImGuiKey_NumLock:
			return 282;
		case ImGuiKey_PrintScreen:
			return 283;
		case ImGuiKey_Pause:
			return 284;
		default:
			break;
	}
	// A-Z (ImGuiKey_A = 546 in some versions, but we use the enum directly)
	if (iKey >= ImGuiKey_A && iKey <= ImGuiKey_Z)
		return 65 + (iKey - ImGuiKey_A);
	// 0-9
	if (iKey >= ImGuiKey_0 && iKey <= ImGuiKey_9)
		return 48 + (iKey - ImGuiKey_0);
	// F1-F12
	if (iKey >= ImGuiKey_F1 && iKey <= ImGuiKey_F12)
		return 290 + (iKey - ImGuiKey_F1);
	// Keypad 0-9
	if (iKey >= ImGuiKey_Keypad0 && iKey <= ImGuiKey_Keypad9)
		return 320 + (iKey - ImGuiKey_Keypad0);

	// Modifier keys
	if (iKey == ImGuiKey_LeftCtrl)
		return 341;
	if (iKey == ImGuiKey_LeftShift)
		return 340;
	if (iKey == ImGuiKey_LeftAlt)
		return 342;
	if (iKey == ImGuiKey_LeftSuper)
		return 343;
	if (iKey == ImGuiKey_RightCtrl)
		return 345;
	if (iKey == ImGuiKey_RightShift)
		return 344;
	if (iKey == ImGuiKey_RightAlt)
		return 346;
	if (iKey == ImGuiKey_RightSuper)
		return 347;
	return 0;
}
OWL_DIAG_POP
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

}// namespace owl::nest::panel
