/**
 * @file AssetThumbnails_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/AssetThumbnails.h"
#include "testHelper.h"

using namespace owl::nest::panel;

TEST(AssetThumbnails, ClassifiesTheFilesItCanPreview) {
	EXPECT_EQ(AssetThumbnails::getKind("materials/gold.owlmat"), AssetThumbnails::Kind::Material);
	EXPECT_EQ(AssetThumbnails::getKind("prefabs/door.owlprefab"), AssetThumbnails::Kind::Prefab);
	EXPECT_EQ(AssetThumbnails::getKind("meshes/crate.glb"), AssetThumbnails::Kind::Mesh);
	EXPECT_EQ(AssetThumbnails::getKind("notes.txt"), AssetThumbnails::Kind::None);
}
