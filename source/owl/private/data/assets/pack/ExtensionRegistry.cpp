/**
 * @file ExtensionRegistry.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/pack/ExtensionRegistry.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace owl::data::assets::pack {

namespace {

auto normalizedEntry(const std::string& iPath) -> std::string {
	return std::filesystem::path{iPath}.lexically_normal().generic_string();
}

auto checkDependencies(const std::vector<ExtensionManifest>& iManifests, const size_t iIndex,
					   const std::unordered_map<std::string, size_t>& iById, const std::vector<bool>& iAccepted)
		-> std::optional<ExtensionIssue> {
	const auto& manifest = iManifests[iIndex];
	for (const auto& dependency: manifest.dependencies) {
		ExtensionIssue issue{.id = manifest.id, .source = {}, .error = {}, .detail = dependency.id};
		const auto found = iById.find(dependency.id);
		if (found == iById.end())
			issue.error = ExtensionLoadError::MissingDependency;
		else if (!iAccepted[found->second])
			issue.error = ExtensionLoadError::DependencyRejected;
		else if (iManifests[found->second].version < dependency.minVersion)
			issue.error = ExtensionLoadError::DependencyTooOld;
		else
			continue;
		return issue;
	}
	return std::nullopt;
}

}// namespace

auto resolveLoadOrder(const std::vector<ExtensionManifest>& iManifests) -> ExtensionResolution {
	ExtensionResolution result;
	const size_t count = iManifests.size();
	std::vector<bool> accepted(count, true);
	std::unordered_map<std::string, size_t> byId;
	for (size_t index = 0; index < count; ++index) {
		if (!byId.emplace(iManifests[index].id, index).second) {
			accepted[index] = false;
			result.issues.push_back({.id = iManifests[index].id,
									 .source = {},
									 .error = ExtensionLoadError::DuplicateId,
									 .detail = iManifests[index].id});
		}
	}
	for (bool changed = true; changed;) {
		changed = false;
		for (size_t index = 0; index < count; ++index) {
			if (!accepted[index])
				continue;
			if (auto issue = checkDependencies(iManifests, index, byId, accepted); issue) {
				accepted[index] = false;
				result.issues.push_back(std::move(*issue));
				changed = true;
			}
		}
	}
	std::vector<size_t> pending(count, 0);
	std::vector<std::vector<size_t>> dependents(count);
	for (size_t index = 0; index < count; ++index) {
		if (!accepted[index])
			continue;
		for (const auto& dependency: iManifests[index].dependencies) {
			dependents[byId.at(dependency.id)].push_back(index);
			++pending[index];
		}
	}
	std::set<std::tuple<int32_t, std::string, size_t>> ready;
	for (size_t index = 0; index < count; ++index)
		if (accepted[index] && pending[index] == 0)
			ready.emplace(iManifests[index].loadOrder, iManifests[index].id, index);
	while (!ready.empty()) {
		const size_t index = std::get<2>(*ready.begin());
		ready.erase(ready.begin());
		result.order.push_back(index);
		for (const size_t dependent: dependents[index])
			if (--pending[dependent] == 0)
				ready.emplace(iManifests[dependent].loadOrder, iManifests[dependent].id, dependent);
	}
	for (size_t index = 0; index < count; ++index)
		if (accepted[index] && pending[index] > 0)
			result.issues.push_back({.id = iManifests[index].id,
									 .source = {},
									 .error = ExtensionLoadError::DependencyCycle,
									 .detail = {}});
	return result;
}

ExtensionRegistry::~ExtensionRegistry() = default;

auto ExtensionRegistry::listSources(const std::filesystem::path& iFolder) -> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> sources;
	std::error_code error;
	if (!std::filesystem::is_directory(iFolder, error))
		return sources;
	for (auto it = std::filesystem::directory_iterator(iFolder, error);
		 !error && it != std::filesystem::directory_iterator{}; it.increment(error)) {
		const auto& path = it->path();
		if (path.filename().string().starts_with('.'))
			continue;
		if (it->is_directory(error)) {
			if (std::filesystem::is_regular_file(path / g_extensionManifestEntry, error))
				sources.push_back(path);
		} else if (it->is_regular_file(error) && path.extension() == g_extensionPackExtension) {
			sources.push_back(path);
		}
	}
	std::ranges::sort(sources);
	return sources;
}

auto ExtensionRegistry::listSources(const std::filesystem::path& iFolder, const std::vector<std::string>& iExcluded)
		-> std::vector<std::filesystem::path> {
	auto sources = listSources(iFolder);
	std::erase_if(sources, [&iExcluded](const std::filesystem::path& iSource) -> bool {
		const auto manifest = ExtensionPack::readManifest(iSource);
		return !manifest || std::ranges::contains(iExcluded, manifest->id);
	});
	return sources;
}

auto ExtensionRegistry::mount(const std::vector<std::filesystem::path>& iSources,
							  const std::unordered_map<std::string, int32_t>& iLoadOrder) -> size_t {
	OWL_PROFILE_FUNCTION()

	for (const auto& [id, rank]: iLoadOrder) m_loadOrder[id] = rank;
	for (const auto& source: iSources) {
		std::error_code error;
		auto path = std::filesystem::weakly_canonical(source, error);
		if (error)
			path = source;
		if (std::ranges::any_of(m_candidates,
								[&path](const ExtensionPack& iPack) -> bool { return iPack.getSource() == path; }))
			continue;
		auto pack = ExtensionPack::open(path);
		if (!pack) {
			m_openIssues.push_back({.id = {}, .source = path, .error = pack.error(), .detail = {}});
			continue;
		}
		m_candidates.push_back(std::move(*pack));
	}
	std::vector<ExtensionManifest> manifests;
	manifests.reserve(m_candidates.size());
	for (const auto& candidate: m_candidates) {
		manifests.push_back(candidate.getManifest());
		if (const auto rank = m_loadOrder.find(manifests.back().id); rank != m_loadOrder.end())
			manifests.back().loadOrder = rank->second;
	}
	auto resolution = resolveLoadOrder(manifests);
	m_order = std::move(resolution.order);
	m_issues = m_openIssues;
	for (auto& issue: resolution.issues) {
		const auto found = std::ranges::find(manifests, issue.id, &ExtensionManifest::id);
		if (found != manifests.end())
			issue.source = m_candidates[static_cast<size_t>(found - manifests.begin())].getSource();
		OWL_CORE_WARN("Extensions: '{}' is not mounted: {}{}.", issue.id, describe(issue.error),
					  issue.detail.empty() ? std::string{} : std::format(" ({})", issue.detail))
		m_issues.push_back(std::move(issue));
	}
	if (!m_order.empty()) {
		std::string names;
		for (const size_t index: m_order)
			names += std::format("{}{}", names.empty() ? "" : ", ", m_candidates[index].getManifest().id);
		OWL_CORE_INFO("Extensions: {} pack(s) mounted ({}).", m_order.size(), names)
	}
	return m_order.size();
}

auto ExtensionRegistry::mountFolder(const std::filesystem::path& iFolder) -> size_t {
	return mount(listSources(iFolder));
}

void ExtensionRegistry::clear() {
	m_order.clear();
	m_candidates.clear();
	m_issues.clear();
	m_openIssues.clear();
	m_loadOrder.clear();
}

auto ExtensionRegistry::getPacks() const -> std::vector<const ExtensionPack*> {
	std::vector<const ExtensionPack*> packs;
	packs.reserve(m_order.size());
	for (const size_t index: m_order) packs.push_back(&m_candidates[index]);
	return packs;
}

auto ExtensionRegistry::find(const std::string& iId) const -> const ExtensionPack* {
	for (const size_t index: m_order)
		if (m_candidates[index].getManifest().id == iId)
			return &m_candidates[index];
	return nullptr;
}

auto ExtensionRegistry::findProvider(const std::string& iPath) const -> const ExtensionPack* {
	if (m_order.empty() || iPath.empty())
		return nullptr;
	const auto path = normalizedEntry(iPath);
	for (const size_t index: std::views::reverse(m_order))
		if (m_candidates[index].contains(path))
			return &m_candidates[index];
	return nullptr;
}

auto ExtensionRegistry::readEntry(const std::string& iPath) const -> std::optional<std::vector<uint8_t>> {
	const auto* pack = findProvider(iPath);
	if (pack == nullptr)
		return std::nullopt;
	return pack->readEntry(normalizedEntry(iPath));
}

auto ExtensionRegistry::listEntries() const -> std::vector<std::string> {
	std::set<std::string> entries;
	for (const size_t index: m_order)
		entries.insert(m_candidates[index].listEntries().begin(), m_candidates[index].listEntries().end());
	return {entries.begin(), entries.end()};
}

auto ExtensionRegistry::getConflicts() const -> std::vector<ExtensionConflict> {
	std::map<std::string, std::vector<std::string>> providers;
	for (const size_t index: m_order)
		for (const auto& entry: m_candidates[index].listEntries())
			providers[entry].push_back(m_candidates[index].getManifest().id);
	std::vector<ExtensionConflict> conflicts;
	for (auto& [path, ids]: providers)
		if (ids.size() > 1)
			conflicts.push_back({.path = path, .providers = std::move(ids)});
	return conflicts;
}

auto ExtensionRegistry::findEntries(const std::string_view iExtension) const -> std::vector<ExtensionEntry> {
	std::map<std::string, std::string> served;
	for (const size_t index: m_order)
		for (const auto& entry: m_candidates[index].listEntries())
			if (std::filesystem::path{entry}.extension() == iExtension)
				served[entry] = m_candidates[index].getManifest().id;
	std::vector<ExtensionEntry> entries;
	entries.reserve(served.size());
	for (auto& [path, id]: served) entries.push_back({.id = std::move(id), .path = path});
	return entries;
}

}// namespace owl::data::assets::pack
