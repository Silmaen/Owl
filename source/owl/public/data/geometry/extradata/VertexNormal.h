/**
* @file VertexNormal.h
 * @author Silmaen
 * @date 19/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/component/MeshComponentExtraData.h"
#include "data/extradata/ExtraDataBase.h"

/**
 * @brief Namespace for geometry extra data.
 */
namespace owl::data::geometry::extradata {
/**
 * @brief
 *  Extra data to store normal for a vertex.
 */
class OWL_API VertexNormal : public data::extradata::MeshExtraData<VertexNormal, math::vec3> {
public:
	/**
	 * @brief Default constructor.
	 */
	VertexNormal();

	/**
	 * @brief Copy constructor.
	 */
	VertexNormal(const VertexNormal& iDataToCopy) = default;

	/**
	 * @brief Move constructor.
	 */
	VertexNormal(VertexNormal&&) = default;

	/**
	 * @brief Default destructor.
	 */
	~VertexNormal() override;

	/**
	 * @brief Assignment operator.
	 */
	auto operator=(const VertexNormal& iDataToCopy) -> VertexNormal& = default;

	/**
	 * @brief Move assignment operator.
	 */
	auto operator=(VertexNormal&&) -> VertexNormal& = default;

	/**
	 * @brief Get the normal of the vertex.
	 * @return The normal of the vertex.
	 */
	[[nodiscard]] auto getNormal() const -> const Type&;

	/**
	 * @brief Set the normal of the vertex.
	 * @param iNormal The normal to set.
	 */
	void setNormal(const Type& iNormal);

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
	[[nodiscard]] auto getValue() const -> const Type& override { return m_normal; }

private:
	/// normal for the vertex.
	Type m_normal;
};

}// namespace owl::data::geometry::extradata

// Define components for vertex normal
namespace owl::data::component {

/// @brief Read iterate component for vertex normal.
const inline ReadMeshVertexExtraData<geometry::extradata::VertexNormal> VertexNormal;
/// @brief Write iterate component for vertex normal.
const inline WriteMeshVertexExtraData<geometry::extradata::VertexNormal> WriteVertexNormal;

}// namespace owl::data::component
