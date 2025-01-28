/**
 * @file Tag.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "fonts/Font.h"
#include "math/YamlSerializers.h"
#include "core/Application.h"
#include "core/Core.h"

namespace owl::scene::component {

/**
 * @brief A text rendering component.
 */
struct OWL_API Text {
	/// The text.
	std::string text;
	/// The tex's font.
	shared<fonts::Font> font = nullptr;
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
	 * @param ioOut The YAML context.
	 */
	void serialize(YAML::Emitter& ioOut) const {
		ioOut << YAML::Key << key();
		ioOut << YAML::BeginMap;
		ioOut << YAML::Key << "color" << YAML::Value << color;
		ioOut << YAML::Key << "kerning" << YAML::Value << kerning;
		ioOut << YAML::Key << "lineSpacing" << YAML::Value << lineSpacing;
		ioOut << YAML::Key << "text" << YAML::Value << text;
		if (font && !font->isDefault()) {
			ioOut << YAML::Key << "font" << YAML::Value << font->getName();
		}
		ioOut << YAML::EndMap;
	}

	/**
	 * @brief Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const YAML::Node& iNode) {
		color = iNode["color"].as<math::vec4>();
		kerning = iNode["kerning"].as<float>();
		lineSpacing = iNode["lineSpacing"].as<float>();
		text = iNode["text"].as<std::string>();
		if (core::Application::instanced()) {
			auto& lib = core::Application::get().getFontLibrary();
			if (iNode["font"]) {
				font = lib.getFont(iNode["font"].as<std::string>());
			} else {
				font = lib.getDefaultFont();
			}
		}
	}
};

}// namespace owl::scene::component
