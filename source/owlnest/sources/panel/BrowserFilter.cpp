/**
 * @file BrowserFilter.cpp
 * @author Silmaen
 * @date 02/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "BrowserFilter.h"

#include <algorithm>

namespace owl::nest::panel {

namespace {

auto normalized(const std::filesystem::path& iPath) -> std::string { return iPath.lexically_normal().generic_string(); }

}// namespace

auto isBrowsedRoot(const AssetOrigin iOrigin, const bool iShowAll) -> bool {
	return iShowAll || iOrigin != AssetOrigin::Application;
}

auto isHiddenEntry(const std::filesystem::path& iPath, const std::filesystem::path& iRoot, const AssetOrigin iOrigin)
		-> bool {
	const auto name = iPath.filename().string();
	if (name == "spirv" || iPath.extension() == ".spv" || name.ends_with(".spv.hash"))
		return true;
	if (iOrigin != AssetOrigin::Engine)
		return false;
	const auto relative = iPath.lexically_relative(iRoot);
	return !relative.empty() && *relative.begin() == "logo";
}

void PackagedFiles::assign(const std::vector<std::filesystem::path>& iFiles) {
	m_files.clear();
	m_files.reserve(iFiles.size());
	for (const auto& file: iFiles) m_files.push_back(normalized(file));
	std::ranges::sort(m_files);
	const auto [first, last] = std::ranges::unique(m_files);
	m_files.erase(first, last);
}

auto PackagedFiles::contains(const std::filesystem::path& iFile) const -> bool {
	return std::ranges::binary_search(m_files, normalized(iFile));
}

auto PackagedFiles::holds(const std::filesystem::path& iFolder) const -> bool {
	auto prefix = normalized(iFolder);
	if (!prefix.ends_with('/'))
		prefix += '/';
	const auto found = std::ranges::lower_bound(m_files, prefix);
	return found != m_files.end() && found->starts_with(prefix);
}

}// namespace owl::nest::panel
