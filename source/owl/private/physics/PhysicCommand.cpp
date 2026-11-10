/**
 * @file PhysicCommand.cpp
 * @author Silmaen
 * @date 12/27/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "GravityField.h"
#include "JoltWorld.h"
#include "SolverTaskPool.h"
#include "app/EngineContext.h"
#include "physics/PhysicCommand.h"
#include "renderer/MeshCache.h"
#include "scene/Entity.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"
#include "scene/component/components.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <numbers>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

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

	// A contact between two sub shapes, and whether it sleeps.
	struct TrackedContact {
		std::pair<core::UUID, core::UUID> pair;
		bool dormant = false;
		uint32_t awakeSteps = 0;
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

	[[nodiscard]] auto isSensorContact(const BodyContact& iContact) const -> bool {
		return sensorBodies.contains(iContact.bodyA) || sensorBodies.contains(iContact.bodyB);
	}

	// Jolt drops the contacts of a body that falls asleep: they stay, dormant, until it wakes up elsewhere.
	void collectContactEvents() {
		const auto [begins, ends] = jolt->takeContacts();
		for (const auto& contact: begins) {
			if (const auto it = contacts.find(contact); it != contacts.end()) {
				it->second.dormant = false;
				it->second.awakeSteps = 0;
				unlinkDormant(contact);
				continue;
			}
			const auto pair = pairKey(contact);
			if (!pair)
				continue;
			contacts.emplace(contact, TrackedContact{.pair = *pair});
			if (++touching[ordered(*pair)] == 1)
				reportBegin(*pair, isSensorContact(contact));
		}
		for (const auto& contact: ends) {
			const auto it = contacts.find(contact);
			if (it == contacts.end())
				continue;
			if (!jolt->isAwake(contact.bodyA) && !jolt->isAwake(contact.bodyB)) {
				it->second = {.pair = it->second.pair, .dormant = true, .awakeSteps = 0};
				linkDormant(contact);
				continue;
			}
			removeContact(it);
		}
		for (const uint32_t body: jolt->takeActivatedBodies()) {
			const auto [first, last] = dormantByBody.equal_range(body);
			for (auto it = first; it != last; ++it) waking.insert(it->second);
		}
		// A woken body touches again within a step or two when it did not move away.
		std::vector<BodyContact> ended;
		for (const auto& contact: waking) {
			const auto tracked = contacts.find(contact);
			if (tracked != contacts.end() && (jolt->isAwake(contact.bodyA) || jolt->isAwake(contact.bodyB)) &&
				++tracked->second.awakeSteps >= 2)
				ended.push_back(contact);
		}
		for (const auto& contact: ended)
			if (const auto it = contacts.find(contact); it != contacts.end())
				removeContact(it);
	}

	// A sleeping contact is listed under both its bodies, to be found again when either one wakes up.
	void linkDormant(const BodyContact& iContact) {
		dormantByBody.emplace(iContact.bodyA, iContact);
		dormantByBody.emplace(iContact.bodyB, iContact);
	}

	void unlinkDormant(const BodyContact& iContact) {
		for (const uint32_t body: {iContact.bodyA, iContact.bodyB}) {
			const auto [first, last] = dormantByBody.equal_range(body);
			if (const auto it = std::ranges::find_if(
						first, last, [&iContact](const auto& iEntry) -> bool { return iEntry.second == iContact; });
				it != last)
				dormantByBody.erase(it);
		}
		waking.erase(iContact);
	}

	void removeContact(const std::map<BodyContact, TrackedContact>::iterator iContact) {
		const auto pair = iContact->second.pair;
		const bool sensor = isSensorContact(iContact->first);
		if (iContact->second.dormant)
			unlinkDormant(iContact->first);
		contacts.erase(iContact);
		if (const auto it = touching.find(ordered(pair)); it != touching.end() && --it->second == 0) {
			touching.erase(it);
			collisionEvents.push_back({.entityA = pair.first, .entityB = pair.second, .ended = true, .sensor = sensor});
		}
	}


	// A pair that ended earlier in the frame and touches again only jittered: neither change is reported.
	void reportBegin(const std::pair<core::UUID, core::UUID>& iPair, const bool iSensor) {
		const auto key = ordered(iPair);
		for (size_t i = collisionEvents.size(); i > frameEventsStart; --i) {
			const auto& event = collisionEvents[i - 1];
			if (ordered({event.entityA, event.entityB}) != key)
				continue;
			if (event.ended) {
				collisionEvents.erase(std::next(collisionEvents.begin(), static_cast<std::ptrdiff_t>(i - 1)));
				return;
			}
			break;
		}
		collisionEvents.push_back({.entityA = iPair.first, .entityB = iPair.second, .ended = false, .sensor = iSensor});
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

	// The partners of a body that goes away stop touching it.
	void forgetContacts(const core::UUID iEntity, const bool iSensor) {
		std::erase_if(contacts, [this, iEntity](const auto& iEntry) -> bool {
			if (iEntry.second.pair.first != iEntity && iEntry.second.pair.second != iEntity)
				return false;
			if (iEntry.second.dormant)
				unlinkDormant(iEntry.first);
			return true;
		});
		const auto entity = static_cast<uint64_t>(iEntity);
		std::erase_if(touching, [this, entity, iEntity, iSensor](const auto& iEntry) -> bool {
			const auto& [first, second] = iEntry.first;
			if (first != entity && second != entity)
				return false;
			const core::UUID other{first == entity ? second : first};
			collisionEvents.push_back({.entityA = iEntity,
									   .entityB = other,
									   .ended = true,
									   .sensor = iSensor || sensorOwners.contains(static_cast<uint64_t>(other))});
			return true;
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
		if (iDescription.sensor) {
			sensorBodies.insert(*body);
			sensorOwners.insert(static_cast<uint64_t>(iOwner.getUUID()));
		}
		return bodyId;
	}

	// Reads no component, so it is safe inside a destruction signal.
	void release(uint64_t& ioBodyId) {
		if (ioBodyId == 0)
			return;
		if (const auto it = bodies.find(ioBodyId); it != bodies.end()) {
			if (const auto owner = bodyOwners.find(it->second); owner != bodyOwners.end()) {
				const bool sensor = sensorBodies.erase(it->second) > 0;
				if (sensor)
					sensorOwners.erase(static_cast<uint64_t>(owner->second));
				forgetContacts(owner->second, sensor);
				bodyOwners.erase(owner);
			}
			heldBodies.erase(it->second);
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
				.boxes = {{.halfExtents = iHalfExtents}},
				.meshVertices = {},
				.meshIndices = {},
				.parts = {}};
	}

	// The collider of an entity's PhysicBody at its world pose, without its parts.
	[[nodiscard]] static auto describe(scene::Scene& ioScene, const scene::Entity& iEntity) -> BodyDescription {
		const auto& sbody = iEntity.getComponent<scene::component::PhysicBody>().body;
		const math::Transform worldTransform = ioScene.getWorldTransform(iEntity);
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
									.restitution = sbody.restitution,
									.sensor = sbody.sensor,
									.layer = sbody.layer,
									.mask = sbody.mask,
									.meshVertices = {},
									.meshIndices = {},
									.parts = {}};
		if (sbody.shape == scene::SceneBody::Shape::ConvexHull || sbody.shape == scene::SceneBody::Shape::Mesh)
			readMesh(ioScene, iEntity, scale, description);
		if (!space && sbody.shape == scene::SceneBody::Shape::Box)
			description.boxes.push_back({.halfExtents = {sbody.colliderSize.x() * scale.x() * 0.5f,
														 sbody.colliderSize.y() * scale.y() * 0.5f}});
		return description;
	}

	void createPhysicBody(scene::Scene& ioScene, const entt::entity iEntity) {
		const scene::Entity entity{iEntity, &ioScene};
		auto& [sbody] = entity.getComponent<scene::component::PhysicBody>();
		if (sbody.part) {
			if (const auto root = assemblyRootOf(ioScene, entity); root) {
				joinPart(ioScene, iEntity, *root);
				return;
			}
			OWL_CORE_WARN("Physics: Part '{}' has no ancestor body to join, it gets a body of its own.",
						  entity.getName())
		}
		const bool space = sbody.dimension == scene::SceneBody::Dimension::Space;
		sbody.bodyId = create(describe(ioScene, entity), entity);
		if (sbody.bodyId == 0)
			return;
		trackBody(iEntity, sbody.bodyId, bodies[sbody.bodyId], space);
		if (sbody.type == scene::SceneBody::BodyType::Dynamic && (sbody.gravityScale < 1.f || sbody.gravityScale > 1.f))
			jolt->setGravityFactor(bodies[sbody.bodyId], sbody.gravityScale);
		if (const auto it = partsOf.find(iEntity); it != partsOf.end() && !it->second.empty())
			rebuildAssembly(ioScene, iEntity);
	}

	// The nearest ancestor holding a body of its own and of the same dimension: the body a part merges into.
	[[nodiscard]] static auto assemblyRootOf(const scene::Scene& iScene, const scene::Entity& iPart)
			-> std::optional<entt::entity> {
		const auto dimension = iPart.getComponent<scene::component::PhysicBody>().body.dimension;
		core::UUID parentId = iPart.getComponent<scene::component::Hierarchy>().parentId;
		while (parentId != core::UUID{0}) {
			const scene::Entity parent = iScene.findEntityByUUID(parentId);
			if (!parent)
				return std::nullopt;
			if (const auto* body =
						iScene.registry.try_get<scene::component::PhysicBody>(static_cast<entt::entity>(parent));
				body != nullptr && !body->body.part)
				return body->body.dimension == dimension ? std::optional{static_cast<entt::entity>(parent)}
														 : std::nullopt;
			parentId = parent.getComponent<scene::component::Hierarchy>().parentId;
		}
		return std::nullopt;
	}

	// Merges a part into the body of its root (created first when the root waits for its body).
	void joinPart(scene::Scene& ioScene, const entt::entity iPart, const entt::entity iRoot) {
		partRoot[iPart] = iRoot;
		auto& parts = partsOf[iRoot];
		const auto uuidOf = [&ioScene](const entt::entity iEntity) -> uint64_t {
			return static_cast<uint64_t>(scene::Entity{iEntity, &ioScene}.getUUID());
		};
		// Parts in UUID order, so the collider is the same whatever order they joined in.
		parts.insert(std::ranges::upper_bound(parts, uuidOf(iPart), {}, uuidOf), iPart);
		if (ioScene.registry.get<scene::component::PhysicBody>(iRoot).body.bodyId != 0)
			rebuildAssembly(ioScene, iRoot);
	}

	// Takes a part out of its assembly; with `iOwnBody`, the part flies on in a body of its own at the speed of its point.
	void leavePart(scene::Scene& ioScene, const entt::entity iPart, const bool iOwnBody) {
		const auto it = partRoot.find(iPart);
		if (it == partRoot.end())
			return;
		const entt::entity root = it->second;
		partRoot.erase(it);
		if (const auto parts = partsOf.find(root); parts != partsOf.end()) {
			std::erase(parts->second, iPart);
			if (parts->second.empty())
				partsOf.erase(parts);
		}
		const auto* rootBody = ioScene.registry.try_get<scene::component::PhysicBody>(root);
		const auto rootJolt = rootBody != nullptr ? find(rootBody->body.bodyId) : std::nullopt;
		if (rootJolt)
			rebuildAssembly(ioScene, root);
		if (!iOwnBody)
			return;
		const scene::Entity part{iPart, &ioScene};
		auto& sbody = part.getComponent<scene::component::PhysicBody>().body;
		sbody.part = false;
		createPhysicBody(ioScene, iPart);
		const auto own = find(sbody.bodyId);
		if (!own || !rootJolt || sbody.type != scene::SceneBody::BodyType::Dynamic)
			return;
		const math::vec3f point = ioScene.getWorldTransform(part).translation();
		const math::vec3f velocity = jolt->getPointVelocity(*rootJolt, point);
		if (sbody.dimension == scene::SceneBody::Dimension::Space) {
			jolt->setSpaceVelocity(*own, velocity);
			jolt->setSpaceAngularVelocity(*own, jolt->getSpaceAngularVelocity(*rootJolt));
		} else {
			jolt->setLinearVelocity(*own, {velocity.x(), velocity.y()});
			jolt->setAngularVelocity(*own, jolt->getAngularVelocity(*rootJolt));
		}
	}

	// The collider of a root rebuilt with its parts placed relative to it.
	void rebuildAssembly(scene::Scene& ioScene, const entt::entity iRoot) {
		const scene::Entity root{iRoot, &ioScene};
		const auto body = find(root.getComponent<scene::component::PhysicBody>().body.bodyId);
		if (!body)
			return;
		BodyDescription description = describe(ioScene, root);
		const math::Transform rootWorld = ioScene.getWorldTransform(root);
		const math::Transform rootFrame{rootWorld.translation(), rootWorld.rotation()};
		const math::mat4 toRoot = math::inverse(rootFrame());
		if (const auto it = partsOf.find(iRoot); it != partsOf.end()) {
			for (const auto entity: it->second) {
				const scene::Entity part{entity, &ioScene};
				BodyDescription shape = describe(ioScene, part);
				shape.type = description.type;
				const math::Transform partWorld = ioScene.getWorldTransform(part);
				const math::vec4 local = toRoot * math::vec4{partWorld.translation().x(), partWorld.translation().y(),
															 partWorld.translation().z(), 1.f};
				shape.pose = {.position = {local.x(), local.y()},
							  .angle = partWorld.rotation().z() - rootWorld.rotation().z()};
				shape.spacePose = {.position = {local.x(), local.y(), local.z()},
								   .rotation = JoltWorld::toRotation(JoltWorld::toLocalEuler(
										   rootWorld.rotation(), JoltWorld::toRotation(partWorld.rotation())))};
				description.parts.push_back(std::move(shape));
			}
		}
		std::ignore = jolt->setShape(*body, description);
	}

	// The body an entity moves: its own, or the one of the assembly it is a part of.
	[[nodiscard]] auto bodyOf(const scene::Scene& iScene, const entt::entity iEntity) const -> std::optional<uint32_t> {
		const auto it = partRoot.find(iEntity);
		const entt::entity owner = it != partRoot.end() ? it->second : iEntity;
		const auto* body = iScene.registry.try_get<scene::component::PhysicBody>(owner);
		return body != nullptr ? find(body->body.bodyId) : std::nullopt;
	}

	// Measures the load of the breakable joints after a step; a joint over its threshold breaks.
	void breakOverloadedJoints(scene::Scene& ioScene, const float iSubStepSeconds) {
		for (auto it = breakableJoints.begin(); it != breakableJoints.end();) {
			auto* component = ioScene.registry.try_get<scene::component::PhysicJoint>(it->second);
			if (component == nullptr || component->joint.jointId != it->first) {
				it = breakableJoints.erase(it);
				continue;
			}
			auto& sjoint = component->joint;
			const auto [force, torque] = jolt->getJointLoad(it->first, iSubStepSeconds);
			if ((sjoint.breakForce <= 0.f || force <= sjoint.breakForce) &&
				(sjoint.breakTorque <= 0.f || torque <= sjoint.breakTorque)) {
				++it;
				continue;
			}
			jolt->destroyJoint(it->first);
			sjoint.jointId = 0;
			jointBreaks.push_back({.entity = scene::Entity{it->second, &ioScene}.getUUID(),
								   .connected = sjoint.connected,
								   .force = force,
								   .torque = torque});
			it = breakableJoints.erase(it);
		}
	}

	// The collider of a mesh shape is the mesh of the entity's MeshRenderer, scaled like the entity.
	static void readMesh(const scene::Scene& iScene, const scene::Entity& iEntity, const math::vec3f& iScale,
						 BodyDescription& ioDescription) {
		const auto* renderer = iEntity.hasComponent<scene::component::MeshRenderer>()
									   ? &iEntity.getComponent<scene::component::MeshRenderer>()
									   : nullptr;
		shared<renderer::MeshResource> mesh = renderer != nullptr ? renderer->mesh : nullptr;
		if (mesh == nullptr && renderer != nullptr && iScene.getEngineContext() != nullptr)
			mesh = iScene.getEngineContext()->getMeshCache().get(renderer->meshPath);
		if (mesh == nullptr || mesh->getGeometry().isEmpty()) {
			OWL_CORE_WARN("Physics: Entity '{}' has a mesh collider but no mesh to read, it gets a box.",
						  iEntity.getName())
			ioDescription.shape = scene::SceneBody::Shape::Box;
			return;
		}
		const auto& geometry = mesh->getGeometry();
		ioDescription.meshVertices.reserve(geometry.vertices.size());
		for (const auto& vertex: geometry.vertices)
			ioDescription.meshVertices.emplace_back(vertex.position.x() * iScale.x(), vertex.position.y() * iScale.y(),
													vertex.position.z() * iScale.z());
		ioDescription.meshIndices = geometry.indices;
	}

	// Created once both bodies exist; a joint whose connected entity has no body is dropped.
	void createPendingJoints(scene::Scene& ioScene) {
		if (pendingJoints.empty())
			return;
		std::vector<entt::entity> waiting;
		for (const auto entity: std::exchange(pendingJoints, {})) {
			auto* joint = ioScene.registry.try_get<scene::component::PhysicJoint>(entity);
			const auto* body = ioScene.registry.try_get<scene::component::PhysicBody>(entity);
			if (joint == nullptr || joint->joint.jointId != 0)
				continue;
			if (body == nullptr) {
				OWL_CORE_WARN("Physics: Joint on an entity without a PhysicBody, not created.")
				continue;
			}
			if (body->body.bodyId == 0 && std::ranges::contains(pendingBodies, entity)) {
				waiting.push_back(entity);
				continue;
			}
			switch (buildJoint(ioScene, {entity, &ioScene}, *joint)) {
				case JointBuild::Built:
				case JointBuild::Failed:
					break;
				case JointBuild::Waiting:
					waiting.push_back(entity);
					break;
			}
		}
		pendingJoints = std::move(waiting);
	}

	enum struct JointBuild : uint8_t { Built, Waiting, Failed };

	auto buildJoint(scene::Scene& ioScene, const scene::Entity& iEntity, scene::component::PhysicJoint& ioJoint)
			-> JointBuild {
		const auto& sjoint = ioJoint.joint;
		const auto self = bodyOf(ioScene, static_cast<entt::entity>(iEntity));
		if (!self)
			return JointBuild::Failed;
		const math::Transform selfWorld = ioScene.getWorldTransform(iEntity);
		const bool plane = iEntity.getComponent<scene::component::PhysicBody>().body.dimension ==
						   scene::SceneBody::Dimension::Plane;
		JointDescription description{.type = sjoint.type,
									 .body = *self,
									 .other = std::nullopt,
									 .point = worldPoint(selfWorld, sjoint.anchor),
									 .otherPoint = sjoint.connectedAnchor,
									 .axis = worldAxis(selfWorld, sjoint.axis, plane, sjoint.type),
									 .limited = sjoint.limited,
									 .lower = sjoint.lowerLimit,
									 .upper = sjoint.upperLimit,
									 .motor = sjoint.motor,
									 .motorSpeed = sjoint.motorSpeed,
									 .motorForce = sjoint.motorForce,
									 .springFrequency = sjoint.springFrequency,
									 .springDamping = sjoint.springDamping};
		if (sjoint.connected != core::UUID{0}) {
			const scene::Entity other = ioScene.findEntityByUUID(sjoint.connected);
			const auto* otherBody = other && other.hasComponent<scene::component::PhysicBody>()
											? &other.getComponent<scene::component::PhysicBody>()
											: nullptr;
			if (otherBody == nullptr) {
				OWL_CORE_WARN("Physics: Joint of '{}' connects an entity without a PhysicBody, not created.",
							  iEntity.getName())
				return JointBuild::Failed;
			}
			description.other = bodyOf(ioScene, static_cast<entt::entity>(other));
			if (!description.other)
				return std::ranges::contains(pendingBodies, static_cast<entt::entity>(other)) ? JointBuild::Waiting
																							  : JointBuild::Failed;
			description.otherPoint = worldPoint(ioScene.getWorldTransform(other), sjoint.connectedAnchor);
		}
		const auto id = jolt->createJoint(description);
		if (!id)
			return JointBuild::Failed;
		ioJoint.joint.jointId = *id;
		if (sjoint.breakForce > 0.f || sjoint.breakTorque > 0.f)
			breakableJoints[*id] = static_cast<entt::entity>(iEntity);
		return JointBuild::Built;
	}

	[[nodiscard]] static auto worldPoint(const math::Transform& iWorld, const math::vec3f& iLocal) -> math::vec3f {
		const math::vec4 point = iWorld() * math::vec4{iLocal.x(), iLocal.y(), iLocal.z(), 1.f};
		return {point.x(), point.y(), point.z()};
	}

	// A 2D hinge turns around Z and a 2D slider moves in the plane.
	[[nodiscard]] static auto worldAxis(const math::Transform& iWorld, const math::vec3f& iLocal, const bool iPlane,
										const scene::SceneJoint::Type iType) -> math::vec3f {
		if (iPlane && iType == scene::SceneJoint::Type::Hinge)
			return {0.f, 0.f, 1.f};
		const math::vec4 axis = iWorld() * math::vec4{iLocal.x(), iLocal.y(), iLocal.z(), 0.f};
		return {axis.x(), axis.y(), iPlane ? 0.f : axis.z()};
	}

	void onPhysicJointAdded(entt::registry& ioRegistry, const entt::entity iEntity) {
		ioRegistry.get<scene::component::PhysicJoint>(iEntity).joint.jointId = 0;
		pendingJoints.push_back(iEntity);
	}

	void onPhysicJointRemoved(entt::registry& ioRegistry, const entt::entity iEntity) {
		std::erase(pendingJoints, iEntity);
		auto& jointId = ioRegistry.get<scene::component::PhysicJoint>(iEntity).joint.jointId;
		if (jointId != 0) {
			breakableJoints.erase(static_cast<uint32_t>(jointId));
			jolt->destroyJoint(static_cast<uint32_t>(std::exchange(jointId, 0)));
		}
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
									.boxes = {},
									.meshVertices = {},
									.meshIndices = {},
									.parts = {}};
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

	// Created a frame late, once the entity is set up; the bodies before the parts that merge into them.
	void createPendingBodies(scene::Scene& ioScene) {
		for (const auto root: std::exchange(reshapedRoots, {}))
			if (ioScene.registry.valid(root) && ioScene.registry.all_of<scene::component::PhysicBody>(root))
				rebuildAssembly(ioScene, root);
		std::erase_if(partsOf, [](const auto& iEntry) -> bool { return iEntry.second.empty(); });
		const auto pending = std::exchange(pendingBodies, {});
		for (const bool parts: {false, true})
			for (const auto entity: pending) {
				const auto* physicBody = ioScene.registry.try_get<scene::component::PhysicBody>(entity);
				if (physicBody != nullptr && physicBody->body.part == parts && physicBody->body.bodyId == 0 &&
					!partRoot.contains(entity) &&
					ioScene.registry.all_of<scene::component::Transform, scene::component::Hierarchy>(entity))
					createPhysicBody(ioScene, entity);
			}
		createPendingJoints(ioScene);
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
		if (const auto it = partRoot.find(iEntity); it != partRoot.end()) {
			// A part leaving: its root's collider is rebuilt on the next step.
			auto& parts = partsOf[it->second];
			std::erase(parts, iEntity);
			reshapedRoots.push_back(it->second);
			partRoot.erase(it);
		}
		// The parts of a root going away wait for an ancestor body again, or get bodies of their own.
		if (const auto it = partsOf.find(iEntity); it != partsOf.end()) {
			for (const auto part: it->second) {
				partRoot.erase(part);
				pendingBodies.push_back(part);
			}
			partsOf.erase(it);
		}
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
		ioRegistry.on_construct<scene::component::PhysicJoint>().connect<&PhysicsWorld::onPhysicJointAdded>(*this);
		ioRegistry.on_destroy<scene::component::PhysicJoint>().connect<&PhysicsWorld::onPhysicJointRemoved>(*this);
		ioRegistry.on_construct<scene::component::RaycastDoor>().connect<&PhysicsWorld::onDoorAdded>();
		ioRegistry.on_construct<scene::component::RaycastPushWall>().connect<&PhysicsWorld::onPushWallAdded>();
		ioRegistry.on_destroy<scene::component::RaycastDoor>().connect<&PhysicsWorld::onDoorRemoved>(*this);
		ioRegistry.on_destroy<scene::component::RaycastPushWall>().connect<&PhysicsWorld::onPushWallRemoved>(*this);
	}

	void disconnect(entt::registry& ioRegistry) {
		ioRegistry.on_construct<scene::component::PhysicBody>().disconnect(static_cast<const void*>(this));
		ioRegistry.on_destroy<scene::component::PhysicBody>().disconnect(static_cast<const void*>(this));
		ioRegistry.on_destroy<scene::component::Tilemap>().disconnect(static_cast<const void*>(this));
		ioRegistry.on_construct<scene::component::PhysicJoint>().disconnect(static_cast<const void*>(this));
		ioRegistry.on_destroy<scene::component::PhysicJoint>().disconnect(static_cast<const void*>(this));
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

	// Velocity of the body of an entity, for a gravity source that moves.
	[[nodiscard]] auto velocityOf(const scene::Scene& iScene, const core::UUID iEntity) const -> math::vec3f {
		const scene::Entity entity = iScene.findEntityByUUID(iEntity);
		if (!entity || !entity.hasComponent<scene::component::PhysicBody>())
			return {0.f, 0.f, 0.f};
		const auto body = find(entity.getComponent<scene::component::PhysicBody>().body.bodyId);
		return body ? jolt->getSpaceVelocity(*body) : math::vec3f{0.f, 0.f, 0.f};
	}

	void gatherSources(const scene::Scene& iScene) {
		gatherFieldSources(
				iScene,
				[this, &iScene](const core::UUID iEntity) -> math::vec3f { return velocityOf(iScene, iEntity); },
				fieldSources);
	}

	// A dynamic body Owl simulates: awake, not held by the simulation level of detail.
	[[nodiscard]] auto isFree(const scene::Scene& iScene, const SyncedBody& iTracked) const -> const scene::SceneBody* {
		const auto& sbody = iScene.registry.get<scene::component::PhysicBody>(iTracked.entity).body;
		if (sbody.type != scene::SceneBody::BodyType::Dynamic || heldBodies.contains(iTracked.body) ||
			!jolt->isAwake(iTracked.body))
			return nullptr;
		return &sbody;
	}

	// The fields act as the world gravity does: a force over the next step, scaled by the gravity factor.
	void applyGravityFields(const scene::Scene& iScene) {
		gatherSources(iScene);
		if (fieldSources.empty())
			return;
		for (const auto& tracked: synced) {
			const auto* sbody = isFree(iScene, tracked);
			if (sbody == nullptr || !sbody->gravityFields)
				continue;
			const auto owner = ownerOf(tracked.body).value_or(core::UUID{0});
			const math::vec3f pull = fieldAcceleration(fieldSources, jolt->getSpacePose(tracked.body).position, owner) *
									 jolt->getGravityFactor(tracked.body);
			if (pull.normSq() > 0.f)
				jolt->accelerate(tracked.body, pull);
		}
	}

	// A body on rails during a time warp.
	struct RailedBody {
		uint32_t body = 0;
		core::UUID owner;
		bool plane = false;
		math::OrbitState start;
		RailsArc arc;
		bool moved = false;
		math::OrbitState end;
	};

	// The railed body whose own source is a railed body not moved yet, which must move first; none when it can move.
	[[nodiscard]] auto waitsFor(const std::vector<RailedBody>& iRailed,
								const std::unordered_map<uint64_t, size_t>& iByOwner, const RailedBody& iBody) const
			-> std::optional<size_t> {
		if (!iBody.arc.source)
			return std::nullopt;
		const auto it = iByOwner.find(static_cast<uint64_t>(fieldSources[*iBody.arc.source].owner));
		if (it == iByOwner.end() || iRailed[it->second].moved)
			return std::nullopt;
		return it->second;
	}

	// Moves a railed body, carried along with its source when that source is a railed body that already moved.
	void moveOnRails(std::vector<RailedBody>& ioRailed, const std::unordered_map<uint64_t, size_t>& iByOwner,
					 const size_t iIndex, const double iSeconds) const {
		auto& railed = ioRailed[iIndex];
		math::OrbitState relative = railed.start;
		math::vec3d sourceEnd{0.0, 0.0, 0.0};
		math::vec3d sourceEndVelocity{0.0, 0.0, 0.0};
		if (railed.arc.source) {
			const auto& source = fieldSources[*railed.arc.source];
			const math::vec3d sourceStart = toDouble(source.position);
			const math::vec3d sourceVelocity = toDouble(source.velocity);
			sourceEnd = sourceStart;
			sourceEndVelocity = sourceVelocity;
			if (const auto it = iByOwner.find(static_cast<uint64_t>(source.owner));
				it != iByOwner.end() && it->second != iIndex && ioRailed[it->second].moved) {
				sourceEnd = ioRailed[it->second].end.position;
				sourceEndVelocity = ioRailed[it->second].end.velocity;
			}
			relative = {.position = railed.start.position - sourceStart,
						.velocity = railed.start.velocity - sourceVelocity};
		}
		const auto moved = followArc(railed.arc, fieldSources, relative, iSeconds);
		railed.end = {.position = sourceEnd + moved.position, .velocity = sourceEndVelocity + moved.velocity};
		if (railed.plane) {
			railed.end.position.z() = railed.start.position.z();
			railed.end.velocity.z() = 0.0;
		}
		railed.moved = true;
	}

	// Sources first, so a moon carries its satellites; a cycle of sources moves in body order.
	void moveAllOnRails(std::vector<RailedBody>& ioRailed, const std::unordered_map<uint64_t, size_t>& iByOwner,
						const double iSeconds) const {
		size_t left = ioRailed.size();
		while (left > 0) {
			bool progress = false;
			for (size_t index = 0; index < ioRailed.size(); ++index) {
				if (ioRailed[index].moved)
					continue;
				if (const auto other = waitsFor(ioRailed, iByOwner, ioRailed[index]); other && *other != index)
					continue;
				moveOnRails(ioRailed, iByOwner, index, iSeconds);
				progress = true;
				--left;
			}
			if (progress)
				continue;
			for (size_t index = 0; index < ioRailed.size(); ++index)
				if (!ioRailed[index].moved) {
					moveOnRails(ioRailed, iByOwner, index, iSeconds);
					--left;
					break;
				}
		}
	}

	[[nodiscard]] static auto toDouble(const math::vec3f& iVector) -> math::vec3d {
		return {static_cast<double>(iVector.x()), static_cast<double>(iVector.y()), static_cast<double>(iVector.z())};
	}

	[[nodiscard]] static auto toFloat(const math::vec3d& iVector) -> math::vec3f {
		return {static_cast<float>(iVector.x()), static_cast<float>(iVector.y()), static_cast<float>(iVector.z())};
	}

	// The solver stops; the awake dynamic bodies follow their arcs for the warped duration of the step.
	void railsStep(const scene::Scene& iScene, const double iSeconds) {
		gatherSources(iScene);
		const math::vec3f worldGravity = settings.gravity;
		std::vector<RailedBody> railed;
		std::unordered_map<uint64_t, size_t> byOwner;
		for (const auto& tracked: synced) {
			const auto* sbody = isFree(iScene, tracked);
			if (sbody == nullptr)
				continue;
			const auto owner = ownerOf(tracked.body).value_or(core::UUID{0});
			const math::vec3f position = jolt->getSpacePose(tracked.body).position;
			byOwner[static_cast<uint64_t>(owner)] = railed.size();
			railed.push_back({.body = tracked.body,
							  .owner = owner,
							  .plane = sbody->dimension == scene::SceneBody::Dimension::Plane,
							  .start = {.position = toDouble(position),
										.velocity = toDouble(jolt->getSpaceVelocity(tracked.body))},
							  .arc = railsArc(fieldSources, worldGravity, position, owner,
											  jolt->getGravityFactor(tracked.body), sbody->gravityFields),
							  .moved = false,
							  .end = {}});
		}
		moveAllOnRails(railed, byOwner, iSeconds);
		for (const auto& body: railed)
			jolt->setSpaceMotion(body.body, toFloat(body.end.position), toFloat(body.end.velocity));
	}

	// A held body woken by a contact goes back to sleep.
	void holdBodies() const {
		for (const uint32_t body: heldBodies)
			if (jolt->isAwake(body))
				jolt->sleep(body);
	}

	uint64_t nextId = 1;
	std::unordered_map<uint64_t, uint32_t> bodies;
	std::unordered_map<entt::entity, uint64_t> tilemapBodies;
	std::vector<entt::entity> pendingBodies;
	std::unordered_map<uint32_t, core::UUID> bodyOwners;
	std::map<BodyContact, TrackedContact> contacts;
	std::multimap<uint32_t, BodyContact> dormantByBody;
	std::set<BodyContact> waking;
	std::map<std::pair<uint64_t, uint64_t>, uint32_t> touching;
	std::unordered_set<uint32_t> sensorBodies;
	std::unordered_set<uint64_t> sensorOwners;
	std::vector<PhysicCommand::CollisionEvent> collisionEvents;
	std::vector<entt::entity> pendingJoints;
	size_t frameEventsStart = 0;
	// Gravity sources of the step, reused from step to step.
	std::vector<FieldSource> fieldSources;
	// Bodies held out of the simulation (`PhysicCommand::setSimulated`).
	std::set<uint32_t> heldBodies;
	// The last step ran on rails.
	bool onRails = false;
	// Parts merged into each root body, in UUID order.
	std::map<entt::entity, std::vector<entt::entity>> partsOf;
	// Root body of each merged part.
	std::unordered_map<entt::entity, entt::entity> partRoot;
	// Roots that lost a part inside a destruction signal, rebuilt before the next step.
	std::vector<entt::entity> reshapedRoots;
	// Built joints with a break threshold, by joint id (checked in id order, deterministic).
	std::map<uint32_t, entt::entity> breakableJoints;
	// Joints broken since the last `takeJointBreaks`.
	std::vector<PhysicCommand::JointBreak> jointBreaks;

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

template<typename T>
void appendValue(std::vector<uint8_t>& ioBytes, const T iValue) {
	const auto bytes = std::bit_cast<std::array<uint8_t, sizeof(T)>>(iValue);
	ioBytes.insert(ioBytes.end(), bytes.begin(), bytes.end());
}

template<typename T>
auto readValue(const std::span<const uint8_t> iBytes, size_t& ioOffset) -> std::optional<T> {
	if (sizeof(T) > iBytes.size() - ioOffset)
		return std::nullopt;
	std::array<uint8_t, sizeof(T)> bytes{};
	std::ranges::copy(iBytes.subspan(ioOffset, sizeof(T)), bytes.begin());
	ioOffset += sizeof(T);
	return std::bit_cast<T>(bytes);
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
	world->jolt->setGravity(world->settings.gravity);
	world->jolt->setSolverIterations(world->settings.velocitySteps, world->settings.positionSteps);
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
	for (const auto e: ioScene.registry.view<scene::component::PhysicJoint>()) world->pendingJoints.push_back(e);
	world->createPendingJoints(ioScene);
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
	impl.holdBodies();
	if (impl.settings.interpolate)
		impl.capturePrevious();
	const double stepSeconds = ioScene.getSimulationClock().getStepSeconds();
	impl.onRails = ioScene.getRailsFactor() > 1.0;
	if (impl.onRails)
		impl.railsStep(ioScene, stepSeconds * ioScene.getRailsFactor());
	else {
		impl.applyGravityFields(ioScene);
		impl.jolt->step(static_cast<float>(stepSeconds), impl.settings.solverSubSteps);
		impl.collectContactEvents();
		if (!impl.breakableJoints.empty())
			impl.breakOverloadedJoints(ioScene, static_cast<float>(stepSeconds) /
														static_cast<float>(std::max(impl.settings.solverSubSteps, 1u)));
	}
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
	world->frameEventsStart = world->collisionEvents.size();
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
	return std::exchange(world->collisionEvents, {});
}

auto PhysicCommand::saveState(const scene::Scene& iScene) -> std::vector<uint8_t> {
	OWL_PROFILE_FUNCTION()

	const auto* const world = iScene.getPhysicsWorld();
	if (world == nullptr)
		return {};
	const auto simulation = world->jolt->saveState();
	std::vector<uint8_t> state;
	appendValue(state, static_cast<uint64_t>(simulation.size()));
	state.insert(state.end(), simulation.begin(), simulation.end());
	appendValue(state, static_cast<uint64_t>(world->contacts.size()));
	for (const auto& [contact, tracked]: world->contacts) {
		for (const uint32_t value: {contact.bodyA, contact.shapeA, contact.bodyB, contact.shapeB, tracked.awakeSteps,
									tracked.dormant ? 1u : 0u})
			appendValue(state, value);
		appendValue(state, static_cast<uint64_t>(tracked.pair.first));
		appendValue(state, static_cast<uint64_t>(tracked.pair.second));
	}
	return state;
}

auto PhysicCommand::restoreState(scene::Scene& ioScene, const std::span<const uint8_t> iState) -> bool {
	OWL_PROFILE_FUNCTION()

	auto* const world = ioScene.getPhysicsWorld();
	if (world == nullptr) {
		logNotInitialized("restoreState");
		return false;
	}
	size_t offset = 0;
	const auto simulationSize = readValue<uint64_t>(iState, offset);
	if (!simulationSize || *simulationSize > iState.size() - offset) {
		OWL_CORE_WARN("Physics: State of {} bytes is truncated, not restored.", iState.size())
		return false;
	}
	const auto simulation = iState.subspan(offset, static_cast<size_t>(*simulationSize));
	offset += simulation.size();
	std::map<BodyContact, PhysicsWorld::TrackedContact> contacts;
	const auto contactCount = readValue<uint64_t>(iState, offset);
	for (uint64_t i = 0; contactCount && i < *contactCount; ++i) {
		std::array<uint32_t, 6> values{};
		bool complete = true;
		for (auto& value: values) {
			const auto read = readValue<uint32_t>(iState, offset);
			complete = complete && read.has_value();
			value = read.value_or(0);
		}
		const auto first = readValue<uint64_t>(iState, offset);
		const auto second = readValue<uint64_t>(iState, offset);
		if (!complete || !first || !second) {
			OWL_CORE_WARN("Physics: State of {} bytes is truncated, not restored.", iState.size())
			return false;
		}
		contacts[{.bodyA = values[0], .shapeA = values[1], .bodyB = values[2], .shapeB = values[3]}] = {
				.pair = {core::UUID{*first}, core::UUID{*second}},
				.dormant = values[5] != 0,
				.awakeSteps = values[4]};
	}
	if (!contactCount || offset != iState.size()) {
		OWL_CORE_WARN("Physics: State of {} bytes is malformed, not restored.", iState.size())
		return false;
	}
	if (!world->jolt->restoreState(simulation)) {
		OWL_CORE_WARN("Physics: State saved with other bodies than the world holds, not restored.")
		return false;
	}
	std::ignore = world->jolt->takeActivatedBodies();
	world->touching.clear();
	world->dormantByBody.clear();
	world->waking.clear();
	for (const auto& [contact, tracked]: contacts) {
		++world->touching[PhysicsWorld::ordered(tracked.pair)];
		if (!tracked.dormant)
			continue;
		world->linkDormant(contact);
		if (tracked.awakeSteps > 0)
			world->waking.insert(contact);
	}
	world->contacts = std::move(contacts);
	world->collisionEvents.clear();
	world->frameEventsStart = 0;
	world->captureCurrent();
	world->capturePrevious();
	return true;
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
	world->forgetContacts(iEntity.getUUID(), world->sensorOwners.contains(static_cast<uint64_t>(iEntity.getUUID())));
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
	snapshot.linearVelocity = world->jolt->getSpaceVelocity(*body);
	snapshot.angularVelocity = world->jolt->getSpaceAngularVelocity(*body);
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
	world->jolt->setSpaceVelocity(*body, iSnapshot.linearVelocity);
	world->jolt->setSpaceAngularVelocity(*body, iSnapshot.angularVelocity);
	if (iSnapshot.awake)
		world->jolt->wake(*body);
}


auto PhysicCommand::getGravityAt(const scene::Scene& iScene, const math::vec3f& iPoint) -> math::vec3f {
	std::vector<FieldSource> sources;
	gatherFieldSources(iScene, {}, sources);
	return iScene.getPhysicsSettings().clamped().gravity + fieldAcceleration(sources, iPoint, core::UUID{0});
}

auto PhysicCommand::predictTrajectory(const scene::Entity& iEntity, const uint32_t iSteps) -> std::vector<math::vec3f> {
	if (!iEntity || !iEntity.hasComponent<scene::component::PhysicBody>())
		return {};
	const auto& sbody = iEntity.getComponent<scene::component::PhysicBody>().body;
	if (sbody.type != scene::SceneBody::BodyType::Dynamic)
		return {};
	const auto& scene = *iEntity.getScene();
	const auto* world = scene.getPhysicsWorld();
	const auto body = world != nullptr ? world->find(sbody.bodyId) : std::nullopt;
	std::vector<FieldSource> sources;
	if (world != nullptr)
		gatherFieldSources(
				scene,
				[world, &scene](const core::UUID iOwner) -> math::vec3f { return world->velocityOf(scene, iOwner); },
				sources);
	else
		gatherFieldSources(scene, {}, sources);
	const auto settings = world != nullptr ? world->settings : scene.getPhysicsSettings().clamped();
	const PathSettings path{.worldGravity = settings.gravity,
							.gravityScale = body ? world->jolt->getGravityFactor(*body) : sbody.gravityScale,
							.fields = sbody.gravityFields,
							.plane = sbody.dimension == scene::SceneBody::Dimension::Plane,
							.stepSeconds = static_cast<float>(scene.getSimulationClock().getStepSeconds()),
							.subSteps = settings.solverSubSteps,
							.self = iEntity.getUUID()};
	const math::vec3f position =
			body ? world->jolt->getSpacePose(*body).position : scene.getWorldTransform(iEntity).translation();
	const math::vec3f velocity = body ? world->jolt->getSpaceVelocity(*body) : math::vec3f{0.f, 0.f, 0.f};
	return predictPath(sources, path, position, velocity, std::min(iSteps, maxPredictionSteps));
}

auto PhysicCommand::isOnRails(const scene::Scene& iScene) -> bool {
	const auto* const world = iScene.getPhysicsWorld();
	return world != nullptr && world->onRails;
}

void PhysicCommand::setSimulated(const scene::Entity& iEntity, const bool iSimulated, const math::vec3f& iVelocity) {
	auto* const world = runningWorld(iEntity, "setSimulated");
	if (world == nullptr)
		return;
	const auto body = movingBody(*world, iEntity);
	if (!body || iEntity.getComponent<scene::component::PhysicBody>().body.type != scene::SceneBody::BodyType::Dynamic)
		return;
	if (iSimulated) {
		if (world->heldBodies.erase(*body) == 0)
			return;
		world->jolt->wake(*body);
		world->jolt->setSpaceVelocity(*body,
									  isInSpace(iEntity) ? iVelocity : math::vec3f{iVelocity.x(), iVelocity.y(), 0.f});
		return;
	}
	if (!world->heldBodies.insert(*body).second)
		return;
	world->jolt->setSpaceVelocity(*body, {0.f, 0.f, 0.f});
	world->jolt->setSpaceAngularVelocity(*body, {0.f, 0.f, 0.f});
	world->jolt->sleep(*body);
}

auto PhysicCommand::isSimulated(const scene::Entity& iEntity) -> bool {
	const auto* const world = worldOf(iEntity);
	if (world == nullptr)
		return true;
	const auto body = movingBody(*world, iEntity);
	return !body || !world->heldBodies.contains(*body);
}

auto PhysicCommand::followRails(const scene::Entity& iEntity, const math::vec3f& iVelocity, const float iSeconds)
		-> math::vec3f {
	auto* const world = runningWorld(iEntity, "followRails");
	if (world == nullptr)
		return iVelocity;
	const auto body = movingBody(*world, iEntity);
	if (!body || !world->heldBodies.contains(*body))
		return iVelocity;
	const auto& scene = *iEntity.getScene();
	const auto& sbody = iEntity.getComponent<scene::component::PhysicBody>().body;
	world->gatherSources(scene);
	const math::vec3f position = world->jolt->getSpacePose(*body).position;
	const auto arc = railsArc(world->fieldSources, world->settings.gravity, position, iEntity.getUUID(),
							  world->jolt->getGravityFactor(*body), sbody.gravityFields);
	math::OrbitState state{.position = PhysicsWorld::toDouble(position), .velocity = PhysicsWorld::toDouble(iVelocity)};
	math::vec3d origin{0.0, 0.0, 0.0};
	if (arc.source) {
		const auto& source = world->fieldSources[*arc.source];
		origin = PhysicsWorld::toDouble(source.position);
		state = {.position = state.position - origin,
				 .velocity = state.velocity - PhysicsWorld::toDouble(source.velocity)};
	}
	auto moved = followArc(arc, world->fieldSources, state, static_cast<double>(iSeconds));
	moved.position += origin;
	if (arc.source)
		moved.velocity += PhysicsWorld::toDouble(world->fieldSources[*arc.source].velocity);
	if (sbody.dimension == scene::SceneBody::Dimension::Plane) {
		moved.position.z() = static_cast<double>(position.z());
		moved.velocity.z() = 0.0;
	}
	world->jolt->setSpaceMotion(*body, PhysicsWorld::toFloat(moved.position), {0.f, 0.f, 0.f});
	world->snapBody(sbody.bodyId);
	return PhysicsWorld::toFloat(moved.velocity);
}

namespace {

auto toQueryVolume(const PhysicCommand::QueryShape& iShape) -> QueryVolume {
	return {.sphere = iShape.sphere,
			.halfExtents = iShape.halfExtents,
			.rotation = JoltWorld::toRotation(iShape.rotation)};
}

auto normalized(const math::vec3f& iVector) -> std::optional<math::vec3f> {
	const float length = std::sqrt(iVector.x() * iVector.x() + iVector.y() * iVector.y() + iVector.z() * iVector.z());
	if (length <= 0.f || !std::isfinite(length))
		return std::nullopt;
	return iVector / length;
}

auto toHit(const PhysicsWorld& iWorld, const std::optional<QueryHit>& iHit) -> std::optional<PhysicCommand::QueryHit> {
	if (!iHit)
		return std::nullopt;
	const auto owner = iWorld.ownerOf(iHit->body);
	if (!owner)
		return std::nullopt;
	return PhysicCommand::QueryHit{.entity = *owner,
								   .point = iHit->point,
								   .normal = iHit->normal,
								   .distance = iHit->distance};
}

auto jointOf(const scene::Entity& iEntity, const char* iFunc) -> std::optional<std::pair<PhysicsWorld*, uint32_t>> {
	auto* const world = runningWorld(iEntity, iFunc);
	if (world == nullptr || !iEntity.hasComponent<scene::component::PhysicJoint>())
		return std::nullopt;
	const uint64_t id = iEntity.getComponent<scene::component::PhysicJoint>().joint.jointId;
	if (id == 0)
		return std::nullopt;
	return std::pair{world, static_cast<uint32_t>(id)};
}

}// namespace

auto PhysicCommand::raycast(const scene::Scene& iScene, const math::vec3f& iOrigin, const math::vec3f& iDirection,
							const float iDistance, const uint8_t iMask) -> std::optional<QueryHit> {
	const auto* const world = iScene.getPhysicsWorld();
	const auto direction = normalized(iDirection);
	if (world == nullptr || !direction || !(iDistance > 0.f))
		return std::nullopt;
	return toHit(*world, world->jolt->castRay(iOrigin, *direction, iDistance, iMask));
}

auto PhysicCommand::shapeCast(const scene::Scene& iScene, const QueryShape& iShape, const math::vec3f& iOrigin,
							  const math::vec3f& iDirection, const float iDistance, const uint8_t iMask)
		-> std::optional<QueryHit> {
	const auto* const world = iScene.getPhysicsWorld();
	const auto direction = normalized(iDirection);
	if (world == nullptr || !direction || !(iDistance > 0.f))
		return std::nullopt;
	return toHit(*world, world->jolt->castVolume(toQueryVolume(iShape), iOrigin, *direction, iDistance, iMask));
}

auto PhysicCommand::overlap(const scene::Scene& iScene, const QueryShape& iShape, const math::vec3f& iCentre,
							const uint8_t iMask) -> std::vector<core::UUID> {
	const auto* const world = iScene.getPhysicsWorld();
	if (world == nullptr)
		return {};
	std::vector<core::UUID> entities;
	for (const auto body: world->jolt->overlap(toQueryVolume(iShape), iCentre, iMask))
		if (const auto owner = world->ownerOf(body); owner && !std::ranges::contains(entities, *owner))
			entities.push_back(*owner);
	return entities;
}

void PhysicCommand::setJointMotor(const scene::Entity& iEntity, const bool iEnabled, const float iSpeed) {
	if (const auto joint = jointOf(iEntity, "setJointMotor"); joint)
		joint->first->jolt->setJointMotor(joint->second, iEnabled, iSpeed);
}

auto PhysicCommand::getJointPosition(const scene::Entity& iEntity) -> float {
	const auto joint = jointOf(iEntity, "getJointPosition");
	return joint ? joint->first->jolt->getJointPosition(joint->second) : 0.f;
}


namespace {

// The body an entity moves and whether it may be pushed: its own or its assembly's, never a static one.
auto pushedBody(const scene::Entity& iEntity, const char* iFunc) -> std::optional<std::pair<PhysicsWorld*, uint32_t>> {
	auto* const world = runningWorld(iEntity, iFunc);
	if (world == nullptr || !iEntity.hasComponent<scene::component::PhysicBody>())
		return std::nullopt;
	const auto body = world->bodyOf(*iEntity.getScene(), static_cast<entt::entity>(iEntity));
	if (!body)
		return std::nullopt;
	const auto it = world->partRoot.find(static_cast<entt::entity>(iEntity));
	const auto& owner = it != world->partRoot.end()
								? iEntity.getScene()->registry.get<scene::component::PhysicBody>(it->second).body
								: iEntity.getComponent<scene::component::PhysicBody>().body;
	if (owner.type == scene::SceneBody::BodyType::Static)
		return std::nullopt;
	return std::pair{world, *body};
}

// The joints on an entity or linked to it are built again, on the body it moves now.
void rebindJoints(PhysicsWorld& ioWorld, scene::Scene& ioScene, const scene::Entity& iEntity) {
	const core::UUID uuid = iEntity.getUUID();
	for (const auto [handle, component]: ioScene.registry.view<scene::component::PhysicJoint>().each()) {
		if (handle != static_cast<entt::entity>(iEntity) && component.joint.connected != uuid)
			continue;
		if (component.joint.jointId != 0) {
			ioWorld.breakableJoints.erase(static_cast<uint32_t>(component.joint.jointId));
			ioWorld.jolt->destroyJoint(static_cast<uint32_t>(component.joint.jointId));
			component.joint.jointId = 0;
		}
		if (!std::ranges::contains(ioWorld.pendingJoints, handle))
			ioWorld.pendingJoints.push_back(handle);
	}
	ioWorld.createPendingJoints(ioScene);
}

}// namespace

auto PhysicCommand::takeJointBreaks(scene::Scene& ioScene) -> std::vector<JointBreak> {
	auto* const world = ioScene.getPhysicsWorld();
	return world != nullptr ? std::exchange(world->jointBreaks, {}) : std::vector<JointBreak>{};
}

void PhysicCommand::addForce(const scene::Entity& iEntity, const math::vec3f& iForce) {
	const auto pushed = pushedBody(iEntity, "addForce");
	if (!pushed)
		return;
	const auto& [world, body] = *pushed;
	std::optional<math::vec3f> point;
	if (world->partRoot.contains(static_cast<entt::entity>(iEntity)))
		point = iEntity.getScene()->getWorldTransform(iEntity).translation();
	world->jolt->addForce(body, iForce, point);
}

void PhysicCommand::addForceAtPoint(const scene::Entity& iEntity, const math::vec3f& iForce,
									const math::vec3f& iPoint) {
	if (const auto pushed = pushedBody(iEntity, "addForceAtPoint"); pushed)
		pushed->first->jolt->addForce(pushed->second, iForce, iPoint);
}

void PhysicCommand::addTorque(const scene::Entity& iEntity, const math::vec3f& iTorque) {
	if (const auto pushed = pushedBody(iEntity, "addTorque"); pushed)
		pushed->first->jolt->addTorque(pushed->second, iTorque);
}

auto PhysicCommand::attach(const scene::Entity& iEntity) -> bool {
	if (!iEntity || !iEntity.hasComponent<scene::component::PhysicBody>())
		return false;
	auto& sbody = iEntity.getComponent<scene::component::PhysicBody>().body;
	auto* const world = worldOf(iEntity);
	if (world == nullptr) {
		sbody.part = true;
		return false;
	}
	if (world->partRoot.contains(static_cast<entt::entity>(iEntity)))
		return true;
	auto& scene = *iEntity.getScene();
	sbody.part = true;
	const auto root = PhysicsWorld::assemblyRootOf(scene, iEntity);
	if (!root) {
		OWL_CORE_WARN("Physics: Part '{}' has no ancestor body to join, it keeps its own body.", iEntity.getName())
		sbody.part = false;
		return false;
	}
	world->release(sbody.bodyId);
	world->joinPart(scene, static_cast<entt::entity>(iEntity), *root);
	rebindJoints(*world, scene, iEntity);
	return true;
}

void PhysicCommand::detach(const scene::Entity& iEntity) {
	if (!iEntity || !iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	auto* const world = worldOf(iEntity);
	if (world == nullptr || !world->partRoot.contains(static_cast<entt::entity>(iEntity))) {
		iEntity.getComponent<scene::component::PhysicBody>().body.part = false;
		return;
	}
	auto& scene = *iEntity.getScene();
	world->leavePart(scene, static_cast<entt::entity>(iEntity), true);
	rebindJoints(*world, scene, iEntity);
}

auto PhysicCommand::getAssemblyRoot(const scene::Entity& iEntity) -> core::UUID {
	const auto* const world = worldOf(iEntity);
	if (world == nullptr)
		return core::UUID{0};
	const auto it = world->partRoot.find(static_cast<entt::entity>(iEntity));
	return it != world->partRoot.end() ? scene::Entity{it->second, iEntity.getScene()}.getUUID() : core::UUID{0};
}

auto PhysicCommand::getMass(const scene::Entity& iEntity) -> float {
	const auto* const world = worldOf(iEntity);
	if (world == nullptr)
		return 0.f;
	const auto body = world->bodyOf(*iEntity.getScene(), static_cast<entt::entity>(iEntity));
	return body ? world->jolt->getMass(*body) : 0.f;
}

auto PhysicCommand::getCenterOfMass(const scene::Entity& iEntity) -> math::vec3f {
	const auto* const world = worldOf(iEntity);
	if (world == nullptr)
		return {0.f, 0.f, 0.f};
	const auto body = world->bodyOf(*iEntity.getScene(), static_cast<entt::entity>(iEntity));
	return body ? world->jolt->getCenterOfMass(*body) : math::vec3f{0.f, 0.f, 0.f};
}

}// namespace owl::physics
