/**
 * @file Environment.h
 * @author Silmaen
 * @date 13/02/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Core.h"

namespace owl::core {

#ifdef OWL_PLATFORM_WINDOWS
constexpr char g_sep[] = ";";
#else
constexpr char g_sep[] = ":";
#endif

/**
 * @brief Get an environment variable value.
 * @param[in] iKey The Environment variable's name.
 * @return Value or empty string.
 */
auto OWL_API getEnv(const std::string& iKey) -> std::string;

/**
 * @brief Define (overwrite) an environment variable.
 * @param[in] iKey The Environment variable's name.
 * @param[in] iValue The new value.
 */
void OWL_API setEnv(const std::string& iKey, const std::string& iValue);

/**
 * @brief Append a value to existing environment variable.
 * @param[in] iKey The Environment variable's name.
 * @param[in] iValue The value to add.
 * @param[in] iSeparator The separator between values.
 */
void OWL_API appendEnv(const std::string& iKey, const std::string& iValue, const std::string& iSeparator = g_sep);

/**
 * @brief Define (overwrite) an environment variable.
 * @tparam T The type of value.
 * @param[in] iKey The Environment variable's name.
 * @param[in] iValue The new value.
 */
template<typename T>
void OWL_API setEnvValue(const std::string& iKey, const T& iValue);

/**
 * @brief Append a value to existing environment variable.
 * @tparam T The type of value.
 * @param[in] iKey The Environment variable's name.
 * @param[in] iValue The value to add.
 */
template<typename T>
void OWL_API appendEnvValue(const std::string& iKey, const T& iValue);

}// namespace owl::core
