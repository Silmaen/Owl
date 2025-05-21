/**
 * @file components.h
 * @author Silmaen
 * @date 10/24/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Camera.h"
#include "CircleRenderer.h"
#include "EntityLink.h"
#include "ID.h"
#include "NativeScript.h"
#include "PhysicBody.h"
#include "Player.h"
#include "SpriteRenderer.h"
#include "Tag.h"
#include "Text.h"
#include "Transform.h"
#include "Trigger.h"

namespace owl::scene::component {

/**
 * @brief Concept that type is a component.
 */
template<typename Component>
concept isComponent = std::is_same_v<Component, Camera> || std::is_same_v<Component, CircleRenderer> ||
					  std::is_same_v<Component, EntityLink> || std::is_same_v<Component, ID> ||
					  std::is_same_v<Component, NativeScript> || std::is_same_v<Component, PhysicBody> ||
					  std::is_same_v<Component, Player> || std::is_same_v<Component, SpriteRenderer> ||
					  std::is_same_v<Component, Tag> || std::is_same_v<Component, Text> ||
					  std::is_same_v<Component, Transform> || std::is_same_v<Component, Trigger>;

/**
 * @brief Concept that type has a name() method.
 */
template<typename Component>
concept isNamedComponent = isComponent<Component> && requires {
	{ Component::name() } -> std::convertible_to<const char*>;
};

/**
 * @brief Concept that type is serializable.
 */
template<typename Component>
concept isSerializableComponent =
		isComponent<Component> && requires(const Component& iComponent, const core::Serializer& iOut) {
			{ iComponent.serialize(iOut) } -> std::same_as<void>;
			{ Component::key() } -> std::convertible_to<const char*>;
		};

/**
 * @brief Concept that type is deserializable.
 */
template<typename Component>
concept isDeserializableComponent =
		isComponent<Component> && requires(Component& iComponent, const core::Serializer& iNode) {
			{ iComponent.deserialize(iNode) } -> std::same_as<void>;
			{ Component::key() } -> std::convertible_to<const char*>;
		};

/**
 * @brief List of copiable components.
 * @note All except ID and Tag.
 */
using CopiableComponents =
		std::tuple<Transform, Camera, SpriteRenderer, CircleRenderer, Text, PhysicBody, Player, Trigger, EntityLink>;

/**
 * @brief List all serializable components.
 * @note All except ID which is serialized directly in the entity.
 */
using SerializableComponents = std::tuple<Tag, Transform, Camera, SpriteRenderer, CircleRenderer, Text, PhysicBody,
										  Player, Trigger, EntityLink>;

/**
 * @brief List all optional components.
 * @note All except ID, Tag & Transform that are mandatory.
 */
using OptionalComponents =
		std::tuple<Camera, SpriteRenderer, CircleRenderer, Text, PhysicBody, Player, Trigger, EntityLink>;

}// namespace owl::scene::component
