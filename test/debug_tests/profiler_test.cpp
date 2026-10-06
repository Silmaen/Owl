
#include "testHelper.h"

#include <debug/Profiler.h>

#include <filesystem>

using namespace owl::debug;

TEST(profiler, creation) {
	auto& prof = Profiler::get();
	prof.beginSession("bob", "");
	prof.endSession();
	owl::core::Log::init(owl::core::Log::Level::Off);
	prof.beginSession("bob", "");
	prof.endSession();
	const std::filesystem::path file("test_profile.json");
	prof.beginSession("bob2", file.string());
	const std::filesystem::path file2("test_profile2.json");
	prof.beginSession("bob2", file.string());
	prof.endSession();
	EXPECT_TRUE(exists(file));
	EXPECT_FALSE(exists(file2));
	remove(file);
	remove(file2);
	owl::core::Log::invalidate();
}

TEST(profiler, timer) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	{ const ProfileTimer timer("toto"); }
	{
		ProfileTimer timer("toto2");
		timer.stop();
	}
	owl::core::Log::invalidate();
}

TEST(profiler, backendMatchesTheBuild) {
#if defined(OWL_PROFILER_TRACY)
	EXPECT_EQ(getProfilerBackend(), ProfilerBackend::Tracy);
#elif defined(OWL_PROFILER_CHROME)
	EXPECT_EQ(getProfilerBackend(), ProfilerBackend::Chrome);
#else
	EXPECT_EQ(getProfilerBackend(), ProfilerBackend::None);
#endif
}

TEST(profiler, facadeWithoutConnectedProfiler) {
	EXPECT_FALSE(isProfilerConnected());
	setProfilerThreadName("Test thread");
	static constexpr ProfileSourceLocation location{.name = "zone",
													.function = "facadeWithoutConnectedProfiler",
													.file = __FILE__,
													.line = __LINE__,
													.color = 0};
	{ const ProfileZone zone{&location}; }
	markProfilerFrame();
}

namespace {
void profiledFunction() {
	OWL_PROFILE_FUNCTION()

	OWL_PROFILE_SCOPE("Macro scope")
	OWL_PROFILE_FRAME_MARK()
}
}// namespace

TEST(profiler, macrosCompileInEveryBackend) {
	OWL_PROFILE_THREAD_NAME("Macro thread")
	profiledFunction();
	SUCCEED();
}
