/**
 * @file TriangleNormals.cpp
 * @author Silmaen
 * @date 19/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/geometry/extradata/TriangleNormals.h"

namespace owl::data::geometry::extradata {

TriangleNormals::~TriangleNormals() = default;

TriangleNormals::TriangleNormals()
	: m_normals{math::vec3{0.0f, 0.0f, 1.0f}, math::vec3{0.0f, 0.0f, 1.0f}, math::vec3{0.0f, 0.0f, 1.0f}} {}

auto TriangleNormals::getStaticType() -> std::string { return "__OwlTriangleNormals"; }

auto TriangleNormals::getNormal(const size_t iVertId) const -> const math::vec3& {
	OWL_CORE_ASSERT(iVertId < 3, "Vertex ID must be 0, 1 or 2.")
	return m_normals.at(iVertId);
}

void TriangleNormals::setNormal(const size_t iVertId, const math::vec3& iNormal) {
	OWL_CORE_ASSERT(iVertId < 3, "Vertex ID must be 0, 1 or 2.")
	m_normals.at(iVertId) = iNormal;
}

}// namespace owl::data::geometry::extradata
