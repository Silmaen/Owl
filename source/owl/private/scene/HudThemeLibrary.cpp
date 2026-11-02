/**
 * @file HudThemeLibrary.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/HudThemeLibrary.h"

#include "scene/HudTheme.h"

#include <string>
#include <utility>

namespace owl::scene {

HudThemeLibrary::HudThemeLibrary() = default;

HudThemeLibrary::~HudThemeLibrary() = default;

auto HudThemeLibrary::get(const std::string& iPath) -> shared<const HudTheme> {
	if (iPath.empty())
		return nullptr;
	if (const auto found = m_themes.find(iPath); found != m_themes.end())
		return found->second;
	++m_loadCount;
	shared<const HudTheme> theme;
	if (auto loaded = HudTheme::load(iPath); loaded)
		theme = mkShared<const HudTheme>(std::move(*loaded));
	else
		OWL_CORE_WARN("HudThemeLibrary: Theme '{}' unusable, check the `HudTheme:` key or `DefaultHudTheme`.", iPath)
	m_themes.insert_or_assign(iPath, theme);
	return theme;
}

void HudThemeLibrary::set(const std::string& iPath, const HudTheme& iTheme) {
	auto theme = mkShared<HudTheme>(iTheme);
	theme->resolveFonts();
	m_themes.insert_or_assign(iPath, std::move(theme));
}

void HudThemeLibrary::invalidate(const std::string& iPath) { m_themes.erase(iPath); }

void HudThemeLibrary::clear() { m_themes.clear(); }

}// namespace owl::scene
