/**
 * @file SceneTrigger.cpp
 * @author Silmaen
 * @date 12/30/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/SceneTrigger.h"

#include "input/Input.h"
#include "physic/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/component/components.h"

namespace owl::scene {

namespace {
/**
 * @brief
 *  Dispatch a named Lua callback on an entity's script instance if present.
 */
void dispatchLuaCallback(const Entity& iEntity, const std::string& iFuncName, const uint64_t iArg) {
	if (!iEntity.hasComponent<component::LuaScript>())
		return;
	if (const auto& ls = iEntity.getComponent<component::LuaScript>(); ls.instance && ls.instance->isValid())
		std::ignore = ls.instance->callFunction(iFuncName);
	static_cast<void>(iArg);
}

/**
 * @brief
 *  Dispatch a named Lua callback with no argument.
 */
void dispatchLuaCallbackNoArg(const Entity& iEntity, const std::string& iFuncName) {
	if (!iEntity.hasComponent<component::LuaScript>())
		return;
	if (const auto& ls = iEntity.getComponent<component::LuaScript>(); ls.instance && ls.instance->isValid())
		std::ignore = ls.instance->callFunction(iFuncName);
}

}// namespace

SceneTrigger::SceneTrigger() = default;

SceneTrigger::~SceneTrigger() = default;

void SceneTrigger::onTriggered(const Entity& ioPlayer, const Entity& iTriggerEntity) {
	auto* scene = ioPlayer.getScene();
	switch (type) {
		case TriggerType::Victory:
			if (m_triggered)
				return;
			m_triggered = true;
			if (!levelName.empty()) {
				// Load custom victory scene.
				scene->teleportRequest = {.pending = true, .levelName = levelName, .targetName = {}};
			} else {
				scene->status = Scene::Status::Victory;
			}
			break;
		case TriggerType::Death:
			if (m_triggered)
				return;
			m_triggered = true;
			if (!levelName.empty()) {
				// Load custom game over scene.
				scene->teleportRequest = {.pending = true, .levelName = levelName, .targetName = {}};
			} else {
				scene->status = Scene::Status::Death;
			}
			break;
		case TriggerType::Target:
			// Passive marker, no action on collisions
		case TriggerType::Timer:
			// Target: passive marker. Timer: handled by updateTimer(), not overlap.
			break;
		case TriggerType::Interaction:
			{
				const bool keyDown = input::Input::isKeyPressed(input::key::E);
				if (keyDown && !m_interactKeyWasDown) {
					const auto& func = callbackName.empty() ? std::string("on_interact") : callbackName;

					dispatchLuaCallbackNoArg(iTriggerEntity, func);
				}
				m_interactKeyWasDown = keyDown;
			}
			break;
		case TriggerType::LuaCallback:
			{
				const auto& func = callbackName.empty() ? std::string("on_triggered") : callbackName;

				dispatchLuaCallback(iTriggerEntity, func, ioPlayer.getUUID());
			}
			break;
		case TriggerType::Teleport:
			{
				const auto triggerRotation =
						iTriggerEntity.getComponent<component::Transform>().transform.rotation().z();
				const auto playerVelocity = physic::PhysicCommand::getVelocity(ioPlayer);
				const auto playerRotation = ioPlayer.getComponent<component::Transform>().transform.rotation().z();
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
							const math::vec2f rotatedVelocity = {playerVelocity.x() * cosD - playerVelocity.y() * sinD,
																 playerVelocity.x() * sinD + playerVelocity.y() * cosD};

							// Teleport player: keep player's own rotation, only move position.
							physic::PhysicCommand::setTransform(
									ioPlayer, {targetTransform.translation().x(), targetTransform.translation().y()},
									playerRotation);

							physic::PhysicCommand::setVelocity(ioPlayer, rotatedVelocity);
							// Also update the transform component so rendering is in sync.
							auto& playerTransform = ioPlayer.getComponent<component::Transform>().transform;
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
					scene->teleportRequest = {
							.pending = true,
							.levelName = levelName,
							.targetName = targetName,
							.initialVelocity = {playerVelocity.x() * cosD - playerVelocity.y() * sinD,
												playerVelocity.x() * sinD + playerVelocity.y() * cosD},
							.rotationDelta = 0.f};
				}
			}
			break;
	}
}

void SceneTrigger::onTriggerEnter(const Entity& ioPlayer, const Entity& iTriggerEntity) {
	dispatchLuaCallback(iTriggerEntity, "on_trigger_enter", ioPlayer.getUUID());
	// Also notify the player script.
	dispatchLuaCallback(ioPlayer, "on_trigger_enter", iTriggerEntity.getUUID());
}

void SceneTrigger::onTriggerExit(const Entity& ioPlayer, const Entity& iTriggerEntity) {
	dispatchLuaCallback(iTriggerEntity, "on_trigger_exit", ioPlayer.getUUID());
	dispatchLuaCallback(ioPlayer, "on_trigger_exit", iTriggerEntity.getUUID());
}

void SceneTrigger::updateTimer(const float iDeltaTime, const Entity& iTriggerEntity) {
	if (!m_timerRunning)
		return;
	m_timerElapsed += iDeltaTime;
	if (m_timerElapsed >= timerDuration) {
		const auto& func = callbackName.empty() ? std::string("on_timer") : callbackName;
		dispatchLuaCallbackNoArg(iTriggerEntity, func);
		if (timerRepeating)
			m_timerElapsed -= timerDuration;
		else
			m_timerRunning = false;
	}
}

void SceneTrigger::startTimer() {
	m_timerRunning = true;
	m_timerElapsed = 0.0f;
}

void SceneTrigger::stopTimer() { m_timerRunning = false; }

void SceneTrigger::resetTimer() { m_timerElapsed = 0.0f; }

}// namespace owl::scene
