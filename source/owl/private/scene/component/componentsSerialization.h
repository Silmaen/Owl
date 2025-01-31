/**
 * @file componentsSerialization.h
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "core/SerializerImpl.h"
#include "scene/component/components.h"

namespace owl::scene::component {

/**
 * @brief Serialize a single component.
 * @tparam Component The Serializable component type.
 * @param iEntity The Entity to serialize.
 * @param iOut The YAML context.
 */
template<isSerializableComponent Component>
void serializeComponent(const Entity& iEntity, const core::Serializer& iOut) {
	if (iEntity.hasComponent<Component>()) {
		iEntity.getComponent<Component>().serialize(iOut);
	}
}

/**
 * @brief Serialize a list of component.
 * @tparam Components The Serializable component types (deduced from the last parameter).
 * @param iEntity The Entity to serialize.
 * @param iOut The YAML context.
 */
template<isSerializableComponent... Components>
void serializeComponents(const Entity& iEntity, const core::Serializer& iOut, std::tuple<Components...>) {
	(..., serializeComponent<Components>(iEntity, iOut));
}

/**
 * @brief Deserialize a single component.
 * @tparam Component The Serializable component type.
 * @param iEntity The Entity to deserialize.
 * @param iNode The YAML context.
 */
template<isDeserializableComponent Component>
void deserializeComponent(Entity& iEntity, const core::Serializer& iNode) {
	if (auto node = iNode.getImpl()->node[Component::key()]; node) {
		auto& comp = iEntity.addComponent<Component>();
		const core::Serializer sNode;
		sNode.getImpl()->node.reset(node);
		comp.deserialize(sNode);
	}
}

/**
 * @brief Deserialize a list of components.
 * @tparam Components The Serializable component types (deduced from the last parameter).
 * @param iEntity The Entity to deserialize.
 * @param iNode The YAML context.
 */
template<isDeserializableComponent... Components>
void deserializeComponents(Entity& iEntity, const core::Serializer& iNode, std::tuple<Components...>) {
	(..., deserializeComponent<Components>(iEntity, iNode));
}

}// namespace owl::scene::component
