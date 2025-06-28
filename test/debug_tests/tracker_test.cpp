
#include "core/utils/StringUtils.h"
#include "testHelper.h"

#include <debug/TrackerClient.h>
#include <math/matrices.h>

using namespace owl::debug;

TEST(Tracker, base) {
	const auto& state = TrackerAPI::checkState();
	EXPECT_TRUE(state.allocationCalls <= TrackerAPI::globals().allocationCalls);
#ifndef OWL_SANITIZER_CUSTOM_ALLOCATOR
	ASSERT_FALSE(state.allocs.empty());
	const auto s = state.allocs.back().toStr();
	ASSERT_FALSE(s.empty());
#else
	// Some sanitizers use their own memory allocator, our tracker is useless.
	ASSERT_TRUE(state.allocs.empty());
#endif
}

TEST(Tracker, stacktrace) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	{
		const auto& globals = TrackerAPI::globals();
		const size_t initialAlloc = globals.allocs.size();
		EXPECT_EQ(globals.allocationCalls - globals.deallocationCalls, initialAlloc);
		{
			auto mat = owl::mkShared<owl::math::mat2>();
#ifndef OWL_SANITIZER_CUSTOM_ALLOCATOR
			EXPECT_EQ(globals.allocs.size(), initialAlloc + 1);
			EXPECT_FALSE(globals.allocs.back().toStr(false, true).empty());
#else
			// Some sanitizers use their own memory allocator, our tracker is useless.
			EXPECT_EQ(globals.allocs.size(), initialAlloc);
#endif
			mat.reset();
			EXPECT_LT(std::abs(static_cast<int64_t>(globals.allocs.size()) - static_cast<int64_t>(initialAlloc)), 2);
		}
		EXPECT_LT(std::abs(static_cast<int64_t>(globals.allocationCalls) -
						   static_cast<int64_t>(globals.deallocationCalls) - static_cast<int64_t>(initialAlloc)),
				  2);
		EXPECT_LT(std::abs(static_cast<int64_t>(globals.allocs.size()) - static_cast<int64_t>(initialAlloc)), 2);
	}
}

TEST(MemorySize, formating) {
	std::size_t st{488};
	EXPECT_STREQ(owl::core::utils::sizeToString(st).c_str(), "488 B");
	st += 1024;
	EXPECT_STREQ(owl::core::utils::sizeToString(st).c_str(), "1.48 KB");
	st *= 410;
	st += 1024ull * 1024ull;
	EXPECT_STREQ(owl::core::utils::sizeToString(st).c_str(), "1.59 MB");
	st *= 154;
	st += 77 * 1024ull * 1024ull * 1024ull;
	EXPECT_STREQ(owl::core::utils::sizeToString(st).c_str(), "77.24 GB");
	st += 1024ull * 1024ull * 1024ull * 1024ull;
	EXPECT_STREQ(owl::core::utils::sizeToString(st).c_str(), "1.08 TB");
}
