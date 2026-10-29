/**
 * @file LogPanel.cpp
 * @author Silmaen
 * @date 16/02/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "LogPanel.h"

#include "../DockLayout.h"

#include <debug/LogSink.h>
#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::panel {

namespace {
auto getLevelColor(const core::Log::Level iLevel) -> math::vec4 {
	const auto& theme = gui::UiLayer::getTheme();
	switch (iLevel) {
		case core::Log::Level::Trace:
			return theme.textDisabled;
		case core::Log::Level::Debug:
			return theme.info;
		case core::Log::Level::Info:
			return theme.success;
		case core::Log::Level::Warning:
			return theme.warning;
		case core::Log::Level::Error:
			return theme.error;
		case core::Log::Level::Critical:
			return gui::Theme::mix(theme.error, theme.text, 0.3f);
		case core::Log::Level::Off:
			return theme.textMuted;
	}
	return theme.text;
}

auto getLevelLabel(const core::Log::Level iLevel) -> const char* {
	switch (iLevel) {
		case core::Log::Level::Trace:
			return "[TRACE]";
		case core::Log::Level::Debug:
			return "[DEBUG]";
		case core::Log::Level::Info:
			return "[INFO] ";
		case core::Log::Level::Warning:
			return "[WARN] ";
		case core::Log::Level::Error:
			return "[ERROR]";
		case core::Log::Level::Critical:
			return "[CRIT] ";
		case core::Log::Level::Off:
			return "[OFF]  ";
	}
	return "[???]  ";
}

constexpr ImVec4 g_cyanColor{0.3f, 0.9f, 1.0f, 1.0f};
constexpr ImVec4 g_timestampColor{0.5f, 0.5f, 0.5f, 1.0f};
constexpr float g_fontScale = 0.85f;

void renderColoredText(const std::string_view iText, const ImVec4& iBaseColor) {
	size_t pos = 0;
	while (pos < iText.size()) {
		// Find the start of a number (digit or leading minus/dot before digits).
		const size_t numStart = iText.find_first_of("0123456789", pos);
		if (numStart == std::string_view::npos) {
			// No more numbers, render the rest in base colour.
			ImGui::PushStyleColor(ImGuiCol_Text, iBaseColor);
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextUnformatted(iText.data() + pos, iText.data() + iText.size());
			ImGui::PopStyleColor();
			break;
		}
		// Render text before the number.
		if (numStart > pos) {
			ImGui::PushStyleColor(ImGuiCol_Text, iBaseColor);
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextUnformatted(iText.data() + pos, iText.data() + numStart);
			ImGui::PopStyleColor();
		}
		// Parse the full number (digits, dots, minus prefix, hex 0x...).
		size_t numEnd = numStart;
		// Include a leading minus if present just before.
		if (numStart > 0 && numStart > pos && iText[numStart - 1] == '-') {
			// Don't rewind, minus was already rendered; just note it.
		}
		// Hex prefix.
		if (numEnd + 1 < iText.size() && iText[numEnd] == '0' &&
			(iText[numEnd + 1] == 'x' || iText[numEnd + 1] == 'X')) {
			numEnd += 2;
			while (numEnd < iText.size() &&
				   ((iText[numEnd] >= '0' && iText[numEnd] <= '9') || (iText[numEnd] >= 'a' && iText[numEnd] <= 'f') ||
					(iText[numEnd] >= 'A' && iText[numEnd] <= 'F')))
				++numEnd;
		} else {
			while (numEnd < iText.size() && ((iText[numEnd] >= '0' && iText[numEnd] <= '9') || iText[numEnd] == '.'))
				++numEnd;
		}
		// Render the number in cyan.
		ImGui::PushStyleColor(ImGuiCol_Text, g_cyanColor);
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextUnformatted(iText.data() + numStart, iText.data() + numEnd);
		ImGui::PopStyleColor();
		pos = numEnd;
	}
}

auto formatTimestamp(const std::chrono::system_clock::time_point& iTime) -> std::string {
	const auto timeT = std::chrono::system_clock::to_time_t(iTime);
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(iTime.time_since_epoch()) % 1000;
	std::tm tm{};
#ifdef _WIN32
	localtime_s(&tm, &timeT);
#else
	localtime_r(&timeT, &tm);
#endif
	return std::format("{:02}:{:02}:{:02}.{:03}", tm.tm_hour, tm.tm_min, tm.tm_sec, ms.count());
}

auto formatLine(const debug::LogEntry& iEntry) -> std::string {
	return std::format("{} {} {}: {}", formatTimestamp(iEntry.timestamp), getLevelLabel(iEntry.level),
					   iEntry.loggerName, iEntry.message);
}

auto joinLines(const std::vector<const debug::LogEntry*>& iShown) -> std::string {
	std::string text;
	for (const auto* entry: iShown) {
		text += formatLine(*entry);
		text += '\n';
	}
	return text;
}

}// namespace

void LogPanel::onImGuiRender() {
	const auto entries = core::Log::getLogBuffer().getEntries();
	std::array<size_t, magic_enum::enum_count<core::Log::Level>()> counts{};
	for (const auto& entry: entries) ++counts[magic_enum::enum_index(entry.level).value_or(0)];
	const auto countOf = [&counts](const core::Log::Level iLevel) -> size_t {
		return counts[magic_enum::enum_index(iLevel).value_or(0)];
	};
	const size_t problems =
			countOf(core::Log::Level::Warning) + countOf(core::Log::Level::Error) + countOf(core::Log::Level::Critical);
	const std::string label = problems > 0 ? std::format("Log  {}", problems) : std::string{"Log"};
	ImGui::Begin(panelWindow::title(gui::icon::scrollText, label, panelWindow::log).c_str());

	// Toolbar: follow, level and logger filter chips, then search and clear on the right.
	if (gui::widgets::iconButton("follow", {.glyph = gui::icon::arrowDown,
											.tooltip = "Follow new messages",
											.description = "Keep the view scrolled to the latest message.",
											.selected = m_autoScroll}))
		m_autoScroll = !m_autoScroll;
	ImGui::SameLine();
	if (gui::widgets::iconButton("textMode", {.glyph = gui::icon::textCursorInput,
											  .tooltip = "Select text",
											  .description = "Show the messages as plain text to select and copy "
															 "(Ctrl+C); the colours come back when it is off.",
											  .selected = m_textMode}))
		m_textMode = !m_textMode;
	ImGui::SameLine();
	const auto chipToggle = [](const char* iLabel, bool& ioFlag) -> void {
		if (gui::widgets::chip(iLabel, ioFlag))
			ioFlag = !ioFlag;
		ImGui::SameLine();
	};
	const auto levelChip = [&countOf](const char* iLabel, bool& ioFlag, const core::Log::Level iLevel) -> void {
		if (gui::widgets::chip(std::format("{}  {}###{}", iLabel, countOf(iLevel), iLabel).c_str(), ioFlag,
							   getLevelColor(iLevel)))
			ioFlag = !ioFlag;
		ImGui::SameLine();
	};
	levelChip("Trace", m_showTrace, core::Log::Level::Trace);
	levelChip("Debug", m_showDebug, core::Log::Level::Debug);
	levelChip("Info", m_showInfo, core::Log::Level::Info);
	levelChip("Warning", m_showWarning, core::Log::Level::Warning);
	levelChip("Error", m_showError, core::Log::Level::Error);
	levelChip("Critical", m_showCritical, core::Log::Level::Critical);
	ImGui::Dummy({ImGui::GetFontSize() * 0.5f, 0.f});
	ImGui::SameLine();
	chipToggle("Engine##logger", m_showCore);
	chipToggle("Game##logger", m_showApp);
	const float clearWidth = ImGui::CalcTextSize("Clear").x + ImGui::GetFontSize() * 2.f +
							 ImGui::GetStyle().FramePadding.x * 2.f + ImGui::GetStyle().ItemSpacing.x;
	const float searchWidth =
			std::max(ImGui::GetFontSize() * 8.f,
					 std::min(ImGui::GetFontSize() * 18.f, ImGui::GetContentRegionAvail().x - clearWidth));
	ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - clearWidth - searchWidth));
	ImGui::SetNextItemWidth(searchWidth);
	ImGui::InputTextWithHint("##Search", "Filter messages", m_searchBuffer.data(), m_searchBuffer.size());
	ImGui::SameLine();
	if (gui::widgets::button("clear", {.glyph = gui::icon::eraser, .label = "Clear", .tooltip = "Clear the log"}))
		core::Log::getLogBuffer().clear();

	ImGui::Separator();

	std::vector<const debug::LogEntry*> shown;
	shown.reserve(entries.size());
	for (const auto& entry: entries)
		if (isShown(entry))
			shown.push_back(&entry);
	if (m_textMode)
		renderSelectableText(shown);
	else
		renderColoredLog(shown);
	ImGui::End();
}

auto LogPanel::isShown(const debug::LogEntry& iEntry) const -> bool {
	bool levelShown = false;
	switch (iEntry.level) {
		case core::Log::Level::Trace:
			levelShown = m_showTrace;
			break;
		case core::Log::Level::Debug:
			levelShown = m_showDebug;
			break;
		case core::Log::Level::Info:
			levelShown = m_showInfo;
			break;
		case core::Log::Level::Warning:
			levelShown = m_showWarning;
			break;
		case core::Log::Level::Error:
			levelShown = m_showError;
			break;
		case core::Log::Level::Critical:
			levelShown = m_showCritical;
			break;
		case core::Log::Level::Off:
			break;
	}
	if (!levelShown || (iEntry.loggerName == "OWL" && !m_showCore) || (iEntry.loggerName == "APP" && !m_showApp))
		return false;
	const std::string_view searchFilter(m_searchBuffer.data());
	return searchFilter.empty() || iEntry.message.find(searchFilter) != std::string::npos;
}

void LogPanel::renderColoredLog(const std::vector<const debug::LogEntry*>& iShown) {
	ImGui::SetWindowFontScale(g_fontScale);
	ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
	// The scroll of the previous frame: read before this frame's lines make the content taller.
	const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.f;
	if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel > 0.f)
		m_autoScroll = false;
	else if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel < 0.f && atBottom)
		m_autoScroll = true;
	std::optional<size_t> hoveredLine;
	for (size_t i = 0; i < iShown.size(); ++i) {
		const auto& [level, loggerName, message, timestamp] = *iShown[i];
		const ImVec4 levelColor = gui::vec(getLevelColor(level));
		const float top = ImGui::GetCursorScreenPos().y;
		ImGui::PushStyleColor(ImGuiCol_Text, g_timestampColor);
		ImGui::TextUnformatted(formatTimestamp(timestamp).c_str());
		ImGui::PopStyleColor();
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, levelColor);
		ImGui::TextUnformatted(std::format(" {} {}: ", getLevelLabel(level), loggerName).c_str());
		ImGui::PopStyleColor();
		renderColoredText(message, levelColor);
		if (const float mouseY = ImGui::GetIO().MousePos.y;
			ImGui::IsWindowHovered() && mouseY >= top && mouseY < ImGui::GetCursorScreenPos().y)
			hoveredLine = i;
	}
	if (hoveredLine.has_value() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		m_menuLine = *hoveredLine;
		ImGui::OpenPopup("##logMenu");
	}
	if (ImGui::BeginPopup("##logMenu")) {
		ImGui::SetWindowFontScale(1.f);
		if (gui::widgets::menuItem(
					{.glyph = gui::icon::copy, .label = "Copy Line", .enabled = m_menuLine < iShown.size()}))
			ImGui::SetClipboardText(formatLine(*iShown[m_menuLine]).c_str());
		if (gui::widgets::menuItem({.glyph = gui::icon::clipboardCopy, .label = "Copy All Shown"}))
			ImGui::SetClipboardText(joinLines(iShown).c_str());
		if (gui::widgets::menuItem({.glyph = gui::icon::textCursorInput, .label = "Select Text"}))
			m_textMode = true;
		ImGui::EndPopup();
	}
	if (m_autoScroll)
		ImGui::SetScrollHereY(1.0f);
	ImGui::EndChild();
	ImGui::SetWindowFontScale(1.0f);
}

void LogPanel::renderSelectableText(const std::vector<const debug::LogEntry*>& iShown) {
	m_selectableText = joinLines(iShown);
	ImGui::SetWindowFontScale(g_fontScale);
	ImGui::InputTextMultiline("##logText", m_selectableText.data(), m_selectableText.size() + 1,
							  ImGui::GetContentRegionAvail(), ImGuiInputTextFlags_ReadOnly);
	ImGui::SetWindowFontScale(1.0f);
}

}// namespace owl::nest::panel
