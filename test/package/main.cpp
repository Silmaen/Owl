/**
 * @file main.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include <owl.h>

#include <print>

// The signature must match the one app/Application.h declares.
auto main(int /*iArgc*/, char* /*iArgv*/[]) -> int {
	std::println("OwlEngine {} found and linked.", owl::getVersionString());
	return owl::getVersionString().empty() ? 1 : 0;
}
