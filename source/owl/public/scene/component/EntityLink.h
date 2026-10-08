/**
 * @file EntityLink.h
 * @author Silmaen
 * @date 1/1/25
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/UUID.h"
#include "scene/Entity.h"

#include <string>

namespace owl::scene::component {
/**
 * @brief
 *  Component to describe link between entities.
 *
 * The target is referenced by UUID (`linkedEntityId`), so renaming it keeps the link and duplicating a linked group
 * relinks the copies together. The name stays for display and as a fallback: a link without UUID, or whose UUID is
 * not in the scene (prefab instantiated elsewhere), binds to the first entity of that name.
 */
struct OWL_API EntityLink {
	/// UUID of the linked entity (0: not bound yet, resolved from `linkedEntityName`).
	core::UUID linkedEntityId{0};
	/// The name of the linked entity (display, and fallback when the UUID is unset or absent).
	std::string linkedEntityName;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Entity Link"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "EntityLink"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
	/// The linked entity (runtime-resolved from `linkedEntityName`, invalid while the target is missing).
	Entity linkedEntity;
	/// Runtime flag: the missing target was already reported, so the warning is emitted once (not serialized).
	bool wasUnresolvedReported = false;
};
}// namespace owl::scene::component
