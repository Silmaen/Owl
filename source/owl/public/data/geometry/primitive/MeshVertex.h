/**
 * @file MeshVertex.h
 * @author Silmaen
 * @date 18/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::data::geometry::primitive {

/**
 * @brief Class MeshVertex.
 */
class OWL_API MeshVertex {
public:
	/**
	 * @brief Default constructor.
	 */
	MeshVertex();
	/**
	 * @brief Default destructor.
	 */
	virtual ~MeshVertex();
	/**
	 * @brief Default copy constructor.
	 */
	MeshVertex(const MeshVertex&) = default;
	/**
	 * @brief Default move constructor.
	 */
	MeshVertex(MeshVertex&&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const MeshVertex&) -> MeshVertex& = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(MeshVertex&&) -> MeshVertex& = default;

	/**
	 * @brief Constructor with position.
	 * @param iPosition The position of the vertex.
	 */
	explicit MeshVertex(const math::vec3& iPosition) : m_position(iPosition) {}

	/**
	 * @brief Assignment operator with position.
	 * @param iPosition The position to assign.
	 * @return The current vertex.
	 */
	auto operator=(const math::vec3& iPosition) -> MeshVertex& {
		m_position = iPosition;
		return *this;
	}

	/**
	 * @brief Compare operator.
	 * @param[in] iOther Other vertex to compare.
	 * @return True if the vertices are equal.
	 */
	constexpr auto operator==(const MeshVertex& iOther) const -> bool { return m_position == iOther.m_position; }
	/**
	 * @brief
	 *  Gets the vertex index.
	 * @return Vertex's index.
	 */
	[[nodiscard]] constexpr auto getIndex() const -> uint32_t { return m_index; }

	/**
	 * @brief
	 *  Sets the vertex index.
	 * @param[in] iIndex Index to set.
	 */
	constexpr void setIndex(const uint32_t iIndex) { m_index = iIndex; }


	/**
	 * @brief Get the position of the vertex.
	 * @return The position of the vertex.
	 */
	[[nodiscard]] constexpr auto getPosition() const -> const math::vec3& { return m_position; }

	/**
	 * @brief Get the position of the vertex.
	 * @return The position of the vertex.
	 */
	[[nodiscard]] constexpr auto getPosition() -> math::vec3& { return m_position; }

	/**
	 * @brief
	 * To convert to math::vec3
	 */
	explicit operator const math::vec3&() const { return m_position; }

	/**
	 * @brief Set the position of the vertex.
	 * @param iPosition The position of the vertex.
	 */
	void setPosition(const math::vec3& iPosition) { m_position = iPosition; }

private:
	/// Index of the vertex.
	uint32_t m_index = std::numeric_limits<uint32_t>::max();
	/// Position of the vertex.
	math::vec3 m_position;
};

}// namespace owl::data::geometry::primitive
