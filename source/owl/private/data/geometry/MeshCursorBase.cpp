/**
 * @file MeshCursorBase.cpp
 * @author Silmaen
 * @date 20/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/geometry/MeshCursorBase.h"
#include "data/geometry/StaticMesh.h"

namespace owl::data::geometry {

MeshCursorBase<true, MeshElementType::Vertex>::MeshCursorBase(const MeshType& iMesh, const size_t iIndex)
	: m_mesh(&iMesh), m_meshSize(iMesh.getVertexCount()) {
	reset(iIndex);
}

MeshCursorBase<true, MeshElementType::Triangle>::MeshCursorBase(const MeshType& iMesh, const size_t iIndex)
	: BaseCursor(iMesh, iIndex) {
	m_meshSize = iMesh.getTriangleCount();
	reset(iIndex);
}

auto MeshCursorBase<true, MeshElementType::Vertex>::getExtraDataContainer(const core::FactoryPid& iPid) const
		-> const extradata::ExtraDataContainer* {
	return m_mesh->getVertexExtraData(iPid);
}
auto MeshCursorBase<false, MeshElementType::Vertex>::getExtraDataContainer(const core::FactoryPid& iPid) const
		-> const extradata::ExtraDataContainer* {
	return m_mesh->getVertexExtraData(iPid);
}
auto MeshCursorBase<true, MeshElementType::Triangle>::getExtraDataContainer(const core::FactoryPid& iPid) const
		-> const extradata::ExtraDataContainer* {
	return m_mesh->getTriangleExtraData(iPid);
}
auto MeshCursorBase<false, MeshElementType::Triangle>::getExtraDataContainer(const core::FactoryPid& iPid) const
		-> const extradata::ExtraDataContainer* {
	return m_mesh->getTriangleExtraData(iPid);
}

}// namespace owl::data::geometry
