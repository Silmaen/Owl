/**
 * @file UiText.h
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
 * @brief
 *  UI text widget component.
 *
 * Renders text in screen space within a Canvas hierarchy.
 */
struct OWL_API UiText {
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

	/**
	 * @brief
	 *  Get the class title.
	 * @return The display name of the component.
	 */
	static auto name() noexcept -> const char* { return "UI Text"; }

	/**
	 * @brief
	 *  Get the YAML key.
	 * @return The YAML serialization key.
	 */
	static auto key() noexcept -> const char* { return "UiText"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The serializer used as output.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param[in] iNode The serializer wrapping the source YAML node.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
