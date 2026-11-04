/**
 * @file PhysicCommand.cpp
 * @author Silmaen
 * @date 12/27/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "JoltWorld.h"
#include "SolverTaskPool.h"
#include "physics/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"
#include "scene/component/components.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <utility>

namespace owl::physics {

namespace {

// Jolt is built for SSE4.2 on x86_64: on an older CPU physics stays off instead of crashing on an illegal instruction.
auto isCpuUnsupported() -> bool {
#if defined(__x86_64__) || defined(_M_X64)
	static const bool unsupported = !__builtin_cpu_supports("sse4.2") || !__builtin_cpu_supports("popcnt");
	return unsupported;
#else
	return false;
#endif
}

inline void logNotInitialized(const char* iFunc) {
	if (isCpuUnsupported())
		return;
	OWL_CORE_WARN("Physics: {} called before initialization, ignored.", iFunc)
}

inline void logNullEntity(const char* iFunc) { OWL_CORE_WARN("Physics: {} called with a null entity, ignored.", iFunc) }

}// namespace


class PhysicsWorld {
public:
	// Body whose pose is copied to its entity each frame, with the last two step poses for interpolation.
	struct SyncedBody {
		entt::entity entity;
		uint64_t bodyId;
		uint32_t body;
		BodyPose previous;
		BodyPose current;
		// A 3D body blends these poses instead.
		bool space = false;
		SpacePose previousSpace;
		SpacePose currentSpace;
	};

	PhysicsWorld() = default;

	~PhysicsWorld() = default;

	PhysicsWorld(const PhysicsWorld&) = delete;

	auto operator=(const PhysicsWorld&) -> PhysicsWorld& = delete;

	PhysicsWorld(PhysicsWorld&&) = delete;

	auto operator=(PhysicsWorld&&) -> PhysicsWorld& = delete;

	[[nodiscard]] auto ownerOf(const uint32_t iBody) const -> std::optional<core::UUID> {
		if (const auto it = bodyOwners.find(iBody); it != bodyOwners.end())
			return it->second;
		return std::nullopt;
	}

	[[nodiscard]] auto pairKey(const BodyContact& iContact) const -> std::optional<std::pair<core::UUID, core::UUID>> {
		const auto ownerA = ownerOf(iContact.bodyA);
		const auto ownerB = ownerOf(iContact.bodyB);
		if (!ownerA || !ownerB || *ownerA == *ownerB)
			return std::nullopt;
		return std::pair{*ownerA, *ownerB};
	}

	void collectContactEvents() {
		const auto [begins, ends] = jolt->takeContacts();
		for (const auto& contact: begins) {
			const auto pair = pairKey(contact);
			if (!pair)
				continue;
			if (++touching[ordered(*pair)] == 1 && !isReportedThisFrame(*pair))
				collisionBegins.push_back({.entityA = pair->first, .entityB = pair->second});
		}
		for (const auto& contact: ends) {
			const auto pair = pairKey(contact);
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

	void trackBody(const entt::entity iEntity, const uint64_t iBodyId, const uint32_t iBody, const bool iSpace) {
		SyncedBody tracked{.entity = iEntity,
						   .bodyId = iBodyId,
						   .body = iBody,
						   .previous = {},
						   .current = {},
						   .space = iSpace,
						   .previousSpace = {},
						   .currentSpace = {}};
		if (iSpace)
			tracked.previousSpace = tracked.currentSpace = jolt->getSpacePose(iBody);
		else
			tracked.previous = tracked.current = jolt->getPose(iBody);
		syncedIndex[iBodyId] = synced.size();
		synced.push_back(tracked);
	}

	void capture(SyncedBody& ioTracked) const {
		if (ioTracked.space)
			ioTracked.currentSpace = jolt->getSpacePose(ioTracked.body);
		else
			ioTracked.current = jolt->getPose(ioTracked.body);
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
			if (tracked.space) {
				tracked.previousSpace = tracked.currentSpace = jolt->getSpacePose(tracked.body);
				return;
			}
			const BodyPose moved = jolt->getPose(tracked.body);
			// Carry the previous pose along with the teleport so the blend between steps goes on without a hitch.
			tracked.previous = JoltWorld::carryPrevious(tracked.previous, tracked.current, moved);
			tracked.current = moved;
		}
	}

	void capturePrevious() {
		for (auto& tracked: synced) {
			tracked.previous = tracked.current;
			tracked.previousSpace = tracked.currentSpace;
		}
	}

	void shiftOrigin(const math::vec2f& iShift) {
		for (const auto body: bodies | std::views::values)
			jolt->setPositionQuietly(body, jolt->getPose(body).position - iShift);
		const math::vec3f spaceShift{iShift.x(), iShift.y(), 0.f};
		for (auto& tracked: synced) {
			tracked.previous.position -= iShift;
			tracked.current.position -= iShift;
			tracked.previousSpace.position -= spaceShift;
			tracked.currentSpace.position -= spaceShift;
		}
	}

	// Below this many bodies the per-frame copies stay on the calling thread (dispatch costs more than it saves).
	static constexpr size_t parallelSyncMinBodies = 1024;
	static constexpr size_t parallelSyncMinRange = 256;

	void captureCurrent() {
		const auto copy = [this](const size_t iBegin, const size_t iEnd) -> void {
			for (size_t i = iBegin; i < iEnd; ++i) capture(synced[i]);
		};
		if (taskPool != nullptr && synced.size() >= parallelSyncMinBodies)
			taskPool->parallelFor(synced.size(), parallelSyncMinRange, copy);
		else
			copy(0, synced.size());
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
										  transform != nullptr)
										  writeRoot(transform->transform, tracked, iAlpha);
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
		if (iTracked.space)
			writeWorldSpacePose(ioScene, iTracked.entity, transform->transform,
								interpolatedSpacePose(iTracked, iAlpha));
		else
			writeWorldPose(ioScene, iTracked.entity, transform->transform, interpolatedPose(iTracked, iAlpha));
	}

	static void writeRoot(math::Transform& ioTransform, const SyncedBody& iTracked, const float iAlpha) {
		if (iTracked.space)
			writeRootSpacePose(ioTransform, interpolatedSpacePose(iTracked, iAlpha));
		else
			writeRootPose(ioTransform, interpolatedPose(iTracked, iAlpha));
	}

	[[nodiscard]] static auto interpolatedSpacePose(const SyncedBody& iTracked, const float iAlpha) -> SpacePose {
		return iAlpha >= 1.f ? iTracked.currentSpace
							 : JoltWorld::blend(iTracked.previousSpace, iTracked.currentSpace, iAlpha);
	}

	static void writeRootSpacePose(math::Transform& ioTransform, const SpacePose& iPose) {
		ioTransform.translation() = iPose.position;
		ioTransform.rotation() = JoltWorld::toEuler(iPose.rotation);
	}

	static void writeWorldSpacePose(scene::Scene& ioScene, const entt::entity iEntity, math::Transform& ioTransform,
									const SpacePose& iPose) {
		const auto& hierarchy = ioScene.registry.get<scene::component::Hierarchy>(iEntity);
		if (hierarchy.parentId != core::UUID{0}) {
			if (const scene::Entity parent = ioScene.findEntityByUUID(hierarchy.parentId); parent) {
				const math::Transform parentWorld = ioScene.getWorldTransform(parent);
				const math::vec4 localPos =
						math::inverse(parentWorld()) *
						math::vec4{iPose.position.x(), iPose.position.y(), iPose.position.z(), 1.0f};
				ioTransform.translation() = {localPos.x(), localPos.y(), localPos.z()};
				ioTransform.rotation() = JoltWorld::toLocalEuler(parentWorld.rotation(), iPose.rotation);
				return;
			}
		}
		writeRootSpacePose(ioTransform, iPose);
	}

	[[nodiscard]] static auto wrapAngle(const float iAngle) -> float {
		if (iAngle > std::numbers::pi_v<float>)
			return iAngle - 2.f * std::numbers::pi_v<float>;
		if (iAngle < -std::numbers::pi_v<float>)
			return iAngle + 2.f * std::numbers::pi_v<float>;
		return iAngle;
	}

	[[nodiscard]] static auto interpolatedPose(const SyncedBody& iTracked, const float iAlpha) -> BodyPose {
		if (iAlpha >= 1.f)
			return iTracked.current;
		const auto& [p0, a0] = iTracked.previous;
		const auto& [p1, a1] = iTracked.current;
		return {.position = p0 + (p1 - p0) * iAlpha, .angle = wrapAngle(a0 + wrapAngle(a1 - a0) * iAlpha)};
	}

	static void writeRootPose(math::Transform& ioTransform, const BodyPose& iPose) {
		ioTransform.translation().x() = iPose.position.x();
		ioTransform.translation().y() = iPose.position.y();
		ioTransform.rotation().z() = iPose.angle;
	}

	static void writeWorldPose(scene::Scene& ioScene, const entt::entity iEntity, math::Transform& ioTransform,
							   const BodyPose& iPose) {
		const auto& hierarchy = ioScene.registry.get<scene::component::Hierarchy>(iEntity);
		if (hierarchy.parentId != core::UUID{0}) {
			if (const scene::Entity parent = ioScene.findEntityByUUID(hierarchy.parentId); parent) {
				const math::Transform parentWorld = ioScene.getWorldTransform(parent);
				const math::vec4 localPos =
						math::inverse(parentWorld()) *
						math::vec4{iPose.position.x(), iPose.position.y(), ioTransform.translation().z(), 1.0f};
				ioTransform.translation().x() = localPos.x();
				ioTransform.translation().y() = localPos.y();
				ioTransform.rotation().z() = iPose.angle - parentWorld.rotation().z();
				return;
			}
		}
		writeRootPose(ioTransform, iPose);
	}

	void forgetContacts(const core::UUID iEntity) {
		std::erase_if(touching, [iEntity](const auto& iEntry) -> bool {
			return iEntry.first.first == iEntity || iEntry.first.second == iEntity;
		});
	}

	[[nodiscard]] auto find(const uint64_t iBodyId) const -> std::optional<uint32_t> {
		if (iBodyId == 0)
			return std::nullopt;
		if (const auto it = bodies.find(iBodyId); it != bodies.end())
			return it->second;
		return std::nullopt;
	}

	// Creates the body and names it for the components; 0 when Jolt refused it.
	auto create(const BodyDescription& iDescription, const scene::Entity& iOwner) -> uint64_t {
		const auto body = jolt->createBody(iDescription);
		if (!body)
			return 0;
		const uint64_t bodyId = nextId++;
		bodies[bodyId] = *body;
		bodyOwners[*body] = iOwner.getUUID();
		return bodyId;
	}

	// Reads no component, so it is safe inside a destruction signal.
	void release(uint64_t& ioBodyId) {
		if (ioBodyId == 0)
			return;
		if (const auto it = bodies.find(ioBodyId); it != bodies.end()) {
			if (const auto owner = bodyOwners.find(it->second); owner != bodyOwners.end()) {
				forgetContacts(owner->second);
				bodyOwners.erase(owner);
			}
			jolt->destroyBody(it->second);
			bodies.erase(it);
		}
		untrackBody(ioBodyId);
		ioBodyId = 0;
	}

	[[nodiscard]] static auto poseOf(const math::Transform& iWorldTransform) -> BodyPose {
		return {.position = {iWorldTransform.translation().x(), iWorldTransform.translation().y()},
				.angle = iWorldTransform.rotation().z()};
	}

	// A body Owl moves itself (raycast door or pushwall): kinematic, upright, one box.
	[[nodiscard]] static auto movedBlock(const math::Transform& iWorldTransform, const math::vec2f& iHalfExtents)
			-> BodyDescription {
		return {.type = scene::SceneBody::BodyType::Kinematic,
				.fixedRotation = true,
				.pose = poseOf(iWorldTransform),
				.spacePose = {},
				.boxes = {{.halfExtents = iHalfExtents}}};
	}

	void createPhysicBody(scene::Scene& ioScene, const entt::entity iEntity) {
		const scene::Entity entity{iEntity, &ioScene};
		auto& [sbody] = entity.getComponent<scene::component::PhysicBody>();
		const math::Transform worldTransform = ioScene.getWorldTransform(entity);
		const bool space = sbody.dimension == scene::SceneBody::Dimension::Space;
		const auto& scale = worldTransform.scale();
		BodyDescription description{.type = sbody.type,
									.fixedRotation = sbody.fixedRotation,
									.dimension = sbody.dimension,
									.pose = poseOf(worldTransform),
									.spacePose = {.position = worldTransform.translation(),
												  .rotation = JoltWorld::toRotation(worldTransform.rotation())},
									.boxes = {},
									.shape = sbody.shape,
									.size = {sbody.colliderSize.x() * scale.x(), sbody.colliderSize.y() * scale.y(),
											 sbody.colliderSize.z() * scale.z()},
									.density = sbody.density,
									.friction = sbody.friction,
									.restitution = sbody.restitution};
		if (!space && sbody.shape == scene::SceneBody::Shape::Box)
			description.boxes.push_back({.halfExtents = {sbody.colliderSize.x() * scale.x() * 0.5f,
														 sbody.colliderSize.y() * scale.y() * 0.5f}});
		sbody.bodyId = create(description, entity);
		if (sbody.bodyId != 0)
			trackBody(iEntity, sbody.bodyId, bodies[sbody.bodyId], space);
	}

	void createTilemapBody(scene::Scene& ioScene, const entt::entity iEntity) {
		const scene::Entity entity{iEntity, &ioScene};
		const auto& tilemap = entity.getComponent<scene::component::Tilemap>();
		if (!tilemap.asset || !tilemap.asset->tileset || tilemap.asset->layers.empty())
			return;
		const auto& assetData = *tilemap.asset;
		const auto& tileset = *assetData.tileset;
		const math::Transform worldTransform = ioScene.getWorldTransform(entity);
		const float cellSize = assetData.cellSize;
		const float originX = -static_cast<float>(assetData.width - 1) * 0.5f * cellSize;
		const float originY = static_cast<float>(assetData.height - 1) * 0.5f * cellSize;
		const math::vec2f half{cellSize * worldTransform.scale().x() * 0.5f,
							   cellSize * worldTransform.scale().y() * 0.5f};
		BodyDescription description{.type = scene::SceneBody::BodyType::Static,
									.pose = poseOf(worldTransform),
									.spacePose = {},
									.boxes = {}};
		for (const auto& layer: assetData.layers) {
			for (uint32_t y = 0; y < assetData.height; ++y) {
				for (uint32_t x = 0; x < assetData.width; ++x) {
					const int32_t tileIdx = layer.getTile(x, y);
					if (tileIdx < 0 || !tileset.isCollidable(static_cast<uint32_t>(tileIdx)))
						continue;
					description.boxes.push_back(
							{.halfExtents = half,
							 .offset = {(originX + static_cast<float>(x) * cellSize) * worldTransform.scale().x(),
										(originY - static_cast<float>(y) * cellSize) * worldTransform.scale().y()}});
				}
			}
		}
		if (description.boxes.empty())
			return;
		if (const uint64_t bodyId = create(description, entity); bodyId != 0)
			tilemapBodies[iEntity] = bodyId;
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

	uint64_t nextId = 1;
	std::unordered_map<uint64_t, uint32_t> bodies;
	std::unordered_map<entt::entity, uint64_t> tilemapBodies;
	std::vector<entt::entity> pendingBodies;
	std::unordered_map<uint32_t, core::UUID> bodyOwners;
	std::map<std::pair<uint64_t, uint64_t>, uint32_t> touching;
	std::vector<PhysicCommand::CollisionEvent> collisionBegins;
	size_t frameEventsStart = 0;

	std::vector<SyncedBody> synced;
	std::unordered_map<uint64_t, size_t> syncedIndex;
	PhysicsSettings settings;
	uint32_t stepsSinceWrite = 0;
	uint32_t lastStepCount = 0;
	float alpha = 1.f;
	// Declared before the Jolt world, which runs its jobs on it: destroyed after it.
	uniq<SolverTaskPool> taskPool;
	uniq<JoltWorld> jolt;
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

// The entity has a 3D PhysicBody.
auto isInSpace(const scene::Entity& iEntity) -> bool {
	return iEntity && iEntity.hasComponent<scene::component::PhysicBody>() &&
		   iEntity.getComponent<scene::component::PhysicBody>().body.dimension == scene::SceneBody::Dimension::Space;
}

// The Jolt body of an entity's PhysicBody, when it is not static.
auto movingBody(const PhysicsWorld& iWorld, const scene::Entity& iEntity) -> std::optional<uint32_t> {
	if (!iEntity.hasComponent<scene::component::PhysicBody>())
		return std::nullopt;
	const auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type == scene::SceneBody::BodyType::Static)
		return std::nullopt;
	return iWorld.find(body.bodyId);
}

}// namespace

void PhysicCommand::init(scene::Scene& ioScene) {
	if (isInitialized(ioScene))
		destroy(ioScene);
	if (isCpuUnsupported()) {
		OWL_CORE_WARN("Physics: Jolt is built with SSE4.2, which this CPU lacks; physics disabled.")
		return;
	}
	const auto world = mkShared<PhysicsWorld>();
	ioScene.m_physicsWorld = world;
	world->settings = ioScene.getPhysicsSettings().clamped();
	uint32_t dynamicBodies = 0;
	for (const auto [e, body]: ioScene.registry.view<scene::component::PhysicBody>().each())
		if (body.body.type == scene::SceneBody::BodyType::Dynamic)
			++dynamicBodies;
	if (const uint32_t workers = world->settings.getEffectiveWorkerCount(dynamicBodies); workers > 1)
		world->taskPool = mkUniq<SolverTaskPool>(workers);
	world->jolt = mkUniq<JoltWorld>(world->taskPool.get());
	OWL_CORE_DEBUG("Physics: World created ({} workers).", getWorkerCount(ioScene))

	for (const auto e: ioScene.registry.view<scene::component::PhysicBody, scene::component::Transform>())
		world->createPhysicBody(ioScene, e);
	for (const auto e: ioScene.registry.view<scene::component::Tilemap, scene::component::Transform>())
		world->createTilemapBody(ioScene, e);
	for (const auto view = ioScene.registry.view<scene::component::RaycastDoor, scene::component::Transform>();
		 const auto e: view) {
		const scene::Entity entity{e, &ioScene};
		if (entity.hasComponent<scene::component::PhysicBody>())
			continue;// designer-managed body, leave it alone
		auto& door = view.get<scene::component::RaycastDoor>(e);
		using OD = scene::component::RaycastDoor::OpeningDirection;
		const bool slideAlongY = (door.openingDirection == OD::North || door.openingDirection == OD::South);
		constexpr float kPlateHalfThickness = 0.05f;// matches the renderer's lateral bias
		const math::vec2f half{slideAlongY ? kPlateHalfThickness : 0.5f, slideAlongY ? 0.5f : kPlateHalfThickness};
		door.bodyId = world->create(PhysicsWorld::movedBlock(ioScene.getWorldTransform(entity), half), entity);
	}
	for (const auto view = ioScene.registry.view<scene::component::RaycastPushWall, scene::component::Transform>();
		 const auto e: view) {
		const scene::Entity entity{e, &ioScene};
		if (entity.hasComponent<scene::component::PhysicBody>())
			continue;
		// Pushwall footprint: full 1×1 block — same as the rendered cube.
		view.get<scene::component::RaycastPushWall>(e).bodyId =
				world->create(PhysicsWorld::movedBlock(ioScene.getWorldTransform(entity), {0.5f, 0.5f}), entity);
	}
	// Bodies follow their components from now on: added ones are created, removed ones destroyed.
	world->connect(ioScene.registry);
}

void PhysicCommand::destroy(scene::Scene& ioScene) {
	const auto world = std::exchange(ioScene.m_physicsWorld, nullptr);
	if (!world)
		return;
	world->disconnect(ioScene.registry);
	world->jolt.reset();
	world->bodies.clear();
}

auto PhysicCommand::isInitialized(const scene::Scene& iScene) -> bool { return iScene.getPhysicsWorld() != nullptr; }

void PhysicCommand::step(scene::Scene& ioScene) {
	OWL_PROFILE_FUNCTION()

	auto* const world = ioScene.getPhysicsWorld();
	if (world == nullptr) {
		logNotInitialized("step");
		return;
	}
	auto& impl = *world;
	impl.createPendingBodies(ioScene);
	if (impl.settings.interpolate)
		impl.capturePrevious();
	impl.jolt->step(static_cast<float>(ioScene.getSimulationClock().getStepSeconds()), impl.settings.solverSubSteps);
	impl.collectContactEvents();
	impl.captureCurrent();
	++impl.stepsSinceWrite;
}

void PhysicCommand::writeFrameTransforms(scene::Scene& ioScene) {
	auto* const world = ioScene.getPhysicsWorld();
	if (world == nullptr) {
		logNotInitialized("writeFrameTransforms");
		return;
	}
	auto& impl = *world;
	impl.lastStepCount = std::exchange(impl.stepsSinceWrite, 0);
	impl.alpha = impl.settings.interpolate ? ioScene.getSimulationClock().getAlpha() : 1.f;
	if (impl.lastStepCount > 0 || impl.settings.interpolate)
		impl.writeTransforms(ioScene, impl.alpha);
}

void PhysicCommand::frame(scene::Scene& ioScene, const core::Timestep& iTimestep) {
	OWL_PROFILE_FUNCTION()

	auto* const world = ioScene.getPhysicsWorld();
	if (world == nullptr) {
		logNotInitialized("frame");
		return;
	}
	world->createPendingBodies(ioScene);
	world->frameEventsStart = world->collisionBegins.size();
	const uint32_t steps = ioScene.getSimulationClock().advance(iTimestep);
	for (uint32_t i = 0; i < steps; ++i) step(ioScene);
	writeFrameTransforms(ioScene);
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

void PhysicCommand::shiftOrigin(scene::Scene& ioScene, const math::vec2f& iShift) {
	OWL_PROFILE_FUNCTION()

	if (auto* const world = ioScene.getPhysicsWorld(); world != nullptr)
		world->shiftOrigin(iShift);
}

auto PhysicCommand::takeCollisionEvents(scene::Scene& ioScene) -> std::vector<CollisionEvent> {
	auto* const world = ioScene.getPhysicsWorld();
	if (world == nullptr)
		return {};
	world->frameEventsStart = 0;
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
	if (world == nullptr)
		return;
	if (const auto body = movingBody(*world, iEntity); body)
		world->jolt->addImpulse(*body, iImpulse);
}

auto PhysicCommand::getVelocity(const scene::Entity& iEntity) -> math::vec2f {
	const auto* const world = runningWorld(iEntity, "getVelocity");
	if (world == nullptr)
		return {0.0f, 0.0f};
	const auto body = movingBody(*world, iEntity);
	return body ? world->jolt->getLinearVelocity(*body) : math::vec2f{0.0f, 0.0f};
}

void PhysicCommand::setTransform(const scene::Entity& iEntity, const math::vec2f& iPosition, const float iRotation) {
	auto* const world = runningWorld(iEntity, "setTransform");
	if (world == nullptr)
		return;
	const BodyPose pose{.position = iPosition, .angle = iRotation};
	// Explicit `PhysicBody` takes priority — that's the designer-authored body.
	if (iEntity.hasComponent<scene::component::PhysicBody>()) {
		auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
		if (const auto handle = world->find(body.bodyId); handle) {
			if (body.dimension == scene::SceneBody::Dimension::Space) {
				auto spacePose = world->jolt->getSpacePose(*handle);
				auto euler = JoltWorld::toEuler(spacePose.rotation);
				euler.z() = iRotation;
				spacePose.position = {iPosition.x(), iPosition.y(), spacePose.position.z()};
				spacePose.rotation = JoltWorld::toRotation(euler);
				world->jolt->setSpacePose(*handle, spacePose);
			} else
				world->jolt->setPose(*handle, pose);
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
		world->jolt->setPose(*handle, pose);
}

void PhysicCommand::setVelocity(const scene::Entity& iEntity, const math::vec2f& iVelocity) {
	const auto* const world = runningWorld(iEntity, "setVelocity");
	if (world == nullptr)
		return;
	const auto body = movingBody(*world, iEntity);
	if (!body)
		return;
	if (isInSpace(iEntity))
		world->jolt->setSpaceVelocity(*body, {iVelocity.x(), iVelocity.y(), world->jolt->getSpaceVelocity(*body).z()});
	else
		world->jolt->setLinearVelocity(*body, iVelocity);
}

void PhysicCommand::impulse3D(const scene::Entity& iEntity, const math::vec3f& iImpulse) {
	const auto* const world = runningWorld(iEntity, "impulse3D");
	if (world == nullptr)
		return;
	if (const auto body = movingBody(*world, iEntity); body)
		world->jolt->addSpaceImpulse(*body,
									 isInSpace(iEntity) ? iImpulse : math::vec3f{iImpulse.x(), iImpulse.y(), 0.f});
}

auto PhysicCommand::getVelocity3D(const scene::Entity& iEntity) -> math::vec3f {
	const auto* const world = runningWorld(iEntity, "getVelocity3D");
	if (world == nullptr)
		return {0.f, 0.f, 0.f};
	const auto body = movingBody(*world, iEntity);
	return body ? world->jolt->getSpaceVelocity(*body) : math::vec3f{0.f, 0.f, 0.f};
}

void PhysicCommand::setVelocity3D(const scene::Entity& iEntity, const math::vec3f& iVelocity) {
	const auto* const world = runningWorld(iEntity, "setVelocity3D");
	if (world == nullptr)
		return;
	if (const auto body = movingBody(*world, iEntity); body)
		world->jolt->setSpaceVelocity(*body,
									  isInSpace(iEntity) ? iVelocity : math::vec3f{iVelocity.x(), iVelocity.y(), 0.f});
}

void PhysicCommand::setTransform3D(const scene::Entity& iEntity, const math::vec3f& iPosition,
								   const math::vec3f& iRotation) {
	if (!isInSpace(iEntity)) {
		setTransform(iEntity, {iPosition.x(), iPosition.y()}, iRotation.z());
		return;
	}
	auto* const world = runningWorld(iEntity, "setTransform3D");
	if (world == nullptr)
		return;
	const auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (const auto handle = world->find(body.bodyId); handle) {
		world->jolt->setSpacePose(*handle, {.position = iPosition, .rotation = JoltWorld::toRotation(iRotation)});
		world->snapBody(body.bodyId);
	}
}

void PhysicCommand::setGravityScale(const scene::Entity& iEntity, const float iScale) {
	const auto* const world = runningWorld(iEntity, "setGravityScale");
	if (world == nullptr || !iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	if (iEntity.getComponent<scene::component::PhysicBody>().body.type != scene::SceneBody::BodyType::Dynamic)
		return;
	if (const auto body = movingBody(*world, iEntity); body)
		world->jolt->setGravityFactor(*body, iScale);
}

auto PhysicCommand::getSnapshot(const scene::Entity& iEntity) -> PhysicsSnapshot {
	PhysicsSnapshot snapshot;
	const auto* const world = worldOf(iEntity);
	if (world == nullptr)
		return snapshot;
	const auto body = movingBody(*world, iEntity);
	if (!body)
		return snapshot;
	snapshot.linearVelocity = world->jolt->getLinearVelocity(*body);
	snapshot.angularVelocity = world->jolt->getAngularVelocity(*body);
	snapshot.awake = world->jolt->isAwake(*body);
	return snapshot;
}

void PhysicCommand::applySnapshot(const scene::Entity& iEntity, const PhysicsSnapshot& iSnapshot) {
	const auto* const world = worldOf(iEntity);
	if (world == nullptr)
		return;
	const auto body = movingBody(*world, iEntity);
	if (!body)
		return;
	world->jolt->setLinearVelocity(*body, iSnapshot.linearVelocity);
	world->jolt->setAngularVelocity(*body, iSnapshot.angularVelocity);
	if (iSnapshot.awake)
		world->jolt->wake(*body);
}

}// namespace owl::physics
