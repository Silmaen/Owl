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

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

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
 *  Deserialize the components of an entity node: the mandatory ones (Transform, Visibility, Hierarchy) when present,
 *  then every optional registered component found, in registry order.
 * @param[in,out] ioEntity The Entity to fill.
 * @param[in] iEntityNode The YAML entity node (a map of component keys).
 * @param[in] iScratch Serializer whose `node` is pointed at each component value in turn (reused across calls).
 * @param[in] iMandatory Also read the mandatory components.
 */
inline void deserializeComponents(Entity& ioEntity, const core::YamlNode& iEntityNode, const core::Serializer& iScratch,
								  const bool iMandatory) {
	static constexpr size_t g_maxKeys = 64;
	std::array<core::YamlNode, g_maxKeys> values;
	std::array<std::string_view, g_maxKeys> keys;
	size_t count = 0;
	for (const auto child: iEntityNode) {
		if (count == g_maxKeys)
			break;
		keys[count] = child.getKey();
		values[count++] = child;
	}
	const auto lookup = [&](const std::string_view iKey) -> const core::YamlNode* {
		for (size_t i = 0; i < count; ++i)
			if (keys[i] == iKey)
				return &values[i];
		return nullptr;
	};
	auto& slot = iScratch.getImpl()->node;
	if (iMandatory) {
		if (const auto* value = lookup(Transform::key()); value != nullptr) {
			slot = *value;
			ioEntity.getComponent<Transform>().deserialize(iScratch);
		}
		if (const auto* value = lookup(Visibility::key()); value != nullptr) {
			slot = *value;
			ioEntity.getComponent<Visibility>().deserialize(iScratch);
		}
		if (const auto* value = lookup(Hierarchy::key()); value != nullptr) {
			slot = *value;
			ioEntity.getComponent<Hierarchy>().deserialize(iScratch);
		}
	}
	for (const auto& desc: ComponentRegistry::getAll()) {
		if (!desc.optional)
			continue;
		if (const auto* value = lookup(desc.key); value != nullptr) {
			slot = *value;
			desc.deserialize(ioEntity, iScratch);
		}
	}
	slot = {};
}

}// namespace owl::scene::component
