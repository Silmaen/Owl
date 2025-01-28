/**
 * @file Entity.cpp
 * @author Silmaen
 * @date 22/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/Entity.h"

namespace owl::scene {

Entity::Entity(const entt::entity iHandle, Scene* iScene) : m_entityHandle{iHandle}, mp_scene{iScene} {}

}// namespace owl::scene
