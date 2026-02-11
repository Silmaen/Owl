/**
 * @file FontLibrary.cpp
 * @author Silmaen
 * @date 10/21/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/fonts/FontLibrary.h"

#include "core/Application.h"

namespace owl::data::fonts {

namespace {

auto getFontPath() -> std::list<std::filesystem::path> {
	if (!core::Application::instanced())
		return {};
	const auto& app = core::Application::get();
	if (app.getState() != core::Application::State::Running)
		return {};
	std::list<std::filesystem::path> paths;
	for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
		if (exists(assetsPath / "fonts")) {
			paths.push_back(assetsPath / "fonts");
		}
	}
	return paths;
}

}// namespace

FontLibrary::FontLibrary() : m_defaultFontName("OpenSans-Regular") {
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
	m_defaultFontName = "OpenSans-Regular";
}

FontLibrary::~FontLibrary() = default;

void FontLibrary::loadFont(const std::string& iName) {
	for (const auto& path: getFontPath()) {
		for (const auto& item: std::filesystem::recursive_directory_iterator(path)) {
			if (!item.is_regular_file() || item.path().extension() != ".ttf")
				continue;
			if (item.path().stem() == iName) {
				m_fonts.emplace(iName, mkShared<Font>(item.path(), iName == m_defaultFontName));
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
			OWL_CORE_ERROR("Font '{}' not found", iName)
			if (it != m_fonts.end())
				m_fonts.erase(it);
			return m_fonts.at("null");
		}
	}
	return it->second;
}

auto FontLibrary::getLoadedFontNames() const -> std::list<std::string> {
	std::list<std::string> list;
	for (const auto& [key, item]: m_fonts) { list.push_back(key); }
	return list;
}

auto FontLibrary::getFoundFontNames() const -> std::list<std::string> {
	std::list<std::string> list;
	for (const auto& path: getFontPath()) {
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
