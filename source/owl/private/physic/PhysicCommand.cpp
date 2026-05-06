/**
 * @file PhysicCommand.cpp
 * @author Silmaen
 * @date 12/27/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "physic/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/Tileset.h"
#include "scene/component/components.h"
#include <box2d/box2d.h>

namespace owl::physic {

namespace {

/**
 * @brief
 *  Emit a uniform "called before init" warning for a Physic API call.
 * @param[in] iFunc Calling function name (used as the log subject).
 */
inline void logNotInitialized(const char* iFunc) {
	OWL_CORE_WARN("Physic: {} called before initialisation; ignoring.", iFunc)
}

/**
 * @brief
 *  Emit a uniform "null entity" warning for a Physic API call.
 * @param[in] iFunc Calling function name (used as the log subject).
 */
inline void logNullEntity(const char* iFunc) {
	OWL_CORE_WARN("Physic: {} called with null entity; ignoring.", iFunc)
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

	b2WorldId worldId{0, 0};
	uint64_t nextId = 1;
	std::unordered_map<uint64_t, b2BodyId> bodies;
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
	b2WorldDef def = b2DefaultWorldDef();
	def.gravity = {.x = 0.0f, .y = -9.81f};
	m_impl->worldId = b2CreateWorld(&def);

	OWL_INFO("PhysicCommand::init(), world created ({} {})", m_impl->worldId.index1, m_impl->worldId.generation)

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
		OWL_INFO("PhysicCommand::init(), body created ({} {} {})", body.index1, body.world0, body.generation)
		sbody.bodyId = m_impl->nextId;
		m_impl->bodies[m_impl->nextId] = body;
		m_impl->nextId++;

		const b2Polygon dynamicBox = b2MakeBox(sbody.colliderSize.x() * worldTransform.scale().x() * 0.5f,
											   sbody.colliderSize.y() * worldTransform.scale().y() * 0.5f);
		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.density = sbody.density;
		shapeDef.material.friction = sbody.friction;
		shapeDef.material.restitution = sbody.restitution;
		b2CreatePolygonShape(body, &shapeDef, &dynamicBox);
	}

	// Generate static colliders from tilemaps. One static b2Body per tilemap entity,
	// one polygon shape per collidable cell. The body is destroyed with b2DestroyWorld.
	for (const auto view = m_scene->registry.view<scene::component::Tilemap, scene::component::Transform>();
		 const auto e: view) {
		const scene::Entity entity{e, m_scene};
		const auto& tilemap = entity.getComponent<scene::component::Tilemap>();
		if (!tilemap.tileset || tilemap.layers.empty())
			continue;
		// Skip if every tile in the tileset is non-collidable — common for purely cosmetic
		// tilemaps (backgrounds). Avoids creating an empty body for nothing.
		const auto* tilesetPtr = tilemap.tileset.get();
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
		const float cellSize = tilemap.cellSize;
		const float originX = -static_cast<float>(tilemap.width - 1) * 0.5f * cellSize;
		const float originY = static_cast<float>(tilemap.height - 1) * 0.5f * cellSize;
		const float halfX = cellSize * worldTransform.scale().x() * 0.5f;
		const float halfY = cellSize * worldTransform.scale().y() * 0.5f;
		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.density = 0.f;// static
		shapeDef.material.friction = 0.5f;
		for (const auto& layer: tilemap.layers) {
			for (uint32_t y = 0; y < tilemap.height; ++y) {
				for (uint32_t x = 0; x < tilemap.width; ++x) {
					const size_t flat = static_cast<size_t>(y) * tilemap.width + x;
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
}

void PhysicCommand::destroy() {
	m_scene = nullptr;
	b2DestroyWorld(m_impl->worldId);
	m_impl->worldId = {.index1 = 0, .generation = 0};
	m_impl->bodies.clear();
	m_impl.reset();
}

auto PhysicCommand::isInitialized() -> bool { return m_scene != nullptr; }

void PhysicCommand::frame(const core::Timestep& iTimestep) {
	if (!isInitialized()) {
		logNotInitialized("frame");
		return;
	}
	// Update the physical world
	b2World_Step(m_impl->worldId, iTimestep.getSeconds(), 4);

	// apply to the entities
	for (const auto view = m_scene->registry.view<scene::component::Transform, scene::component::PhysicBody>();
		 const auto entity: view) {
		auto&& [transform, physic] = view.get<scene::component::Transform, scene::component::PhysicBody>(entity);
		const auto [x, y] = b2Body_GetPosition(m_impl->bodies[physic.body.bodyId]);
		const float angle = b2Rot_GetAngle(b2Body_GetRotation(m_impl->bodies[physic.body.bodyId]));
		// Convert world position from Box2D back to local space.
		const scene::Entity ent{entity, m_scene};
		const auto& hierarchy = ent.getComponent<scene::component::Hierarchy>();
		if (hierarchy.parentId != core::UUID{0}) {
			if (const scene::Entity parent = m_scene->findEntityByUUID(hierarchy.parentId); parent) {
				const math::mat4 parentWorldInv = math::inverse(m_scene->getWorldTransform(parent)());
				const math::vec4 localPos = parentWorldInv * math::vec4{x, y, transform.transform.translation().z(), 1.0f};
				transform.transform.translation().x() = localPos.x();
				transform.transform.translation().y() = localPos.y();
				const float parentWorldRotZ = m_scene->getWorldTransform(parent).rotation().z();
				transform.transform.rotation().z() = angle - parentWorldRotZ;
			} else {
				transform.transform.translation().x() = x;
				transform.transform.translation().y() = y;
				transform.transform.rotation().z() = angle;
			}
		} else {
			transform.transform.translation().x() = x;
			transform.transform.translation().y() = y;
			transform.transform.rotation().z() = angle;
		}
	}
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
	if (!iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	b2Body_SetTransform(m_impl->bodies[body.bodyId], {iPosition.x(), iPosition.y()}, b2MakeRot(iRotation));
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

}// namespace owl::physic
