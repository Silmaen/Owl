/**
 * @file ScenePicking.cpp
 * @author Silmaen
 * @date 23/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/ScenePicking.h"

#include "math/box.h"
#include "renderer/ClipSpace.h"
#include "renderer/MeshCache.h"
#include "renderer/Renderer.h"
#include "renderer/RendererIsometricLayer.h"
#include "scene/Scene.h"
#include "scene/component/components.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::scene {

namespace {

constexpr math::vec2ui g_fallbackViewport{1280u, 720u};

auto viewportOf(const Scene& iScene) -> math::vec2ui {
	const auto& viewport = iScene.getViewportSize();
	return viewport.x() == 0 || viewport.y() == 0 ? g_fallbackViewport : viewport;
}

struct Ray {
	math::vec3 origin;
	math::vec3 direction;
};

// The ray of the primary camera through a viewport pixel, in world space.
auto makeCameraRay(const Scene& iScene, const math::vec2& iPixel) -> std::optional<Ray> {
	const Entity camera = iScene.getPrimaryCamera();
	if (!camera)
		return std::nullopt;
	const math::mat4 world = iScene.getWorldTransform(camera)();
	math::mat4 projection = camera.getComponent<component::Camera>().camera.getProjection();
	if (renderer::getActiveClipSpace() == renderer::ClipSpace::Vulkan)
		projection = renderer::toClipSpace(projection, renderer::ClipSpace::Vulkan);
	const math::mat4 unproject = math::inverse(projection * math::inverse(world));
	const auto viewport = viewportOf(iScene);
	const float ndcX = 2.f * iPixel.x() / static_cast<float>(viewport.x()) - 1.f;
	const float ndcY = 1.f - 2.f * iPixel.y() / static_cast<float>(viewport.y());
	const auto unprojectAt = [&](const float iDepth) -> math::vec3 {
		const math::vec4 point = unproject * math::vec4{ndcX, ndcY, iDepth, 1.f};
		return math::vec3{point.x(), point.y(), point.z()} * (1.f / point.w());
	};
	const math::vec3 first = unprojectAt(1.f);
	math::vec3 direction = unprojectAt(0.5f) - first;
	const math::vec3 eye{world(0, 3), world(1, 3), world(2, 3)};
	const math::vec3 forward = math::vec3{-world(0, 2), -world(1, 2), -world(2, 2)};
	if (direction * forward < 0.f)
		direction = -1.f * direction;
	const float length = direction.norm();
	if (!std::isfinite(length) || length <= 0.f)
		return std::nullopt;
	direction *= 1.f / length;
	// Start the ray in the plane of the eye, so that t >= 0 is in front of the camera.
	const math::vec3 origin = first - direction * ((first - eye) * direction);
	return Ray{.origin = origin, .direction = direction};
}

enum struct Shape : uint8_t { Quad, Disc, Box };

// Distance along the ray to a shape given in the local space of an entity.
auto hitLocal(const Ray& iRay, const math::mat4& iWorld, const Shape iShape, const math::box3f& iBox)
		-> std::optional<float> {
	const math::mat4 toLocal = math::inverse(iWorld);
	const math::vec4 o = toLocal * math::vec4{iRay.origin.x(), iRay.origin.y(), iRay.origin.z(), 1.f};
	const math::vec4 d = toLocal * math::vec4{iRay.direction.x(), iRay.direction.y(), iRay.direction.z(), 0.f};
	if (!std::isfinite(o.x()) || !std::isfinite(d.x()))
		return std::nullopt;
	if (iShape != Shape::Box) {
		if (std::abs(d.z()) < 1e-8f)
			return std::nullopt;
		const float t = -o.z() / d.z();
		const float x = o.x() + t * d.x();
		const float y = o.y() + t * d.y();
		const bool inside = iShape == Shape::Quad ? std::abs(x) <= 0.5f && std::abs(y) <= 0.5f : x * x + y * y <= 0.25f;
		return inside && t >= 0.f ? std::optional{t} : std::nullopt;
	}
	float tMin = 0.f;
	float tMax = std::numeric_limits<float>::max();
	for (size_t axis = 0; axis < 3; ++axis) {
		const float lo = iBox.min()[axis];
		const float hi = iBox.max()[axis];
		if (std::abs(d[axis]) < 1e-8f) {
			if (o[axis] < lo || o[axis] > hi)
				return std::nullopt;
			continue;
		}
		float t0 = (lo - o[axis]) / d[axis];
		float t1 = (hi - o[axis]) / d[axis];
		if (t0 > t1)
			std::swap(t0, t1);
		tMin = std::max(tMin, t0);
		tMax = std::min(tMax, t1);
		if (tMin > tMax)
			return std::nullopt;
	}
	return tMin;
}

// Index of the stack layer drawing an entity, and whether that layer is isometric.
struct Route {
	size_t layer = 0;
	bool isometric = false;
};

auto routeOf(const Entity& iEntity) -> Route {
	const auto& layers = renderer::Renderer::getRenderStack().getLayers();
	if (layers.empty())
		return {};
	size_t index = 0;
	if (iEntity.hasComponent<component::RendererTag>()) {
		const auto& name = iEntity.getComponent<component::RendererTag>().rendererName;
		const auto found = std::ranges::find_if(
				layers, [&name](const auto& iLayer) -> bool { return iLayer && iLayer->getName() == name; });
		index = found != layers.end() ? static_cast<size_t>(found - layers.begin()) : 0;
	}
	return {.layer = index,
			.isometric = dynamic_cast<const renderer::RendererIsometricLayer*>(layers[index].get()) != nullptr};
}

struct Hit {
	Entity entity;
	size_t layer = 0;
	float distance = 0.f;
};

auto isBetter(const Hit& iCandidate, const std::optional<Hit>& iBest) -> bool {
	if (!iBest)
		return true;
	if (iCandidate.layer != iBest->layer)
		return iCandidate.layer > iBest->layer;
	return iCandidate.distance < iBest->distance;
}

const math::box3f g_unitBox{math::vec3{-0.5f, -0.5f, -0.5f}, math::vec3{0.5f, 0.5f, 0.5f}};

}// namespace

auto makeRuntimeIsometricCamera(const Scene& iScene) -> renderer::IsometricCamera {
	renderer::IsometricCamera camera;
	for (const auto& layer: renderer::Renderer::getRenderStack().getLayers()) {
		if (const auto* isometric = dynamic_cast<const renderer::RendererIsometricLayer*>(layer.get());
			isometric != nullptr) {
			camera.setConfig(isometric->getConfig());
			camera.setZoomLevel(isometric->getZoomLevel());
			break;
		}
	}
	camera.setViewportSize(viewportOf(iScene));
	if (const auto primary = iScene.getPrimaryCamera(); primary) {
		const auto eye = iScene.getWorldTransform(primary).translation();
		camera.setTarget({eye.x(), eye.y(), 0.f});
	}
	return camera;
}

auto pickEntity(Scene& ioScene, const math::vec2& iPixel, const std::string_view iTag) -> Entity {
	OWL_PROFILE_FUNCTION()

	const auto ray = makeCameraRay(ioScene, iPixel);
	std::optional<renderer::IsometricCamera> isoCamera;
	std::optional<Hit> best;
	const auto consider = [&](const entt::entity iHandle, const Shape iShape, const math::box3f& iBox,
							  const bool iSprite) -> void {
		const Entity entity{iHandle, &ioScene};
		if (!iTag.empty() &&
			(!entity.hasComponent<component::Tag>() || entity.getComponent<component::Tag>().tag != iTag))
			return;
		if (!ioScene.isEffectivelyVisible(entity, false))
			return;
		const Route route = routeOf(entity);
		const math::Transform world = ioScene.getWorldTransform(entity);
		if (route.isometric) {
			if (!iSprite)
				return;
			if (!isoCamera)
				isoCamera = makeRuntimeIsometricCamera(ioScene);
			const auto tileWidth = static_cast<float>(isoCamera->getConfig().tileSize.x());
			const renderer::IsometricSprite sprite{
					.foot = world.translation(),
					.size = {world.scale().x() * tileWidth, world.scale().y() * tileWidth}};
			const math::vec4 rect = renderer::getSpriteScreenRect(*isoCamera, sprite);
			if (iPixel.x() < rect.x() || iPixel.x() > rect.z() || iPixel.y() < rect.y() || iPixel.y() > rect.w())
				return;
			const Hit hit{.entity = entity,
						  .layer = route.layer,
						  .distance = -isoCamera->getViewDepth(world.translation())};
			if (isBetter(hit, best))
				best = hit;
			return;
		}
		if (!ray)
			return;
		if (const auto distance = hitLocal(*ray, world(), iShape, iBox); distance) {
			const Hit hit{.entity = entity, .layer = route.layer, .distance = *distance};
			if (isBetter(hit, best))
				best = hit;
		}
	};
	for (const auto entity: ioScene.registry.view<component::Transform, component::SpriteRenderer>())
		consider(entity, Shape::Quad, g_unitBox, true);
	for (const auto entity: ioScene.registry.view<component::Transform, component::AnimatedSpriteRenderer>())
		consider(entity, Shape::Quad, g_unitBox, true);
	for (const auto entity: ioScene.registry.view<component::Transform, component::CircleRenderer>())
		consider(entity, Shape::Disc, g_unitBox, false);
	for (const auto view = ioScene.registry.view<component::Transform, component::MeshRenderer>();
		 const auto entity: view) {
		const auto& mesh = view.get<component::MeshRenderer>(entity).mesh;
		if (!mesh)
			continue;
		const auto& geometry = mesh->getGeometry();
		consider(entity, Shape::Box, math::box3f{geometry.boundsMin, geometry.boundsMax}, false);
	}
	return best ? best->entity : Entity{};
}

}// namespace owl::scene
