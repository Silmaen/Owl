/**
 * @file componentsSerialization.h
 * @author Silmaen
 * @date 1/29/25
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "core/SerializerImpl.h"
#include "scene/ComponentRegistry.h"
#include "scene/Entity.h"
#include "scene/component/components.h"

#include <string>

namespace owl::scene::component {

/**
 * @brief
 *  Serialize every registered component of an entity, in registry order.
 * @param[in] iEntity The Entity to serialize.
 * @param[in] iOut The YAML context (inside the entity map).
 */
inline void serializeComponents(const Entity& iEntity, const core::Serializer& iOut) {
	for (const auto& desc: ComponentRegistry::getAll()) desc.serialize(iEntity, iOut);
}

/**
 * @brief
 *  Deserialize one registered component of an entity when its key is in the entity node.
 * @param[in,out] ioEntity The Entity to fill.
 * @param[in] iNode The YAML entity node.
 * @param[in] iDesc The component descriptor.
 */
inline void deserializeComponent(Entity& ioEntity, const core::Serializer& iNode, const ComponentDescriptor& iDesc) {
	if (auto node = iNode.getImpl()->node[iDesc.key]; node) {
		const core::Serializer sNode;
		sNode.getImpl()->node.reset(node);
		iDesc.deserialize(ioEntity, sNode);
	}
}

/**
 * @brief
 *  Deserialize every optional registered component found in an entity node.
 * @param[in,out] ioEntity The Entity to fill.
 * @param[in] iNode The YAML entity node.
 */
inline void deserializeOptionalComponents(Entity& ioEntity, const core::Serializer& iNode) {
	for (const auto& desc: ComponentRegistry::getAll())
		if (desc.optional)
			deserializeComponent(ioEntity, iNode, desc);
}

}// namespace owl::scene::component
