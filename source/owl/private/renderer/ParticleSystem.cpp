/**
 * @file ParticleSystem.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/ParticleSystem.h"

#include "math/matrixCreation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numbers>
#include <span>

namespace owl::renderer {

namespace {

constexpr float g_twoPi = 2.f * std::numbers::pi_v<float>;

auto pcgHash(const uint32_t iValue) -> uint32_t {
	const uint32_t state = iValue * 747796405u + 2891336453u;
	const uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

auto columnsOf(const math::mat4& iMatrix) -> std::array<math::vec4, 4> {
	std::array<math::vec4, 4> columns{};
	for (size_t col = 0; col < 4; ++col)
		columns[col] = math::vec4{iMatrix(0, col), iMatrix(1, col), iMatrix(2, col), iMatrix(3, col)};
	return columns;
}

auto xyz(const math::vec4& iVector) -> math::vec3 { return {iVector.x(), iVector.y(), iVector.z()}; }

auto transformPoint(const std::array<math::vec4, 4>& iColumns, const math::vec3& iPoint) -> math::vec3 {
	return xyz(iColumns[0]) * iPoint.x() + xyz(iColumns[1]) * iPoint.y() + xyz(iColumns[2]) * iPoint.z() +
		   xyz(iColumns[3]);
}

auto transformDirection(const std::array<math::vec4, 4>& iColumns, const math::vec3& iDirection) -> math::vec3 {
	return xyz(iColumns[0]) * iDirection.x() + xyz(iColumns[1]) * iDirection.y() + xyz(iColumns[2]) * iDirection.z();
}

auto mix(const float iFrom, const float iTo, const float iRatio) -> float { return iFrom + (iTo - iFrom) * iRatio; }

auto randomUnit(const float iU, const float iV) -> math::vec3 {
	const float z = 1.f - 2.f * iU;
	const float radius = std::sqrt(std::max(0.f, 1.f - z * z));
	const float phi = g_twoPi * iV;
	return {radius * std::cos(phi), radius * std::sin(phi), z};
}

struct Spawn {
	math::vec3 position{0.f, 0.f, 0.f};
	math::vec3 direction{0.f, 1.f, 0.f};
};

auto spawnInShape(const ParticleStepParams& iParams, const std::array<float, 3>& iRandom) -> Spawn {
	const math::vec3 size = xyz(iParams.shape);
	switch (static_cast<ParticleShape>(iParams.config[0])) {
		case ParticleShape::Point:
			return {.position = {0.f, 0.f, 0.f}, .direction = randomUnit(iRandom[0], iRandom[1])};
		case ParticleShape::Sphere:
			{
				const math::vec3 direction = randomUnit(iRandom[0], iRandom[1]);
				return {.position = direction * (size.x() * std::cbrt(iRandom[2])), .direction = direction};
			}
		case ParticleShape::Box:
			return {.position = {(iRandom[0] * 2.f - 1.f) * size.x(), (iRandom[1] * 2.f - 1.f) * size.y(),
								 (iRandom[2] * 2.f - 1.f) * size.z()},
					.direction = {0.f, 1.f, 0.f}};
		case ParticleShape::Cone:
			{
				const float angle = iParams.shape.w() * std::sqrt(iRandom[0]);
				const float phi = g_twoPi * iRandom[1];
				const float radius = size.x() * std::sqrt(iRandom[2]);
				return {.position = {radius * std::cos(phi), 0.f, radius * std::sin(phi)},
						.direction = {std::sin(angle) * std::cos(phi), std::cos(angle),
									  std::sin(angle) * std::sin(phi)}};
			}
		case ParticleShape::Circle:
			{
				const float phi = g_twoPi * iRandom[0];
				const math::vec3 direction{std::cos(phi), std::sin(phi), 0.f};
				return {.position = direction * (size.x() * std::sqrt(iRandom[1])), .direction = direction};
			}
	}
	return {};
}

void spawnParticle(GpuParticle& oParticle, const std::span<math::vec4> oTrail, const uint32_t iRank,
				   const ParticleStepParams& iParams) {
	const uint32_t serial = iParams.spawn[2] + iRank;
	const uint32_t seed = iParams.config[2];
	const auto random = [seed, serial](const uint32_t iChannel) -> float {
		return particleRandom(seed, serial, iChannel);
	};
	const float lifetime = std::max(mix(iParams.life.x(), iParams.life.y(), random(0)), 1e-3f);
	const float speed = mix(iParams.life.z(), iParams.life.w(), random(1));
	const Spawn spawn = spawnInShape(iParams, {random(2), random(3), random(4)});
	math::vec3 position = spawn.position;
	math::vec3 velocity = spawn.direction * speed + xyz(iParams.velocity);
	if (iParams.config[1] == 0) {
		const float ratio = (static_cast<float>(iRank) + 0.5f) / static_cast<float>(std::max(iParams.spawn[1], 1u));
		const math::vec3 from = transformPoint(iParams.previous, position);
		const math::vec3 to = transformPoint(iParams.emitter, position);
		position = from + (to - from) * ratio;
		velocity = transformDirection(iParams.emitter, velocity);
	}
	oParticle.position = math::vec4{position.x(), position.y(), position.z(), 0.f};
	oParticle.velocity = math::vec4{velocity.x(), velocity.y(), velocity.z(), lifetime};
	oParticle.extra = math::vec4{mix(iParams.sizes.x(), iParams.sizes.y(), random(5)), g_twoPi * random(6),
								 mix(iParams.sizes.z(), iParams.sizes.w(), random(7)), random(8)};
	for (auto& point: oTrail) point = oParticle.position;
}

}// namespace

auto ColorGradient::addKey(const ColorKey& iKey) -> size_t {
	const auto at = std::ranges::upper_bound(keys, iKey.time, {}, &ColorKey::time);
	const auto inserted = keys.insert(at, iKey);
	return static_cast<size_t>(std::distance(keys.begin(), inserted));
}

auto ColorGradient::evaluate(const float iTime) const -> math::vec4 {
	if (keys.empty())
		return {1.f, 1.f, 1.f, 1.f};
	if (iTime <= keys.front().time)
		return keys.front().color;
	if (iTime >= keys.back().time)
		return keys.back().color;
	const auto next = std::ranges::upper_bound(keys, iTime, {}, &ColorKey::time);
	const auto& to = *next;
	const auto& from = *std::prev(next);
	const float span = to.time - from.time;
	const float ratio = span > 0.f ? (iTime - from.time) / span : 0.f;
	return from.color + (to.color - from.color) * ratio;
}

auto makeParticlePreset(const ParticlePreset iPreset) -> ParticleEmitterSettings {
	ParticleEmitterSettings settings;
	switch (iPreset) {
		case ParticlePreset::Fire:
			settings.maxParticles = 512;
			settings.rate = 120.f;
			settings.lifetimeMin = 0.5f;
			settings.lifetimeMax = 1.f;
			settings.shapeSize = {0.25f, 0.25f, 0.25f};
			settings.coneAngle = 0.25f;
			settings.speedMin = 0.6f;
			settings.speedMax = 1.4f;
			settings.gravity = {0.f, 1.f, 0.f};
			settings.drag = 0.8f;
			settings.sizeMin = 0.25f;
			settings.sizeMax = 0.45f;
			settings.sizeOverLife.addKey({0.f, 0.6f});
			settings.sizeOverLife.addKey({0.3f, 1.f});
			settings.sizeOverLife.addKey({1.f, 0.f});
			settings.spinMin = -1.f;
			settings.spinMax = 1.f;
			settings.colorOverLife.addKey({.time = 0.f, .color = {1.f, 0.9f, 0.5f, 1.f}});
			settings.colorOverLife.addKey({.time = 0.4f, .color = {1.f, 0.5f, 0.1f, 0.8f}});
			settings.colorOverLife.addKey({.time = 1.f, .color = {0.5f, 0.1f, 0.05f, 0.f}});
			settings.emissive = 30000.f;
			settings.blend = ParticleBlend::Additive;
			break;
		case ParticlePreset::Smoke:
			settings.maxParticles = 256;
			settings.rate = 25.f;
			settings.lifetimeMin = 2.f;
			settings.lifetimeMax = 4.f;
			settings.shapeSize = {0.3f, 0.3f, 0.3f};
			settings.coneAngle = 0.3f;
			settings.speedMin = 0.3f;
			settings.speedMax = 0.6f;
			settings.gravity = {0.f, 0.3f, 0.f};
			settings.drag = 0.3f;
			settings.sizeMin = 0.4f;
			settings.sizeMax = 0.7f;
			settings.sizeOverLife.addKey({0.f, 0.5f});
			settings.sizeOverLife.addKey({1.f, 2.5f});
			settings.spinMin = -0.4f;
			settings.spinMax = 0.4f;
			settings.colorOverLife.addKey({.time = 0.f, .color = {0.5f, 0.5f, 0.5f, 0.f}});
			settings.colorOverLife.addKey({.time = 0.15f, .color = {0.45f, 0.45f, 0.45f, 0.6f}});
			settings.colorOverLife.addKey({.time = 1.f, .color = {0.3f, 0.3f, 0.3f, 0.f}});
			settings.blend = ParticleBlend::Alpha;
			break;
		case ParticlePreset::Sparks:
			settings.maxParticles = 256;
			settings.rate = 40.f;
			settings.burst = 30;
			settings.lifetimeMin = 0.6f;
			settings.lifetimeMax = 1.2f;
			settings.shape = ParticleShape::Point;
			settings.speedMin = 2.f;
			settings.speedMax = 5.f;
			settings.gravity = {0.f, -9.81f, 0.f};
			settings.drag = 0.5f;
			settings.sizeMin = 0.04f;
			settings.sizeMax = 0.08f;
			settings.colorOverLife.addKey({.time = 0.f, .color = {1.f, 0.95f, 0.7f, 1.f}});
			settings.colorOverLife.addKey({.time = 1.f, .color = {1.f, 0.4f, 0.1f, 0.f}});
			settings.emissive = 60000.f;
			settings.trailLength = 6;
			settings.trailInterval = 0.03f;
			settings.trailWidth = 0.8f;
			break;
		case ParticlePreset::Magic:
			settings.maxParticles = 256;
			settings.rate = 40.f;
			settings.lifetimeMin = 1.5f;
			settings.lifetimeMax = 3.f;
			settings.shape = ParticleShape::Sphere;
			settings.shapeSize = {0.6f, 0.6f, 0.6f};
			settings.speedMin = 0.05f;
			settings.speedMax = 0.3f;
			settings.velocity = {0.f, 0.2f, 0.f};
			settings.sizeMin = 0.05f;
			settings.sizeMax = 0.12f;
			settings.sizeOverLife.addKey({0.f, 0.f});
			settings.sizeOverLife.addKey({0.2f, 1.f});
			settings.sizeOverLife.addKey({1.f, 0.f});
			settings.colorOverLife.addKey({.time = 0.f, .color = {0.4f, 0.7f, 1.f, 1.f}});
			settings.colorOverLife.addKey({.time = 1.f, .color = {0.8f, 0.4f, 1.f, 1.f}});
			settings.emissive = 20000.f;
			settings.space = ParticleSpace::Local;
			settings.trailLength = 8;
			settings.trailInterval = 0.05f;
			settings.trailWidth = 0.6f;
			break;
		case ParticlePreset::Snow:
			settings.maxParticles = 2048;
			settings.rate = 300.f;
			settings.lifetimeMin = 4.f;
			settings.lifetimeMax = 6.f;
			settings.shape = ParticleShape::Box;
			settings.shapeSize = {8.f, 0.1f, 8.f};
			settings.speedMin = 0.f;
			settings.speedMax = 0.1f;
			settings.velocity = {0.f, -1.f, 0.f};
			settings.drag = 0.1f;
			settings.sizeMin = 0.03f;
			settings.sizeMax = 0.06f;
			settings.spinMin = -2.f;
			settings.spinMax = 2.f;
			settings.colorOverLife.addKey({.time = 0.f, .color = {1.f, 1.f, 1.f, 0.f}});
			settings.colorOverLife.addKey({.time = 0.1f, .color = {1.f, 1.f, 1.f, 0.9f}});
			settings.colorOverLife.addKey({.time = 1.f, .color = {1.f, 1.f, 1.f, 0.f}});
			break;
	}
	return settings;
}

auto ParticleTables::bake(const ParticleEmitterSettings& iSettings) -> ParticleTables {
	ParticleTables tables;
	for (uint32_t index = 0; index < g_particleTableSize; ++index) {
		const float time = static_cast<float>(index) / static_cast<float>(g_particleTableSize - 1);
		const math::vec4 color = iSettings.colorOverLife.evaluate(time);
		tables.color[index] = math::vec4{iSettings.color.x() * color.x(), iSettings.color.y() * color.y(),
										 iSettings.color.z() * color.z(), iSettings.color.w() * color.w()};
		tables.size[index] = iSettings.sizeOverLife.empty() ? 1.f : iSettings.sizeOverLife.evaluate(time);
	}
	return tables;
}

auto ParticleTables::colorAt(const float iLife) const -> math::vec4 {
	const float position = std::clamp(iLife, 0.f, 1.f) * static_cast<float>(g_particleTableSize - 1);
	const auto low = std::min(static_cast<uint32_t>(position), g_particleTableSize - 1);
	const uint32_t high = std::min(low + 1, g_particleTableSize - 1);
	return color[low] + (color[high] - color[low]) * (position - static_cast<float>(low));
}

auto ParticleTables::sizeAt(const float iLife) const -> float {
	const float position = std::clamp(iLife, 0.f, 1.f) * static_cast<float>(g_particleTableSize - 1);
	const auto low = std::min(static_cast<uint32_t>(position), g_particleTableSize - 1);
	const uint32_t high = std::min(low + 1, g_particleTableSize - 1);
	return mix(size[low], size[high], position - static_cast<float>(low));
}

auto particleRandom(const uint32_t iSeed, const uint32_t iSerial, const uint32_t iChannel) -> float {
	return static_cast<float>(pcgHash(iSeed ^ pcgHash(iSerial * 16u + iChannel)) >> 8u) * (1.f / 16777216.f);
}

void simulateParticle(GpuParticle& ioParticle, const std::span<math::vec4> ioTrail, const uint32_t iIndex,
					  const ParticleStepParams& iParams) {
	if (iParams.flags[0] != 0) {
		ioParticle.position.w() = 1.f;
		ioParticle.velocity.w() = 0.f;
	}
	const uint32_t capacity = std::max(iParams.spawn[3], 1u);
	if (const uint32_t rank = (iIndex + capacity - iParams.spawn[0] % capacity) % capacity; rank < iParams.spawn[1]) {
		spawnParticle(ioParticle, ioTrail, rank, iParams);
		return;
	}
	if (!ioParticle.isAlive())
		return;
	const float seconds = iParams.velocity.w();
	math::vec3 position = xyz(ioParticle.position);
	if (iParams.config[1] == 0) {
		const math::vec3 shift = xyz(iParams.shift);
		position = position - shift;
		for (auto& point: ioTrail)
			point = math::vec4{point.x() - shift.x(), point.y() - shift.y(), point.z() - shift.z(), point.w()};
	}
	const float previousAge = ioParticle.position.w();
	const float age = previousAge + seconds;
	if (age >= ioParticle.velocity.w()) {
		ioParticle.position = math::vec4{position.x(), position.y(), position.z(), age};
		return;
	}
	math::vec3 velocity = xyz(ioParticle.velocity) + xyz(iParams.gravity) * seconds;
	velocity = velocity * (1.f / (1.f + iParams.gravity.w() * seconds));
	position = position + velocity * seconds;
	ioParticle.position = math::vec4{position.x(), position.y(), position.z(), age};
	ioParticle.velocity = math::vec4{velocity.x(), velocity.y(), velocity.z(), ioParticle.velocity.w()};
	ioParticle.extra.y() += ioParticle.extra.z() * seconds;
	if (const float interval = iParams.shift.w();
		!ioTrail.empty() && interval > 0.f && std::floor(age / interval) > std::floor(previousAge / interval)) {
		for (size_t point = ioTrail.size() - 1; point > 0; --point) ioTrail[point] = ioTrail[point - 1];
		ioTrail[0] = ioParticle.position;
	}
}

ParticleSystem::ParticleSystem(const Path iPath) : m_path{iPath} {}

ParticleSystem::~ParticleSystem() = default;

void ParticleSystem::play() {
	if (!m_playing && !m_emitting && m_spawned == 0) {
		m_emitting = true;
		m_burstDue = true;
	}
	m_playing = true;
}

void ParticleSystem::pause() { m_playing = false; }

void ParticleSystem::stop() { m_emitting = false; }

void ParticleSystem::restart() {
	m_playing = true;
	m_emitting = true;
	m_burstDue = true;
	m_reset = true;
	m_pending = false;
	m_head = 0;
	m_requested = 0;
	m_spawned = 0;
	m_emitCarry = 0.f;
	m_emitTime = 0.f;
	m_shift = {0.f, 0.f, 0.f};
	for (auto& particle: m_particles) particle = GpuParticle{};
}

void ParticleSystem::emit(const uint32_t iCount) { m_requested = std::min(m_requested + iCount, g_maxParticles); }

auto ParticleSystem::getAliveCount() const -> uint32_t {
	return static_cast<uint32_t>(std::ranges::count_if(
			m_particles, [](const GpuParticle& iParticle) -> bool { return iParticle.isAlive(); }));
}

void ParticleSystem::resize(const ParticleEmitterSettings& iSettings) {
	uint32_t capacity = std::clamp(iSettings.maxParticles, 1u, g_maxParticles);
	if (iSettings.blend == ParticleBlend::Alpha)
		capacity = std::min(capacity, g_maxSortedParticles);
	const uint32_t trail = std::min(iSettings.trailLength, g_maxParticleTrail);
	if (capacity == m_capacity && trail == m_trailLength)
		return;
	m_capacity = capacity;
	m_trailLength = trail;
	m_head = 0;
	m_reset = true;
	m_pending = false;
	m_gpu.reset();
	if (m_path == Path::Cpu) {
		m_particles.assign(capacity, GpuParticle{});
		m_trails.assign(size_t{capacity} * trail, math::vec4{0.f, 0.f, 0.f, 0.f});
	}
}

auto ParticleSystem::countSpawns(const ParticleEmitterSettings& iSettings, const float iSeconds) -> uint32_t {
	uint32_t count = m_requested;
	m_requested = 0;
	if (m_emitting) {
		if (m_burstDue) {
			count += iSettings.burst;
			m_burstDue = false;
		}
		m_emitCarry += std::max(iSettings.rate, 0.f) * iSeconds;
		const float whole = std::floor(m_emitCarry);
		m_emitCarry -= whole;
		count += static_cast<uint32_t>(std::min(whole, static_cast<float>(g_maxParticles)));
		m_emitTime += iSeconds;
		if (iSettings.duration > 0.f && m_emitTime >= iSettings.duration) {
			if (iSettings.loop) {
				m_emitTime = std::fmod(m_emitTime, iSettings.duration);
				m_burstDue = true;
			} else {
				m_emitting = false;
			}
		}
	}
	return std::min(count, m_capacity);
}

void ParticleSystem::recordStep(const ParticleEmitterSettings& iSettings, const float iSeconds,
								const math::mat4& iEmitter, const uint32_t iSpawns) {
	const bool merge = m_pending;
	auto& step = m_step;
	if (!merge) {
		step.previous = columnsOf(m_hasEmitter ? m_emitter : iEmitter);
		step.spawn = {m_head, 0, m_serial, m_capacity};
		step.velocity.w() = 0.f;
		step.shift = math::vec4{0.f, 0.f, 0.f, 0.f};
		step.flags = {m_reset ? 1u : 0u, 0, 0, 0};
	}
	step.emitter = columnsOf(iEmitter);
	const bool local = iSettings.space == ParticleSpace::Local;
	math::vec3 gravity = iSettings.gravity;
	if (local)
		gravity = xyz(inverse(iEmitter) * math::vec4{gravity.x(), gravity.y(), gravity.z(), 0.f});
	step.gravity = math::vec4{gravity.x(), gravity.y(), gravity.z(), std::max(iSettings.drag, 0.f)};
	step.shape =
			math::vec4{iSettings.shapeSize.x(), iSettings.shapeSize.y(), iSettings.shapeSize.z(), iSettings.coneAngle};
	step.life = math::vec4{iSettings.lifetimeMin, std::max(iSettings.lifetimeMax, iSettings.lifetimeMin),
						   iSettings.speedMin, std::max(iSettings.speedMax, iSettings.speedMin)};
	step.sizes = math::vec4{iSettings.sizeMin, std::max(iSettings.sizeMax, iSettings.sizeMin), iSettings.spinMin,
							std::max(iSettings.spinMax, iSettings.spinMin)};
	step.velocity = math::vec4{iSettings.velocity.x(), iSettings.velocity.y(), iSettings.velocity.z(),
							   step.velocity.w() + iSeconds};
	step.shift = math::vec4{step.shift.x() + m_shift.x(), step.shift.y() + m_shift.y(), step.shift.z() + m_shift.z(),
							std::max(iSettings.trailInterval, 0.f)};
	step.spawn[1] = std::min(step.spawn[1] + iSpawns, m_capacity);
	step.config = {static_cast<uint32_t>(iSettings.shape), local ? 1u : 0u, iSettings.seed, m_trailLength};
	m_head = (m_head + iSpawns) % m_capacity;
	m_serial += iSpawns;
	m_spawned += iSpawns;
	m_shift = {0.f, 0.f, 0.f};
	m_reset = false;
	m_emitter = iEmitter;
	m_hasEmitter = true;
}

void ParticleSystem::advance(const ParticleEmitterSettings& iSettings, const float iSeconds,
							 const math::mat4& iEmitter) {
	resize(iSettings);
	const bool shifted = m_shift.x() != 0.f || m_shift.y() != 0.f || m_shift.z() != 0.f;
	if (!m_playing && !shifted && !m_reset)
		return;
	const float seconds = m_playing ? std::max(iSeconds, 0.f) : 0.f;
	const uint32_t spawns = m_playing ? countSpawns(iSettings, seconds) : 0u;
	recordStep(iSettings, seconds, iEmitter, spawns);
	if (m_path == Path::Gpu) {
		m_pending = true;
		return;
	}
	const uint32_t trail = m_trailLength;
	for (uint32_t index = 0; index < m_capacity; ++index) {
		const std::span<math::vec4> points =
				trail == 0 ? std::span<math::vec4>{} : std::span{m_trails}.subspan(size_t{index} * trail, trail);
		simulateParticle(m_particles[index], points, index, m_step);
	}
}

void ParticleSystem::shiftOrigin(const math::vec3& iShift) {
	m_shift = m_shift + iShift;
	m_emitter(0, 3) -= iShift.x();
	m_emitter(1, 3) -= iShift.y();
	m_emitter(2, 3) -= iShift.z();
}

auto ParticleSystem::takeStep() -> const ParticleStepParams& {
	m_pending = false;
	return m_step;
}

}// namespace owl::renderer
