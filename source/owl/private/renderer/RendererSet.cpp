/**
 * @file RendererSet.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererSet.h"

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::renderer {

namespace {

constexpr std::string_view g_raycastName{"RendererRaycast"};
constexpr std::string_view g_isometricName{"RendererIsometric"};
constexpr std::string_view g_renderer3DName{"Renderer3D"};

constexpr std::array<std::string_view, 11> g_baseFolders{
		"renderer2D",        "common",    "background", "color_encode", "outline", "post_process", "depth_effects",
		"tilemap_instanced", "particles", "lens_flare", "bitonic_sort"};
constexpr std::array<std::string_view, 3> g_raycastFolders{"raycast_dda", "raycast_stripe", "raycast_sprite"};
constexpr std::array<std::string_view, 1> g_isometricFolders{"isometric"};
constexpr std::array<std::string_view, 5> g_renderer3DFolders{"renderer3D", "mesh_culling", "hiz_build", "probe_filter",
															  "planet"};

}// namespace

auto RendererSet::base() -> RendererSet { return {.raycast = false, .isometric = false, .renderer3D = false}; }

auto RendererSet::forLayerType(const std::string_view iTypeKey) -> RendererSet {
	auto set = base();
	if (iTypeKey == "RendererRaycast")
		set.raycast = true;
	else if (iTypeKey == "RendererIsometric")
		set.isometric = true;
	else if (iTypeKey == "RendererMesh" || iTypeKey == "RendererVoxel" || iTypeKey == "RendererPlanet")
		set.renderer3D = true;
	return set;
}

auto RendererSet::operator|=(const RendererSet& iOther) -> RendererSet& {
	raycast = raycast || iOther.raycast;
	isometric = isometric || iOther.isometric;
	renderer3D = renderer3D || iOther.renderer3D;
	return *this;
}

auto RendererSet::getNames() const -> std::vector<std::string> {
	std::vector<std::string> names;
	if (raycast)
		names.emplace_back(g_raycastName);
	if (isometric)
		names.emplace_back(g_isometricName);
	if (renderer3D)
		names.emplace_back(g_renderer3DName);
	return names;
}

auto RendererSet::fromNames(const std::span<const std::string> iNames) -> RendererSet {
	const auto has = [&iNames](const std::string_view iName) -> bool {
		return std::ranges::find(iNames, iName) != iNames.end();
	};
	return {.raycast = has(g_raycastName), .isometric = has(g_isometricName), .renderer3D = has(g_renderer3DName)};
}

auto RendererSet::getShaderFolders() const -> std::vector<std::string> {
	std::vector<std::string> folders{g_baseFolders.begin(), g_baseFolders.end()};
	const auto add = [&folders](const auto& iFolders) -> void {
		folders.insert(folders.end(), iFolders.begin(), iFolders.end());
	};
	if (raycast)
		add(g_raycastFolders);
	if (isometric)
		add(g_isometricFolders);
	if (renderer3D)
		add(g_renderer3DFolders);
	return folders;
}

}// namespace owl::renderer
