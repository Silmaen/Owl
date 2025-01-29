/**
 * @file Theme.h
 * @author Silmaen
 * @date 10/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "math/vectors.h"

namespace owl::gui {

/**
 * @brief Definition  of the GUI theme.
 */
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
struct OWL_API Theme {
	math::vec4 text{0.753f, 0.753f, 0.753f, 1.0};
	math::vec4 windowBackground{0.1f, 0.105f, 0.11f, 1.0f};
	math::vec4 childBackground{0.141f, 0.141f, 0.141f, 1.0f};
	math::vec4 backgroundPopup{0.196f, 0.196f, 0.196f, 1.0f};
	math::vec4 border{0.102f, 0.102f, 0.102f, 1.0f};

	math::vec4 frameBackground{0.0588f, 0.0588f, 0.0588f, 1.0};
	math::vec4 frameBackgroundHovered{0.0588f, 0.0588f, 0.0588f, 1.0};
	math::vec4 frameBackgroundActive{0.0588f, 0.0588f, 0.0588f, 1.0};

	math::vec4 titleBar{0.0824f, 0.0824f, 0.0824f, 1.0f};
	math::vec4 titleBarActive{0.0824f, 0.0824f, 0.0824f, 1.0f};
	math::vec4 titleBarCollapsed{0.15f, 0.1505f, 0.151f, 1.0f};

	math::vec4 menubarBackground{0.0f, 0.0f, 0.0f, 0.0f};

	math::vec4 scrollbarBackground{0.02f, 0.02f, 0.02f, 0.53f};
	math::vec4 scrollbarGrab{0.31f, 0.31f, 0.31f, 1.0f};
	math::vec4 scrollbarGrabHovered{0.41f, 0.41f, 0.41f, 1.0f};
	math::vec4 scrollbarGrabActive{0.51f, 0.51f, 0.51f, 1.0f};

	math::vec4 checkMark{0.753f, 0.753f, 0.753f, 1.0};

	math::vec4 sliderGrab{0.51f, 0.51f, 0.51f, 0.7f};
	math::vec4 sliderGrabActive{0.66f, 0.66f, 0.66f, 1.0f};

	math::vec4 button{0.22f, 0.22f, 0.22f, 0.784f};
	math::vec4 buttonHovered{0.275f, 0.275f, 0.275f, 1.0f};
	math::vec4 buttonActive{0.22f, 0.22f, 0.22f, 0.588f};

	math::vec4 groupHeader{0.184f, 0.184f, 0.184f, 1.0f};
	math::vec4 groupHeaderHovered{0.184f, 0.184f, 0.184f, 1.0f};
	math::vec4 groupHeaderActive{0.184f, 0.184f, 0.184f, 1.0f};

	math::vec4 separator{0.102f, 0.102f, 0.102f, 1.0f};
	math::vec4 separatorActive{0.153f, 0.725f, 0.949f, 1.0f};
	math::vec4 separatorHovered{0.153f, 0.725f, 0.949f, 0.588f};

	math::vec4 resizeGrip{0.91f, 0.91f, 0.91f, 0.25f};
	math::vec4 resizeGripHovered{0.81f, 0.81f, 0.81f, 0.67f};
	math::vec4 resizeGripActive{0.46f, 0.46f, 0.46f, 0.95f};

	math::vec4 tabHovered{0.149f, 0.157f, 0.076f, 1.0f};
	math::vec4 tab{0.0824f, 0.0824f, 0.0824f, 1.0f};
	math::vec4 tabSelected{0.051f, 0.051f, 0.051f, 1.0f};
	math::vec4 tabSelectedOverline{0.898f, 0.706f, 0.051f, 1.0f};
	math::vec4 tabDimmed{0.0824f, 0.0824f, 0.0824f, 1.0f};
	math::vec4 tabDimmedSelected{0.072f, 0.072f, 0.072f, 1.0f};
	math::vec4 tabDimmedSelectedOverline{0.339f, 0.321f, 0.210f, 1.0f};

	math::vec4 dockingPreview{0.898f, 0.706f, 0.051f, 0.67f};
	math::vec4 dockingEmptyBackground{0.2f, 0.2f, 0.2f, 1.f};

	math::vec4 highlight{0.153f, 0.725f, 0.949f, 1.0f};

	math::vec4 propertyField{0.0588f, 0.0588f, 0.0588f, 1.0};

	float windowRounding{7.f};
	float frameRounding{2.5f};
	float frameBorderSize{1.0f};
	float indentSpacing{11.f};

	float tabRounding{10.f};
	float tabOverline{1.f};
	float tabBorder{1.f};

	float controlsRounding{9};
	/**
	 * @brief Load from a yaml config file.
	 * @param[in] iFile The file to load.
	 */
	void loadFromFile(const std::filesystem::path& iFile);

	/**
	 * @brief Save To a yaml file.
	 * @param[in] iFile The file to save.
	 */
	void saveToFile(const std::filesystem::path& iFile) const;
};
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

}// namespace owl::gui
