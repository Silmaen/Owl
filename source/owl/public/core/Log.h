/**
 * @file Log.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include <cstdint>
#include <format>
#include <string_view>
#include <utility>

#ifndef OWL_LOG_COMPILED_LEVEL
/// Lowest log level compiled in (index in `owl::core::Log::Level`), set by `OWL_LOG_LEVEL` at configure time.
#define OWL_LOG_COMPILED_LEVEL 0
#endif

namespace owl::debug {

class LogBuffer;
}// namespace owl::debug

/**
 * @brief
 *  Namespace for the core objects.
 */
namespace owl::core {
/// Default frequency for frame output.
constexpr uint64_t g_DefaultFrequency{100};

/**
 * @brief
 *  Logging system.
 */
class OWL_API Log {
public:
	/// Log Level.
	enum struct Level : uint8_t {
		Trace,///< TRACE level
		Debug,///< DEBUG level
		Info,///< INFO level
		Warning,///< WARNING level
		Error,///< ERROR level
		Critical,///< CRITICAL level
		Off///< OFF level
	};

	/**
	 * @brief
	 *  initialize the logging system.
	 * @param[in] iLevel Verbosity level of the logger.
	 * @param[in] iFrequency Frequency of frame output (number of frames).
	 */
	static void init(const Level& iLevel = Level::Trace, uint64_t iFrequency = g_DefaultFrequency);

	/**
	 * @brief
	 *  Defines the Verbosity level.
	 * @param[in] iLevel Verbosity level.
	 */
	static void setVerbosityLevel(const Level& iLevel);

	/**
	 * @brief
	 *  Destroy the logger.
	 */
	static void invalidate();

	/**
	 * @brief
	 *  Check if logger is initiated.
	 * @return True if initiated.
	 */
	static auto initiated() -> bool;

	/**
	 * @brief
	 *  To know if in logging frame.
	 * @return True if in logging frame.
	 */
	static auto frameLog() -> bool { return s_frequency > 0 && s_frameCounter % s_frequency == 0; }

	/**
	 * @brief
	 *  Start a new logging frame.
	 */
	static void newFrame();

	/**
	 * @brief
	 *  define a new frame log frequency.
	 * @param[in] iFrequency New frequency.
	 */
	static void setFrameFrequency(const uint64_t iFrequency) { s_frequency = iFrequency; }

	/**
	 * @brief
	 *  Check whether a level is compiled in (`OWL_LOG_LEVEL`); the macros below it expand to nothing evaluated.
	 * @param[in] iLevel Severity level.
	 * @return True if messages of this level are compiled in.
	 */
	static constexpr auto isLevelCompiled(const Level& iLevel) -> bool {
		return iLevel >= static_cast<Level>(OWL_LOG_COMPILED_LEVEL) && iLevel != Level::Off;
	}

	/**
	 * @brief
	 *  Check whether a level passes the runtime verbosity, before any formatting is paid.
	 * @param[in] iLevel Severity level.
	 * @return True if messages of this level are emitted.
	 */
	static auto isLevelEnabled(const Level& iLevel) -> bool { return iLevel >= s_verbosity && iLevel != Level::Off; }

	/**
	 * @brief
	 *  Log a formatted message on the engine ("core") logger.
	 * @tparam Args Format argument types.
	 * @param[in] iLevel Severity level.
	 * @param[in] iFmt `std::format` compatible format string.
	 * @param[in] iArgs Format arguments.
	 */
	template<typename... Args>
	static void logCore(const Level& iLevel, std::format_string<Args...> iFmt, Args&&... iArgs) noexcept {
		if (!isLevelEnabled(iLevel))
			return;
		try {
			logCore(iLevel, std::format(iFmt, std::forward<Args>(iArgs)...));
		} catch (...) {// NOLINT(bugprone-empty-catch) logging must never propagate exceptions
		}
	}

	/**
	 * @brief
	 *  Log a pre-formatted message on the engine ("core") logger.
	 * @param[in] iLevel Severity level.
	 * @param[in] iMsg Already formatted message.
	 */
	static void logCore(const Level& iLevel, const std::string_view& iMsg);

	/**
	 * @brief
	 *  Log a formatted message on the application ("client") logger.
	 * @tparam Args Format argument types.
	 * @param[in] iLevel Severity level.
	 * @param[in] iFmt `std::format` compatible format string.
	 * @param[in] iArgs Format arguments.
	 */
	template<typename... Args>
	static void logClient(const Level& iLevel, std::format_string<Args...> iFmt, Args&&... iArgs) noexcept {
		if (!isLevelEnabled(iLevel))
			return;
		try {
			logClient(iLevel, std::format(iFmt, std::forward<Args>(iArgs)...));
		} catch (...) {// NOLINT(bugprone-empty-catch) logging must never propagate exceptions
		}
	}

	/**
	 * @brief
	 *  Log a pre-formatted message on the application ("client") logger.
	 * @param[in] iLevel Severity level.
	 * @param[in] iMsg Already formatted message.
	 */
	static void logClient(const Level& iLevel, const std::string_view& iMsg);

	/**
	 * @brief
	 *  Access the shared log buffer for UI display.
	 * @return Reference to the global LogBuffer.
	 */
	static auto getLogBuffer() -> debug::LogBuffer&;

private:
	/// The level of verbosity.
	static Level s_verbosity;
	/// Counter for the frames.
	static uint64_t s_frameCounter;
	/// Frequency of frame trace.
	static uint64_t s_frequency;
};
}// namespace owl::core

// A level below OWL_LOG_COMPILED_LEVEL or below the verbosity evaluates neither the format nor its arguments.
#define OWL_LOG_AT(logger, level, ...)                                                                                 \
	((::owl::core::Log::isLevelCompiled(::owl::core::Log::Level::level) &&                                             \
	  ::owl::core::Log::isLevelEnabled(::owl::core::Log::Level::level))                                                \
			 ? ::owl::core::Log::logger(::owl::core::Log::Level::level, __VA_ARGS__)                                   \
			 : void());

// Core log macros
#define OWL_CORE_FRAME_TRACE(...)                                                                                      \
	((::owl::core::Log::isLevelCompiled(::owl::core::Log::Level::Trace) && ::owl::core::Log::frameLog() &&             \
	  ::owl::core::Log::isLevelEnabled(::owl::core::Log::Level::Trace))                                                \
			 ? ::owl::core::Log::logCore(::owl::core::Log::Level::Trace, __VA_ARGS__)                                  \
			 : void());
#define OWL_CORE_FRAME_ADVANCE ::owl::core::Log::newFrame();

#define OWL_CORE_TRACE(...) OWL_LOG_AT(logCore, Trace, __VA_ARGS__)
#define OWL_CORE_INFO(...) OWL_LOG_AT(logCore, Info, __VA_ARGS__)
#define OWL_CORE_WARN(...) OWL_LOG_AT(logCore, Warning, __VA_ARGS__)
#define OWL_CORE_ERROR(...) OWL_LOG_AT(logCore, Error, __VA_ARGS__)
#define OWL_CORE_CRITICAL(...) OWL_LOG_AT(logCore, Critical, __VA_ARGS__)

// Client log macros
#define OWL_TRACE(...) OWL_LOG_AT(logClient, Trace, __VA_ARGS__)
#define OWL_INFO(...) OWL_LOG_AT(logClient, Info, __VA_ARGS__)
#define OWL_WARN(...) OWL_LOG_AT(logClient, Warning, __VA_ARGS__)
#define OWL_ERROR(...) OWL_LOG_AT(logClient, Error, __VA_ARGS__)
#define OWL_CRITICAL(...) OWL_LOG_AT(logClient, Critical, __VA_ARGS__)
