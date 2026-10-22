/**
 * @file MaterialLibrary.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/MaterialLibrary.h"

#include "app/Application.h"
#include "platform/FileUtils.h"
#include "platform/FileWatcher.h"

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <string>
#include <utility>

namespace owl::renderer {

namespace {
// The text of a material: the open pack first, then the asset directories, then the path itself.
auto readMaterialText(const std::filesystem::path& iAssetPath) -> std::pair<std::string, std::filesystem::path> {
	if (app::Application::instanced()) {
		const auto& app = app::Application::get();
		if (const std::string rel = iAssetPath.generic_string(); app.hasOpenPack() && app.packContains(rel)) {
			if (const auto bytes = app.loadFromPack(rel); bytes)
				return {std::string{bytes->begin(), bytes->end()}, {}};
		}
		if (iAssetPath.is_relative()) {
			for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
				if (const auto full = assetsPath / iAssetPath; exists(full))
					return {platform::fileToString(full), full};
			}
		}
	}
	if (exists(iAssetPath))
		return {platform::fileToString(iAssetPath), iAssetPath};
	return {};
}

}// namespace

auto MaterialLibrary::get(const std::filesystem::path& iAssetPath) -> shared<const Material> {
	if (iAssetPath.empty())
		return nullptr;
	return resolve(iAssetPath, 0).material;
}

auto MaterialLibrary::getAsset(const std::filesystem::path& iAssetPath) -> std::optional<MaterialAsset> {
	if (iAssetPath.empty())
		return std::nullopt;
	return resolve(iAssetPath, 0).asset;
}

// NOLINTNEXTLINE(misc-no-recursion): bounded by g_maxParentDepth.
auto MaterialLibrary::resolve(const std::filesystem::path& iAssetPath, const uint32_t iDepth) -> const Entry& {
	const auto key = iAssetPath.generic_string();
	if (const auto found = m_entries.find(key); found != m_entries.end())
		return found->second;
	Entry entry;
	auto [text, file] = readMaterialText(iAssetPath);
	entry.file = std::move(file);
	if (text.empty()) {
		OWL_CORE_WARN("MaterialLibrary: {} not found.", key)
		return m_entries[key] = std::move(entry);
	}
	entry.asset = MaterialAsset::fromYaml(text);
	if (!entry.asset)
		return m_entries[key] = std::move(entry);
	MaterialParameters values;
	if (entry.asset->parent.empty()) {
		values = entry.asset->values;
	} else {
		if (iDepth >= g_maxParentDepth) {
			OWL_CORE_WARN("MaterialLibrary: {} has a parent chain too long or cyclic.", key)
			return m_entries[key] = std::move(entry);
		}
		const auto parentKey = entry.asset->parent.generic_string();
		// Marks the entry in progress, so that a cycle back to it resolves to a missing parent.
		m_entries[key] = Entry{};
		const Entry& parent = resolve(entry.asset->parent, iDepth + 1);
		if (!parent.material) {
			OWL_CORE_WARN("MaterialLibrary: {} has no usable parent {}.", key, parentKey)
			return m_entries[key] = std::move(entry);
		}
		values = parent.material->getParameters();
		values.copyFields(entry.asset->values, entry.asset->overrides);
		entry.ancestors = parent.ancestors;
		entry.ancestors.push_back(parentKey);
	}
	entry.material = mkShared<const Material>(values);
	return m_entries[key] = std::move(entry);
}

auto MaterialLibrary::invalidate(const std::filesystem::path& iFile) -> uint32_t {
	const auto changed = iFile.generic_string();
	std::vector<std::string> dropped;
	for (const auto& [key, entry]: m_entries) {
		if (key == changed || changed.ends_with("/" + key) ||
			(!entry.file.empty() && platform::isSameFile(entry.file, iFile)))
			dropped.push_back(key);
	}
	if (dropped.empty())
		return 0;
	// The instances built on a dropped material go too.
	for (const auto& [key, entry]: m_entries) {
		if (std::ranges::any_of(entry.ancestors,
								[&dropped](const std::string& iAncestor) -> bool {
									return std::ranges::find(dropped, iAncestor) != dropped.end();
								}) &&
			std::ranges::find(dropped, key) == dropped.end())
			dropped.push_back(key);
	}
	for (const auto& key: dropped) m_entries.erase(key);
	++m_generation;
	OWL_CORE_INFO("MaterialLibrary: {} reloads ({} materials).", changed, dropped.size())
	return static_cast<uint32_t>(dropped.size());
}

void MaterialLibrary::clear() {
	m_entries.clear();
	++m_generation;
}

}// namespace owl::renderer
