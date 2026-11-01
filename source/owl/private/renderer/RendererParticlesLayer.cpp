/**
 * @file RendererParticlesLayer.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererParticlesLayer.h"

#include "math/matrixCreation.h"
#include "math/trigonometry.h"
#include "renderer/ClipSpace.h"
#include "renderer/RenderLayerFactory.h"
#include "renderer/RenderStackYaml.h"
#include "renderer/RendererParticles.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <utility>

namespace owl::renderer {

namespace {
// Nearest and farthest depths of the raycast view, in map cells.
constexpr float g_raycastNear = 0.02f;
constexpr float g_raycastFar = 1000.f;

auto parseProjection(const std::string& iValue) -> RendererParticlesLayer::Projection {
	std::string lower = iValue;
	std::ranges::transform(lower, lower.begin(),
						   [](const unsigned char iC) -> char { return static_cast<char>(std::tolower(iC)); });
	return lower == "raycast" ? RendererParticlesLayer::Projection::Raycast : RendererParticlesLayer::Projection::Scene;
}

auto planar(const math::vec4& iDirection, const math::vec2& iFallback) -> math::vec2 {
	constexpr float epsilon = 1e-5f;
	math::vec2 result{iDirection.x(), iDirection.y()};
	if (result.normSq() < epsilon)
		return iFallback;
	result.normalize();
	return result;
}
}// namespace

void RendererParticlesLayer::registerWithFactory() {
	if (RenderLayerFactory::hasType(typeKey()))
		return;
	RenderLayerFactory::registerType(typeKey(), [](const std::string& iName) -> shared<RenderLayer> {
		return mkShared<RendererParticlesLayer>(iName);
	});
}

RendererParticlesLayer::RendererParticlesLayer(std::string iName) : m_name{std::move(iName)} {}

void RendererParticlesLayer::onBeginFrame(const Camera& iCamera) {
	if (m_projection == Projection::Raycast) {
		RendererParticles::beginScene(makeRaycastCamera(iCamera, m_fovDegrees));
		return;
	}
	RendererParticles::beginScene(iCamera);
}

void RendererParticlesLayer::onRender([[maybe_unused]] scene::Scene& ioScene) {}

void RendererParticlesLayer::onEndFrame() { RendererParticles::endScene(); }

void RendererParticlesLayer::applyConfig(const std::string& iConfig) {
	const auto config = parseYamlText(iConfig);
	if (!config || !config.IsMap())
		return;
	if (const auto v = config["Projection"]; v && v.IsScalar())
		m_projection = parseProjection(v.as<std::string>());
	if (const auto v = config["Fov"]; v && v.IsScalar())
		m_fovDegrees = std::clamp(v.as<float>(m_fovDegrees), 1.f, 179.f);
}

auto RendererParticlesLayer::getEffectiveViewProjection(const Camera& iCamera) const -> math::mat4 {
	if (m_projection == Projection::Raycast)
		return makeRaycastCamera(iCamera, m_fovDegrees).getViewProjection();
	return iCamera.getViewProjection();
}

auto RendererParticlesLayer::makeRaycastCamera(const Camera& iCamera, const float iFovDegrees) -> Camera {
	const math::mat4 toWorld = inverse(iCamera.getView());
	const math::vec4 eye = toWorld * math::vec4{0.f, 0.f, 0.f, 1.f};
	const math::vec2 forward = planar(toWorld * math::vec4{0.f, 1.f, 0.f, 0.f}, {0.f, 1.f});
	const math::vec2 right = planar(toWorld * math::vec4{1.f, 0.f, 0.f, 0.f}, {1.f, 0.f});
	const float halfTan = std::tan(math::radians(iFovDegrees) * 0.5f);
	// A wall one unit high at one unit fills the screen height: the vertical half-angle has a tangent of 0.5.
	Camera camera{toActiveClipSpace(
			math::perspectiveReversedZ(2.f * std::atan(0.5f), 2.f * halfTan, g_raycastNear, g_raycastFar))};
	math::mat4 transform = math::identity<float, 4>();
	transform(0, 0) = right.x();
	transform(1, 0) = right.y();
	transform(2, 0) = 0.f;
	transform(0, 1) = 0.f;
	transform(1, 1) = 0.f;
	transform(2, 1) = 1.f;
	transform(0, 2) = -forward.x();
	transform(1, 2) = -forward.y();
	transform(2, 2) = 0.f;
	transform(0, 3) = eye.x();
	transform(1, 3) = eye.y();
	transform(2, 3) = 0.f;
	camera.setTransform(transform);
	return camera;
}

}// namespace owl::renderer
