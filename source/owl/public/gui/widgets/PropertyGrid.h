/**
 * @file PropertyGrid.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <span>
#include <string>

namespace owl::gui::widgets {

/**
 * @brief
 *  Begin a property grid: one row per property, the label on the left (38 % of the width), the value on the right,
 *  then a narrow column holding a reset button when the value differs from its default.
 * @param[in] iId Unique ImGui identifier of the grid.
 * @return True if the grid is visible; call `endProperties` only then.
 */
OWL_API auto beginProperties(const char* iId) -> bool;

/**
 * @brief
 *  End the grid opened by `beginProperties`.
 */
OWL_API void endProperties();

/**
 * @brief
 *  Start a row: draw the label, then move to the value column with the item width set to fill it.
 * @param[in] iLabel The label.
 * @param[in] iTooltip Help shown when the label is hovered, or null.
 */
OWL_API void propertyLabel(const char* iLabel, const char* iTooltip = nullptr);

/**
 * @brief
 *  End a row: draw the reset button in the last column when the value is modified.
 * @param[in] iModified Whether the value differs from its default.
 * @return True when the reset button was clicked.
 */
OWL_API auto propertyReset(bool iModified) -> bool;

/**
 * @brief
 *  A row of draggable number fields, each with a thin coloured axis mark (X red, Y green, Z blue, W muted) instead
 *  of a saturated box.
 * @param[in] iId Unique ImGui identifier.
 * @param[in,out] ioValues Two to four components.
 * @param[in] iSpeed Drag speed.
 * @param[in] iFormat Number format.
 * @param[in] iWidth Total width of the row, 0 to fill the available width.
 * @return True if a component changed.
 */
OWL_API auto vectorField(const char* iId, std::span<float> ioValues, float iSpeed = 0.1f, const char* iFormat = "%.2f",
						 float iWidth = 0.f) -> bool;

/**
 * @brief
 *  Property row holding a draggable float, with reset to its default.
 * @param[in] iLabel The label.
 * @param[in,out] ioValue The value.
 * @param[in] iDefault The value the reset button restores.
 * @param[in] iSpeed Drag speed.
 * @param[in] iFormat Number format.
 * @param[in] iTooltip Help shown on the label, or null.
 * @return True if the value changed.
 */
OWL_API auto propertyFloat(const char* iLabel, float& ioValue, float iDefault, float iSpeed = 0.1f,
						   const char* iFormat = "%.3f", const char* iTooltip = nullptr) -> bool;

/**
 * @brief
 *  Property row holding a three-component vector, with reset to its default.
 * @param[in] iLabel The label.
 * @param[in,out] ioValue The value.
 * @param[in] iDefault The value the reset button restores.
 * @param[in] iSpeed Drag speed.
 * @param[in] iTooltip Help shown on the label, or null.
 * @return True if the value changed.
 */
OWL_API auto propertyVec3(const char* iLabel, math::vec3& ioValue, const math::vec3& iDefault, float iSpeed = 0.1f,
						  const char* iTooltip = nullptr) -> bool;

/**
 * @brief
 *  Property row holding a check box, with reset to its default.
 * @param[in] iLabel The label.
 * @param[in,out] ioValue The value.
 * @param[in] iDefault The value the reset button restores.
 * @param[in] iTooltip Help shown on the label, or null.
 * @return True if the value changed.
 */
OWL_API auto propertyBool(const char* iLabel, bool& ioValue, bool iDefault, const char* iTooltip = nullptr) -> bool;

/**
 * @brief
 *  Property row holding a draggable integer kept in a range, with reset to its default.
 * @param[in] iLabel The label.
 * @param[in,out] ioValue The value.
 * @param[in] iDefault The value the reset button restores.
 * @param[in] iMin Smallest value.
 * @param[in] iMax Largest value.
 * @param[in] iFormat Number format.
 * @param[in] iTooltip Help shown on the label, or null.
 * @return True if the value changed.
 */
OWL_API auto propertyInt(const char* iLabel, int& ioValue, int iDefault, int iMin, int iMax, const char* iFormat = "%d",
						 const char* iTooltip = nullptr) -> bool;

/**
 * @brief
 *  Property row holding a single-line text field.
 * @param[in] iLabel The label.
 * @param[in,out] ioValue The text.
 * @param[in] iHint Text shown while the field is empty, or null.
 * @param[in] iTooltip Help shown on the label, or null.
 * @return True if the text changed.
 */
OWL_API auto propertyText(const char* iLabel, std::string& ioValue, const char* iHint = nullptr,
						  const char* iTooltip = nullptr) -> bool;

}// namespace owl::gui::widgets
