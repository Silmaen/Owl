/**
 * @file Hierarchy.h
 * @author Silmaen
 * @date 04/08/26
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "core/UUID.h"

#include <vector>

namespace owl::scene::component {

/**
 * @brief Component describing parent-child hierarchy relationships between entities.
 *
 * Every entity has this component. Root entities have parentId == 0.
 * Transform and visibility are inherited from parent to children.
 */
struct OWL_API Hierarchy {
	/// Parent entity UUID (0 = root / no parent).
	core::UUID parentId{0};
	/// Ordered list of children entity UUIDs.
	std::vector<core::UUID> childrenIds;

	/**
	 * @brief Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Hierarchy"; }

	/**
	 * @brief Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Hierarchy"; }

	/**
	 * @brief Write this component to a YAML context.
	 * @param iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
