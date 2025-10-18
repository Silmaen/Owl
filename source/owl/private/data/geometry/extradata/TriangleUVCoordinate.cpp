/**
 * @file TriangleUVCoordinate.cpp
 * @author Silmaen
 * @date 19/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/geometry/extradata/TriangleUVCoordinate.h"

namespace owl::data::geometry::extradata {

TriangleUVCoordinate::~TriangleUVCoordinate() = default;

TriangleUVCoordinate::TriangleUVCoordinate()
	: m_uvCoords{math::vec2{0.0f, 0.0f}, math::vec2{0.0f, 0.0f}, math::vec2{0.0f, 0.0f}} {}

auto TriangleUVCoordinate::getStaticType() -> std::string { return "__OwlTriangleUVCoordinate"; }

auto TriangleUVCoordinate::getUvCoord(const size_t iVertId) const -> const math::vec2& {
	OWL_CORE_ASSERT(iVertId < 3, "Vertex ID must be 0, 1 or 2.")
	return m_uvCoords.at(iVertId);
}

void TriangleUVCoordinate::setUvCoord(const size_t iVertId, const math::vec2& iUvCoord) {
	OWL_CORE_ASSERT(iVertId < 3, "Vertex ID must be 0, 1 or 2.")
	m_uvCoords.at(iVertId) = iUvCoord;
}
}// namespace owl::data::geometry::extradata
