/**
 * @file Environment.cpp
 * @author Silmaen
 * @date 13/02/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/Environment.h"

namespace owl::core {

auto getEnv(const std::string& iKey) -> std::string {
#ifdef OWL_PLATFORM_WINDOWS
	size_t requiredSize = 0;
	auto err = getenv_s(&requiredSize, nullptr, 0, iKey.c_str());
	if (requiredSize == 0u or err != 0u)
		return "";
	std::string buffer;
	buffer.resize(requiredSize);
	err = getenv_s(&requiredSize, buffer.data(), requiredSize, iKey.c_str());
	if (err != 0)
		return "";
	return buffer;
#else
	const char* oldPath = getenv(iKey.c_str());
	if (oldPath == nullptr)
		return "";
	return oldPath;
#endif
}

void setEnv(const std::string& iKey, const std::string& iValue) {
#ifdef OWL_PLATFORM_WINDOWS
	_putenv_s(iKey.c_str(), iValue.c_str());
#else
	setenv(iKey.c_str(), iValue.c_str(), 1 /* overwrite */);
#endif
}

void appendEnv(const std::string& iKey, const std::string& iValue, const std::string& iSeparator) {
	auto str = getEnv(iKey);
	if (str.empty())
		str = iValue;
	else
		str = iValue + iSeparator + str;
	setEnv(iKey, str);
}

template<typename T>
void setEnvValue(const std::string& iKey, const T& iValue) {
	setEnv(iKey, std::format("{}", iValue));
}

template<typename T>
void appendEnvValue(const std::string& iKey, const T& iValue) {
	appendEnv(iKey, std::format("{}", iValue));
}

}// namespace owl::core
