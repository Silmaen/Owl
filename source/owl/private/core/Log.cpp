/**
 * @file Log.cpp
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/external/spdlog.h"
#include "debug/LogSink.h"
OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
OWL_DIAG_DISABLE_CLANG("-Wundefined-func-template")
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
OWL_DIAG_POP


namespace owl::core {

namespace {

debug::LogBuffer g_logBuffer;

auto fromLevel(const Log::Level& iLevel) -> spdlog::level::level_enum {
	switch (iLevel) {
		case Log::Level::Error:
			return spdlog::level::err;
		case Log::Level::Warning:
			return spdlog::level::warn;
		case Log::Level::Info:
			return spdlog::level::info;
		case Log::Level::Debug:
			return spdlog::level::debug;
		case Log::Level::Trace:
			return spdlog::level::trace;
		case Log::Level::Off:
			return spdlog::level::off;
		case Log::Level::Critical:
			return spdlog::level::critical;
	}
	return spdlog::level::off;
}

auto toLevel(const spdlog::level::level_enum iLevel) -> Log::Level {
	switch (iLevel) {
		case spdlog::level::trace:
			return Log::Level::Trace;
		case spdlog::level::debug:
			return Log::Level::Debug;
		case spdlog::level::info:
			return Log::Level::Info;
		case spdlog::level::warn:
			return Log::Level::Warning;
		case spdlog::level::err:
			return Log::Level::Error;
		case spdlog::level::critical:
			return Log::Level::Critical;
		case spdlog::level::off:
		case spdlog::level::n_levels:
			return Log::Level::Off;
	}
	return Log::Level::Off;
}

/**
 * @brief spdlog sink that pushes entries into a LogBuffer for UI display.
 */
class EditorLogSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
	explicit EditorLogSink(debug::LogBuffer& ioBuffer) : m_buffer{ioBuffer} {}

protected:
	void sink_it_(const spdlog::details::log_msg& iMsg) override {
		debug::LogEntry entry;
		entry.level = toLevel(iMsg.level);
		entry.loggerName = std::string(iMsg.logger_name.data(), iMsg.logger_name.size());
		entry.message = std::string(iMsg.payload.data(), iMsg.payload.size());
		entry.timestamp = iMsg.time;
		m_buffer.push(std::move(entry));
	}
	void flush_() override {}

private:
	debug::LogBuffer& m_buffer;
};

std::shared_ptr<spdlog::logger> g_CoreLogger;
std::shared_ptr<spdlog::logger> g_ClientLogger;
}// namespace

Log::Level Log::s_verbosity = Level::Trace;
uint64_t Log::s_frameCounter = 0;
uint64_t Log::s_frequency = g_DefaultFrequency;

void Log::init(const Level& iLevel, const uint64_t iFrequency) {
	OWL_SCOPE_UNTRACK
	if (g_CoreLogger != nullptr) {
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

	logSinks.emplace_back(std::make_shared<EditorLogSink>(g_logBuffer));

	logSinks[0]->set_pattern("%^[%T] %n: %v%$");
	logSinks[1]->set_pattern("[%T] [%l] %n: %v");

	g_CoreLogger = std::make_shared<spdlog::logger>("OWL", begin(logSinks), end(logSinks));
	register_logger(g_CoreLogger);

	g_ClientLogger = std::make_shared<spdlog::logger>("APP", begin(logSinks), end(logSinks));
	register_logger(g_ClientLogger);
	setVerbosityLevel(iLevel);
	s_frameCounter = 0;
	s_frequency = iFrequency;
}

void Log::setVerbosityLevel(const Level& iLevel) {
	s_verbosity = iLevel;
	if (g_CoreLogger) {
		g_CoreLogger->set_level(fromLevel(s_verbosity));
		g_CoreLogger->flush_on(fromLevel(s_verbosity));
	}
	if (g_ClientLogger) {
		g_ClientLogger->set_level(fromLevel(s_verbosity));
		g_ClientLogger->flush_on(fromLevel(s_verbosity));
	}
}

void Log::invalidate() {
	OWL_SCOPE_UNTRACK
	spdlog::drop_all();
	g_CoreLogger.reset();
	g_ClientLogger.reset();
}

auto Log::initiated() -> bool { return g_CoreLogger != nullptr; }

void Log::newFrame() { ++s_frameCounter; }

void Log::logCore(const Level& iLevel, const std::string_view& iMsg) { g_CoreLogger->log(fromLevel(iLevel), iMsg); }
void Log::logClient(const Level& iLevel, const std::string_view& iMsg) { g_ClientLogger->log(fromLevel(iLevel), iMsg); }

auto Log::getLogBuffer() -> debug::LogBuffer& { return g_logBuffer; }

}// namespace owl::core
