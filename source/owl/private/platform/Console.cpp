/**
 * @file Console.cpp
 * @author Silmaen
 * @date 19/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "platform/Console.h"

#ifdef OWL_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <io.h>
#endif

namespace owl::platform {

#ifdef OWL_PLATFORM_WINDOWS
namespace {

auto isRedirected(const DWORD iStream) -> bool {
	const HANDLE handle = GetStdHandle(iStream);
	if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
		return false;
	const DWORD type = GetFileType(handle);
	return type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE;
}

auto reopenOnConsole(std::FILE* iStream, const DWORD iStdHandle) -> bool {
	if (std::freopen("CONOUT$", "w", iStream) == nullptr)
		return false;
	std::setvbuf(iStream, nullptr, _IONBF, 0);
	// The logger and the C runtime both write through the standard handle: point it at the console too.
	SetStdHandle(iStdHandle, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(iStream))));
	return true;
}

}// namespace

auto attachParentConsole() -> bool {
	const bool outRedirected = isRedirected(STD_OUTPUT_HANDLE);
	const bool errRedirected = isRedirected(STD_ERROR_HANDLE);
	if (outRedirected && errRedirected)
		return false;
	if (AttachConsole(ATTACH_PARENT_PROCESS) == 0)
		return false;
	bool attached = false;
	if (!outRedirected)
		attached = reopenOnConsole(stdout, STD_OUTPUT_HANDLE);
	if (!errRedirected)
		reopenOnConsole(stderr, STD_ERROR_HANDLE);
	return attached;
}
#else
auto attachParentConsole() -> bool { return false; }
#endif

}// namespace owl::platform
