
#include "testHelper.h"

#include <debug/Profiler.h>

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
