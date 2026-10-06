/**
 * @file PhysicCommand.cpp
 * @author Silmaen
 * @date 12/27/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "SolverTaskPool.h"
#include "physics/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"
#include "scene/component/components.h"
#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <span>

namespace owl::physics {

namespace {

inline void logNotInitialized(const char* iFunc) {
	OWL_CORE_WARN("Physic: {} called before initialisation; ignoring.", iFunc)
}

inline void logNullEntity(const char* iFunc) { OWL_CORE_WARN("Physic: {} called with null entity; ignoring.", iFunc) }

auto solverPool(const uint32_t iWorkerCount) -> SolverTaskPool* {
	static uniq<SolverTaskPool> pool;
	if (iWorkerCount <= 1)
		return nullptr;
	if (!pool || pool->getWorkerCount() != iWorkerCount)
		pool = mkUniq<SolverTaskPool>(iWorkerCount);
	return pool.get();
}

}// namespace

class PhysicCommand::Impl {
public:
	Impl() = default;

	~Impl() = default;

	Impl(const Impl&) = delete;

	auto operator=(const Impl&) -> Impl& = delete;

	Impl(Impl&&) = delete;

	auto operator=(Impl&&) -> Impl& = delete;

	void registerBody(const b2BodyId iBody, const scene::Entity& iOwner) {
		bodyOwners[b2StoreBodyId(iBody)] = iOwner.getUUID();
	}

	[[nodiscard]] auto ownerOf(const b2ShapeId iShape) const -> std::optional<core::UUID> {
		if (!b2Shape_IsValid(iShape))
			return std::nullopt;
		if (const auto it = bodyOwners.find(b2StoreBodyId(b2Shape_GetBody(iShape))); it != bodyOwners.end())
			return it->second;
		return std::nullopt;
	}

	[[nodiscard]] auto pairKey(const b2ShapeId iShapeA, const b2ShapeId iShapeB) const
			-> std::optional<std::pair<core::UUID, core::UUID>> {
		const auto ownerA = ownerOf(iShapeA);
		const auto ownerB = ownerOf(iShapeB);
		if (!ownerA || !ownerB || *ownerA == *ownerB)
			return std::nullopt;
		return std::pair{*ownerA, *ownerB};
	}

	void collectContactEvents() {
		const b2ContactEvents events = b2World_GetContactEvents(worldId);
		for (const auto& event: std::span(events.beginEvents, static_cast<size_t>(events.beginCount))) {
			const auto pair = pairKey(event.shapeIdA, event.shapeIdB);
			if (!pair)
				continue;
			if (++touching[ordered(*pair)] == 1 && !isReportedThisFrame(*pair))
				collisionBegins.push_back({.entityA = pair->first, .entityB = pair->second});
		}
		for (const auto& event: std::span(events.endEvents, static_cast<size_t>(events.endCount))) {
			const auto pair = pairKey(event.shapeIdA, event.shapeIdB);
			if (!pair)
				continue;
			if (const auto it = touching.find(ordered(*pair)); it != touching.end() && --it->second == 0)
				touching.erase(it);
		}
	}

	[[nodiscard]] auto isReportedThisFrame(const std::pair<core::UUID, core::UUID>& iPair) const -> bool {
		const auto key = ordered(iPair);
		return std::ranges::any_of(
				std::span(collisionBegins).subspan(std::min(frameEventsStart, collisionBegins.size())),
				[&key](const CollisionEvent& iEvent) -> bool {
					return ordered({iEvent.entityA, iEvent.entityB}) == key;
				});
	}

	void trackBody(const entt::entity iEntity, const uint64_t iBodyId, const b2BodyId iBody) {
		const b2Transform pose = b2Body_GetTransform(iBody);
		syncedIndex[iBodyId] = synced.size();
		synced.push_back({.entity = iEntity, .bodyId = iBodyId, .body = iBody, .previous = pose, .current = pose});
	}

	void untrackBody(const uint64_t iBodyId) {
		const auto it = syncedIndex.find(iBodyId);
		if (it == syncedIndex.end())
			return;
		const size_t index = it->second;
		syncedIndex.erase(it);
		if (index + 1 != synced.size()) {
			synced[index] = synced.back();
			syncedIndex[synced[index].bodyId] = index;
		}
		synced.pop_back();
	}

	void snapBody(const uint64_t iBodyId) {
		if (const auto it = syncedIndex.find(iBodyId); it != syncedIndex.end()) {
			auto& tracked = synced[it->second];
			tracked.current = b2Body_GetTransform(tracked.body);
			tracked.previous = tracked.current;
		}
	}

	void capturePrevious(const bool iFromWorld) {
		for (auto& tracked: synced) tracked.previous = iFromWorld ? b2Body_GetTransform(tracked.body) : tracked.current;
	}

	void captureCurrent() {
		for (auto& tracked: synced) tracked.current = b2Body_GetTransform(tracked.body);
	}

	void writeTransforms(scene::Scene& ioScene, const float iAlpha) const {
		for (const auto& tracked: synced) {
			auto* transform = ioScene.registry.try_get<scene::component::Transform>(tracked.entity);
			if (transform == nullptr)
				continue;
			const b2Vec2 position =
					iAlpha >= 1.f ? tracked.current.p : b2Lerp(tracked.previous.p, tracked.current.p, iAlpha);
			const float angle = b2Rot_GetAngle(iAlpha >= 1.f ? tracked.current.q
															 : b2NLerp(tracked.previous.q, tracked.current.q, iAlpha));
			writeWorldPose(ioScene, tracked.entity, transform->transform, position, angle);
		}
	}

	static void writeWorldPose(scene::Scene& ioScene, const entt::entity iEntity, math::Transform& ioTransform,
							   const b2Vec2 iPosition, const float iAngle) {
		const auto& hierarchy = ioScene.registry.get<scene::component::Hierarchy>(iEntity);
		if (hierarchy.parentId != core::UUID{0}) {
			if (const scene::Entity parent = ioScene.findEntityByUUID(hierarchy.parentId); parent) {
				const math::Transform parentWorld = ioScene.getWorldTransform(parent);
				const math::vec4 localPos = math::inverse(parentWorld()) *
											math::vec4{iPosition.x, iPosition.y, ioTransform.translation().z(), 1.0f};
				ioTransform.translation().x() = localPos.x();
				ioTransform.translation().y() = localPos.y();
				ioTransform.rotation().z() = iAngle - parentWorld.rotation().z();
				return;
			}
		}
		ioTransform.translation().x() = iPosition.x;
		ioTransform.translation().y() = iPosition.y;
		ioTransform.rotation().z() = iAngle;
	}

	void forgetEntity(const core::UUID iEntity) {
		std::erase_if(touching, [iEntity](const auto& iEntry) -> bool {
			return iEntry.first.first == iEntity || iEntry.first.second == iEntity;
		});
		std::erase_if(bodyOwners, [iEntity](const auto& iEntry) -> bool { return iEntry.second == iEntity; });
	}

	[[nodiscard]] static auto ordered(const std::pair<core::UUID, core::UUID>& iPair) -> std::pair<uint64_t, uint64_t> {
		const auto first = static_cast<uint64_t>(iPair.first);
		const auto second = static_cast<uint64_t>(iPair.second);
		return first < second ? std::pair{first, second} : std::pair{second, first};
	}

	b2WorldId worldId{0, 0};
	uint64_t nextId = 1;
	std::unordered_map<uint64_t, b2BodyId> bodies;
	std::unordered_map<uint64_t, core::UUID> bodyOwners;
	std::map<std::pair<uint64_t, uint64_t>, uint32_t> touching;
	std::vector<CollisionEvent> collisionBegins;
	size_t frameEventsStart = 0;

	struct SyncedBody {
		entt::entity entity;
		uint64_t bodyId;
		b2BodyId body;
		b2Transform previous;
		b2Transform current;
	};
	std::vector<SyncedBody> synced;
	std::unordered_map<uint64_t, size_t> syncedIndex;
	PhysicsSettings settings;
	double stepSeconds = 1.0 / static_cast<double>(PhysicsSettings::defaultTickRate);
	double accumulator = 0.0;
	uint32_t lastStepCount = 0;
	float alpha = 1.f;
	SolverTaskPool* taskPool = nullptr;
};
shared<PhysicCommand::Impl> PhysicCommand::m_impl = nullptr;
scene::Scene* PhysicCommand::m_scene = nullptr;

PhysicCommand::PhysicCommand() = default;

void PhysicCommand::init(scene::Scene* iScene) {
	if (iScene == nullptr) {
		OWL_CORE_ERROR("Physic: init() called with null scene; physics not initialised.")
		return;
	}
	if (isInitialized())
		destroy();
	m_impl = mkShared<Impl>();
	m_scene = iScene;
	m_impl->settings = iScene->getPhysicsSettings().clamped();
	m_impl->stepSeconds = m_impl->settings.getStepSeconds();
	b2WorldDef def = b2DefaultWorldDef();
	def.gravity = {.x = 0.0f, .y = -9.81f};
	uint32_t dynamicBodies = 0;
	for (const auto [e, body]: m_scene->registry.view<scene::component::PhysicBody>().each())
		if (body.body.type == scene::SceneBody::BodyType::Dynamic)
			++dynamicBodies;
	m_impl->taskPool = solverPool(m_impl->settings.getEffectiveWorkerCount(dynamicBodies));
	if (m_impl->taskPool != nullptr)
		m_impl->taskPool->configure(def);
	m_impl->worldId = b2CreateWorld(&def);

	OWL_INFO("PhysicCommand::init(), world created ({} {}).", m_impl->worldId.index1, m_impl->worldId.generation)

	// Add entities...
	for (const auto& view = m_scene->registry.view<scene::component::PhysicBody, scene::component::Transform>();
		 const auto& e: view) {
		const scene::Entity entity{e, m_scene};
		auto& [sbody] = entity.getComponent<scene::component::PhysicBody>();
		const math::Transform worldTransform = m_scene->getWorldTransform(entity);
		b2BodyDef bodyDef = b2DefaultBodyDef();
		switch (sbody.type) {
			case scene::SceneBody::BodyType::Static:
				bodyDef.type = b2_staticBody;
				break;
			case scene::SceneBody::BodyType::Dynamic:
				bodyDef.type = b2_dynamicBody;
				break;
			case scene::SceneBody::BodyType::Kinematic:
				bodyDef.type = b2_kinematicBody;
				break;
		}
		bodyDef.fixedRotation = sbody.fixedRotation;
		bodyDef.position.x = worldTransform.translation().x();
		bodyDef.position.y = worldTransform.translation().y();
		bodyDef.rotation = b2MakeRot(worldTransform.rotation().z());

		const b2BodyId body = b2CreateBody(m_impl->worldId, &bodyDef);
		OWL_INFO("PhysicCommand::init(), body created ({} {} {}).", body.index1, body.world0, body.generation)
		sbody.bodyId = m_impl->nextId;
		m_impl->bodies[m_impl->nextId] = body;
		m_impl->trackBody(e, m_impl->nextId, body);
		m_impl->nextId++;
		m_impl->registerBody(body, entity);

		const b2Polygon dynamicBox = b2MakeBox(sbody.colliderSize.x() * worldTransform.scale().x() * 0.5f,
											   sbody.colliderSize.y() * worldTransform.scale().y() * 0.5f);
		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.density = sbody.density;
		shapeDef.material.friction = sbody.friction;
		shapeDef.material.restitution = sbody.restitution;
		shapeDef.enableContactEvents = true;
		b2CreatePolygonShape(body, &shapeDef, &dynamicBox);
	}

	for (const auto view = m_scene->registry.view<scene::component::Tilemap, scene::component::Transform>();
		 const auto e: view) {
		const scene::Entity entity{e, m_scene};
		const auto& tilemap = entity.getComponent<scene::component::Tilemap>();
		if (!tilemap.asset || !tilemap.asset->tileset || tilemap.asset->layers.empty())
			continue;
		const auto& assetData = *tilemap.asset;
		const auto* tilesetPtr = assetData.tileset.get();
		bool anyCollidable = false;
		for (uint32_t i = 0; i < tilesetPtr->tileCount(); ++i) {
			if (tilesetPtr->isCollidable(i)) {
				anyCollidable = true;
				break;
			}
		}
		if (!anyCollidable)
			continue;
		const math::Transform worldTransform = m_scene->getWorldTransform(entity);
		b2BodyDef bodyDef = b2DefaultBodyDef();
		bodyDef.type = b2_staticBody;
		bodyDef.position.x = worldTransform.translation().x();
		bodyDef.position.y = worldTransform.translation().y();
		bodyDef.rotation = b2MakeRot(worldTransform.rotation().z());
		const b2BodyId tileBody = b2CreateBody(m_impl->worldId, &bodyDef);
		m_impl->bodies[m_impl->nextId] = tileBody;
		m_impl->nextId++;
		m_impl->registerBody(tileBody, entity);
		const float cellSize = assetData.cellSize;
		const float originX = -static_cast<float>(assetData.width - 1) * 0.5f * cellSize;
		const float originY = static_cast<float>(assetData.height - 1) * 0.5f * cellSize;
		const float halfX = cellSize * worldTransform.scale().x() * 0.5f;
		const float halfY = cellSize * worldTransform.scale().y() * 0.5f;
		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.density = 0.f;// static
		shapeDef.material.friction = 0.5f;
		shapeDef.enableContactEvents = true;
		for (const auto& layer: assetData.layers) {
			for (uint32_t y = 0; y < assetData.height; ++y) {
				for (uint32_t x = 0; x < assetData.width; ++x) {
					const size_t flat = static_cast<size_t>(y) * assetData.width + x;
					if (flat >= layer.tiles.size())
						continue;
					const int32_t tileIdx = layer.tiles[flat];
					if (tileIdx < 0 || !tilesetPtr->isCollidable(static_cast<uint32_t>(tileIdx)))
						continue;
					const float cx = (originX + static_cast<float>(x) * cellSize) * worldTransform.scale().x();
					const float cy = (originY - static_cast<float>(y) * cellSize) * worldTransform.scale().y();
					const b2Polygon cellBox = b2MakeOffsetBox(halfX, halfY, b2Vec2{.x = cx, .y = cy}, b2MakeRot(0.f));

					b2CreatePolygonShape(tileBody, &shapeDef, &cellBox);
				}
			}
		}
	}

	for (const auto view = m_scene->registry.view<scene::component::RaycastDoor, scene::component::Transform>();
		 const auto e: view) {
		const scene::Entity entity{e, m_scene};
		if (entity.hasComponent<scene::component::PhysicBody>())
			continue;// designer-managed body, leave it alone
		auto& door = view.get<scene::component::RaycastDoor>(e);
		const math::Transform worldTransform = m_scene->getWorldTransform(entity);
		b2BodyDef bodyDef = b2DefaultBodyDef();
		bodyDef.type = b2_kinematicBody;
		bodyDef.fixedRotation = true;
		bodyDef.position.x = worldTransform.translation().x();
		bodyDef.position.y = worldTransform.translation().y();
		bodyDef.rotation = b2MakeRot(worldTransform.rotation().z());
		const b2BodyId body = b2CreateBody(m_impl->worldId, &bodyDef);
		using OD = scene::component::RaycastDoor::OpeningDirection;
		const bool slideAlongY = (door.openingDirection == OD::North || door.openingDirection == OD::South);
		constexpr float kPlateHalfThickness = 0.05f;// matches the renderer's lateral bias
		const float halfX = slideAlongY ? kPlateHalfThickness : 0.5f;
		const float halfY = slideAlongY ? 0.5f : kPlateHalfThickness;
		const b2Polygon plateBox = b2MakeBox(halfX, halfY);
		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.density = 0.f;
		shapeDef.material.friction = 0.5f;
		shapeDef.enableContactEvents = true;
		b2CreatePolygonShape(body, &shapeDef, &plateBox);
		door.bodyId = m_impl->nextId;
		m_impl->bodies[m_impl->nextId] = body;
		m_impl->nextId++;
		m_impl->registerBody(body, entity);
	}
	for (const auto view = m_scene->registry.view<scene::component::RaycastPushWall, scene::component::Transform>();
		 const auto e: view) {
		const scene::Entity entity{e, m_scene};
		if (entity.hasComponent<scene::component::PhysicBody>())
			continue;
		auto& push = view.get<scene::component::RaycastPushWall>(e);
		const math::Transform worldTransform = m_scene->getWorldTransform(entity);
		b2BodyDef bodyDef = b2DefaultBodyDef();
		bodyDef.type = b2_kinematicBody;
		bodyDef.fixedRotation = true;
		bodyDef.position.x = worldTransform.translation().x();
		bodyDef.position.y = worldTransform.translation().y();
		bodyDef.rotation = b2MakeRot(worldTransform.rotation().z());
		const b2BodyId body = b2CreateBody(m_impl->worldId, &bodyDef);
		// Pushwall footprint: full 1×1 block — same as the rendered cube.
		const b2Polygon block = b2MakeBox(0.5f, 0.5f);
		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.density = 0.f;
		shapeDef.material.friction = 0.5f;
		shapeDef.enableContactEvents = true;
		b2CreatePolygonShape(body, &shapeDef, &block);
		push.bodyId = m_impl->nextId;
		m_impl->bodies[m_impl->nextId] = body;
		m_impl->nextId++;
		m_impl->registerBody(body, entity);
	}
}

void PhysicCommand::destroy() {
	m_scene = nullptr;
	if (!m_impl)
		return;
	b2DestroyWorld(m_impl->worldId);
	m_impl->worldId = {.index1 = 0, .generation = 0};
	m_impl->bodies.clear();
	m_impl.reset();
}

void PhysicCommand::releaseScene(const scene::Scene* iScene) {
	if (iScene == nullptr || m_scene != iScene)
		return;
	destroy();
}

auto PhysicCommand::isInitialized() -> bool { return m_scene != nullptr; }

void PhysicCommand::frame(const core::Timestep& iTimestep) {
	OWL_PROFILE_FUNCTION()

	if (!isInitialized()) {
		logNotInitialized("frame");
		return;
	}
	auto& impl = *m_impl;
	const double step = impl.stepSeconds;
	impl.accumulator += std::max(0.0, static_cast<double>(iTimestep.getMilliseconds()) / 1000.0);
	// The tolerance absorbs the rounding of frame durations that are whole multiples of the step.
	auto stepCount = static_cast<uint32_t>(std::floor((impl.accumulator + step * 1e-4) / step));
	impl.accumulator = std::max(0.0, impl.accumulator - static_cast<double>(stepCount) * step);
	if (stepCount > impl.settings.maxStepsPerFrame) {
		stepCount = impl.settings.maxStepsPerFrame;
		impl.accumulator = std::fmod(impl.accumulator, step);
	}
	impl.lastStepCount = stepCount;
	impl.frameEventsStart = impl.collisionBegins.size();
	const auto subSteps = static_cast<int>(impl.settings.solverSubSteps);
	for (uint32_t i = 0; i < stepCount; ++i) {
		if (impl.settings.interpolate && i + 1 == stepCount)
			impl.capturePrevious(/*iFromWorld=*/stepCount > 1);
		b2World_Step(impl.worldId, static_cast<float>(step), subSteps);
		if (impl.taskPool != nullptr)
			impl.taskPool->recycle();
		impl.collectContactEvents();
	}
	if (stepCount > 0)
		impl.captureCurrent();
	impl.alpha = impl.settings.interpolate ? static_cast<float>(std::min(impl.accumulator / step, 1.0)) : 1.f;
	if (stepCount > 0 || impl.settings.interpolate)
		impl.writeTransforms(*m_scene, impl.alpha);
}

auto PhysicCommand::getSettings() -> PhysicsSettings { return isInitialized() ? m_impl->settings : PhysicsSettings{}; }

auto PhysicCommand::getWorkerCount() -> uint32_t {
	if (!isInitialized())
		return 0;
	return m_impl->taskPool != nullptr ? m_impl->taskPool->getWorkerCount() : 1;
}

auto PhysicCommand::getLastFrameStepCount() -> uint32_t { return isInitialized() ? m_impl->lastStepCount : 0; }

auto PhysicCommand::getInterpolationAlpha() -> float { return isInitialized() ? m_impl->alpha : 1.f; }

void PhysicCommand::syncSimulatedTransforms() {
	if (!isInitialized())
		return;
	m_impl->writeTransforms(*m_scene, 1.f);
}

auto PhysicCommand::takeCollisionEvents() -> std::vector<CollisionEvent> {
	if (!isInitialized())
		return {};
	return std::exchange(m_impl->collisionBegins, {});
}

void PhysicCommand::destroyBody(const scene::Entity& iEntity) {
	if (!isInitialized() || !iEntity)
		return;
	const auto release = [](uint64_t& ioBodyId) -> void {
		if (ioBodyId == 0)
			return;
		if (const auto it = m_impl->bodies.find(ioBodyId); it != m_impl->bodies.end()) {
			b2DestroyBody(it->second);
			m_impl->bodies.erase(it);
		}
		m_impl->untrackBody(ioBodyId);
		ioBodyId = 0;
	};
	if (iEntity.hasComponent<scene::component::PhysicBody>())
		release(iEntity.getComponent<scene::component::PhysicBody>().body.bodyId);
	if (iEntity.hasComponent<scene::component::RaycastDoor>())
		release(iEntity.getComponent<scene::component::RaycastDoor>().bodyId);
	if (iEntity.hasComponent<scene::component::RaycastPushWall>())
		release(iEntity.getComponent<scene::component::RaycastPushWall>().bodyId);
	m_impl->forgetEntity(iEntity.getUUID());
}

void PhysicCommand::impulse(const scene::Entity& iEntity, const math::vec2f& iImpulse) {
	if (!isInitialized()) {
		logNotInitialized("impulse");
		return;
	}
	if (!iEntity) {
		// Void entity !!
		logNullEntity("impulse");
		return;
	}
	if (!iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type == scene::SceneBody::BodyType::Static)
		return;
	b2Body_ApplyLinearImpulseToCenter(m_impl->bodies[body.bodyId], {iImpulse.x(), iImpulse.y()}, true);
}

auto PhysicCommand::getVelocity(const scene::Entity& iEntity) -> math::vec2f {
	if (!isInitialized()) {
		logNotInitialized("getVelocity");
		return {0.0f, 0.0f};
	}
	if (!iEntity) {
		// Void entity !!
		logNullEntity("getVelocity");
		return {0.0f, 0.0f};
	}
	if (!iEntity.hasComponent<scene::component::PhysicBody>())
		return {0.0f, 0.0f};
	auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type == scene::SceneBody::BodyType::Static)
		return {0.0f, 0.0f};
	const auto [x, y] = b2Body_GetLinearVelocity(m_impl->bodies[body.bodyId]);
	return {x, y};
}

void PhysicCommand::setTransform(const scene::Entity& iEntity, const math::vec2f& iPosition, const float iRotation) {
	if (!isInitialized()) {
		logNotInitialized("setTransform");
		return;
	}
	if (!iEntity) {
		logNullEntity("setTransform");
		return;
	}
	// Explicit `PhysicBody` takes priority — that's the designer-authored body.
	if (iEntity.hasComponent<scene::component::PhysicBody>()) {
		auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
		b2Body_SetTransform(m_impl->bodies[body.bodyId], {iPosition.x(), iPosition.y()}, b2MakeRot(iRotation));
		m_impl->snapBody(body.bodyId);
		return;
	}
	// Otherwise fall back to the auto-created kinematic body for raycast doors / pushwalls.
	uint64_t bodyId = 0;
	if (iEntity.hasComponent<scene::component::RaycastDoor>())
		bodyId = iEntity.getComponent<scene::component::RaycastDoor>().bodyId;
	else if (iEntity.hasComponent<scene::component::RaycastPushWall>())
		bodyId = iEntity.getComponent<scene::component::RaycastPushWall>().bodyId;
	if (bodyId == 0)
		return;
	if (const auto it = m_impl->bodies.find(bodyId); it != m_impl->bodies.end())
		b2Body_SetTransform(it->second, {iPosition.x(), iPosition.y()}, b2MakeRot(iRotation));
}

void PhysicCommand::setVelocity(const scene::Entity& iEntity, const math::vec2f& iVelocity) {
	if (!isInitialized()) {
		logNotInitialized("setVelocity");
		return;
	}
	if (!iEntity) {
		logNullEntity("setVelocity");
		return;
	}
	if (!iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type == scene::SceneBody::BodyType::Static)
		return;
	b2Body_SetLinearVelocity(m_impl->bodies[body.bodyId], {iVelocity.x(), iVelocity.y()});
}

void PhysicCommand::setGravityScale(const scene::Entity& iEntity, const float iScale) {
	if (!isInitialized()) {
		logNotInitialized("setGravityScale");
		return;
	}
	if (!iEntity) {
		logNullEntity("setGravityScale");
		return;
	}
	if (!iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type != scene::SceneBody::BodyType::Dynamic)
		return;
	b2Body_SetGravityScale(m_impl->bodies[body.bodyId], iScale);
	// Wake the body so the new scale takes effect immediately.
	b2Body_SetAwake(m_impl->bodies[body.bodyId], true);
}

auto PhysicCommand::getSnapshot(const scene::Entity& iEntity) -> PhysicsSnapshot {
	PhysicsSnapshot snapshot;
	if (!isInitialized() || !iEntity || !iEntity.hasComponent<scene::component::PhysicBody>())
		return snapshot;
	const auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type == scene::SceneBody::BodyType::Static)
		return snapshot;
	const auto bodyId = m_impl->bodies[body.bodyId];
	const auto [vx, vy] = b2Body_GetLinearVelocity(bodyId);
	snapshot.linearVelocity = {vx, vy};
	snapshot.angularVelocity = b2Body_GetAngularVelocity(bodyId);
	snapshot.awake = b2Body_IsAwake(bodyId);
	return snapshot;
}

void PhysicCommand::applySnapshot(const scene::Entity& iEntity, const PhysicsSnapshot& iSnapshot) {
	if (!isInitialized() || !iEntity || !iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	const auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type == scene::SceneBody::BodyType::Static)
		return;
	const auto bodyId = m_impl->bodies[body.bodyId];
	b2Body_SetLinearVelocity(bodyId, {iSnapshot.linearVelocity.x(), iSnapshot.linearVelocity.y()});
	b2Body_SetAngularVelocity(bodyId, iSnapshot.angularVelocity);
	if (iSnapshot.awake)
		b2Body_SetAwake(bodyId, true);
}

}// namespace owl::physics
