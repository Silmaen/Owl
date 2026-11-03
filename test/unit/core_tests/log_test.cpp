/**
 * @file log_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <debug/LogSink.h>

#include <cstdlib>

using namespace owl::core;

namespace {
auto lastEntry() -> owl::debug::LogEntry {
	const auto entries = Log::getLogBuffer().getEntries();
	if (entries.empty())
		return {};
	return entries.back();
}

auto countEvaluations(int& ioCounter) -> int {
	++ioCounter;
	return ioCounter;
}
}// namespace

TEST(Log, basic) {
	Log::setVerbosityLevel(Log::Level::Off);
	Log::init();
	Log::init();
	EXPECT_TRUE(Log::initiated());
	Log::newFrame();
	Log::setFrameFrequency(0);
	Log::newFrame();
	EXPECT_FALSE(Log::frameLog());// frequency 0: no frame log
	Log::setFrameFrequency(1);
	Log::newFrame();
	EXPECT_TRUE(Log::frameLog());// every frame
	Log::invalidate();
	EXPECT_FALSE(Log::initiated());
}

TEST(Log, clientMessagesGoToTheClientLogger) {
	Log::init(Log::Level::Trace);
	Log::getLogBuffer().clear();
	OWL_INFO("client message {}.", 1)
	EXPECT_EQ(lastEntry().loggerName, "APP");
	EXPECT_EQ(lastEntry().message, "client message 1.");
	OWL_INFO("client message without argument.")
	EXPECT_EQ(lastEntry().loggerName, "APP");
	OWL_CORE_INFO("core message {}", 2)
	EXPECT_EQ(lastEntry().loggerName, "OWL");
	EXPECT_EQ(lastEntry().message, "core message 2");
	OWL_CORE_WARN("core message without argument.")
	EXPECT_EQ(lastEntry().loggerName, "OWL");
	Log::invalidate();
}

TEST(Log, disabledLevelEvaluatesNothing) {
	Log::init(Log::Level::Warning);
	Log::getLogBuffer().clear();
	int evaluations = 0;
	OWL_CORE_TRACE("trace {}", countEvaluations(evaluations))
	OWL_CORE_INFO("info {}", countEvaluations(evaluations))
	OWL_TRACE("client trace {}", countEvaluations(evaluations))
	EXPECT_EQ(evaluations, 0);
	EXPECT_TRUE(Log::getLogBuffer().getEntries().empty());
	OWL_CORE_WARN("warn {}", countEvaluations(evaluations))
	EXPECT_EQ(evaluations, 1);
	EXPECT_EQ(lastEntry().message, "warn 1");
	Log::invalidate();
}

TEST(Log, levelPredicates) {
	Log::init(Log::Level::Info);
	EXPECT_FALSE(Log::isLevelEnabled(Log::Level::Trace));
	EXPECT_FALSE(Log::isLevelEnabled(Log::Level::Debug));
	EXPECT_TRUE(Log::isLevelEnabled(Log::Level::Info));
	EXPECT_TRUE(Log::isLevelEnabled(Log::Level::Critical));
	EXPECT_FALSE(Log::isLevelEnabled(Log::Level::Off));
	EXPECT_FALSE(Log::isLevelCompiled(Log::Level::Off));
	EXPECT_EQ(Log::isLevelCompiled(Log::Level::Trace), OWL_LOG_COMPILED_LEVEL == 0);
	EXPECT_TRUE(Log::isLevelCompiled(Log::Level::Critical) || OWL_LOG_COMPILED_LEVEL > 5);
	Log::invalidate();
}

TEST(Log, frameTraceIsSampled) {
	Log::init(Log::Level::Trace, 2);
	Log::getLogBuffer().clear();
	const int expected = Log::isLevelCompiled(Log::Level::Trace) ? 2 : 0;
	int evaluations = 0;
	for (int frame = 0; frame < 4; ++frame) {
		OWL_CORE_FRAME_ADVANCE
		OWL_CORE_FRAME_TRACE("frame {}", countEvaluations(evaluations))
	}
	EXPECT_EQ(evaluations, expected);
	Log::setVerbosityLevel(Log::Level::Info);
	for (int frame = 0; frame < 4; ++frame) {
		OWL_CORE_FRAME_ADVANCE
		OWL_CORE_FRAME_TRACE("frame {}", countEvaluations(evaluations))
	}
	EXPECT_EQ(evaluations, expected);
	Log::invalidate();
}

TEST(Log, parseLevelReadsEveryName) {
	EXPECT_EQ(Log::parseLevel("trace"), Log::Level::Trace);
	EXPECT_EQ(Log::parseLevel("DEBUG"), Log::Level::Debug);
	EXPECT_EQ(Log::parseLevel("Info"), Log::Level::Info);
	EXPECT_EQ(Log::parseLevel("warn"), Log::Level::Warning);
	EXPECT_EQ(Log::parseLevel("warning"), Log::Level::Warning);
	EXPECT_EQ(Log::parseLevel("error"), Log::Level::Error);
	EXPECT_EQ(Log::parseLevel("critical"), Log::Level::Critical);
	EXPECT_EQ(Log::parseLevel("off"), Log::Level::Off);
	EXPECT_FALSE(Log::parseLevel("loud").has_value());
	EXPECT_FALSE(Log::parseLevel("").has_value());
}

#if defined(OWL_PLATFORM_LINUX)
TEST(Log, defaultLevelFollowsTheBuildAndTheEnvironment) {
	unsetenv("OWL_LOG_VERBOSITY");
#ifdef OWL_DEBUG_BUILD
	EXPECT_EQ(Log::getDefaultLevel(), Log::Level::Debug);
#else
	EXPECT_EQ(Log::getDefaultLevel(), Log::Level::Info);
#endif
	setenv("OWL_LOG_VERBOSITY", "trace", 1);
	EXPECT_EQ(Log::getDefaultLevel(), Log::Level::Trace);
	setenv("OWL_LOG_VERBOSITY", "nonsense", 1);
	EXPECT_NE(Log::getDefaultLevel(), Log::Level::Trace);
	unsetenv("OWL_LOG_VERBOSITY");
}
#endif

TEST(Log, debugMacrosReachTheBuffer) {
	Log::init(Log::Level::Debug);
	Log::getLogBuffer().clear();
	OWL_CORE_DEBUG("Core debug {}.", 1)
	EXPECT_EQ(lastEntry().level, Log::Level::Debug);
	OWL_DEBUG("Client debug {}.", 2)
	EXPECT_EQ(lastEntry().level, Log::Level::Debug);
	Log::setVerbosityLevel(Log::Level::Info);
	OWL_CORE_DEBUG("Hidden {}.", 3)
	EXPECT_EQ(lastEntry().message, "Client debug 2.");
	Log::invalidate();
}

TEST(Log, onceMacrosLogTheFirstTimeOnly) {
	Log::init(Log::Level::Trace);
	Log::getLogBuffer().clear();
	for (int i = 0; i < 3; ++i) OWL_CORE_WARN_ONCE("Once {}.", i)
	const auto entries = Log::getLogBuffer().getEntries();
	ASSERT_EQ(entries.size(), 1u);
	EXPECT_EQ(entries.front().message, "Once 0.");
	EXPECT_EQ(entries.front().level, Log::Level::Warning);
	Log::invalidate();
}
