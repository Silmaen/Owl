/**
 * @file IsometricObject.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "math/vectors.h"
#include "renderer/IsometricObjects.h"

#include <filesystem>
#include <string>

namespace owl::scene::component {
/**
 * @brief
 *  A building or prop of an isometric object sheet, drawn as one depth-correct slice per footprint tile.
 *
 * The entity translation is the back corner of the object's footprint (the corner farthest from the viewer); every
 * slice stands on the front corner of its tile. Rotation and scale are ignored. Only the isometric render layer
 * draws it.
 *
 * On-disk form:
 * ```yaml
 * IsometricObject:
 *   atlasPath: textures/iso/buildings.json
 *   building: townhouse
 *   color: [1, 1, 1, 1]
 * ```.
 */
struct OWL_API IsometricObject {
	/// Relative path of the sheet description (`buildings.json`); the texture is the `.png` beside it.
	std::filesystem::path atlasPath;
	/// Name of the object in the sheet.
	std::string building;
	/// Tint multiplied with the sprites.
	math::vec4 color{1.f, 1.f, 1.f, 1.f};
	/// Resolved sheet (runtime only, built from the path at the next scene resolve).
	shared<renderer::IsometricObjectSet> atlas;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Isometric Object"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "IsometricObject"; }

	/**
	 * @brief
	 *  Get the resolved object of the sheet.
	 * @return The object, or null while the sheet is not resolved or has no object of that name.
	 */
	[[nodiscard]] auto getObject() const -> const renderer::IsometricObjectDef*;

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
