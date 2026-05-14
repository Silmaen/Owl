
#include "core/utils/StringUtils.h"
#include "testHelper.h"

#include <debug/TrackerClient.h>
#include <math/matrices.h>

using namespace owl::debug;

namespace {
/// Probe whether the engine's global `operator new` overrides actually
/// intercept allocations from this test executable. On Windows with shared
/// libraries the overrides live inside `OwlEngine.dll` and the test exe's
/// own allocations bypass them; on custom-allocator sanitisers the runtime
/// owns `new`/`delete` outright. In both cases the static
/// `OWL_TRACKER_ACTIVE` macro is true but the tracker doesn't see anything,
/// so the assertions that "an alloc was recorded" would spuriously fail.
auto trackerIsObservingThisExecutable() -> bool {
	TrackerAPI::checkState();// reset the per-tick counters
	auto probe = owl::mkShared<int>(0);
	const auto& after = TrackerAPI::checkState();
	static_cast<void>(probe);
	return after.allocationCalls > 0;
}
}// namespace

TEST(Tracker, base) {
	const auto& state = TrackerAPI::checkState();
	EXPECT_TRUE(state.allocationCalls <= TrackerAPI::globals().allocationCalls);
	if (!trackerIsObservingThisExecutable()) {
		// Tracker compiled in but not intercepting our `new` (Windows shared,
		// sanitiser custom allocator, tracker option off, …) — there's nothing
		// to assert about `allocs` then; the call sites above already validated
		// the API surface compiles and returns sane values.
		SUCCEED() << "Tracker not intercepting allocations on this build / platform";
		return;
	}
	// `trackerIsObservingThisExecutable` consumed the per-tick state via
	// `checkState`. Reset, do a fresh allocation that survives until after
	// `checkState`, and inspect what was recorded.
	TrackerAPI::checkState();
	auto probe = owl::mkShared<int>(42);
	const auto& probed = TrackerAPI::checkState();
	ASSERT_FALSE(probed.allocs.empty());
	const auto s = probed.allocs.back().toStr();
	ASSERT_FALSE(s.empty());
	static_cast<void>(probe);
}

TEST(Tracker, stacktrace) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	if (!trackerIsObservingThisExecutable()) {
		SUCCEED() << "Tracker not intercepting allocations on this build / platform";
		return;
	}
	{
		const auto& globals = TrackerAPI::globals();
		const size_t initialAlloc = globals.allocs.size();
		EXPECT_EQ(globals.allocationCalls - globals.deallocationCalls, initialAlloc);
		{
			auto mat = owl::mkShared<owl::math::mat2>();
			EXPECT_EQ(globals.allocs.size(), initialAlloc + 1);
			EXPECT_FALSE(globals.allocs.back().toStr(false, true).empty());
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
