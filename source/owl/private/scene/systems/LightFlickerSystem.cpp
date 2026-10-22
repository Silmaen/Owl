/**
 * @file LightFlickerSystem.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"

#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace owl::scene::systems {

namespace {

// Hashed value in [-1, 1] per integer.
auto hash(const double iValue) -> double {
	const double x = std::sin(iValue * 127.1 + 311.7) * 43758.5453;
	return (x - std::floor(x)) * 2.0 - 1.0;
}

// Smooth 1D value noise in [-1, 1]: a hashed value per integer, eased in between.
auto noise(const double iTime) -> double {
	const double cell = std::floor(iTime);
	const double fraction = iTime - cell;
	const double eased = fraction * fraction * (3.0 - 2.0 * fraction);
	return hash(cell) * (1.0 - eased) + hash(cell + 1.0) * eased;
}

// Hashed value in [0, 1].
auto unitHash(const double iValue) -> double { return hash(iValue) * 0.5 + 0.5; }

struct Sample {
	/// Relative intensity, 1 at rest.
	double factor = 1.0;
	/// Shape of the flicker in about [-1, 1], the weight of the colour shift.
	double shape = 0.0;
};

auto sampleFlicker(const component::LightFlicker& iFlicker, const double iSeconds) -> Sample {
	using Style = component::LightFlicker::Style;
	const double t = iSeconds * static_cast<double>(iFlicker.speed);
	const double seed = static_cast<double>(iFlicker.seed);
	Sample sample;
	switch (iFlicker.style) {
		case Style::Fire:
			sample.shape = 0.5 * noise(t * 1.3 + seed) + 0.3 * noise(t * 4.7 + seed * 2.1) +
						   0.2 * noise(t * 11.0 + seed * 3.7) + 0.08 * std::sin(t * 17.0 + seed);
			break;
		case Style::Candle:
			{
				const double slow = 0.6 + 0.5 * unitHash(seed);
				const double fast = 3.0 + 3.0 * unitHash(seed + 1.0);
				sample.shape = (0.22 * noise(t * slow + seed) + 0.13 * noise(t * fast + seed * 1.7)) / 0.35;
				break;
			}
		case Style::Pulse:
			{
				const double phase = t + seed;
				sample.shape = std::sin(phase) * std::sin(phase * 1.73 + 1.3);
				break;
			}
	}
	sample.factor = 1.0 + static_cast<double>(iFlicker.amplitude) * sample.shape;
	if (iFlicker.dipRate > 0.f) {
		const double rate = static_cast<double>(iFlicker.dipRate) * (0.7 + 0.6 * unitHash(seed + 2.0));
		const double gust = noise(iSeconds * rate + seed * 2.3);
		if (gust > 0.55)
			sample.factor -= (gust - 0.55) / 0.45 * static_cast<double>(iFlicker.dipDepth);
	}
	sample.factor = std::max(0.2, sample.factor);
	return sample;
}

auto hasSprite(const Entity& iEntity) -> bool {
	return iEntity.hasComponent<component::AnimatedSpriteRenderer>() || iEntity.hasComponent<component::SpriteRenderer>();
}

void captureBase(const Entity& iEntity, component::LightFlicker& ioFlicker) {
	const auto& transform = iEntity.getComponent<component::Transform>().transform;
	ioFlicker.m_basePosition = transform.translation();
	ioFlicker.m_baseScale = transform.scale();
	if (iEntity.hasComponent<component::Light>()) {
		const auto& light = iEntity.getComponent<component::Light>();
		ioFlicker.m_baseIntensity = light.intensity;
		ioFlicker.m_baseColor = {light.color.x(), light.color.y(), light.color.z(), 1.f};
	} else if (iEntity.hasComponent<component::Light2D>()) {
		const auto& light = iEntity.getComponent<component::Light2D>();
		ioFlicker.m_baseIntensity = light.intensity;
		ioFlicker.m_baseColor = {light.color.x(), light.color.y(), light.color.z(), 1.f};
	}
	if (iEntity.hasComponent<component::AnimatedSpriteRenderer>())
		ioFlicker.m_baseColor = iEntity.getComponent<component::AnimatedSpriteRenderer>().color;
	else if (iEntity.hasComponent<component::SpriteRenderer>())
		ioFlicker.m_baseColor = iEntity.getComponent<component::SpriteRenderer>().color;
	ioFlicker.m_captured = true;
}

auto shifted(const math::vec4& iBase, const math::vec3& iShift, const double iShape) -> math::vec4 {
	const auto weight = static_cast<float>(iShape);
	return {iBase.r() + iShift.x() * weight, iBase.g() + iShift.y() * weight, iBase.b() + iShift.z() * weight,
			iBase.a()};
}

// The light of the entity, 3D or 2D: its intensity, and its colour when the entity has no sprite to tint instead.
void applyToLight(const Entity& iEntity, const component::LightFlicker& iFlicker, const Sample& iSample) {
	const auto intensity = iFlicker.m_baseIntensity * static_cast<float>(iSample.factor);
	const auto color = shifted(iFlicker.m_baseColor, iFlicker.colorShift, iSample.shape);
	const bool tintsLight = !hasSprite(iEntity);
	if (iEntity.hasComponent<component::Light>()) {
		auto& light = iEntity.getComponent<component::Light>();
		light.intensity = intensity;
		if (tintsLight)
			light.color = {color.r(), color.g(), color.b()};
	} else if (iEntity.hasComponent<component::Light2D>()) {
		auto& light = iEntity.getComponent<component::Light2D>();
		light.intensity = intensity;
		if (tintsLight)
			light.color = {color.r(), color.g(), color.b()};
	}
}

// The tint of a sprite of the entity.
void applyToSprite(const Entity& iEntity, const component::LightFlicker& iFlicker, const Sample& iSample) {
	const auto color = shifted(iFlicker.m_baseColor, iFlicker.colorShift, iSample.shape);
	if (iEntity.hasComponent<component::AnimatedSpriteRenderer>())
		iEntity.getComponent<component::AnimatedSpriteRenderer>().color = color;
	else if (iEntity.hasComponent<component::SpriteRenderer>())
		iEntity.getComponent<component::SpriteRenderer>().color = color;
}

void applyJitter(const Entity& iEntity, const component::LightFlicker& iFlicker, const double iSeconds) {
	const auto& jitter = iFlicker.positionJitter;
	if (jitter.x() == 0.f && jitter.y() == 0.f && jitter.z() == 0.f)
		return;
	const double t = iSeconds * static_cast<double>(iFlicker.speed);
	const double seed = static_cast<double>(iFlicker.seed);
	iEntity.getComponent<component::Transform>().transform.translation() = {
			iFlicker.m_basePosition.x() + jitter.x() * static_cast<float>(noise(t * 3.1 + seed * 3.1 + 7.0)),
			iFlicker.m_basePosition.y() + jitter.y() * static_cast<float>(noise(t * 3.7 + seed * 4.3 + 13.0)),
			iFlicker.m_basePosition.z() + jitter.z() * static_cast<float>(noise(t * 2.9 + seed * 5.9 + 21.0))};
}

void applyFollow(Scene& ioScene, const Entity& iEntity, const component::LightFlicker& iFlicker) {
	const Entity target = ioScene.findEntityByUUID(iFlicker.follow);
	if (!target || !target.hasComponent<component::LightFlicker>())
		return;
	const auto deviation = target.getComponent<component::LightFlicker>().m_deviation;
	const auto& base = iFlicker.m_baseScale;
	const auto& response = iFlicker.scaleResponse;
	iEntity.getComponent<component::Transform>().transform.scale() = {base.x() * (1.f + response.x() * deviation),
																	  base.y() * (1.f + response.y() * deviation),
																	  base.z() * (1.f + response.z() * deviation)};
}

}// namespace

void updateLightFlickers(Scene& ioScene, [[maybe_unused]] const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const double seconds = ioScene.getRuntimeSeconds();
	std::vector<Entity> followers;
	for (const auto view = ioScene.registry.view<component::LightFlicker, component::Transform>();
		 const auto handle: view) {
		const Entity entity{handle, &ioScene};
		auto& flicker = view.get<component::LightFlicker>(handle);
		if (!flicker.enabled)
			continue;
		if (!flicker.m_captured)
			captureBase(entity, flicker);
		const auto sample = sampleFlicker(flicker, seconds);
		flicker.m_deviation = static_cast<float>(sample.factor - 1.0);
		applyToLight(entity, flicker, sample);
		applyToSprite(entity, flicker, sample);
		applyJitter(entity, flicker, seconds);
		if (flicker.follow != core::UUID{0})
			followers.push_back(entity);
	}
	for (const auto& entity: followers) applyFollow(ioScene, entity, entity.getComponent<component::LightFlicker>());
}

}// namespace owl::scene::systems
