/**
 * @file TriangleUVCoordinate.h
 * @author Silmaen
 * @date 19/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/component/MeshComponentExtraData.h"
#include "data/extradata/ExtraDataBase.h"

namespace owl::data::geometry::extradata {
/**
 * @brief
 *  Extra data to store UV coordinates for a triangle.
 */
class OWL_API TriangleUVCoordinate
	: public data::extradata::MeshExtraData<TriangleUVCoordinate, std::array<math::vec2, 3>> {
public:
	/**
	 * @brief Default constructor.
	 */
	TriangleUVCoordinate();

	/**
	 * @brief Copy constructor.
	 */
	TriangleUVCoordinate(const TriangleUVCoordinate& iDataToCopy) = default;

	/**
	 * @brief Move constructor.
	 */
	TriangleUVCoordinate(TriangleUVCoordinate&&) = default;

	/**
	 * @brief Default destructor.
	 */
	~TriangleUVCoordinate() override;

	/**
	 * @brief Assignment operator.
	 */
	auto operator=(const TriangleUVCoordinate& iDataToCopy) -> TriangleUVCoordinate& = default;

	/**
	 * @brief Move assignment operator.
	 */
	auto operator=(TriangleUVCoordinate&&) -> TriangleUVCoordinate& = default;

	/**
	 * @brief Get the UV coordinate of the given vertex.
	 * @param iVertId The vertex ID (0, 1 or 2).
	 * @return The UV coordinate of the given vertex.
	 */
	[[nodiscard]] auto getUvCoord(size_t iVertId) const -> const math::vec2&;

	/**
	 * @brief Set the UV coordinate of the given vertex.
	 * @param iVertId The vertex ID (0, 1 or 2).
	 * @param iUvCoord The UV coordinate to set.
	 */
	void setUvCoord(size_t iVertId, const math::vec2& iUvCoord);

	/**
	 * @brief
	 *  Return the string identifier of the extra data
	 * @note
	 *  This Key should be unique
	 * @return Key of the extra data
	 */
	static auto getStaticType() -> std::string;

	/**
	 * @brief Get the underlying value.
	 * @return The stored value.
	 */
	[[nodiscard]] auto getValue() const -> const Type& override { return m_uvCoords; }

	/**
	 * @brief Set the underlying value.
	 * @param iValue The value to set.
	 */
	void setValue(const Type& iValue) override { m_uvCoords = iValue; }

	/**
	 * @brief Set the underlying value.
	 * @param iValue The value to set.
	 */
	void setValue(Type&& iValue) override { m_uvCoords = std::move(iValue); }

private:
	/// UV coordinates for each vertex of the triangle.
	Type m_uvCoords;
};

}// namespace owl::data::geometry::extradata

// Define components for triangle UV coordinates
namespace owl::data::component {

/// @brief Read iterate component for triangle UV coordinates.
inline constexpr ReadMeshTriangleExtraData<geometry::extradata::TriangleUVCoordinate> UvCoords;
/// @brief Write iterate component for triangle UV coordinates.
inline constexpr WriteMeshTriangleExtraData<geometry::extradata::TriangleUVCoordinate> WriteUvCoords;

}// namespace owl::data::component
