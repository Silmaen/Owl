/**
 * @file AssetKind.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "AssetKind.h"

#include <input/ActionMap.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace owl::nest {

namespace {
constexpr std::array<std::pair<std::string_view, AssetKind>, 33> g_kinds{{
		{".owl", AssetKind::Scene},
		{".owlflow", AssetKind::NodeGraph},
		{".owlanim", AssetKind::Animation},
		{".owltilemap", AssetKind::Tilemap},
		{".owltileset", AssetKind::Tileset},
		{".owlprefab", AssetKind::Prefab},
		{".owlhud", AssetKind::Hud},
		{".owlmacro", AssetKind::Macro},
		{".ttf", AssetKind::Font},
		{".otf", AssetKind::Font},
		{".owlfont", AssetKind::Font},
		{".png", AssetKind::Image},
		{".jpg", AssetKind::Image},
		{".jpeg", AssetKind::Image},
		{".bmp", AssetKind::Image},
		{".tga", AssetKind::Image},
		{".slang", AssetKind::Code},
		{".lua", AssetKind::Code},
		{".py", AssetKind::Code},
		{".c", AssetKind::Code},
		{".cpp", AssetKind::Code},
		{".cc", AssetKind::Code},
		{".cxx", AssetKind::Code},
		{".h", AssetKind::Code},
		{".hpp", AssetKind::Code},
		{".hxx", AssetKind::Code},
		{".yml", AssetKind::Code},
		{".yaml", AssetKind::Code},
		{".json", AssetKind::Code},
		{".md", AssetKind::Code},
		{".markdown", AssetKind::Code},
		{".svg", AssetKind::Code},
		{".xml", AssetKind::Code},
}};
}// namespace

auto classifyAsset(const std::filesystem::path& iPath) -> AssetKind {
	if (iPath.filename() == input::ActionMap::fileName)
		return AssetKind::ActionMap;
	const auto ext = iPath.extension().string();
	const auto* const found =
			std::ranges::find_if(g_kinds, [&ext](const auto& iEntry) -> bool { return iEntry.first == ext; });
	return found == g_kinds.end() ? AssetKind::Unsupported : found->second;
}

auto listAssets(const std::filesystem::path& iDirectory, const std::string_view iExtension)
		-> std::vector<std::string> {
	std::vector<std::string> found;
	std::error_code error;
	if (!is_directory(iDirectory, error))
		return found;
	for (std::filesystem::recursive_directory_iterator it{iDirectory, error}; !error && it != end(it);
		 it.increment(error)) {
		if (it->is_regular_file(error) && it->path().extension() == iExtension)
			found.push_back(relative(it->path(), iDirectory).generic_string());
	}
	std::ranges::sort(found);
	return found;
}

}// namespace owl::nest
