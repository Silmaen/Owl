/**
 * @file Transform.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Core.h"
namespace owl::scene::component {

/**
 * @brief The transformation component.
 */
struct OWL_API Transform {
	/// The translation.
	math::vec3 translation = {0.f, 0.f, 0.f};
	/// The rotation.
	math::vec3 rotation = {0.f, 0.f, 0.f};
	/// The scale.
	math::vec3 scale = {1.f, 1.f, 1.f};

	/**
	 * @brief Get the transformation matrix based on TRS.
	 * @return The transformation matrix.
	 */
	[[nodiscard]] math::mat4 getTransform() const {
		return math::translate(math::identity<float,4>(), translation) * math::toMat4(math::quat(rotation)) *
			   math::scale(math::identity<float,4>(), scale);
	}
};

}// namespace owl::scene::component
