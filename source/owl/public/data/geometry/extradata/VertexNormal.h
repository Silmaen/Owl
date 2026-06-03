/**
 * @file VertexNormal.h
 * @author Silmaen
 * @date 19/10/2025
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/extradata/ExtraDataBase.h"
#include "data/meshrange/MeshComponentExtraData.h"

/**
 * @brief
 *  Namespace for geometry extra data.
 */
namespace owl::data::geometry::extradata {
/**
 * @brief
 *  Extra data storing the normal of a single vertex.
 */
class OWL_API VertexNormal : public data::extradata::MeshExtraData<VertexNormal, math::vec3> {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	VertexNormal();

	/**
	 * @brief
	 *  Copy constructor.
	 * @param[in] iDataToCopy Source data to copy.
	 */
	VertexNormal(const VertexNormal& iDataToCopy) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	VertexNormal(VertexNormal&&) noexcept = default;

	/**
	 * @brief
	 *  Default destructor.
	 */
	~VertexNormal() override;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @param[in] iDataToCopy Source data to copy.
	 * @return A reference to this object.
	 */
	auto operator=(const VertexNormal& iDataToCopy) -> VertexNormal& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(VertexNormal&&) noexcept -> VertexNormal& = default;

	/**
	 * @brief
	 *  Get the normal of the vertex.
	 * @return The normal of the vertex.
	 */
	[[nodiscard]] auto getNormal() const -> const Type&;

	/**
	 * @brief
	 *  Set the normal of the vertex.
	 * @param[in] iNormal The normal to set.
	 */
	void setNormal(const Type& iNormal);

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
	[[nodiscard]] auto getValue() const -> const Type& override { return m_normal; }

private:
	/// Normal for the vertex.
	Type m_normal;
};

}// namespace owl::data::geometry::extradata

// Define components for vertex normal
namespace owl::data::meshrange {
/// Read-iterate component for vertex normal.
const inline ReadMeshVertexExtraData<geometry::extradata::VertexNormal> VertexNormal;
/// Write-iterate component for vertex normal.
const inline WriteMeshVertexExtraData<geometry::extradata::VertexNormal> WriteVertexNormal;

}// namespace owl::data::meshrange
