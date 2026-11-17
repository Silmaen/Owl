/**
 * @file ModManager.cpp
 * @author Silmaen
 * @date 16/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/pack/ModManager.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "data/assets/pack/ExtensionRegistry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ranges>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace owl::data::assets::pack {

namespace {

constexpr std::array<core::MigrationStep, 0> g_modsMigrations{};
constexpr core::DocumentFormat g_modsFormat{.name = "Mod choices", .migrations = g_modsMigrations};
constexpr std::string_view g_rootKey{"OwlMods"};

auto readIds(const YAML::Node& iNode) -> std::vector<std::string> {
	std::vector<std::string> ids;
	if (iNode && iNode.IsSequence())
		for (const auto& item: iNode) ids.push_back(item.as<std::string>());
	return ids;
}

// Id of the pack of a mod, the key of every lookup.
auto idOf(const ModEntry& iMod) -> const std::string& { return iMod.manifest.id; }

}// namespace

void ModManager::configure(const std::filesystem::path& iModsFolder, const std::filesystem::path& iSettingsFile) {
	reset();
	m_folder = iModsFolder;
	m_settingsFile = iSettingsFile;
	load();
	rescan();
}

void ModManager::load() {
	m_savedOrder.clear();
	m_disabled.clear();
	std::error_code error;
	if (m_settingsFile.empty() || !std::filesystem::is_regular_file(m_settingsFile, error))
		return;
	try {
		auto root = YAML::LoadFile(m_settingsFile.string());
		if (const auto version = core::upgradeYamlDocument(g_modsFormat, root, m_settingsFile.string()); !version) {
			OWL_CORE_WARN("Mods: Cannot read '{}': {}.", m_settingsFile.string(), core::describe(version.error()))
			return;
		}
		const auto node = root[std::string{g_rootKey}];
		if (!node || !node.IsMap())
			return;
		m_savedOrder = readIds(node["order"]);
		m_disabled = readIds(node["disabled"]);
	} catch (const YAML::Exception& exception) {
		OWL_CORE_WARN("Mods: Cannot read '{}': {}.", m_settingsFile.string(), exception.what())
	}
}

void ModManager::rescan() {
	if (m_folder.empty())
		return;
	std::vector<ModEntry> found;
	const auto sources = ExtensionRegistry::listSources(m_folder);
	found.reserve(sources.size());
	for (const auto& source: sources) {
		auto manifest = ExtensionPack::readManifest(source);
		if (!manifest) {
			OWL_CORE_WARN("Mods: '{}' is no mod: {}.", source.string(), describe(manifest.error()))
			continue;
		}
		if (std::ranges::contains(found, manifest->id, idOf))
			continue;
		const bool enabled = !std::ranges::contains(m_disabled, manifest->id);
		found.push_back({.source = source,
						 .manifest = std::move(*manifest),
						 .enabled = enabled,
						 .mounted = false,
						 .issue = {}});
	}
	// The order known first (current list, else the saved one), then the new mods by load order and id.
	std::vector<std::string> known;
	known.reserve(m_mods.size());
	for (const auto& mod: m_mods) known.push_back(mod.manifest.id);
	if (known.empty())
		known = m_savedOrder;
	const auto rank = [&known](const ModEntry& iMod) -> std::tuple<size_t, int32_t, std::string> {
		const auto place = std::ranges::find(known, iMod.manifest.id);
		return {static_cast<size_t>(place - known.begin()), iMod.manifest.loadOrder, iMod.manifest.id};
	};
	std::ranges::sort(found, {}, rank);
	for (auto& mod: found) {
		if (const auto* previous = find(mod.manifest.id); previous != nullptr) {
			mod.mounted = previous->mounted;
			mod.issue = previous->issue;
		}
	}
	const bool changed = found.size() != m_mods.size() ||
						 !std::ranges::equal(found, m_mods, [](const ModEntry& iLeft, const ModEntry& iRight) -> bool {
							 return iLeft.manifest == iRight.manifest && iLeft.source == iRight.source;
						 });
	m_mods = std::move(found);
	m_dirty = m_dirty || changed;
}

auto ModManager::find(const std::string& iId) const -> const ModEntry* {
	const auto found = std::ranges::find(m_mods, iId, idOf);
	return found != m_mods.end() ? &*found : nullptr;
}

auto ModManager::setEnabled(const std::string& iId, const bool iEnabled) -> bool {
	const auto found = std::ranges::find(m_mods, iId, idOf);
	if (found == m_mods.end())
		return false;
	if (found->enabled != iEnabled) {
		found->enabled = iEnabled;
		std::erase(m_disabled, iId);
		if (!iEnabled)
			m_disabled.push_back(iId);
		m_dirty = true;
	}
	return true;
}

auto ModManager::move(const std::string& iId, const int32_t iOffset) -> bool {
	const auto found = std::ranges::find(m_mods, iId, idOf);
	if (found == m_mods.end() || m_mods.empty())
		return false;
	const auto from = static_cast<int64_t>(found - m_mods.begin());
	const auto to = std::clamp<int64_t>(from + iOffset, 0, static_cast<int64_t>(m_mods.size()) - 1);
	if (to == from)
		return false;
	auto mod = std::move(*found);
	m_mods.erase(found);
	m_mods.insert(m_mods.begin() + to, std::move(mod));
	m_dirty = true;
	return true;
}

auto ModManager::getEnabledSources() const -> std::vector<std::filesystem::path> {
	return m_mods | std::views::filter(&ModEntry::enabled) | std::views::transform(&ModEntry::source) |
		   std::ranges::to<std::vector>();
}

auto ModManager::getLoadOrder() const -> std::unordered_map<std::string, int32_t> {
	std::unordered_map<std::string, int32_t> order;
	for (size_t index = 0; index < m_mods.size(); ++index)
		order[m_mods[index].manifest.id] = static_cast<int32_t>(index);
	return order;
}

void ModManager::update(const ExtensionRegistry& iRegistry) {
	for (auto& mod: m_mods) {
		mod.mounted = iRegistry.find(mod.manifest.id) != nullptr;
		mod.issue.clear();
		if (!mod.enabled || mod.mounted)
			continue;
		for (const auto& issue: iRegistry.getIssues()) {
			if (issue.id == mod.manifest.id)
				mod.issue = issue.detail.empty() ? std::string{describe(issue.error)}
												 : std::format("{} ({})", describe(issue.error), issue.detail);
		}
	}
	m_dirty = false;
}

auto ModManager::save() const -> bool {
	if (m_settingsFile.empty())
		return false;
	YAML::Emitter out;
	out << YAML::BeginMap;
	core::emitFormatVersion(out, g_modsFormat);
	out << YAML::Key << std::string{g_rootKey} << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "order" << YAML::Value << YAML::BeginSeq;
	for (const auto& mod: m_mods) out << mod.manifest.id;
	out << YAML::EndSeq;
	out << YAML::Key << "disabled" << YAML::Value << YAML::BeginSeq;
	for (const auto& id: m_disabled) out << id;
	out << YAML::EndSeq;
	out << YAML::EndMap;
	out << YAML::EndMap;
	std::error_code error;
	std::filesystem::create_directories(m_settingsFile.parent_path(), error);
	std::ofstream file(m_settingsFile);
	if (!file)
		return false;
	file << out.c_str() << "\n";
	return static_cast<bool>(file);
}

void ModManager::reset() {
	m_folder.clear();
	m_settingsFile.clear();
	m_mods.clear();
	m_savedOrder.clear();
	m_disabled.clear();
	m_dirty = false;
}

}// namespace owl::data::assets::pack
