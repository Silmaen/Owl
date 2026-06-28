/**
 * @file RendererIsometric.cpp
 * @author Silmaen
 * @date 27/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererIsometric.h"

namespace owl::renderer {

auto worldToScreen(const math::vec3& iWorld, const IsometricConfig& iConfig) -> math::vec2 {
	const auto halfW = static_cast<float>(iConfig.tileSize.x()) * 0.5f;
	const auto halfH = static_cast<float>(iConfig.tileSize.y()) * 0.5f;
	return math::vec2{iConfig.origin.x() + (iWorld.x() - iWorld.y()) * halfW,
					  iConfig.origin.y() + (iWorld.x() + iWorld.y()) * halfH - iWorld.z() * iConfig.zStep};
}

auto screenToWorld(const math::vec2& iScreen, const float iWorldZ, const IsometricConfig& iConfig) -> math::vec3 {
	const auto halfW = static_cast<float>(iConfig.tileSize.x()) * 0.5f;
	const auto halfH = static_cast<float>(iConfig.tileSize.y()) * 0.5f;
	const float diff = (iScreen.x() - iConfig.origin.x()) / halfW;
	const float sum = (iScreen.y() - iConfig.origin.y() + iWorldZ * iConfig.zStep) / halfH;
	return math::vec3{(diff + sum) * 0.5f, (sum - diff) * 0.5f, iWorldZ};
}

auto dimetricViewProjection(const math::vec2ui& iViewport, const IsometricConfig& iConfig) -> math::mat4 {
	const auto vw = static_cast<float>(iViewport.x());
	const auto vh = static_cast<float>(iViewport.y());
	const auto tileW = static_cast<float>(iConfig.tileSize.x());
	const auto tileH = static_cast<float>(iConfig.tileSize.y());
	math::mat4 result{};
	result(0, 0) = tileW / vw;
	result(0, 1) = -tileW / vw;
	result(0, 3) = 2.f * iConfig.origin.x() / vw - 1.f;
	result(1, 0) = -tileH / vh;
	result(1, 1) = -tileH / vh;
	result(1, 2) = 2.f * iConfig.zStep / vh;
	result(1, 3) = 1.f - 2.f * iConfig.origin.y() / vh;
	result(3, 3) = 1.f;
	return result;
}

}// namespace owl::renderer
