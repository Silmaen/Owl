/**
 * @file HudMarker.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"
#include "renderer/gpu/Texture.h"

#include <string>

namespace owl::scene::component {
/**
 * @brief
 *  HUD marker anchored to a world entity: an icon and a label drawn in pixels over the entity, and an indicator on
 *  the screen edge pointing at it when it leaves the view.
 *
 * It needs no canvas: the HUD pass projects the entity through the render layer it is routed to (2D, raycast,
 * isometric, voxel, 3D) and draws the marker under the HUD widgets, in reference pixels of a default canvas
 * (1280×720, scaled with the height and the player's `ui_scale`).
 */
struct OWL_API HudMarker {
	/// Whether the marker is drawn (`entity.set_hud_marker_enabled` switches it).
	bool enabled = true;
	/// Icon of the marker; a diamond of `color` without one.
	shared<renderer::gpu::Texture2D> icon = nullptr;
	/// Colour multiplied with the icon, also the colour of the edge arrow.
	math::vec4 color{1.f, 0.8f, 0.2f, 1.f};
	/// Size of the icon, in reference pixels.
	math::vec2 size{24.f, 24.f};
	/// Anchor of the marker relative to the entity origin, in world units (above the head of a character).
	math::vec3 offset{0.f, 0.f, 0.f};
	/// Text drawn under the icon; empty draws none.
	std::string label;
	/// Colour of the label.
	math::vec4 labelColor{1.f, 1.f, 1.f, 1.f};
	/// Font size of the label, in reference pixels.
	float fontSize = 14.f;
	/// Append the distance to the player (the camera without a `Player`) to the label, in whole world units.
	bool showDistance = false;
	/// Hide the marker farther than this distance; 0 never hides it.
	float maxDistance = 0.f;
	/// Keep the marker on the screen edge with an arrow when the entity is out of view; off hides it there.
	bool offscreenIndicator = true;
	/// Distance kept between an edge marker and the screen border, in reference pixels.
	float edgeMargin = 32.f;
	/// Sprite of the edge arrow, pointing right in the texture; a chevron of `color` without one.
	shared<renderer::gpu::Texture2D> arrow = nullptr;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() noexcept -> const char* { return "HUD Marker"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() noexcept -> const char* { return "HudMarker"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
