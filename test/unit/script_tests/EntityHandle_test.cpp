/**
 * @file EntityHandle_test.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Scene.h>
#include <script/EntityHandle.h>
#include <script/ScriptInstance.h>

#include <cstdint>
#include <format>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::script;

TEST(EntityHandle, HandlesNameOnlyLivingEntities) {
	auto world = mkShared<scene::Scene>();
	auto crate = world->createEntity("Crate");

	const auto crateHandle = getEntityHandle(*world, crate);
	EXPECT_NE(crateHandle, 0u);
	EXPECT_EQ(crateHandle & g_HudEntityHandleBit, 0u);
	EXPECT_EQ(getEntityHandle(*world, crate.getUUID()), crateHandle);
	EXPECT_LT(crateHandle, uint64_t{1} << 53U);

	ASSERT_TRUE(resolveEntityHandle(*world, crateHandle).has_value());
	EXPECT_EQ(resolveEntityHandle(*world, crateHandle)->getUUID(), crate.getUUID());
	EXPECT_EQ(getEntityHandleUuid(*world, crateHandle), crate.getUUID());
	EXPECT_FALSE(resolveEntityHandle(*world, crateHandle | g_HudEntityHandleBit).has_value());
	EXPECT_FALSE(resolveEntityHandle(*world, 0).has_value());
	EXPECT_FALSE(resolveEntityHandle(*world, uint64_t{1} << 40U).has_value());
	EXPECT_EQ(getEntityHandle(*world, core::UUID{12345}), 0u);

	world->destroyEntity(crate);
	EXPECT_FALSE(resolveEntityHandle(*world, crateHandle).has_value());
	const auto reused = world->createEntity("Reused");
	EXPECT_NE(getEntityHandle(*world, reused), crateHandle);
	EXPECT_FALSE(resolveEntityHandle(*world, crateHandle).has_value());
}

TEST(EntityHandle, ScriptsFindAnEntityByTheTextOfItsUuid) {
	core::Log::init(core::Log::Level::Off);
	auto world = mkShared<scene::Scene>();
	const auto door = world->createEntity("Door");
	const auto script = std::format("function on_create()\n"
									"  found = scene.find_by_uuid(\"{}\")\n"
									"  none = scene.find_by_uuid(\"not a uuid\")\n"
									"end\n",
									static_cast<uint64_t>(door.getUUID()));
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance instance;
	instance.setScene(world.get());
	ASSERT_TRUE(instance.createFromBuffer(data, "find_by_uuid", 0));
	instance.onCreate();
	EXPECT_EQ(instance.getPropertyInt("found").value_or(-1), static_cast<int64_t>(getEntityHandle(*world, door)));
	EXPECT_EQ(instance.getPropertyInt("none").value_or(-1), 0);
	core::Log::invalidate();
}
