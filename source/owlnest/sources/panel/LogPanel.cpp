/**
 * @file LogPanel.cpp
 * @author Silmaen
 * @date 16/02/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "LogPanel.h"

#include <debug/LogSink.h>

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

/// Render text with numbers highlighted in cyan, rest in the given base color.
void renderColoredText(const std::string_view iText, const ImVec4& iBaseColor) {
	size_t pos = 0;
	while (pos < iText.size()) {
		// Find the start of a number (digit or leading minus/dot before digits).
		const size_t numStart = iText.find_first_of("0123456789", pos);
		if (numStart == std::string_view::npos) {
			// No more numbers, render the rest in base color.
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
		if (numEnd + 1 < iText.size() && iText[numEnd] == '0' && (iText[numEnd + 1] == 'x' || iText[numEnd + 1] == 'X')) {
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

	// Toolbar: Clear, Auto-scroll, level filters, logger filters, search
	if (ImGui::Button("Clear"))
		core::Log::getLogBuffer().clear();
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &m_autoScroll);
	ImGui::SameLine();
	ImGui::Text("|");
	ImGui::SameLine();
	ImGui::Checkbox("Trace", &m_showTrace);
	ImGui::SameLine();
	ImGui::Checkbox("Debug", &m_showDebug);
	ImGui::SameLine();
	ImGui::Checkbox("Info", &m_showInfo);
	ImGui::SameLine();
	ImGui::Checkbox("Warn", &m_showWarning);
	ImGui::SameLine();
	ImGui::Checkbox("Error", &m_showError);
	ImGui::SameLine();
	ImGui::Checkbox("Critical", &m_showCritical);
	ImGui::SameLine();
	ImGui::Text("|");
	ImGui::SameLine();
	ImGui::Checkbox("OWL", &m_showCore);
	ImGui::SameLine();
	ImGui::Checkbox("APP", &m_showApp);
	ImGui::SameLine();
	ImGui::Text("|");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(150.0f);
	ImGui::InputText("##Search", m_searchBuffer.data(), m_searchBuffer.size());

	ImGui::Separator();

	// Log content with smaller font
	ImGui::SetWindowFontScale(g_fontScale);
	ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

	const auto entries = core::Log::getLogBuffer().getEntries();
	const std::string_view searchFilter(m_searchBuffer.data());

	for (const auto& entry : entries) {
		// Level filter
		switch (entry.level) {
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
		if (entry.loggerName == "OWL" && !m_showCore)
			continue;
		if (entry.loggerName == "APP" && !m_showApp)
			continue;
		// Text search filter
		if (!searchFilter.empty() && entry.message.find(searchFilter) == std::string::npos)
			continue;

		const ImVec4 levelColor = getLevelColor(entry.level);

		// Timestamp in grey
		const auto timestamp = formatTimestamp(entry.timestamp);
		ImGui::PushStyleColor(ImGuiCol_Text, g_timestampColor);
		ImGui::TextUnformatted(timestamp.c_str());
		ImGui::PopStyleColor();

		// Level label + logger name in level color
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, levelColor);
		ImGui::TextUnformatted(std::format(" {} {}: ", getLevelLabel(entry.level), entry.loggerName).c_str());
		ImGui::PopStyleColor();

		// Message with numbers in cyan
		renderColoredText(entry.message, levelColor);
	}

	if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		ImGui::SetScrollHereY(1.0f);

	ImGui::EndChild();
	ImGui::SetWindowFontScale(1.0f);
	ImGui::End();
}

}// namespace owl::nest::panel
