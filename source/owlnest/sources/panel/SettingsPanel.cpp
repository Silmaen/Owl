/**
 * @file SettingsPanel.cpp
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SettingsPanel.h"

#include "../EditorResources.h"
#include "LogPanel.h"

#include <app/Application.h>
#include <core/Log.h>
#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <platform/UserDirectory.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>

namespace owl::nest::panel {

namespace {

auto settingInt(const char* iLabel, int& ioValue, const int iMin, const int iMax, const int iDefault,
				const char* iFormat, const char* iTooltip) -> bool {
	gui::widgets::propertyLabel(iLabel, iTooltip);
	bool changed = ImGui::SliderInt("##value", &ioValue, iMin, iMax, iFormat);
	ioValue = std::clamp(ioValue, iMin, iMax);
	if (gui::widgets::propertyReset(ioValue != iDefault)) {
		ioValue = iDefault;
		changed = true;
	}
	return changed;
}

// The fields the window edits; the rest of the settings (session, layout, history) changes elsewhere meanwhile.
template<typename Settings>
auto editedFields(Settings& ioSettings) -> auto {
	return std::tie(ioSettings.showStats, ioSettings.showAllFiles, ioSettings.textScalePercent, ioSettings.uiFontSize,
					ioSettings.codeEditorFontSize, ioSettings.autosaveIntervalSeconds, ioSettings.frameRateLimit,
					ioSettings.backgroundFrameRateLimit, ioSettings.idleWaitMs, ioSettings.snapEnabled,
					ioSettings.snapStep, ioSettings.snapMultiplier, ioSettings.snapRotationDegrees,
					ioSettings.snapAutoFromTilemap, ioSettings.cameraRotatePercent, ioSettings.cameraPanPercent,
					ioSettings.cameraZoomPercent, ioSettings.cameraInvertY, ioSettings.cameraDeadZone,
					ioSettings.cameraFlySpeed, ioSettings.themePreset, ioSettings.logLevel, ioSettings.mcpEnabled,
					ioSettings.mcpPort);
}

auto currentFrameLogFrequency() -> uint64_t {
	return app::Application::instanced() ? app::Application::get().getInitParams().frameLogFrequency
										 : core::g_DefaultFrequency;
}

void copyOnClick(const std::string& iText) {
	if (iText.empty() || !ImGui::IsItemHovered())
		return;
	ImGui::SetTooltip("Click to copy");
	if (ImGui::IsItemClicked())
		ImGui::SetClipboardText(iText.c_str());
}

}// namespace

void SettingsPanel::beginEdit(const EditorSettings& iSettings, const ActionRegistry& iRegistry) {
	m_draft = iSettings;
	m_shortcutsAtOpen = iRegistry.getOverrides();
	m_draftFrameLogFrequency = currentFrameLogFrequency();
}

auto SettingsPanel::hasPendingChanges(const EditorSettings& iSettings) const -> bool {
	if (!m_draft.has_value())
		return false;
	return editedFields(*m_draft) != editedFields(iSettings) || m_draftFrameLogFrequency != currentFrameLogFrequency();
}

void SettingsPanel::apply(EditorSettings& ioSettings, const ActionRegistry& iRegistry) {
	if (!m_draft.has_value())
		return;
	const bool themeChanged = m_draft->themePreset != ioSettings.themePreset;
	editedFields(ioSettings) = editedFields(std::as_const(*m_draft));
	ioSettings.keybindingOverrides = iRegistry.getOverrides();
	m_shortcutsAtOpen = ioSettings.keybindingOverrides;
	if (themeChanged && ImGui::GetCurrentContext() != nullptr)
		utils::applyTheme(gui::Theme::fromPreset(gui::Theme::presetFromName(ioSettings.themePreset)));
	core::Log::setVerbosityLevel(getLogLevelSetting(ioSettings.logLevel));
	if (app::Application::instanced()) {
		if (auto& params = app::Application::get().getInitParams();
			params.frameLogFrequency != m_draftFrameLogFrequency) {
			params.frameLogFrequency = m_draftFrameLogFrequency;
			core::Log::setFrameFrequency(params.frameLogFrequency);
			params.saveToFile(platform::getUserDirectory() / "config.yml");
		}
	}
}

void SettingsPanel::cancel(ActionRegistry& ioRegistry) {
	ioRegistry.resetToDefaults();
	ioRegistry.loadOverrides(m_shortcutsAtOpen);
	ioRegistry.setSuspended(false);
	m_capturing = false;
	m_captureActionId.clear();
	m_conflictMessage.clear();
	m_draft.reset();
	m_visible = false;
}

void SettingsPanel::onImGuiRender(EditorSettings& ioSettings, ActionRegistry& ioRegistry) {
	if (!m_visible) {
		m_draft.reset();
		return;
	}
	if (!m_draft.has_value())
		beginEdit(ioSettings, ioRegistry);
	ImGui::SetNextWindowSize({ImGui::GetFontSize() * 46.f, ImGui::GetFontSize() * 28.f}, ImGuiCond_FirstUseEver);
	const bool expanded = ImGui::Begin("Editor Settings###EditorSettings", &m_visible);
	if (!m_visible) {
		ImGui::End();
		cancel(ioRegistry);
		return;
	}
	if (!expanded) {
		ImGui::End();
		return;
	}
	auto& draft = *m_draft;
	const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.f;
	if (ImGui::BeginChild("##settingsBody", {0.f, -footer}) && ImGui::BeginTabBar("##settingsTabs")) {
		if (ImGui::BeginTabItem(std::format("{}  General###general", gui::icon::settings2).c_str())) {
			renderGeneralSection(draft);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(std::format("{}  Gizmo Snap###snap", gui::icon::magnet).c_str())) {
			renderSnapSection(draft);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(std::format("{}  Camera###camera", gui::icon::video).c_str())) {
			renderCameraSection(draft);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(std::format("{}  Appearance###appearance", gui::icon::paintbrush).c_str())) {
			renderThemeSection(draft);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(std::format("{}  Log###log", gui::icon::scrollText).c_str())) {
			renderLogSection(draft);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(std::format("{}  Shortcuts###shortcuts", gui::icon::keyboard).c_str())) {
			renderKeybindingsSection(draft, ioRegistry);
			ImGui::EndTabItem();
		}
		const ImGuiTabItemFlags mcpFlags = m_selectMcpTab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
		m_selectMcpTab = false;
		if (ImGui::BeginTabItem(std::format("{}  MCP Server###mcp", gui::icon::globe).c_str(), nullptr, mcpFlags)) {
			renderMcpSection(draft);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::EndChild();
	renderFooter(ioSettings, ioRegistry);
	ImGui::End();
}

void SettingsPanel::renderFooter(EditorSettings& ioSettings, ActionRegistry& ioRegistry) {
	ImGui::Separator();
	const auto& style = ImGui::GetStyle();
	const float width = ImGui::CalcTextSize("Cancel").x + ImGui::GetFontSize() + style.ItemInnerSpacing.x +
						style.FramePadding.x * 2.f;
	ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
								  ImGui::GetWindowContentRegionMax().x - width * 3.f - style.ItemSpacing.x * 2.f));
	const bool pending = hasPendingChanges(ioSettings);
	if (gui::widgets::button("OK##settings", {.glyph = gui::icon::check,
											  .label = "OK",
											  .tooltip = "Apply the changes and close the window",
											  .primary = true,
											  .width = width})) {
		apply(ioSettings, ioRegistry);
		m_draft.reset();
		m_visible = false;
		return;
	}
	ImGui::SameLine();
	if (gui::widgets::button("Apply##settings", {.glyph = gui::icon::check,
												 .label = "Apply",
												 .tooltip = "Apply the changes and keep the window open",
												 .disabled = !pending,
												 .width = width}))
		apply(ioSettings, ioRegistry);
	ImGui::SameLine();
	if (gui::widgets::button("Cancel##settings",
							 {.glyph = gui::icon::x,
							  .label = "Cancel",
							  .tooltip = "Close the window and forget the changes since the last Apply",
							  .width = width}))
		cancel(ioRegistry);
}

void SettingsPanel::renderGeneralSection(EditorSettings& ioSettings) {
	const EditorSettings defaults;
	if (!gui::widgets::beginProperties("##general"))
		return;
	std::ignore = gui::widgets::propertyBool("Stats Panel", ioSettings.showStats, defaults.showStats,
											 "Show the frame statistics panel.");
	std::ignore = gui::widgets::propertyBool(
			"Show All Files", ioSettings.showAllFiles, defaults.showAllFiles,
			"List every file in the Content Browser: the compiled shaders (spirv folders, .spv and .spv.hash), "
			"the engine logos and the assets of Owl Nest itself (App assets).");
	std::ignore = settingInt("Text Size", ioSettings.textScalePercent, 75, 150, defaults.textScalePercent, "%d %%",
							 "Scale of every text of the editor, applied at once (Ctrl+= / Ctrl+- / Ctrl+0).");
	std::ignore = settingInt("UI Font Size", ioSettings.uiFontSize, 14, 24, defaults.uiFontSize, "%d px",
							 "Pixel size of the main UI font. Takes effect on the next startup.");
	std::ignore =
			settingInt("Code Font Size", ioSettings.codeEditorFontSize, 8, 32, defaults.codeEditorFontSize, "%d px",
					   "Pixel size used inside the code and text editor tabs. Takes effect on the next "
					   "startup.");
	std::ignore = settingInt("Autosave Every", ioSettings.autosaveIntervalSeconds, 0, 600,
							 defaults.autosaveIntervalSeconds, "%d s",
							 "Modified documents are copied aside at this interval and offered back after a crash. 0 "
							 "disables the autosave.");
	std::ignore = settingInt("Frame Rate Limit", ioSettings.frameRateLimit, 0, 1000, defaults.frameRateLimit, "%d fps",
							 "Most frames drawn per second while the editor has the focus. 0 follows the display.");
	std::ignore = settingInt("Background Frame Rate", ioSettings.backgroundFrameRateLimit, 0, 1000,
							 defaults.backgroundFrameRateLimit, "%d fps",
							 "Most frames drawn per second while another window has the focus, to spare the GPU. 0 "
							 "keeps the Frame Rate Limit. A play started from MCP is never slowed down.");
	std::ignore = settingInt("Idle Wait", ioSettings.idleWaitMs, 0, 1000, defaults.idleWaitMs, "%d ms",
							 "While nothing moves in Edit mode (no input, no play, no loading), the editor waits up to "
							 "this long for an input before drawing again. 0 draws every frame.");
	gui::widgets::endProperties();
}

void SettingsPanel::renderSnapSection(EditorSettings& ioSettings) {
	const EditorSettings defaults;
	if (!gui::widgets::beginProperties("##snap"))
		return;
	std::ignore = gui::widgets::propertyBool(
			"Snap to Grid", ioSettings.snapEnabled, defaults.snapEnabled,
			"Snap the gizmos in Edit mode: translation to a regular grid, rotation by the angle step. Holding Ctrl "
			"during a drag also forces snap.");
	std::ignore = gui::widgets::propertyBool(
			"Step from Tilemap", ioSettings.snapAutoFromTilemap, defaults.snapAutoFromTilemap,
			"When a Tilemap exists in the scene, the snap step is its cell size times the multiplier, with a half-cell "
			"offset so entities align with cell centres; otherwise the manual step is used.");
	if (gui::widgets::propertyFloat("Snap Step", ioSettings.snapStep, defaults.snapStep, 0.05f, "%.3f",
									"Manual snap step, in world units, used without a tilemap."))
		ioSettings.snapStep = std::max(0.0001f, ioSettings.snapStep);
	if (gui::widgets::propertyFloat("Cell Multiplier", ioSettings.snapMultiplier, defaults.snapMultiplier, 0.05f,
									"%.3f", "Multiplier of the tilemap cell size (0.5 = half-cell snap)."))
		ioSettings.snapMultiplier = std::max(0.0001f, ioSettings.snapMultiplier);
	if (gui::widgets::propertyFloat("Rotation Step", ioSettings.snapRotationDegrees, defaults.snapRotationDegrees, 1.f,
									"%.1f deg", "Angle step of the rotation gizmo when it snaps."))
		ioSettings.snapRotationDegrees = std::clamp(ioSettings.snapRotationDegrees, 0.1f, 180.f);
	gui::widgets::endProperties();
}

void SettingsPanel::renderCameraSection(EditorSettings& ioSettings) {
	const EditorSettings defaults;
	if (!gui::widgets::beginProperties("##camera"))
		return;
	std::ignore =
			settingInt("Orbit Speed", ioSettings.cameraRotatePercent, 10, 300, defaults.cameraRotatePercent, "%d %%",
					   "Speed of the orbit (Ctrl + left drag) and of the look-around (Alt + left drag).");
	std::ignore = settingInt("Pan Speed", ioSettings.cameraPanPercent, 10, 300, defaults.cameraPanPercent, "%d %%",
							 "Speed of the pan (Ctrl or Alt + right drag).");
	std::ignore = settingInt("Zoom Speed", ioSettings.cameraZoomPercent, 10, 300, defaults.cameraZoomPercent, "%d %%",
							 "Speed of the wheel zoom and of the dolly (Alt + middle drag).");
	std::ignore =
			gui::widgets::propertyBool("Invert Vertical", ioSettings.cameraInvertY, defaults.cameraInvertY,
									   "Turn the vertical drag of the orbit and of the look-around the other way.");
	std::ignore = settingInt("Dead Zone", ioSettings.cameraDeadZone, 0, 20, defaults.cameraDeadZone, "%d px",
							 "Distance the cursor travels before a camera drag moves the view, so a shaky click does "
							 "not nudge it.");
	if (gui::widgets::propertyFloat("Fly Speed", ioSettings.cameraFlySpeed, defaults.cameraFlySpeed, 0.1f,
									"%.1f units/s",
									"Speed of W A S D, Q and E while a drag looks around; the wheel changes it "
									"during the flight."))
		ioSettings.cameraFlySpeed = std::clamp(ioSettings.cameraFlySpeed, 0.1f, 100.f);
	gui::widgets::endProperties();
}

void SettingsPanel::renderMcpSection(EditorSettings& ioSettings) {
	const EditorSettings defaults;
	if (!gui::widgets::beginProperties("##mcp"))
		return;
	std::ignore = gui::widgets::propertyBool(
			"Enabled", ioSettings.mcpEnabled, defaults.mcpEnabled,
			"Serve the Model Context Protocol on 127.0.0.1 so a local AI agent can read the scene and run the editor "
			"commands (undoable). Off by default: no socket is opened.");
	std::ignore = gui::widgets::propertyInt("Port", ioSettings.mcpPort, defaults.mcpPort, 1024, 65535, "%d",
											"Port listened on 127.0.0.1; the server moves when the field is released.");
	gui::widgets::propertyLabel("Status");
	ImGui::TextUnformatted(m_mcpInfo.status.c_str());
	std::ignore = gui::widgets::propertyReset(false);
	gui::widgets::propertyLabel("Endpoint", "URL to give the client (claude mcp add --transport http).");
	ImGui::TextUnformatted(m_mcpInfo.endpoint.empty() ? "-" : m_mcpInfo.endpoint.c_str());
	copyOnClick(m_mcpInfo.endpoint);
	std::ignore = gui::widgets::propertyReset(false);
	gui::widgets::propertyLabel(
			"Token File", "The client sends its content as `Authorization: Bearer <token>`. Only you can read it; "
						  "it exists while the server listens.");
	ImGui::TextUnformatted(m_mcpInfo.tokenFile.c_str());
	copyOnClick(m_mcpInfo.tokenFile);
	std::ignore = gui::widgets::propertyReset(false);
	gui::widgets::endProperties();
	ImGui::Spacing();
	if (gui::widgets::button("regenerateMcpToken", {.glyph = gui::icon::refreshCw, .label = "Regenerate Token"}))
		m_regenerateTokenRequested = true;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Draw a new token: clients holding the old one are refused from now on.");
}

void SettingsPanel::renderLogSection(EditorSettings& ioSettings) {
	if (!gui::widgets::beginProperties("##log"))
		return;
	gui::widgets::propertyLabel("Level", "Lowest level recorded, in the Log panel and the log file. Default: Info in a "
										 "release build, Debug in a debug build, or the OWL_LOG_VERBOSITY environment "
										 "variable.");
	std::ignore = logLevelCombo("##logLevelSetting", ioSettings.logLevel);
	if (gui::widgets::propertyReset(!ioSettings.logLevel.empty()))
		ioSettings.logLevel.clear();
	auto every = static_cast<int>(m_draftFrameLogFrequency);
	if (gui::widgets::propertyInt("Frame Trace Every", every, 0, 0, 100000, "%d frames",
								  "Frames between two lines of a per-frame trace (OWL_CORE_FRAME_TRACE); 0 turns them "
								  "off. They show at the Trace level only."))
		m_draftFrameLogFrequency = static_cast<uint64_t>(std::max(every, 0));
	gui::widgets::endProperties();
}

void SettingsPanel::renderThemeSection(EditorSettings& ioSettings) {
	if (!gui::widgets::beginProperties("##appearance"))
		return;
	gui::widgets::propertyLabel("Theme", "Colours of the editor.");
	if (gui::widgets::beginCombo("##ThemeCombo", ioSettings.themePreset.c_str())) {
		for (const auto& name: gui::Theme::getPresetNames() | std::views::values) {
			const bool selected = ioSettings.themePreset == name;
			if (ImGui::Selectable(name.c_str(), selected))
				ioSettings.themePreset = name;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	std::ignore = gui::widgets::propertyReset(false);
	gui::widgets::endProperties();
}

void SettingsPanel::renderKeybindingsSection(EditorSettings& ioSettings, ActionRegistry& ioRegistry) {
	if (gui::widgets::button("Reset All to Defaults",
							 {.glyph = gui::icon::rotateCcw, .label = "Reset All to Defaults"})) {
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
			if (gui::widgets::iconButton("##reset",
										 {.glyph = gui::icon::rotateCcw, .tooltip = "Reset to the default"})) {
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
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(gui::vec(gui::UiLayer::getTheme().error), "%s", m_conflictMessage.c_str());
		ImGui::SameLine();
		if (gui::widgets::button("cancelCapture", {.glyph = gui::icon::x, .label = "Cancel"})) {
			m_capturing = false;
			m_captureActionId.clear();
			m_conflictMessage.clear();
			ioRegistry.setSuspended(false);
		}
	}
}


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


}// namespace owl::nest::panel
