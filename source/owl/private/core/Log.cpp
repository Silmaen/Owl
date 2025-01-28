/**
 * @file Log.cpp
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
OWL_DIAG_DISABLE_CLANG("-Wundefined-func-template")
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
OWL_DIAG_POP


namespace owl::core {

std::shared_ptr<spdlog::logger> Log::s_coreLogger;
std::shared_ptr<spdlog::logger> Log::s_clientLogger;
spdlog::level::level_enum Log::s_verbosity = spdlog::level::trace;
uint64_t Log::s_frameCounter = 0;
uint64_t Log::s_frequency = gDefaultFrequency;

void Log::init(const spdlog::level::level_enum& iLevel, const uint64_t iFrequency) {
	OWL_SCOPE_UNTRACK
	if (s_coreLogger != nullptr) {
		OWL_CORE_INFO("Logger already initiated.")
		return;
	}
	std::vector<spdlog::sink_ptr> logSinks;
	logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
#ifdef WIN32
	logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(L"Owl.log", true));
#else
	logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Owl.log", true));
#endif

	logSinks[0]->set_pattern("%^[%T] %n: %v%$");
	logSinks[1]->set_pattern("[%T] [%l] %n: %v");

	s_coreLogger = std::make_shared<spdlog::logger>("OWL", begin(logSinks), end(logSinks));
	spdlog::register_logger(s_coreLogger);

	s_clientLogger = std::make_shared<spdlog::logger>("APP", begin(logSinks), end(logSinks));
	spdlog::register_logger(s_clientLogger);
	setVerbosityLevel(iLevel);
	s_frameCounter = 0;
	s_frequency = iFrequency;
}

void Log::setVerbosityLevel(const spdlog::level::level_enum& iLevel) {
	s_verbosity = iLevel;
	if (s_coreLogger) {
		s_coreLogger->set_level(s_verbosity);
		s_coreLogger->flush_on(s_verbosity);
	}
	if (s_clientLogger) {
		s_clientLogger->set_level(s_verbosity);
		s_clientLogger->flush_on(s_verbosity);
	}
}

void Log::invalidate() {
	OWL_SCOPE_UNTRACK
	spdlog::drop_all();
	s_coreLogger.reset();
	s_clientLogger.reset();
}

void Log::newFrame() { ++s_frameCounter; }

}// namespace owl::core
