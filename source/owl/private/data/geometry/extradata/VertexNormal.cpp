/**
 * @file VertexNormal.cpp
 * @author Silmaen
 * @date 19/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/geometry/extradata/VertexNormal.h"

namespace owl::data::geometry::extradata {

VertexNormal::~VertexNormal() = default;

VertexNormal::VertexNormal() : m_normal{0.0f, 0.0f, 1.0f} {}

auto VertexNormal::getStaticType() -> std::string { return "__OwlVertexNormal"; }

auto VertexNormal::getNormal() const -> const math::vec3& { return m_normal; }

void VertexNormal::setNormal(const math::vec3& iNormal) { m_normal = iNormal; }

}// namespace owl::data::geometry::extradata
