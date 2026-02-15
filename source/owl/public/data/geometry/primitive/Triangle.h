/**
 * @file Triangle.h
 * @author Silmaen
 * @date 18/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/geometry/primitive/MeshVertex.h"

/**
 * @brief Namespace for geometry primitive objects.
 */
namespace owl::data::geometry::primitive {

/**
 * @brief Class Triangle.
 */
class OWL_API Triangle {
public:
	/**
	 * @brief Default constructor.
	 */
	Triangle();
	/**
	 * @brief Default destructor.
	 */
	~Triangle();

	Triangle(const Triangle&) = default;
	Triangle(Triangle&&) noexcept = default;
	auto operator=(const Triangle&) -> Triangle& = default;
	auto operator=(Triangle&&) noexcept -> Triangle& = default;

	/**
	 * @brief Get the index of the triangle.
	 * @return The index of the triangle.
	 */
	[[nodiscard]] auto getIndex() const -> uint32_t { return m_index; }

	/**
	 * @brief Set the index of the triangle.
	 * @param iIndex The index to set.
	 */
	void setIndex(const uint32_t iIndex) { m_index = iIndex; }

	/**
	 * @brief Get the vertex at the given index.
	 * @param iIndex The index of the vertex to get (0, 1 or 2).
	 * @return The vertex at the given index.
	 */
	[[nodiscard]] auto getVertex(uint8_t iIndex) -> MeshVertex*;
	/**
	 * @brief Get the vertex at the given index.
	 * @param iIndex The index of the vertex to get (0, 1 or 2).
	 * @return The vertex at the given index.
	 */
	[[nodiscard]] auto getVertex(uint8_t iIndex) const -> const MeshVertex*;

	/**
	 * @brief Get the vertices of the triangle.
	 * @return The vertices of the triangle.
	 */
	[[nodiscard]] auto getVertices() -> std::array<MeshVertex*, 3>& { return m_vertices; }

	/**
	 * @brief Get the vertices of the triangle.
	 * @return The vertices of the triangle.
	 */
	[[nodiscard]] auto getVertices() const -> const std::array<MeshVertex*, 3>& { return m_vertices; }
	/**
	 * @brief Set the vertex at the given index.
	 * @param iIndex The index of the vertex to set (0, 1 or 2).
	 * @param iVertex The vertex to set.
	 */
	void setVertex(uint8_t iIndex, MeshVertex* iVertex);

	/**
	 * @brief Calculate the normal of the triangle.
	 * @return The normal of the triangle.
	 */
	[[nodiscard]] auto getNormal() const -> math::vec3;

private:
	/// Index of the triangle.
	uint32_t m_index = std::numeric_limits<uint32_t>::max();
	/// Indices of the triangle vertices.
	[[maybe_unused]] std::array<MeshVertex*, 3> m_vertices{};
};

}// namespace owl::data::geometry::primitive
