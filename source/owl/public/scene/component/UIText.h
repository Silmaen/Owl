/**
 * @file UIText.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "data/fonts/Font.h"

namespace owl::scene::component {

/**
 * @brief UI text widget component.
 *
 * Renders text in screen space within a Canvas hierarchy.
 */
struct OWL_API UIText {
	/// The text content.
	std::string text;
	/// The font (nullptr = default font).
	shared<data::fonts::Font> font = nullptr;
	/// Text colour.
	math::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
	/// Font size in pixels.
	float fontSize = 16.f;
	/// Text alignment.
	enum struct Alignment : uint8_t { Left, Center, Right };
	/// Current alignment.
	Alignment alignment = Alignment::Left;
	/// Kerning adjustment.
	float kerning = 0.f;
	/// Line spacing adjustment.
	float lineSpacing = 0.f;

	/// @brief Get the class title.
	static auto name() -> const char* { return "UI Text"; }
	/// @brief Get the YAML key.
	static auto key() -> const char* { return "UIText"; }

	/// @brief Write this component to a YAML context.
	void serialize(const core::Serializer& iOut) const;
	/// @brief Read this component from YAML node.
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
