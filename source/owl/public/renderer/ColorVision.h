/**
 * @file ColorVision.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "gpu/Texture.h"
#include "math/matrices.h"
#include "math/vectors.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace owl::renderer {

/**
 * @brief
 *  A colour vision deficiency, the missing cone of a dichromat.
 */
enum struct ColorDeficiency : uint8_t {
	None,///< Typical colour vision: no filter.
	Protanopia,///< No long-wavelength (red) cones.
	Deuteranopia,///< No medium-wavelength (green) cones.
	Tritanopia,///< No short-wavelength (blue) cones.
};

/**
 * @brief
 *  What the colour vision filter does with its deficiency.
 */
enum struct ColorVisionMode : uint8_t {
	Correct,///< Moves the colours a dichromat confuses towards ones they tell apart (player setting).
	Simulate,///< Shows the frame as a dichromat sees it (designer preview).
};

/**
 * @brief
 *  The colour vision filter of the final frame.
 */
struct OWL_API ColorVisionFilter {
	/// The deficiency, `None` for no filter.
	ColorDeficiency deficiency = ColorDeficiency::None;
	/// Correction or simulation.
	ColorVisionMode mode = ColorVisionMode::Correct;
	/// Strength between 0 (no change) and 1 (full correction or dichromacy).
	float strength = 1.f;

	/**
	 * @brief
	 *  Check the filter changes the frame.
	 * @return True with a deficiency and a strength above 0.
	 */
	[[nodiscard]] auto isActive() const -> bool { return deficiency != ColorDeficiency::None && strength > 0.f; }

	/**
	 * @brief
	 *  The 3×3 matrix the filter applies to the linear RGB colour (simulation of Machado et al. 2009 at full severity,
	 *  correction by redistribution of the lost contrast to the channels still seen), blended with the identity by
	 *  the strength.
	 * @return The matrix (row, column).
	 */
	[[nodiscard]] auto getMatrix() const -> math::mat3;
};

/**
 * @brief
 *  Settings name of a deficiency (`none`, `protanopia`, `deuteranopia`, `tritanopia`).
 * @param[in] iDeficiency The deficiency.
 * @return The name.
 */
OWL_API auto toName(ColorDeficiency iDeficiency) -> std::string_view;

/**
 * @brief
 *  Deficiency of a settings name.
 * @param[in] iName The name, case sensitive.
 * @return The deficiency, empty for an unknown name.
 */
OWL_API auto colorDeficiencyFromName(std::string_view iName) -> std::optional<ColorDeficiency>;

/**
 * @brief
 *  Last pass of a frame: applies the colour vision filter to the whole display image (world, overlays and HUD).
 */
class OWL_API ColorVision final {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	ColorVision() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~ColorVision() = default;

	/**
	 * @brief
	 *  Copy constructor (deleted).
	 */
	ColorVision(const ColorVision&) = delete;

	/**
	 * @brief
	 *  Move constructor (deleted).
	 */
	ColorVision(ColorVision&&) = delete;

	/**
	 * @brief
	 *  Copy assignment (deleted).
	 * @return This pass.
	 */
	auto operator=(const ColorVision&) -> ColorVision& = delete;

	/**
	 * @brief
	 *  Move assignment (deleted).
	 * @return This pass.
	 */
	auto operator=(ColorVision&&) -> ColorVision& = delete;

	/**
	 * @brief
	 *  Create the draw and the uniform block.
	 */
	static void init();

	/**
	 * @brief
	 *  Release the draw and the uniform block.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Draw the display image through the filter into the bound target, pixel for pixel.
	 * @param[in] iDisplay The display-encoded image, of the size of the target.
	 * @param[in] iFilter The filter.
	 * @param[in] iFrameSize Size of the frame in pixels.
	 */
	static void draw(const shared<gpu::Texture2D>& iDisplay, const ColorVisionFilter& iFilter,
					 const math::vec2ui& iFrameSize);
};

}// namespace owl::renderer
