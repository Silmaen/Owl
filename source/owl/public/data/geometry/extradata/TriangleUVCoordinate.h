/**
 * @file TriangleUVCoordinate.h
 * @author Silmaen
 * @date 19/10/2025
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/extradata/ExtraDataBase.h"
#include "data/meshrange/MeshComponentExtraData.h"

namespace owl::data::geometry::extradata {
/**
 * @brief
 *  Extra data storing UV coordinates for each vertex of a triangle.
 */
class OWL_API TriangleUVCoordinate
	: public data::extradata::MeshExtraData<TriangleUVCoordinate, std::array<math::vec2, 3>> {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	TriangleUVCoordinate();

	/**
	 * @brief
	 *  Copy constructor.
	 * @param[in] iDataToCopy Source data to copy.
	 */
	TriangleUVCoordinate(const TriangleUVCoordinate& iDataToCopy) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	TriangleUVCoordinate(TriangleUVCoordinate&&) noexcept = default;

	/**
	 * @brief
	 *  Default destructor.
	 */
	~TriangleUVCoordinate() override;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @param[in] iDataToCopy Source data to copy.
	 * @return A reference to this object.
	 */
	auto operator=(const TriangleUVCoordinate& iDataToCopy) -> TriangleUVCoordinate& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(TriangleUVCoordinate&&) noexcept -> TriangleUVCoordinate& = default;

	/**
	 * @brief
	 *  Get the UV coordinate of the given vertex.
	 * @param[in] iVertId The vertex ID (0, 1 or 2).
	 * @return The UV coordinate of the given vertex.
	 */
	[[nodiscard]] auto getUvCoord(size_t iVertId) const -> const math::vec2&;

	/**
	 * @brief
	 *  Set the UV coordinate of the given vertex.
	 * @param[in] iVertId The vertex ID (0, 1 or 2).
	 * @param[in] iUvCoord The UV coordinate to set.
	 */
	void setUvCoord(size_t iVertId, const math::vec2& iUvCoord);

	/**
	 * @brief
	 *  Return the string identifier of the extra data.
	 * @note This key should be unique.
	 * @return Key of the extra data.
	 */
	static auto getStaticType() -> std::string;

	/**
	 * @brief
	 *  Get the underlying value.
	 * @return The stored value.
	 */
	[[nodiscard]] auto getValue() const -> const Type& override { return m_uvCoords; }

private:
	/// UV coordinates for each vertex of the triangle.
	Type m_uvCoords;
};

}// namespace owl::data::geometry::extradata

// Define components for triangle UV coordinates
namespace owl::data::meshrange {
/// Read-iterate component for triangle UV coordinates.
const inline ReadMeshTriangleExtraData<geometry::extradata::TriangleUVCoordinate> UvCoords;
/// Write-iterate component for triangle UV coordinates.
const inline WriteMeshTriangleExtraData<geometry::extradata::TriangleUVCoordinate> WriteUvCoords;

}// namespace owl::data::meshrange
