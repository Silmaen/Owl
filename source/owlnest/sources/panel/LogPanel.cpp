/**
 * @file LogPanel.cpp
 * @author Silmaen
 * @date 16/02/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "LogPanel.h"

#include <debug/LogSink.h>
#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/IconButton.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <format>
#include <string>
#include <string_view>

namespace owl::nest::panel {

namespace {
auto getLevelColor(const core::Log::Level iLevel) -> ImVec4 {
	switch (iLevel) {
		case core::Log::Level::Trace:
			return {0.6f, 0.6f, 0.6f, 1.0f};
		case core::Log::Level::Debug:
			return {0.4f, 0.7f, 1.0f, 1.0f};
		case core::Log::Level::Info:
			return {0.3f, 0.9f, 0.3f, 1.0f};
		case core::Log::Level::Warning:
			return {1.0f, 0.9f, 0.3f, 1.0f};
		case core::Log::Level::Error:
			return {1.0f, 0.3f, 0.3f, 1.0f};
		case core::Log::Level::Critical:
			return {0.8f, 0.1f, 0.1f, 1.0f};
		case core::Log::Level::Off:
			return {0.5f, 0.5f, 0.5f, 1.0f};
	}
	return {1.0f, 1.0f, 1.0f, 1.0f};
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
	ImGui::Begin("Log");

	// Toolbar: clear, follow, level and logger filter chips, search.
	if (gui::widgets::iconButton("clear", {.glyph = gui::icon::eraser, .tooltip = "Clear the log"}))
		core::Log::getLogBuffer().clear();
	ImGui::SameLine();
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
	chipToggle("Trace", m_showTrace);
	chipToggle("Debug", m_showDebug);
	chipToggle("Info", m_showInfo);
	chipToggle("Warning", m_showWarning);
	chipToggle("Error", m_showError);
	chipToggle("Critical", m_showCritical);
	ImGui::Dummy({ImGui::GetFontSize() * 0.5f, 0.f});
	ImGui::SameLine();
	chipToggle("Engine##logger", m_showCore);
	chipToggle("Game##logger", m_showApp);
	ImGui::SetNextItemWidth(std::max(ImGui::GetFontSize() * 8.f, ImGui::GetContentRegionAvail().x));
	ImGui::InputTextWithHint("##Search", "Filter messages", m_searchBuffer.data(), m_searchBuffer.size());

	ImGui::Separator();

	// Log content with smaller font
	ImGui::SetWindowFontScale(g_fontScale);

	ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

	const auto entries = core::Log::getLogBuffer().getEntries();

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
		const ImVec4 levelColor = getLevelColor(level);
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
