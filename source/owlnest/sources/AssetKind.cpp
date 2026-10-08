/**
 * @file AssetKind.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "AssetKind.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace owl::nest {

namespace {
constexpr std::array<std::pair<std::string_view, AssetKind>, 22> g_kinds{{
		{".owl", AssetKind::Scene},
		{".owlflow", AssetKind::NodeGraph},
		{".owlanim", AssetKind::Animation},
		{".owltilemap", AssetKind::Tilemap},
		{".owltileset", AssetKind::Tileset},
		{".owlprefab", AssetKind::Prefab},
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
	const auto ext = iPath.extension().string();
	const auto* const found =
			std::ranges::find_if(g_kinds, [&ext](const auto& iEntry) -> bool { return iEntry.first == ext; });
	return found == g_kinds.end() ? AssetKind::Unsupported : found->second;
}

}// namespace owl::nest
