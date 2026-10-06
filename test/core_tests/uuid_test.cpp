
#include "testHelper.h"

#include <core/UUID.h>

#include <cstdint>
#include <functional>

using namespace owl::core;

TEST(UUID, generate) {
	const UUID uuid{};
	EXPECT_NE(static_cast<uint64_t>(uuid), 0);
}

TEST(UUID, uint64_test) {
	constexpr uint64_t id = 666;
	const UUID uuid(id);
	EXPECT_EQ(id, static_cast<uint64_t>(uuid));
	EXPECT_EQ(std::hash<UUID>{}(uuid), 666);
}
