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
concept isSerializableComponent = isComponent<Component> && requires(const Component& t, YAML::Emitter& ioOut) {
	{ t.serialize(ioOut) } -> std::same_as<void>;
	{ Component::key() } -> std::convertible_to<const char*>;
};

/**
 * @brief Concept that type is deserializable.
 */
template<typename Component>
concept isDeserializableComponent = isComponent<Component> && requires(Component& t, const YAML::Node& iNode) {
	{ t.deserialize(iNode) } -> std::same_as<void>;
	{ Component::key() } -> std::convertible_to<const char*>;
};

/**
 * @brief List of copiable components.
 * @note All except ID and Tag.
 */
using copiableComponents =
		std::tuple<Transform, Camera, SpriteRenderer, CircleRenderer, Text, PhysicBody, Player, Trigger, EntityLink>;

/**
 * @brief List all serializable components.
 * @note All except ID which is serialized directly in the entity.
 */
using serializableComponents = std::tuple<Tag, Transform, Camera, SpriteRenderer, CircleRenderer, Text, PhysicBody,
										  Player, Trigger, EntityLink>;

/**
 * @brief List all optional components.
 * @note All except ID, Tag & Transform that are mandatory.
 */
using optionalComponents =
		std::tuple<Camera, SpriteRenderer, CircleRenderer, Text, PhysicBody, Player, Trigger, EntityLink>;

/**
 * @brief Serialize a single component.
 * @tparam Component The Serializable component type.
 * @param iEntity The Entity to serialize.
 * @param ioOut The YAML context.
 */
template<isSerializableComponent Component>
void serializeComponent(const Entity& iEntity, YAML::Emitter& ioOut) {
	if (iEntity.hasComponent<Component>()) {
		iEntity.getComponent<Component>().serialize(ioOut);
	}
}

/**
 * @brief Serialize a list of component.
 * @tparam Components The Serializable component types (deduced from the last parameter).
 * @param iEntity The Entity to serialize.
 * @param ioOut The YAML context.
 */
template<isSerializableComponent... Components>
void serializeComponents(const Entity& iEntity, YAML::Emitter& ioOut, std::tuple<Components...>) {
	(..., serializeComponent<Components>(iEntity, ioOut));
}

/**
 * @brief Deserialize a single component.
 * @tparam Component The Serializable component type.
 * @param iEntity The Entity to deserialize.
 * @param iNode The YAML context.
 */
template<isDeserializableComponent Component>
void deserializeComponent(Entity& iEntity, const YAML::Node& iNode) {
	if (auto node = iNode[Component::key()]; node) {
		auto& comp = iEntity.addComponent<Component>();
		comp.deserialize(node);
	}
}

/**
 * @brief Deserialize a list of components.
 * @tparam Components The Serializable component types (deduced from the last parameter).
 * @param iEntity The Entity to deserialize.
 * @param iNode The YAML context.
 */
template<isDeserializableComponent... Components>
void deserializeComponents(Entity& iEntity, const YAML::Node& iNode, std::tuple<Components...>) {
	(..., deserializeComponent<Components>(iEntity, iNode));
}

}// namespace owl::scene::component
