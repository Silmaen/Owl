/**
 * @file TriangleNormals.h
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
 *  Extra data to store normals for a triangle.
 */
class OWL_API TriangleNormals : public data::extradata::MeshExtraData<TriangleNormals, std::array<math::vec3, 3>> {
public:
	/**
	 * @brief Default constructor.
	 */
	TriangleNormals();

	/**
	 * @brief Copy constructor.
	 */
	TriangleNormals(const TriangleNormals& iDataToCopy) = default;

	/**
	 * @brief Move constructor.
	 */
	TriangleNormals(TriangleNormals&&) = default;

	/**
	 * @brief Default destructor.
	 */
	~TriangleNormals() override;

	/**
	 * @brief Assignment operator.
	 */
	auto operator=(const TriangleNormals& iDataToCopy) -> TriangleNormals& = default;

	/**
	 * @brief Move assignment operator.
	 */
	auto operator=(TriangleNormals&&) -> TriangleNormals& = default;

	/**
	 * @brief Get the normal of the given vertex.
	 * @param iVertId The vertex ID (0, 1 or 2).
	 * @return The normal of the given vertex.
	 */
	[[nodiscard]] auto getNormal(size_t iVertId) const -> const math::vec3&;

	/**
	 * @brief Set the normal of the given vertex.
	 * @param iVertId The vertex ID (0, 1 or 2).
	 * @param iNormal The normal to set.
	 */
	void setNormal(size_t iVertId, const math::vec3& iNormal);

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
	[[nodiscard]] auto getValue() const -> const Type& override { return m_normals; }

private:
	/// normal for each vertex of the triangle.
	Type m_normals;
};

}// namespace owl::data::geometry::extradata

// Define components for triangle Normals
namespace owl::data::component {

/// @brief Read iterate component for triangle normals.
const inline ReadMeshTriangleExtraData<geometry::extradata::TriangleNormals> TriangleNormals;
/// @brief Write iterate component for triangle normals.
const inline WriteMeshTriangleExtraData<geometry::extradata::TriangleNormals> WriteTriangleNormals;

}// namespace owl::data::component
