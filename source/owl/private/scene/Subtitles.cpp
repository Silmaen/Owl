/**
 * @file Subtitles.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/Subtitles.h"

#include <algorithm>
#include <string>

namespace owl::scene {

void Subtitles::show(const std::string_view iText, const float iSeconds) {
	if (iText.empty())
		return;
	const float seconds = std::max(iSeconds, 0.1f);
	if (const auto found = std::ranges::find(m_lines, iText, &Line::text); found != m_lines.end()) {
		found->remaining = std::max(found->remaining, seconds);
		return;
	}
	if (m_lines.size() >= g_maxLines)
		m_lines.erase(m_lines.begin());
	m_lines.push_back({.text = std::string{iText}, .remaining = seconds});
}

void Subtitles::update(const float iDeltaSeconds) {
	for (auto& line: m_lines) line.remaining -= iDeltaSeconds;
	std::erase_if(m_lines, [](const Line& iLine) -> bool { return iLine.remaining <= 0.f; });
}

}// namespace owl::scene
