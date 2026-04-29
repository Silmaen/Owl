/**
 * @file openExternalUrl.cpp
 * @author Silmaen
 * @date 29/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/utils/openExternalUrl.h"

#include <core/Log.h>

#ifdef OWL_PLATFORM_WINDOWS
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace owl::core::utils {

namespace {

/// @brief Reject anything that does not look like a safe http(s) or mailto URL.
[[nodiscard]] auto isAllowedScheme(std::string_view iUrl) -> bool {
	return iUrl.starts_with("http://") || iUrl.starts_with("https://") || iUrl.starts_with("mailto:");
}

}// namespace

void openExternalUrl(const std::string_view iUrl) {
	if (iUrl.empty())
		return;
	if (!isAllowedScheme(iUrl)) {
		OWL_CORE_WARN("openExternalUrl: rejected URL with unsupported scheme: '{}'", iUrl)
		return;
	}

#ifdef OWL_PLATFORM_WINDOWS
	const std::string urlCopy{iUrl};
	const int wcLen = MultiByteToWideChar(CP_UTF8, 0, urlCopy.c_str(), -1, nullptr, 0);
	if (wcLen <= 0) {
		OWL_CORE_ERROR("openExternalUrl: failed to convert URL to UTF-16: '{}'", iUrl)
		return;
	}
	std::wstring wurl(static_cast<size_t>(wcLen - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, urlCopy.c_str(), -1, wurl.data(), wcLen);
	const auto result = ShellExecuteW(nullptr, L"open", wurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	// ShellExecuteW returns an HINSTANCE > 32 on success.
	if (reinterpret_cast<INT_PTR>(result) <= 32)
		OWL_CORE_ERROR("openExternalUrl: ShellExecuteW failed for URL '{}'", iUrl)
#else
	// fork + execvp: avoids shell interpolation and is robust against URL contents.
	const std::string urlCopy{iUrl};
	const pid_t pid = fork();
	if (pid < 0) {
		OWL_CORE_ERROR("openExternalUrl: fork failed for URL '{}'", iUrl)
		return;
	}
	if (pid == 0) {
		// Detach from the parent stdio so the browser does not interleave on the terminal.
		setsid();
		const char* const argv[] = {"xdg-open", urlCopy.c_str(), nullptr};
		execvp(argv[0], const_cast<char* const*>(argv));
		// Reached only if execvp failed. Use _exit to avoid running parent atexit handlers.
		_exit(127);
	}
	// Reap the immediate xdg-open wrapper so it does not turn into a zombie.
	int status = 0;
	waitpid(pid, &status, WNOHANG);
#endif
}

}// namespace owl::core::utils
