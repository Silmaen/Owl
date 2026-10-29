/**
 * @file EngineContext.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "app/EngineContext.h"

#include "data/assets/AssetSearchPaths.h"

#include "renderer/MaterialLibrary.h"
#include "renderer/MeshCache.h"
#include "renderer/RendererVoxel.h"
#include "scene/HudLibrary.h"
#include "scene/ScreenTransition.h"
#include "scene/SettingsManager.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace owl::app {

EngineContext::EngineContext()
	: mp_screenTransition{mkUniq<scene::ScreenTransition>()}, mp_settings{mkUniq<scene::SettingsManager>()},
	  mp_meshCache{mkUniq<renderer::MeshCache>()}, mp_materialLibrary{mkUniq<renderer::MaterialLibrary>()},
	  mp_voxelMeshCache{mkUniq<renderer::VoxelMeshCache>()}, mp_hudLibrary{mkUniq<scene::HudLibrary>(this)} {}

EngineContext::~EngineContext() = default;

auto EngineContext::loadActionMap(const std::filesystem::path& iProjectDirectory) -> bool {
	setActionMap({});
	if (iProjectDirectory.empty())
		return false;
	const auto file = iProjectDirectory / input::ActionMap::fileName;
	if (!exists(file))
		return false;
	auto map = input::ActionMap::loadFile(file);
	if (!map)
		return false;
	setActionMap(*map);
	return true;
}

void EngineContext::setLocalisation(data::localisation::Localisation iLocalisation) {
	m_localisation = std::move(iLocalisation);
	m_stringIndex.clear();
	for (const auto& entry: m_localisation.strings) m_stringIndex.emplace(entry.key, &entry);
	m_language = m_localisation.resolveLanguage(m_requestedLanguage);
	++m_localisationGeneration;
}

auto EngineContext::loadLocalisation(const std::filesystem::path& iProjectDirectory) -> bool {
	std::optional<data::localisation::Localisation> loaded;
	if (const auto file = iProjectDirectory / data::localisation::Localisation::fileName;
		!iProjectDirectory.empty() && exists(file))
		loaded = data::localisation::Localisation::loadFile(file);
	else if (const auto text = data::assets::readAssetText(std::string{data::localisation::Localisation::fileName});
			 text.has_value())
		loaded = data::localisation::Localisation::deserializeFromString(*text);
	setLocalisation(loaded.value_or(data::localisation::Localisation{}));
	return loaded.has_value();
}

void EngineContext::setLanguage(const std::string_view iCode) {
	m_requestedLanguage = iCode;
	if (auto resolved = m_localisation.resolveLanguage(iCode); resolved != m_language) {
		m_language = std::move(resolved);
		++m_localisationGeneration;
	}
}

auto EngineContext::translate(const std::string_view iKey) const -> std::string_view {
	const auto found = m_stringIndex.find(iKey);
	return found == m_stringIndex.end()
				   ? iKey
				   : data::localisation::getValue(*found->second, m_language, m_localisation.fallback);
}

auto EngineContext::getLanguageFont() const -> const std::string& {
	static const std::string s_none;
	const auto* language = m_localisation.findLanguage(m_language);
	return language != nullptr ? language->font : s_none;
}

auto EngineContext::loadActionMapFromText(const std::string_view iText) -> bool {
	setActionMap({});
	auto map = input::ActionMap::fromYaml(iText, input::ActionMap::fileName);
	if (!map)
		return false;
	setActionMap(*map);
	return true;
}

}// namespace owl::app
