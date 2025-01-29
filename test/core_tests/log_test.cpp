
#include "testHelper.h"

#include <core/Log.h>

using namespace owl::core;

TEST(Log, basic) {
	Log::setVerbosityLevel(Log::Level::Off);
	Log::init();
	Log::init();
	Log::newFrame();
	Log::setFrameFrequency(0);
	Log::newFrame();
	Log::setFrameFrequency(1);
	Log::newFrame();
	Log::invalidate();
}
