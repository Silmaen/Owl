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
#include <atomic>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <utility>

namespace owl::physics {

namespace {

// An AVX2 Box2D on a CPU without AVX2: physics stays off instead of crashing on an illegal instruction.
auto isCpuUnsupported() -> bool {
#ifdef OWL_PHYSICS_AVX2
	static const bool unsupported = __builtin_cpu_supports("avx2") == 0;
	return unsupported;
#else
	return false;
#endif
}

inline void logNotInitialized(const char* iFunc) {
	if (isCpuUnsupported())
		return;
	OWL_CORE_WARN("Physic: {} called before initialisation; ignoring.", iFunc)
}

inline void logNullEntity(const char* iFunc) { OWL_CORE_WARN("Physic: {} called with null entity; ignoring.", iFunc) }

}// namespace

class PhysicsWorld {
public:
	// Body whose pose is copied to its entity each frame, with the last two step poses for interpolation.
	struct SyncedBody {
		entt::entity entity;
		uint64_t bodyId;
		b2BodyId body;
		b2Transform previous;
		b2Transform current;
	};

	PhysicsWorld() = default;

	~PhysicsWorld() = default;

	PhysicsWorld(const PhysicsWorld&) = delete;

	auto operator=(const PhysicsWorld&) -> PhysicsWorld& = delete;

	PhysicsWorld(PhysicsWorld&&) = delete;

	auto operator=(PhysicsWorld&&) -> PhysicsWorld& = delete;

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
				[&key](const PhysicCommand::CollisionEvent& iEvent) -> bool {
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
			const b2Transform moved = b2Body_GetTransform(tracked.body);
			// Carry the previous pose along with the teleport so the blend between steps goes on without a hitch.
			tracked.previous = b2MulTransforms(moved, b2InvMulTransforms(tracked.current, tracked.previous));
			tracked.current = moved;
		}
	}

	void capturePrevious(const bool iFromWorld) {
		for (auto& tracked: synced) tracked.previous = iFromWorld ? b2Body_GetTransform(tracked.body) : tracked.current;
	}

	// Below this many bodies the per-frame copies stay on the calling thread (dispatch costs more than it saves).
	static constexpr size_t parallelSyncMinBodies = 1024;
	static constexpr size_t parallelSyncMinRange = 256;

	void captureCurrent() {
		const auto capture = [this](const size_t iBegin, const size_t iEnd) -> void {
			for (size_t i = iBegin; i < iEnd; ++i) synced[i].current = b2Body_GetTransform(synced[i].body);
		};
		if (taskPool != nullptr && synced.size() >= parallelSyncMinBodies)
			taskPool->parallelFor(synced.size(), parallelSyncMinRange, capture);
		else
			capture(0, synced.size());
	}

	void writeTransforms(scene::Scene& ioScene, const float iAlpha) const {
		if (taskPool == nullptr || synced.size() < parallelSyncMinBodies) {
			for (const auto& tracked: synced) writeTransform(ioScene, tracked, iAlpha);
			return;
		}
		// Roots are written in parallel; a child reads its parent's world transform, so children get a serial pass.
		std::atomic<bool> hasChildren{false};
		taskPool->parallelFor(synced.size(), parallelSyncMinRange,
							  [&ioScene, &hasChildren, iAlpha, this](const size_t iBegin, const size_t iEnd) -> void {
								  for (size_t i = iBegin; i < iEnd; ++i) {
									  const auto& tracked = synced[i];
									  if (ioScene.registry.get<scene::component::Hierarchy>(tracked.entity).parentId !=
										  core::UUID{0}) {
										  hasChildren.store(true, std::memory_order_relaxed);
										  continue;
									  }
									  if (auto* transform =
												  ioScene.registry.try_get<scene::component::Transform>(tracked.entity);
										  transform != nullptr) {
										  const auto [position, angle] = interpolatedPose(tracked, iAlpha);
										  writeRootPose(transform->transform, position, angle);
									  }
								  }
							  });
		if (!hasChildren.load(std::memory_order_relaxed))
			return;
		for (const auto& tracked: synced)
			if (ioScene.registry.get<scene::component::Hierarchy>(tracked.entity).parentId != core::UUID{0})
				writeTransform(ioScene, tracked, iAlpha);
	}

	static void writeTransform(scene::Scene& ioScene, const SyncedBody& iTracked, const float iAlpha) {
		auto* transform = ioScene.registry.try_get<scene::component::Transform>(iTracked.entity);
		if (transform == nullptr)
			return;
		const auto [position, angle] = interpolatedPose(iTracked, iAlpha);
		writeWorldPose(ioScene, iTracked.entity, transform->transform, position, angle);
	}

	[[nodiscard]] static auto interpolatedPose(const SyncedBody& iTracked, const float iAlpha)
			-> std::pair<b2Vec2, float> {
		if (iAlpha >= 1.f)
			return {iTracked.current.p, b2Rot_GetAngle(iTracked.current.q)};
		return {b2Lerp(iTracked.previous.p, iTracked.current.p, iAlpha),
				b2Rot_GetAngle(b2NLerp(iTracked.previous.q, iTracked.current.q, iAlpha))};
	}

	static void writeRootPose(math::Transform& ioTransform, const b2Vec2 iPosition, const float iAngle) {
		ioTransform.translation().x() = iPosition.x;
		ioTransform.translation().y() = iPosition.y;
		ioTransform.rotation().z() = iAngle;
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
		writeRootPose(ioTransform, iPosition, iAngle);
	}

	void forgetContacts(const core::UUID iEntity) {
		std::erase_if(touching, [iEntity](const auto& iEntry) -> bool {
			return iEntry.first.first == iEntity || iEntry.first.second == iEntity;
		});
	}

	[[nodiscard]] auto find(const uint64_t iBodyId) const -> std::optional<b2BodyId> {
		if (iBodyId == 0)
			return std::nullopt;
		if (const auto it = bodies.find(iBodyId); it != bodies.end())
			return it->second;
		return std::nullopt;
	}

	auto store(const b2BodyId iBody, const scene::Entity& iOwner) -> uint64_t {
		const uint64_t bodyId = nextId++;
		bodies[bodyId] = iBody;
		registerBody(iBody, iOwner);
		return bodyId;
	}

	// Reads no component, so it is safe inside a destruction signal.
	void release(uint64_t& ioBodyId) {
		if (ioBodyId == 0)
			return;
		if (const auto it = bodies.find(ioBodyId); it != bodies.end()) {
			if (const auto owner = bodyOwners.find(b2StoreBodyId(it->second)); owner != bodyOwners.end()) {
				forgetContacts(owner->second);
				bodyOwners.erase(owner);
			}
			b2DestroyBody(it->second);
			bodies.erase(it);
		}
		untrackBody(ioBodyId);
		ioBodyId = 0;
	}

	void createPhysicBody(scene::Scene& ioScene, const entt::entity iEntity) {
		const scene::Entity entity{iEntity, &ioScene};
		auto& [sbody] = entity.getComponent<scene::component::PhysicBody>();
		const math::Transform worldTransform = ioScene.getWorldTransform(entity);
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

		const b2BodyId body = b2CreateBody(worldId, &bodyDef);
		OWL_INFO("PhysicCommand: body created ({} {} {}).", body.index1, body.world0, body.generation)
		sbody.bodyId = store(body, entity);
		trackBody(iEntity, sbody.bodyId, body);

		const b2Polygon dynamicBox = b2MakeBox(sbody.colliderSize.x() * worldTransform.scale().x() * 0.5f,
											   sbody.colliderSize.y() * worldTransform.scale().y() * 0.5f);
		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.density = sbody.density;
		shapeDef.material.friction = sbody.friction;
		shapeDef.material.restitution = sbody.restitution;
		shapeDef.enableContactEvents = true;
		b2CreatePolygonShape(body, &shapeDef, &dynamicBox);
	}

	// Created a frame late so the entity is fully set up (transform, parent) when its body is placed.
	void createPendingBodies(scene::Scene& ioScene) {
		for (const auto entity: std::exchange(pendingBodies, {})) {
			const auto* physicBody = ioScene.registry.try_get<scene::component::PhysicBody>(entity);
			if (physicBody != nullptr && physicBody->body.bodyId == 0 &&
				ioScene.registry.all_of<scene::component::Transform, scene::component::Hierarchy>(entity))
				createPhysicBody(ioScene, entity);
		}
	}

	void onPhysicBodyAdded(entt::registry& ioRegistry, const entt::entity iEntity) {
		// A copied component (duplication) carries the source's id: the copy must own a body of its own.
		ioRegistry.get<scene::component::PhysicBody>(iEntity).body.bodyId = 0;
		pendingBodies.push_back(iEntity);
	}

	static void onDoorAdded(entt::registry& ioRegistry, const entt::entity iEntity) {
		ioRegistry.get<scene::component::RaycastDoor>(iEntity).bodyId = 0;
	}

	static void onPushWallAdded(entt::registry& ioRegistry, const entt::entity iEntity) {
		ioRegistry.get<scene::component::RaycastPushWall>(iEntity).bodyId = 0;
	}

	void onPhysicBodyRemoved(entt::registry& ioRegistry, const entt::entity iEntity) {
		std::erase(pendingBodies, iEntity);
		release(ioRegistry.get<scene::component::PhysicBody>(iEntity).body.bodyId);
	}

	void onTilemapRemoved(entt::registry& /*ioRegistry*/, const entt::entity iEntity) {
		if (const auto it = tilemapBodies.find(iEntity); it != tilemapBodies.end()) {
			release(it->second);
			tilemapBodies.erase(it);
		}
	}

	void onDoorRemoved(entt::registry& ioRegistry, const entt::entity iEntity) {
		release(ioRegistry.get<scene::component::RaycastDoor>(iEntity).bodyId);
	}

	void onPushWallRemoved(entt::registry& ioRegistry, const entt::entity iEntity) {
		release(ioRegistry.get<scene::component::RaycastPushWall>(iEntity).bodyId);
	}

	void connect(entt::registry& ioRegistry) {
		ioRegistry.on_construct<scene::component::PhysicBody>().connect<&PhysicsWorld::onPhysicBodyAdded>(*this);
		ioRegistry.on_destroy<scene::component::PhysicBody>().connect<&PhysicsWorld::onPhysicBodyRemoved>(*this);
		ioRegistry.on_destroy<scene::component::Tilemap>().connect<&PhysicsWorld::onTilemapRemoved>(*this);
		ioRegistry.on_construct<scene::component::RaycastDoor>().connect<&PhysicsWorld::onDoorAdded>();
		ioRegistry.on_construct<scene::component::RaycastPushWall>().connect<&PhysicsWorld::onPushWallAdded>();
		ioRegistry.on_destroy<scene::component::RaycastDoor>().connect<&PhysicsWorld::onDoorRemoved>(*this);
		ioRegistry.on_destroy<scene::component::RaycastPushWall>().connect<&PhysicsWorld::onPushWallRemoved>(*this);
	}

	void disconnect(entt::registry& ioRegistry) {
		ioRegistry.on_construct<scene::component::PhysicBody>().disconnect(static_cast<const void*>(this));
		ioRegistry.on_destroy<scene::component::PhysicBody>().disconnect(static_cast<const void*>(this));
		ioRegistry.on_destroy<scene::component::Tilemap>().disconnect(static_cast<const void*>(this));
		ioRegistry.on_construct<scene::component::RaycastDoor>().disconnect<&PhysicsWorld::onDoorAdded>();
		ioRegistry.on_construct<scene::component::RaycastPushWall>().disconnect<&PhysicsWorld::onPushWallAdded>();
		ioRegistry.on_destroy<scene::component::RaycastDoor>().disconnect(static_cast<const void*>(this));
		ioRegistry.on_destroy<scene::component::RaycastPushWall>().disconnect(static_cast<const void*>(this));
	}

	[[nodiscard]] static auto ordered(const std::pair<core::UUID, core::UUID>& iPair) -> std::pair<uint64_t, uint64_t> {
		const auto first = static_cast<uint64_t>(iPair.first);
		const auto second = static_cast<uint64_t>(iPair.second);
		return first < second ? std::pair{first, second} : std::pair{second, first};
	}

	b2WorldId worldId{0, 0};
	uint64_t nextId = 1;
	std::unordered_map<uint64_t, b2BodyId> bodies;
	std::unordered_map<entt::entity, uint64_t> tilemapBodies;
	std::vector<entt::entity> pendingBodies;
	std::unordered_map<uint64_t, core::UUID> bodyOwners;
	std::map<std::pair<uint64_t, uint64_t>, uint32_t> touching;
	std::vector<PhysicCommand::CollisionEvent> collisionBegins;
	size_t frameEventsStart = 0;

	std::vector<SyncedBody> synced;
	std::unordered_map<uint64_t, size_t> syncedIndex;
	PhysicsSettings settings;
	double stepSeconds = 1.0 / static_cast<double>(PhysicsSettings::defaultTickRate);
	double accumulator = 0.0;
	uint32_t lastStepCount = 0;
	float alpha = 1.f;
	uniq<SolverTaskPool> taskPool;
};
namespace {

auto worldOf(const scene::Entity& iEntity) -> PhysicsWorld* {
	return iEntity ? iEntity.getScene()->getPhysicsWorld() : nullptr;
}

auto runningWorld(const scene::Entity& iEntity, const char* iFunc) -> PhysicsWorld* {
	if (!iEntity) {
		logNullEntity(iFunc);
		return nullptr;
	}
	auto* const world = worldOf(iEntity);
	if (world == nullptr)
		logNotInitialized(iFunc);
	return world;
}

}// namespace

void PhysicCommand::init(scene::Scene& ioScene) {
	if (isInitialized(ioScene))
		destroy(ioScene);
	if (isCpuUnsupported()) {
		OWL_CORE_ERROR("Physic: Box2D is built with AVX2, which this CPU lacks; physics disabled.")
		return;
	}
	const auto world = mkShared<PhysicsWorld>();
	ioScene.m_physicsWorld = world;
	world->settings = ioScene.getPhysicsSettings().clamped();
	world->stepSeconds = world->settings.getStepSeconds();
	b2WorldDef def = b2DefaultWorldDef();
	def.gravity = {.x = 0.0f, .y = -9.81f};
	uint32_t dynamicBodies = 0;
	for (const auto [e, body]: ioScene.registry.view<scene::component::PhysicBody>().each())
		if (body.body.type == scene::SceneBody::BodyType::Dynamic)
			++dynamicBodies;
	if (const uint32_t workers = world->settings.getEffectiveWorkerCount(dynamicBodies); workers > 1) {
		world->taskPool = mkUniq<SolverTaskPool>(workers);
		world->taskPool->configure(def);
	}
	world->worldId = b2CreateWorld(&def);

	OWL_INFO("Physic: World created ({} {}).", world->worldId.index1, world->worldId.generation)

	// Add entities...
	for (const auto e: ioScene.registry.view<scene::component::PhysicBody, scene::component::Transform>())
		world->createPhysicBody(ioScene, e);

	for (const auto view = ioScene.registry.view<scene::component::Tilemap, scene::component::Transform>();
		 const auto e: view) {
		const scene::Entity entity{e, &ioScene};
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
		const math::Transform worldTransform = ioScene.getWorldTransform(entity);
		b2BodyDef bodyDef = b2DefaultBodyDef();
		bodyDef.type = b2_staticBody;
		bodyDef.position.x = worldTransform.translation().x();
		bodyDef.position.y = worldTransform.translation().y();
		bodyDef.rotation = b2MakeRot(worldTransform.rotation().z());
		const b2BodyId tileBody = b2CreateBody(world->worldId, &bodyDef);
		world->tilemapBodies[e] = world->store(tileBody, entity);
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

	for (const auto view = ioScene.registry.view<scene::component::RaycastDoor, scene::component::Transform>();
		 const auto e: view) {
		const scene::Entity entity{e, &ioScene};
		if (entity.hasComponent<scene::component::PhysicBody>())
			continue;// designer-managed body, leave it alone
		auto& door = view.get<scene::component::RaycastDoor>(e);
		const math::Transform worldTransform = ioScene.getWorldTransform(entity);
		b2BodyDef bodyDef = b2DefaultBodyDef();
		bodyDef.type = b2_kinematicBody;
		bodyDef.fixedRotation = true;
		bodyDef.position.x = worldTransform.translation().x();
		bodyDef.position.y = worldTransform.translation().y();
		bodyDef.rotation = b2MakeRot(worldTransform.rotation().z());
		const b2BodyId body = b2CreateBody(world->worldId, &bodyDef);
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
		door.bodyId = world->store(body, entity);
	}
	for (const auto view = ioScene.registry.view<scene::component::RaycastPushWall, scene::component::Transform>();
		 const auto e: view) {
		const scene::Entity entity{e, &ioScene};
		if (entity.hasComponent<scene::component::PhysicBody>())
			continue;
		auto& push = view.get<scene::component::RaycastPushWall>(e);
		const math::Transform worldTransform = ioScene.getWorldTransform(entity);
		b2BodyDef bodyDef = b2DefaultBodyDef();
		bodyDef.type = b2_kinematicBody;
		bodyDef.fixedRotation = true;
		bodyDef.position.x = worldTransform.translation().x();
		bodyDef.position.y = worldTransform.translation().y();
		bodyDef.rotation = b2MakeRot(worldTransform.rotation().z());
		const b2BodyId body = b2CreateBody(world->worldId, &bodyDef);
		// Pushwall footprint: full 1×1 block — same as the rendered cube.
		const b2Polygon block = b2MakeBox(0.5f, 0.5f);
		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.density = 0.f;
		shapeDef.material.friction = 0.5f;
		shapeDef.enableContactEvents = true;
		b2CreatePolygonShape(body, &shapeDef, &block);
		push.bodyId = world->store(body, entity);
	}
	// Bodies follow their components from now on: added ones are created, removed ones destroyed.
	world->connect(ioScene.registry);
}

void PhysicCommand::destroy(scene::Scene& ioScene) {
	const auto world = std::exchange(ioScene.m_physicsWorld, nullptr);
	if (!world)
		return;
	world->disconnect(ioScene.registry);
	b2DestroyWorld(world->worldId);
	world->worldId = {.index1 = 0, .generation = 0};
	world->bodies.clear();
}

auto PhysicCommand::isInitialized(const scene::Scene& iScene) -> bool { return iScene.getPhysicsWorld() != nullptr; }

void PhysicCommand::frame(scene::Scene& ioScene, const core::Timestep& iTimestep) {
	OWL_PROFILE_FUNCTION()

	auto* const world = ioScene.getPhysicsWorld();
	if (world == nullptr) {
		logNotInitialized("frame");
		return;
	}
	auto& impl = *world;
	impl.createPendingBodies(ioScene);
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
		impl.writeTransforms(ioScene, impl.alpha);
}

auto PhysicCommand::getSettings(const scene::Scene& iScene) -> PhysicsSettings {
	const auto* const world = iScene.getPhysicsWorld();
	return world != nullptr ? world->settings : PhysicsSettings{};
}

auto PhysicCommand::getWorkerCount(const scene::Scene& iScene) -> uint32_t {
	const auto* const world = iScene.getPhysicsWorld();
	if (world == nullptr)
		return 0;
	return world->taskPool != nullptr ? world->taskPool->getWorkerCount() : 1;
}

auto PhysicCommand::getLastFrameStepCount(const scene::Scene& iScene) -> uint32_t {
	const auto* const world = iScene.getPhysicsWorld();
	return world != nullptr ? world->lastStepCount : 0;
}

auto PhysicCommand::getInterpolationAlpha(const scene::Scene& iScene) -> float {
	const auto* const world = iScene.getPhysicsWorld();
	return world != nullptr ? world->alpha : 1.f;
}

void PhysicCommand::syncSimulatedTransforms(scene::Scene& ioScene) {
	if (auto* const world = ioScene.getPhysicsWorld(); world != nullptr)
		world->writeTransforms(ioScene, 1.f);
}

auto PhysicCommand::takeCollisionEvents(scene::Scene& ioScene) -> std::vector<CollisionEvent> {
	auto* const world = ioScene.getPhysicsWorld();
	if (world == nullptr)
		return {};
	return std::exchange(world->collisionBegins, {});
}

void PhysicCommand::destroyBody(const scene::Entity& iEntity) {
	auto* const world = worldOf(iEntity);
	if (world == nullptr)
		return;
	auto& registry = iEntity.getScene()->registry;
	const auto handle = static_cast<entt::entity>(iEntity);
	if (iEntity.hasComponent<scene::component::PhysicBody>())
		world->onPhysicBodyRemoved(registry, handle);
	if (iEntity.hasComponent<scene::component::Tilemap>())
		world->onTilemapRemoved(registry, handle);
	if (iEntity.hasComponent<scene::component::RaycastDoor>())
		world->onDoorRemoved(registry, handle);
	if (iEntity.hasComponent<scene::component::RaycastPushWall>())
		world->onPushWallRemoved(registry, handle);
	world->forgetContacts(iEntity.getUUID());
}

void PhysicCommand::impulse(const scene::Entity& iEntity, const math::vec2f& iImpulse) {
	const auto* const world = runningWorld(iEntity, "impulse");
	if (world == nullptr || !iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type == scene::SceneBody::BodyType::Static)
		return;
	if (const auto handle = world->find(body.bodyId); handle)
		b2Body_ApplyLinearImpulseToCenter(*handle, {iImpulse.x(), iImpulse.y()}, true);
}

auto PhysicCommand::getVelocity(const scene::Entity& iEntity) -> math::vec2f {
	const auto* const world = runningWorld(iEntity, "getVelocity");
	if (world == nullptr || !iEntity.hasComponent<scene::component::PhysicBody>())
		return {0.0f, 0.0f};
	auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type == scene::SceneBody::BodyType::Static)
		return {0.0f, 0.0f};
	const auto handle = world->find(body.bodyId);
	if (!handle)
		return {0.0f, 0.0f};
	const auto [x, y] = b2Body_GetLinearVelocity(*handle);
	return {x, y};
}

void PhysicCommand::setTransform(const scene::Entity& iEntity, const math::vec2f& iPosition, const float iRotation) {
	auto* const world = runningWorld(iEntity, "setTransform");
	if (world == nullptr)
		return;
	// Explicit `PhysicBody` takes priority — that's the designer-authored body.
	if (iEntity.hasComponent<scene::component::PhysicBody>()) {
		auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
		if (const auto handle = world->find(body.bodyId); handle) {
			b2Body_SetTransform(*handle, {iPosition.x(), iPosition.y()}, b2MakeRot(iRotation));
			world->snapBody(body.bodyId);
		}
		return;
	}
	// Otherwise fall back to the auto-created kinematic body for raycast doors / pushwalls.
	uint64_t bodyId = 0;
	if (iEntity.hasComponent<scene::component::RaycastDoor>())
		bodyId = iEntity.getComponent<scene::component::RaycastDoor>().bodyId;
	else if (iEntity.hasComponent<scene::component::RaycastPushWall>())
		bodyId = iEntity.getComponent<scene::component::RaycastPushWall>().bodyId;
	if (const auto handle = world->find(bodyId); handle)
		b2Body_SetTransform(*handle, {iPosition.x(), iPosition.y()}, b2MakeRot(iRotation));
}

void PhysicCommand::setVelocity(const scene::Entity& iEntity, const math::vec2f& iVelocity) {
	const auto* const world = runningWorld(iEntity, "setVelocity");
	if (world == nullptr || !iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type == scene::SceneBody::BodyType::Static)
		return;
	if (const auto handle = world->find(body.bodyId); handle)
		b2Body_SetLinearVelocity(*handle, {iVelocity.x(), iVelocity.y()});
}

void PhysicCommand::setGravityScale(const scene::Entity& iEntity, const float iScale) {
	const auto* const world = runningWorld(iEntity, "setGravityScale");
	if (world == nullptr || !iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type != scene::SceneBody::BodyType::Dynamic)
		return;
	const auto handle = world->find(body.bodyId);
	if (!handle)
		return;
	b2Body_SetGravityScale(*handle, iScale);
	// Wake the body so the new scale takes effect immediately.
	b2Body_SetAwake(*handle, true);
}

auto PhysicCommand::getSnapshot(const scene::Entity& iEntity) -> PhysicsSnapshot {
	PhysicsSnapshot snapshot;
	const auto* const world = worldOf(iEntity);
	if (world == nullptr || !iEntity.hasComponent<scene::component::PhysicBody>())
		return snapshot;
	const auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type == scene::SceneBody::BodyType::Static)
		return snapshot;
	const auto handle = world->find(body.bodyId);
	if (!handle)
		return snapshot;
	const auto [vx, vy] = b2Body_GetLinearVelocity(*handle);
	snapshot.linearVelocity = {vx, vy};
	snapshot.angularVelocity = b2Body_GetAngularVelocity(*handle);
	snapshot.awake = b2Body_IsAwake(*handle);
	return snapshot;
}

void PhysicCommand::applySnapshot(const scene::Entity& iEntity, const PhysicsSnapshot& iSnapshot) {
	const auto* const world = worldOf(iEntity);
	if (world == nullptr || !iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	const auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type == scene::SceneBody::BodyType::Static)
		return;
	const auto handle = world->find(body.bodyId);
	if (!handle)
		return;
	b2Body_SetLinearVelocity(*handle, {iSnapshot.linearVelocity.x(), iSnapshot.linearVelocity.y()});
	b2Body_SetAngularVelocity(*handle, iSnapshot.angularVelocity);
	if (iSnapshot.awake)
		b2Body_SetAwake(*handle, true);
}

}// namespace owl::physics
