/**
 * @file FontLibrary.cpp
 * @author Silmaen
 * @date 10/21/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/fonts/FontLibrary.h"

#include "app/Application.h"
#include "data/assets/AssetSearchPaths.h"
#include "data/assets/pack/PackExtractor.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace owl::data::fonts {

namespace {
auto getFontPath() -> std::list<std::filesystem::path> {
	if (!app::Application::instanced())
		return {};
	const auto& app = app::Application::get();
	if (app.getState() != app::Application::State::Running)
		return {};
	std::list<std::filesystem::path> paths;
	for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
		if (exists(assetsPath / "fonts")) {
			paths.push_back(assetsPath / "fonts");
		}
	}
	return paths;
}

auto loadPackedSettings(const std::string& iFontEntry) -> FontSettings {
	FontSettings settings;
	const auto sidecar = FontSettings::sidecarPath(iFontEntry).generic_string();
	if (!app::Application::get().packContains(sidecar))
		return settings;
	if (const auto data = app::Application::get().loadFromPack(sidecar); data) {
		if (!settings.deserializeFromString(
					std::string_view{reinterpret_cast<const char*>(data->data()), data->size()}))
			OWL_CORE_WARN("FontLibrary: Packed settings '{}' ignored, default charset only.", sidecar)
	}
	return settings;
}

}// namespace

FontLibrary::FontLibrary() : m_defaultFontName(g_DefaultFontName) {
	m_fonts["null"] = nullptr;
	m_fonts[m_defaultFontName] = nullptr;
}

void FontLibrary::init() {
	m_fonts.clear();
	loadFont(m_defaultFontName);
	m_fonts["null"] = nullptr;
}

void FontLibrary::destroy() {
	m_fonts.clear();
	m_defaultFontName = g_DefaultFontName;
}

FontLibrary::~FontLibrary() = default;

void FontLibrary::loadFont(const std::string& iName) {
	// Try loading from pack first.
	if (app::Application::instanced() && app::Application::get().hasOpenPack()) {
		const auto& reader = app::Application::get().getPackReader();
		for (const auto& entry: reader.listEntries(data::assets::pack::AssetType::Font)) {
			if (std::filesystem::path(entry).stem() == iName) {
				auto data = app::Application::get().loadFromPack(entry);
				if (data) {
					const auto tempFile = std::filesystem::temp_directory_path() / "owl_pack_cache" /
										  std::filesystem::path(entry).filename();
					if (assets::pack::writeEntryFile(tempFile, *data)) {
						m_fonts.emplace(
								iName, mkShared<Font>(tempFile, iName == m_defaultFontName, loadPackedSettings(entry)));
						return;
					}
					OWL_CORE_WARN("FontLibrary: cannot cache pack entry '{}', trying font directories.", entry)
				}
			}
		}
	}
	// Fallback: load from filesystem.
	for (const auto& path: getFontPath()) {
		if (!std::filesystem::exists(path))
			continue;
		for (const auto& item: std::filesystem::recursive_directory_iterator(path)) {
			if (!item.is_regular_file() || item.path().extension() != ".ttf")
				continue;
			if (item.path().stem() == iName) {
				m_fonts.emplace(iName, mkShared<Font>(item.path(), iName == m_defaultFontName,
													  FontSettings::loadForFont(item.path())));
			}
		}
	}
}

auto FontLibrary::getDefaultFont() const -> const shared<Font>& { return m_fonts.at(m_defaultFontName); }

auto FontLibrary::getFont(const std::string& iName) -> const shared<Font>& {
	auto it = m_fonts.find(iName);
	if (it == m_fonts.end()) {
		loadFont(iName);
		it = m_fonts.find(iName);
		if (it == m_fonts.end() || it->second == nullptr) {
			OWL_CORE_ERROR("Font '{}' not found.", iName)
			if (it != m_fonts.end())
				m_fonts.erase(it);
			return m_fonts.at("null");
		}
	}
	return it->second;
}

auto FontLibrary::reloadFont(const std::string& iName, const FontSettings& iSettings) -> bool {
	const auto it = m_fonts.find(iName);
	if (it == m_fonts.end() || it->second == nullptr) {
		OWL_CORE_WARN("FontLibrary: Font '{}' is not loaded, nothing to reload.", iName)
		return false;
	}
	it->second->reload(iSettings);
	return true;
}

auto FontLibrary::unloadFontsFrom(const std::filesystem::path& iDirectory) -> size_t {
	return static_cast<size_t>(std::erase_if(m_fonts, [this, &iDirectory](const auto& iEntry) -> bool {
		const auto& [name, font] = iEntry;
		return name != m_defaultFontName && font != nullptr && assets::isInDirectory(font->getPath(), iDirectory);
	}));
}

auto FontLibrary::getLoadedFontNames() const -> std::list<std::string> {
	std::list<std::string> list;
	for (const auto& [key, item]: m_fonts) { list.push_back(key); }
	return list;
}

auto FontLibrary::getFoundFontNames() -> std::list<std::string> {
	std::list<std::string> list;
	for (const auto& path: getFontPath()) {
		if (!std::filesystem::exists(path))
			continue;
		for (const auto& item: std::filesystem::recursive_directory_iterator(path)) {
			if (!item.is_regular_file() || item.path().extension() != ".ttf")
				continue;
			list.push_back(item.path().stem().string());
		}
	}
	list.sort();
#if !defined(__clang__) or __clang_major__ > 15
	list.erase(std::ranges::unique(list).begin(), list.end());
#else
	list.erase(std::unique(list.begin(), list.end()), list.end());
#endif
	return list;
}

}// namespace owl::data::fonts
