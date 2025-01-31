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
#include <box2d.h>

namespace owl::physic {

class PhysicCommand::Impl {
public:
	Impl() {}
	~Impl() {}

	b2WorldId worldId{0, 0};
	uint64_t nextId = 1;
	std::unordered_map<uint64_t, b2BodyId> bodies;
};

shared<PhysicCommand::Impl> PhysicCommand::m_impl = std::make_shared<Impl>();
scene::Scene* PhysicCommand::m_scene = nullptr;

PhysicCommand::PhysicCommand() = default;

void PhysicCommand::init(scene::Scene* iScene) {
	if (iScene == nullptr) {
		OWL_CORE_WARN("PhysicCommand::init(): Scene is null")
		return;
	}
	if (isInitialized())
		destroy();
	m_scene = iScene;
	b2WorldDef def = b2DefaultWorldDef();
	def.gravity = {.x = 0.0f, .y = -9.81f};
	m_impl->worldId = b2CreateWorld(&def);
	OWL_INFO("PhysicCommand::init(), world created ({} {})", m_impl->worldId.index1, m_impl->worldId.revision)

	// Add entities...
	for (const auto& view = m_scene->registry.view<scene::component::PhysicBody, scene::component::Transform>();
		 const auto& e: view) {
		const scene::Entity entity{e, m_scene};
		auto& [sbody] = entity.getComponent<scene::component::PhysicBody>();
		auto& [transform] = entity.getComponent<scene::component::Transform>();
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
		bodyDef.position.x = transform.translation().x();
		bodyDef.position.y = transform.translation().y();
		bodyDef.rotation = b2MakeRot(transform.rotation().z());

		const b2BodyId body = b2CreateBody(m_impl->worldId, &bodyDef);
		OWL_INFO("PhysicCommand::init(), body created ({} {} {})", body.index1, body.world0, body.revision)
		sbody.bodyId = m_impl->nextId;
		m_impl->bodies[m_impl->nextId] = body;
		m_impl->nextId++;

		const b2Polygon dynamicBox = b2MakeBox(sbody.colliderSize.x() * transform.scale().x() * 0.5f,
											   sbody.colliderSize.y() * transform.scale().y() * 0.5f);
		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.density = sbody.density;
		shapeDef.friction = sbody.friction;
		shapeDef.restitution = sbody.restitution;
		b2CreatePolygonShape(body, &shapeDef, &dynamicBox);
	}
}

void PhysicCommand::destroy() {
	m_scene = nullptr;
	b2DestroyWorld(m_impl->worldId);
	m_impl->worldId = {.index1 = 0, .revision = 0};
	m_impl->bodies.clear();
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
		auto [x, y] = b2Body_GetPosition(m_impl->bodies[physic.body.bodyId]);
		transform.transform.translation().x() = x;
		transform.transform.translation().y() = y;
		transform.transform.rotation().z() = b2Rot_GetAngle(b2Body_GetRotation(m_impl->bodies[physic.body.bodyId]));
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


}// namespace owl::physic
