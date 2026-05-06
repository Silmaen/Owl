/**
 * @file Log.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include <format>

namespace owl::debug {

class LogBuffer;
} // namespace owl::debug

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
	 *  Defines the Verbosity level
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
	 *  Log a formatted message on the engine ("core") logger.
	 * @tparam Args Format argument types.
	 * @param[in] iLevel Severity level.
	 * @param[in] iFmt `std::format` compatible format string.
	 * @param[in] iArgs Format arguments.
	 */
	template<typename... Args>
	static void logCore(const Level& iLevel, std::format_string<Args...> iFmt, Args&&... iArgs) noexcept {
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
		try {
			logCore(iLevel, std::format(iFmt, std::forward<Args>(iArgs)...));
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

// Core log macros;
#define OWL_CORE_FRAME_TRACE(...)                                                                                      \
	if (::owl::core::Log::frameLog()) {                                                                                \
		::owl::core::Log::logCore(::owl::core::Log::Level::Trace, __VA_ARGS__);                                        \
	}
#define OWL_CORE_FRAME_ADVANCE ::owl::core::Log::newFrame();

#define OWL_CORE_TRACE(...) ::owl::core::Log::logCore(::owl::core::Log::Level::Trace, __VA_ARGS__);
#define OWL_CORE_INFO(...) ::owl::core::Log::logCore(::owl::core::Log::Level::Info, __VA_ARGS__);
#define OWL_CORE_WARN(...) ::owl::core::Log::logCore(::owl::core::Log::Level::Warning, __VA_ARGS__);
#define OWL_CORE_ERROR(...) ::owl::core::Log::logCore(::owl::core::Log::Level::Error, __VA_ARGS__);
#define OWL_CORE_CRITICAL(...) ::owl::core::Log::logCore(::owl::core::Log::Level::Critical, __VA_ARGS__);

// Client log macros
#define OWL_TRACE(...) ::owl::core::Log::logClient(::owl::core::Log::Level::Trace, __VA_ARGS__);
#define OWL_INFO(...) ::owl::core::Log::logClient(::owl::core::Log::Level::Info, __VA_ARGS__);
#define OWL_WARN(...) ::owl::core::Log::logClient(::owl::core::Log::Level::Warning, __VA_ARGS__);
#define OWL_ERROR(...) ::owl::core::Log::logClient(::owl::core::Log::Level::Error, __VA_ARGS__);
#define OWL_CRITICAL(...) ::owl::core::Log::logClient(::owl::core::Log::Level::Critical, __VA_ARGS__);
