/**
 * @file Transform.h
 * @author Silmaen
 * @date 30/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/matrices.h"

namespace owl::math {
/**
 * @brief
 *  Clans handling object transformation.
 *
 * Object transformation is limited to Translation, rotation and scale.
 * when initialized by a matrix, Shear and skew parts will be ignored.
 */
class OWL_API Transform {
public:
	/**
	 * @brief
	 *  Constructor.
 	 */
	constexpr Transform() = default;

	/**
	 * @brief
	 *  Constructor with Translation rotation & scale.
	 * @param iTranslation The translation.
	 * @param iRotation The rotation.
	 * @param iScale Teh scale.
	 */
	Transform(const vec3& iTranslation, const vec3& iRotation, const vec3& iScale);

	/**
	 * @brief
	 *  Constructor with Translation & rotation.
	 * @param iTranslation The translation.
	 * @param iRotation The rotation.
	 */
	Transform(const vec3& iTranslation, const vec3& iRotation);

	/**
	 * @brief
	 *  Destructor.
	 */
	constexpr ~Transform() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	constexpr Transform(const Transform&) = default;

	/**
	 * @brief
	 *  Copy assignment.
	 * @return This object.
	 */
	constexpr auto operator=(const Transform&) -> Transform& = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	constexpr Transform(Transform&&) = default;

	/**
	 * @brief
	 *  Move assignment.
	 * @return This object.
	 */
	constexpr auto operator=(Transform&&) -> Transform& = default;

	// NOLINTBEGIN(google-explicit-constructor)
	// NOLINTBEGIN(hicpp-explicit-conversions)
	/**
	 * @brief
	 *  Constructor from matrix.
	 * @param iTransform The origin matrix.
	 */
	Transform(const mat4& iTransform);

	/**
	 * @brief
	 *  Convert to transformation matrix.
	 * @return Trhe transformation matrix.
	 */
	auto operator()() const -> mat4;
	// NOLINTEND(hicpp-explicit-conversions)
	// NOLINTEND(google-explicit-constructor)

	// Accessors.
	/**
	 * @brief
	 *  Const access to the translation component.
	 * @return The translation vector.
	 */
	[[nodiscard]] constexpr auto translation() const -> const vec3& { return m_translation; }

	/**
	 * @brief
	 *  Mutable access to the translation component.
	 * @return The translation vector.
	 */
	constexpr auto translation() -> vec3& { return m_translation; }

	/**
	 * @brief
	 *  Const access to the rotation component (Euler angles in radians).
	 * @return The rotation vector.
	 */
	[[nodiscard]] constexpr auto rotation() const -> const vec3& { return m_rotation; }

	/**
	 * @brief
	 *  Mutable access to the rotation component (Euler angles in radians).
	 * @return The rotation vector.
	 */
	constexpr auto rotation() -> vec3& { return m_rotation; }

	/**
	 * @brief
	 *  Const access to the scale component.
	 * @return The scale vector.
	 */
	[[nodiscard]] constexpr auto scale() const -> const vec3& { return m_scale; }

	/**
	 * @brief
	 *  Mutable access to the scale component.
	 * @return The scale vector.
	 */
	constexpr auto scale() -> vec3& { return m_scale; }

private:
	/// translation part.
	vec3 m_translation = {0.f, 0.f, 0.f};
	/// Rotation part.
	vec3 m_rotation = {0.f, 0.f, 0.f};
	/// Scale part.
	vec3 m_scale = {1.f, 1.f, 1.f};
};

}// namespace owl::math
