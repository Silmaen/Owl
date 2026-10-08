/**
 * @file ExtraDataRegistry_tests.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/extradata/ExtraDataContainer.h>
#include <data/extradata/ExtraDataRegister.h>
#include <data/extradata/ExtraDataRegistry.h>
#include <data/geometry/extradata/VertexNormal.h>

#include <typeinfo>

using namespace owl::data::extradata;

namespace {

class TestExtraData final : public MeshExtraData<TestExtraData, float> {
public:
	[[nodiscard]] auto getValue() const -> const float& override { return value; }

	float value = 0.f;
};

}// namespace

TEST(ExtraDataRegistry, EngineTypesAreRegistered) {
	const ExtraDataPid pid = getExtraDataPid<owl::data::geometry::extradata::VertexNormal>();
	ASSERT_NE(pid, g_invalidExtraDataPid);
	EXPECT_TRUE(ExtraDataRegistry::isRegistered(pid));
	const auto created = ExtraDataRegistry::create(pid);
	ASSERT_NE(created, nullptr);
	EXPECT_EQ(created->getPid(), pid);
}

TEST(ExtraDataRegistry, RegisterCustomType) {
	EXPECT_TRUE(registerExtraData<TestExtraData>().isRegistered());
	const ExtraDataPid pid = getExtraDataPid<TestExtraData>();
	ASSERT_NE(pid, g_invalidExtraDataPid);
	EXPECT_EQ(getMeshExtraDataPid<TestExtraData>(), pid);
	EXPECT_EQ(ExtraDataRegistry::registerType(typeid(TestExtraData), &createExtraData<TestExtraData>), pid);
	EXPECT_EQ(ExtraDataRegistry::create(pid)->getPid(), pid);
}

TEST(ExtraDataRegistry, UnknownTypesAndPids) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	EXPECT_EQ(ExtraDataRegistry::getPid(typeid(int)), g_invalidExtraDataPid);
	EXPECT_FALSE(ExtraDataRegistry::isRegistered(g_invalidExtraDataPid));
	EXPECT_FALSE(ExtraDataRegistry::isRegistered(1 << 20));
	EXPECT_EQ(ExtraDataRegistry::create(g_invalidExtraDataPid), nullptr);
	EXPECT_EQ(ExtraDataRegistry::registerType(typeid(double), nullptr), g_invalidExtraDataPid);
	owl::core::Log::invalidate();
}

TEST(ExtraDataRegistry, ContainerCloneCopiesValues) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	ASSERT_TRUE(registerExtraData<TestExtraData>().isRegistered());
	const ExtraDataContainer container(getExtraDataPid<TestExtraData>(), 2);
	EXPECT_NE(container.getExtraData(1), nullptr);
	auto* first = dynamic_cast<TestExtraData*>(container.getExtraData(0).get());
	ASSERT_NE(first, nullptr);
	first->value = 4.5f;
	const ExtraDataContainer copy(container);
	EXPECT_NE(copy.getExtraData(1), nullptr);
	EXPECT_EQ(copy.getExtraData(2), nullptr);
	const auto* copied = dynamic_cast<const TestExtraData*>(copy.getExtraData(0).get());
	ASSERT_NE(copied, nullptr);
	EXPECT_FLOAT_EQ(copied->getValue(), 4.5f);
	EXPECT_NE(copied, first);
	owl::core::Log::invalidate();
}
