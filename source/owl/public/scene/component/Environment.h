/**
 * @file Environment.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

#include <filesystem>

namespace owl::scene::component {

/// Luminance of a clear daylight sky, in cd/m²: the default of an environment.
constexpr float g_defaultSkyLuminance = 10000.f;

/**
 * @brief
 *  Image-based lighting of the 3D layers: an equirectangular HDR image (`.hdr`, e.g. from Poly Haven) lighting the
 *  meshes and voxel worlds (diffuse and reflections) and, optionally, drawn as the sky. The first visible one of the
 *  scene is used; without one, the layer ambient lights the meshes as a constant sky.
 */
struct OWL_API Environment {
	/// Asset path of the `.hdr` image.
	std::filesystem::path hdri;
	/// Luminance in cd/m² (nits) of a pixel value of 1 in the image, or of a white `ambient` without an image.
	float intensity = g_defaultSkyLuminance;
	/// Colour (sRGB) of the constant sky lighting the scene without an image.
	math::vec3 ambient{0.63f, 0.63f, 0.67f};
	/// Rotation of the image about the vertical axis, radians.
	float rotation = 0.f;
	/// Whether the image is drawn behind the scene as its sky.
	bool showSky = true;

	/**
	 * @brief
	 *  Name of the component.
	 * @return The name.
	 */
	static auto name() -> const char* { return "Environment"; }

	/**
	 * @brief
	 *  Key of the component in the scene files.
	 * @return The key.
	 */
	static auto key() -> const char* { return "Environment"; }

	/**
	 * @brief
	 *  Write the component.
	 * @param[in] iOut Serializer to write into.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read the component; missing keys keep their defaults.
	 * @param[in] iNode Serializer to read from.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
