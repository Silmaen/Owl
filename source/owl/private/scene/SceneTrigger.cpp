/**
 * @file SceneTrigger.cpp
 * @author Silmaen
 * @date 12/30/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/SceneTrigger.h"

#include "scene/Entity.h"

namespace owl::scene {

SceneTrigger::SceneTrigger() = default;

SceneTrigger::~SceneTrigger() = default;

void SceneTrigger::onTriggered(Entity& ioEntity) {
	auto scene = ioEntity.getScene();
	switch (type) {
		case TriggerType::Victory:
			m_triggered = true;
			scene->status = Scene::Status::Victory;
			break;
		case TriggerType::Death:
			m_triggered = true;
			scene->status = Scene::Status::Death;
			break;
	}
}

}// namespace owl::scene
