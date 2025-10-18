/**
 * @file Tag.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Application.h"
#include "core/Core.h"
#include "core/Serializer.h"
#include "data/fonts/Font.h"

namespace owl::scene::component {

/**
 * @brief A text rendering component.
 */
struct OWL_API Text {
	/// The text.
	std::string text;
	/// The tex's font.
	shared<data::fonts::Font> font = nullptr;
	/// The display color.
	math::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
	/// The kerning.
	float kerning = 0.0f;
	/// The line spacing.
	float lineSpacing = 0.0f;
	/**
	 * @brief Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Text Renderer"; }
	/**
	 * @brief Get the YAML key for this component
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "TextRenderer"; }

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
