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
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <format>
#include <string>
#include <string_view>

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

	// Log content with smaller font
	ImGui::SetWindowFontScale(g_fontScale);

	ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

	const std::string_view searchFilter(m_searchBuffer.data());

	for (const auto& [level, loggerName, message, timestamp]: entries) {
		// Level filter
		switch (level) {
			case core::Log::Level::Trace:
				if (!m_showTrace)
					continue;
				break;
			case core::Log::Level::Debug:
				if (!m_showDebug)
					continue;
				break;
			case core::Log::Level::Info:
				if (!m_showInfo)
					continue;
				break;
			case core::Log::Level::Warning:
				if (!m_showWarning)
					continue;
				break;
			case core::Log::Level::Error:
				if (!m_showError)
					continue;
				break;
			case core::Log::Level::Critical:
				if (!m_showCritical)
					continue;
				break;
			case core::Log::Level::Off:
				continue;
		}
		// Logger filter
		if (loggerName == "OWL" && !m_showCore)
			continue;
		if (loggerName == "APP" && !m_showApp)
			continue;
		// Text search filter
		if (!searchFilter.empty() && message.find(searchFilter) == std::string::npos)
			continue;
		const ImVec4 levelColor = gui::vec(getLevelColor(level));
		// Timestamp in grey
		const auto f_timestamp = formatTimestamp(timestamp);

		ImGui::PushStyleColor(ImGuiCol_Text, g_timestampColor);

		ImGui::TextUnformatted(f_timestamp.c_str());

		ImGui::PopStyleColor();

		// Level label + logger name in level colour
		ImGui::SameLine(0.0f, 0.0f);

		ImGui::PushStyleColor(ImGuiCol_Text, levelColor);

		ImGui::TextUnformatted(std::format(" {} {}: ", getLevelLabel(level), loggerName).c_str());

		ImGui::PopStyleColor();

		// Message with numbers in cyan
		renderColoredText(message, levelColor);
	}

	if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())

		ImGui::SetScrollHereY(1.0f);

	ImGui::EndChild();

	ImGui::SetWindowFontScale(1.0f);

	ImGui::End();
}

}// namespace owl::nest::panel
