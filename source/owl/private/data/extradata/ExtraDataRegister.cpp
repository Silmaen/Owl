/**
 * @file ExtraDataRegister.cpp
 * @author Silmaen
 * @date 19/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/extradata/ExtraDataRegister.h"
#include "data/extradata/ExtraDataRegister_Internal.h"
#include "data/geometry/extradata/TriangleNormals.h"
#include "data/geometry/extradata/TriangleUVCoordinate.h"
#include "data/geometry/extradata/VertexNormal.h"

using namespace owl::data::geometry::extradata;

namespace owl::data::extradata {
//---------------------------------------------------------------------------------------------------------------------
// size check
//---------------------------------------------------------------------------------------------------------------------

static_assert(sizeof(TriangleUVCoordinate) <= 48);
static_assert(sizeof(TriangleNormals) <= 48);
static_assert(sizeof(VertexNormal) <= 24);

using ExtraDataRegistrationScopeVector = std::vector<ExtraDataRegisterScope>;

namespace {

auto factoryExtraDataRegistration() -> ExtraDataRegistrationScopeVector {
	ExtraDataRegistrationScopeVector extraDataRegistrationScopes;

	extraDataRegistrationScopes.emplace_back(registerExtraDataInternal<TriangleUVCoordinate>());
	extraDataRegistrationScopes.emplace_back(registerExtraDataInternal<TriangleNormals>());
	extraDataRegistrationScopes.emplace_back(registerExtraDataInternal<VertexNormal>());

	return extraDataRegistrationScopes;
}

[[maybe_unused]] ExtraDataRegistrationScopeVector s_globalExtraDataRegistration = factoryExtraDataRegistration();

}// namespace

}// namespace owl::data::extradata
