/**
 * @file Frustum_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/Frustum.h>
#include <math/linAlgebra.h>
#include <math/matrixCreation.h>
#include <math/trigonometry.h>

#include <limits>

using namespace owl::math;

namespace {
// Camera at the origin looking down -Z, 90 degrees of vertical field, square, from 1 to 100.
auto cameraFrustum() -> Frustum {
	return Frustum::fromViewProjection(perspectiveReversedZ(radians(90.f), 1.f, 1.f, 100.f));
}
}// namespace

TEST(Frustum, ClassifiesBoxesAgainstAPerspective) {
	const auto frustum = cameraFrustum();
	EXPECT_EQ(frustum.classify(box3f{vec3{-1.f, -1.f, -11.f}, vec3{1.f, 1.f, -9.f}}), Containment::Inside);
	EXPECT_EQ(frustum.classify(box3f{vec3{-1.f, -1.f, 1.f}, vec3{1.f, 1.f, 3.f}}), Containment::Outside);
	EXPECT_EQ(frustum.classify(box3f{vec3{-1.f, -1.f, -120.f}, vec3{1.f, 1.f, -90.f}}), Containment::Intersects);
	EXPECT_EQ(frustum.classify(box3f{vec3{30.f, -1.f, -11.f}, vec3{32.f, 1.f, -9.f}}), Containment::Outside);
	EXPECT_EQ(frustum.classify(box3f{vec3{8.f, -1.f, -11.f}, vec3{12.f, 1.f, -9.f}}), Containment::Intersects);
	EXPECT_FALSE(frustum.intersects(box3f{vec3{-1.f, -1.f, -300.f}, vec3{1.f, 1.f, -200.f}}));
}

TEST(Frustum, KeepsAFlatBoxOfAnOrthographicCamera) {
	const auto frustum = Frustum::fromViewProjection(orthoReversedZ(-10.f, 10.f, -5.f, 5.f, -1.f, 1.f));
	EXPECT_EQ(frustum.classify(box3f{vec3{-1.f, -1.f, 0.f}, vec3{1.f, 1.f, 0.f}}), Containment::Inside);
	EXPECT_EQ(frustum.classify(box3f{vec3{9.f, -1.f, 0.f}, vec3{11.f, 1.f, 0.f}}), Containment::Intersects);
	EXPECT_EQ(frustum.classify(box3f{vec3{-1.f, 6.f, 0.f}, vec3{1.f, 7.f, 0.f}}), Containment::Outside);
}

TEST(Frustum, CullsBetweenTheEyeAndTheNearPlane) {
	const auto frustum = cameraFrustum();
	EXPECT_EQ(frustum.classify(box3f{vec3{-0.1f, -0.1f, -0.8f}, vec3{0.1f, 0.1f, -0.5f}}), Containment::Outside);
	EXPECT_EQ(frustum.classify(box3f{vec3{-0.1f, -0.1f, -1.5f}, vec3{0.1f, 0.1f, -0.5f}}), Containment::Intersects);
}

TEST(Frustum, InfiniteFarPlaneKeepsEveryDistance) {
	const auto frustum = Frustum::fromViewProjection(
			perspectiveReversedZ(radians(90.f), 1.f, 1.f, std::numeric_limits<float>::infinity()));
	EXPECT_EQ(frustum.classify(box3f{vec3{-1.f, -1.f, -1.e7f}, vec3{1.f, 1.f, -0.9e7f}}), Containment::Inside);
	EXPECT_EQ(frustum.classify(box3f{vec3{-1.f, -1.f, 1.f}, vec3{1.f, 1.f, 3.f}}), Containment::Outside);
}

TEST(Frustum, KeepsAFlatBoxOnTheNearPlane) {
	// A sprite at z 6 under an orthographic camera at z 5 whose near plane is at -1: exactly on the plane.
	const auto projection = orthoReversedZ(-14.2f, 14.2f, -4.f, 4.f, -1.f, 10.f);
	const auto view = inverse(translate(identity<float, 4>(), vec3{0.f, 0.f, 5.f}));
	const auto frustum = Frustum::fromViewProjection(projection * view);
	EXPECT_TRUE(frustum.intersects(box3f{vec3{1.8f, -2.4f, 6.f}, vec3{4.8f, -0.4f, 6.f}}));
	EXPECT_FALSE(frustum.intersects(box3f{vec3{1.8f, -2.4f, 6.1f}, vec3{4.8f, -0.4f, 6.1f}}));
}

TEST(Frustum, TransformBoxEnclosesTheRotatedBox) {
	auto rotation = identity<float, 4>();
	// 90 degrees about Z, then a translation of (5, 0, 0).
	rotation(0, 0) = 0.f;
	rotation(0, 1) = -1.f;
	rotation(1, 0) = 1.f;
	rotation(1, 1) = 0.f;
	rotation(0, 3) = 5.f;
	const box3f world = transformBox(rotation, box3f{vec3{0.f, 0.f, 0.f}, vec3{2.f, 1.f, 1.f}});
	EXPECT_NEAR(world.min().x(), 4.f, 1e-5f);
	EXPECT_NEAR(world.max().x(), 5.f, 1e-5f);
	EXPECT_NEAR(world.min().y(), 0.f, 1e-5f);
	EXPECT_NEAR(world.max().y(), 2.f, 1e-5f);
	EXPECT_NEAR(world.max().z(), 1.f, 1e-5f);
}
