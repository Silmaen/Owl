/**
 * @file Triangle.cpp
 * @author Silmaen
 * @date 18/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/geometry/primitive/Triangle.h"

namespace owl::data::geometry::primitive {

Triangle::Triangle() = default;

Triangle::~Triangle() = default;

[[nodiscard]] auto Triangle::getVertex(const uint8_t iIndex) -> MeshVertex* { return m_vertices.at(iIndex); }
[[nodiscard]] auto Triangle::getVertex(const uint8_t iIndex) const -> const MeshVertex* {
	return m_vertices.at(iIndex);
}
void Triangle::setVertex(const uint8_t iIndex, MeshVertex* iVertex) { m_vertices.at(iIndex) = iVertex; }

auto Triangle::getNormal() const -> math::vec3 {
	const auto& v0 = math::vec3(*m_vertices[0]);
	const auto& v1 = math::vec3(*m_vertices[1]);
	const auto& v2 = math::vec3(*m_vertices[2]);
	const math::vec3 edge1 = v1 - v0;
	const math::vec3 edge2 = v2 - v0;
	math::vec3 normal = edge1 ^ edge2;
	normal.normalize();
	return normal;
}

}// namespace owl::data::geometry::primitive
