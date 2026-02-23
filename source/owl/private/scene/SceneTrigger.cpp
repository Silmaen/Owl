/**
 * @file SceneTrigger.cpp
 * @author Silmaen
 * @date 12/30/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/SceneTrigger.h"

#include "physic/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/component/components.h"

namespace owl::scene {

SceneTrigger::SceneTrigger() = default;

SceneTrigger::~SceneTrigger() = default;

void SceneTrigger::onTriggered(Entity& ioPlayer, const Entity& iTriggerEntity) {
	auto* scene = ioPlayer.getScene();
	switch (type) {
		case TriggerType::Victory:
			if (m_triggered)
				return;
			m_triggered = true;
			scene->status = Scene::Status::Victory;
			break;
		case TriggerType::Death:
			if (m_triggered)
				return;
			m_triggered = true;
			scene->status = Scene::Status::Death;
			break;
		case TriggerType::Target:
			// Passive marker, no action on collision.
			break;
		case TriggerType::Teleport:
			{
				const auto triggerRotation =
						iTriggerEntity.getComponent<component::Transform>().transform.rotation().z();
				const auto playerVelocity = physic::PhysicCommand::getVelocity(ioPlayer);
				const auto playerRotation =
						ioPlayer.getComponent<component::Transform>().transform.rotation().z();
				if (levelName.empty()) {
					// Same-level teleport: find target entity by name.
					for (const auto view = scene->registry.view<component::Tag, component::Transform>();
						 const auto ent: view) {
						if (view.get<component::Tag>(ent).tag == targetName) {
							const auto& targetTransform = view.get<component::Transform>(ent).transform;
							const float targetRotation = targetTransform.rotation().z();
							const float delta = targetRotation - triggerRotation;
							// Rotate velocity by delta angle.
							const float cosD = std::cos(delta);
							const float sinD = std::sin(delta);
							const math::vec2f rotatedVelocity = {playerVelocity.x() * cosD -
																		 playerVelocity.y() * sinD,
																 playerVelocity.x() * sinD +
																		 playerVelocity.y() * cosD};
							// Teleport player: keep player's own rotation, only move position.
							physic::PhysicCommand::setTransform(
									ioPlayer,
									{targetTransform.translation().x(),
									 targetTransform.translation().y()},
									playerRotation);
							physic::PhysicCommand::setVelocity(ioPlayer, rotatedVelocity);
							// Also update the transform component so rendering is in sync.
							auto& playerTransform =
									ioPlayer.getComponent<component::Transform>().transform;
							playerTransform.translation().x() = targetTransform.translation().x();
							playerTransform.translation().y() = targetTransform.translation().y();
							break;
						}
					}
				} else {
					// Cross-level teleport: set pending request.
					const float delta = -triggerRotation;// Target rotation unknown until new scene loads.
					const float cosD = std::cos(delta);
					const float sinD = std::sin(delta);
					scene->teleportRequest = {.pending = true,
											  .levelName = levelName,
											  .targetName = targetName,
											  .initialVelocity = {playerVelocity.x() * cosD -
																		  playerVelocity.y() * sinD,
																  playerVelocity.x() * sinD +
																		  playerVelocity.y() * cosD},
											  .rotationDelta = 0.f};
				}
			}
			break;
	}
}

}// namespace owl::scene
