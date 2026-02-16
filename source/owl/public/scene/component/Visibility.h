/**
 * @file Visibility.h
 * @author Silmaen
 * @date 02/16/26
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"

namespace owl::scene::component {

/**
 * @brief A visibility component controlling rendering in editor and game modes.
 */
struct OWL_API Visibility {
	/// Whether the entity is visible during gameplay (serialized).
	bool gameVisible = true;
	/// Whether the entity is visible in the editor viewport (NOT serialized, transient).
	bool editorVisible = true;

	/**
	 * @brief Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Visibility"; }

	/**
	 * @brief Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Visibility"; }

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
