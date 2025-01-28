/**
* @file Profiler.h
* @author Silmaen
* @date 17/08/2022
* Copyright © 2022 All rights reserved.
* All modification must get authorization from the author.
*/
#pragma once

#include "core/Log.h"
#include <fstream>
#include <thread>

namespace owl::debug {

/// The type for microseconds.
using floatingPointMicroseconds = std::chrono::duration<double, std::micro>;

/**
 * @brief Data for profiling result.
 */
struct ProfileResult {
	std::string name;///< Result's name.
	floatingPointMicroseconds start;///< Data's starting time point.
	std::chrono::microseconds elapsedTime;///< Data's elapsed time.
	std::thread::id threadId;///< Data's thread ID.
};

/**
 * @brief Profile Session Data.
 */
struct ProfileSession {
	explicit ProfileSession(std::string iName) : name{std::move(iName)} {}
	std::string name;/// Session's name.
};

/**
 * @brief class for accessing to the internal profiler.
 */
class OWL_API Profiler {
public:
	Profiler(const Profiler&) = delete;
	Profiler(Profiler&&) = delete;
	auto operator=(const Profiler&) -> Profiler& = delete;
	auto operator=(Profiler&&) -> Profiler& = delete;

	/**
	 * @brief Begins a new profiling session.
	 * @param[in] iName Session's name.
	 * @param[in] iFilepath Session File path to store information.
	 */
	void beginSession(const std::string& iName, const std::string& iFilepath = "results.json");

	/**
	 * @brief Terminate profile session.
	 */
	void endSession();

	/**
	 * @brief Write profiling result into json file.
	 * @param[in] iResult The Result to write.
	 */
	void writeProfile(const ProfileResult& iResult);

	/**
	 * @brief Singleton accessor.
	 * @return This instance.
	 */
	static auto get() -> Profiler& {
		static Profiler instance;
		return instance;
	}

private:
	/**
	 * @brief Private Constructor.
	 */
	Profiler();

	/**
	 * @brief Private destructor.
	 */
	~Profiler();

	/**
	 * @brief write json header.
	 */
	void writeHeader();

	/**
	 * @brief write json footer.
	 */
	void writeFooter();

	/**
	 * @brief Terminate the session.
	 *
	 * @note: you must already own lock on m_Mutex before calling InternalEndSession().
	 */
	void internalEndSession();
	/// Mutex.
	std::mutex m_profilerMutex;
	/// Actual running session.
	uniq<ProfileSession> m_currentSession{nullptr};
	/// Output file stream.
	std::ofstream m_outputStream;
};

/**
 * @brief Timer used for profiling.
 */
class OWL_API ProfileTimer {
public:
	/**
	 * @brief Constructor.
	 * @param[in] iName Scope's name.
	 */
	explicit ProfileTimer(const char* iName);

	ProfileTimer(const ProfileTimer&) = delete;
	ProfileTimer(ProfileTimer&&) = delete;
	auto operator=(const ProfileTimer&) -> ProfileTimer& = delete;
	auto operator=(ProfileTimer&&) -> ProfileTimer& = delete;
	/**
	 * @brief Destructor.
	 */
	~ProfileTimer();

	/**
	 * @brief Stop the timer.
	 */
	void stop();

private:
	/// Scope's name.
	const char* m_name;
	/// Timer starting point.
	std::chrono::time_point<std::chrono::steady_clock> m_startTimePoint;
	/// Timer state, true if not running.
	bool m_stopped{false};
};

/**
 * @brief Namespace for profiling utility functions.
 */
namespace utils {

/**
 * @brief Simple char array.
 * @tparam N Size of the array.
 */
template<size_t N>
struct ChangeResult {
	char data[N];
};

/**
 * @brief Simple String cleaner.
 * @tparam N Size of the string.
 * @tparam K Size of pattern to remove.
 * @param[in] iExpr The string.
 * @param[in] iRemove Pattern to remove.
 * @return The corrected string.
 */
OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
template<size_t N, size_t K>
constexpr auto cleanupOutputString(const char (&iExpr)[N], const char (&iRemove)[K]) -> ChangeResult<N> {
	ChangeResult<N> result = {};

	size_t srcIndex = 0;
	size_t dstIndex = 0;
	while (srcIndex < N) {
		size_t matchIndex = 0;
		while (matchIndex < K - 1 && srcIndex + matchIndex < N - 1 &&
			   iExpr[srcIndex + matchIndex] == iRemove[matchIndex])
			matchIndex++;
		if (matchIndex == K - 1)
			srcIndex += matchIndex;
		result.data[dstIndex++] = iExpr[srcIndex] == '"' ? '\'' : iExpr[srcIndex];
		srcIndex++;
	}
	return result;
}
OWL_DIAG_POP
}// namespace utils

}// namespace owl::debug

#ifndef OWL_PROFILE
#define OWL_PROFILE 0
#else
#undef OWL_PROFILE
#define OWL_PROFILE 1
#endif
#if OWL_PROFILE
// Resolve which function signature macro will be used. Note that this only
// is resolved when the (pre)compiler starts, so the syntax highlighting
// could mark the wrong one in your editor!
#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600)) ||      \
		defined(__ghs__)
#define OWL_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(__DMC__) && (__DMC__ >= 0x810)
#define OWL_FUNC_SIG __PRETTY_FUNCTION__
#elif (defined(__FUNCSIG__) || (_MSC_VER))
#define OWL_FUNC_SIG __FUNCSIG__
#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#define OWL_FUNC_SIG __FUNCTION__
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#define OWL_FUNC_SIG __FUNC__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#define OWL_FUNC_SIG __func__
#elif defined(__cplusplus) && (__cplusplus >= 201103)
#define OWL_FUNC_SIG __func__
#else
#define OWL_FUNC_SIG "OWL_FUNC_SIG unknown!"
#endif

#define OWL_PROFILE_BEGIN_SESSION(name, filepath) ::owl::debug::Profiler::get().beginSession(name, filepath);
#define OWL_PROFILE_END_SESSION() ::owl::debug::Profiler::get().endSession();
#define OWL_PROFILE_SCOPE_LINE2(name, line)                                                                            \
	constexpr auto fixedName##line = ::owl::debug::utils::cleanupOutputString(name, "__cdecl ");                       \
	::owl::debug::ProfileTimer timer##line(fixedName##line.data);
#define OWL_PROFILE_SCOPE_LINE(name, line) OWL_PROFILE_SCOPE_LINE2(name, line)
#define OWL_PROFILE_SCOPE(name) OWL_PROFILE_SCOPE_LINE(name, __LINE__)
#define OWL_PROFILE_FUNCTION() OWL_PROFILE_SCOPE(OWL_FUNC_SIG)
#else
#define OWL_PROFILE_BEGIN_SESSION(name, filepath)
#define OWL_PROFILE_END_SESSION()
#define OWL_PROFILE_SCOPE(name)
#define OWL_PROFILE_FUNCTION()
#endif
