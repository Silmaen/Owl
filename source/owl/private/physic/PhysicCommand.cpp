/**
 * @file PhysicCommand.cpp
 * @author Silmaen
 * @date 12/27/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "physic/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/component/components.h"
#include <box2d/box2d.h>

namespace owl::physic {

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
		OWL_CORE_WARN("PhysicCommand::init(): Scene is null")
		return;
	}
	if (isInitialized())
		destroy();
	m_impl = std::make_shared<Impl>();
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
		OWL_CORE_WARN("PhysicCommand::frame(), Physic engine not initialized")
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
		OWL_CORE_WARN("PhysicCommand::impulse(), Physic engine not initialized.")
		return;
	}
	if (!iEntity) {
		// Void entity !!
		OWL_CORE_WARN("PhysicCommand::impulse(), entity is null.")
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
		OWL_CORE_WARN("PhysicCommand::getVelocity(), Physic Engine not initialized.")
		return {0.0f, 0.0f};
	}
	if (!iEntity) {
		// Void entity !!
		OWL_CORE_WARN("PhysicCommand::getVelocity(), entity is null.")
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
		OWL_CORE_WARN("PhysicCommand::setTransform(), Physic engine not initialized.")
		return;
	}
	if (!iEntity) {
		OWL_CORE_WARN("PhysicCommand::setTransform(), entity is null.")
		return;
	}
	if (!iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	b2Body_SetTransform(m_impl->bodies[body.bodyId], {iPosition.x(), iPosition.y()}, b2MakeRot(iRotation));
}

void PhysicCommand::setVelocity(const scene::Entity& iEntity, const math::vec2f& iVelocity) {
	if (!isInitialized()) {
		OWL_CORE_WARN("PhysicCommand::setVelocity(), Physic engine not initialized.")
		return;
	}
	if (!iEntity) {
		OWL_CORE_WARN("PhysicCommand::setVelocity(), entity is null.")
		return;
	}
	if (!iEntity.hasComponent<scene::component::PhysicBody>())
		return;
	auto& [body] = iEntity.getComponent<scene::component::PhysicBody>();
	if (body.type == scene::SceneBody::BodyType::Static)
		return;
	b2Body_SetLinearVelocity(m_impl->bodies[body.bodyId], {iVelocity.x(), iVelocity.y()});
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
