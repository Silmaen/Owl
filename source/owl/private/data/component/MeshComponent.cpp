/**
 * @file MeshComponent.cpp
 * @author Silmaen
 * @date 20/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/component/MeshComponent.h"
#include "data/geometry/StaticMesh.h"

namespace owl::data::component {

template<bool IsConst>
MeshVertexCoordinate<IsConst>::MeshVertexCoordinate(Coordinate, CursorType& iCursor, const size_t iIndex,
													const bool iReset)
	: MeshComponentBase<IsConst, geometry::MeshElementType::Vertex>(iCursor) {
	if (iReset)
		reset(iIndex);
}

template<bool IsConst>
void MeshVertexCoordinate<IsConst>::reset(size_t iStart) {
	if constexpr (IsConst) {
		m_pointIte = this->m_cursor->getMesh()->getVertexIterator(iStart);
	} else {
		m_pointIte = const_cast<geometry::StaticMesh*>(this->m_cursor->getMesh())->getVertexIterator(iStart);
	}
}

template<bool IsConst>
MeshTriangleVertices<IsConst>::MeshTriangleVertices(TriangleVertices, CursorType& iCursor, const size_t iIndex,
													const bool iReset)
	: MeshComponentBase<IsConst, geometry::MeshElementType::Triangle>(iCursor) {
	if (iReset)
		reset(iIndex);
}

template<bool IsConst>
void MeshTriangleVertices<IsConst>::reset(size_t iStart) {
	if constexpr (IsConst) {
		m_pointIte = this->m_cursor->getMesh()->getTriangleIterator(iStart);
	} else {
		m_pointIte = const_cast<geometry::StaticMesh*>(this->m_cursor->getMesh())->getTriangleIterator(iStart);
	}
}

// Explicit template instantiation
template class MeshVertexCoordinate<true>;
template class MeshVertexCoordinate<false>;
template class MeshTriangleVertices<true>;
template class MeshTriangleVertices<false>;

EditMeshVertexCoordinate::EditMeshVertexCoordinate(EditCoordinate, CursorType& iCursor, size_t iIndex)
	: MeshVertexCoordinate(Coordinates, iCursor, iIndex) {
	reset(iIndex);
}

void EditMeshVertexCoordinate::setValue(const math::vec3& iPt) { m_pointIte->setPosition(iPt); }


}// namespace owl::data::component
