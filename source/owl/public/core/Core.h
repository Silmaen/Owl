/**
 * @file Application.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#if defined(OWL_PLATFORM_WINDOWS)
#ifndef _WIN64
#error 32-bit windows platform is not supported
#endif
#ifdef OWL_BUILD_SHARED
#ifdef OWL_BUILD_DLL
#define OWL_API __declspec(dllexport)
#else
#define OWL_API __declspec(dllimport)
#endif
#else
#define OWL_API
#endif
#elif defined(OWL_PLATFORM_LINUX)
#define OWL_API
#else
#define OWL_API
#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR == 1
#error "IOS simulator is not supported!"
#elif TARGET_OS_IPHONE == 1
#error "IOS is not supported!"
#elif TARGET_OS_MAC == 1
#error "MacOS is not supported!"
#else
#error "Unknown Apple platform!"
#endif
#elif defined(__ANDROID__)
#error "Android is not supported!"
#else
#error "Unknown compile platform so: not supported!"
#endif
#endif

#include <memory>
#include <string>

/**
 * @brief Base Namespace for the project.
 */
namespace owl {
/**
 * @brief Gets the version as code with first 2 bytes major, then minor, then patch, then 00.
 * @return version code.
 */
OWL_API auto getVersionCode() -> uint32_t;

/**
 * @brief Gets the major version number.
 * @return Major version code.
 */
OWL_API auto getVersionMajor() -> uint8_t;

/**
 * @brief Gets the minor version number.
 * @return Minor version code.
 */
OWL_API auto getVersionMinor() -> uint8_t;

/**
 * @brief Gets the patch version number.
 * @return Patch version code.
 */
OWL_API auto getVersionPatch() -> uint8_t;

/**
 * @brief Gets the engine version as a string x.y.z.
 * @return The current engin version.
 */
OWL_API auto getVersionString() -> std::string;

/// Wrap to unique pointer.
template<typename T>
using uniq = std::unique_ptr<T>;

/**
 * @brief Wrap to unique pointer creator.
 * @tparam T Type of data.
 * @tparam Args Args type to pass to the constructor.
 * @param[in] iArgs Args to pass to the constructor.
 * @return Unique pointer.
 */
template<typename T, typename... Args>
constexpr auto mkUniq(Args&&... iArgs) -> uniq<T> {
	return std::make_unique<T>(std::forward<Args>(iArgs)...);
}

/// Wrap to shared pointer.
template<typename T>
using shared = std::shared_ptr<T>;

/**
 * @brief Wrap to shared pointer creator.
 * @tparam T Type of data.
 * @tparam Args Args type to pass to the constructor.
 * @param[in] iArgs Args to pass to the constructor.
 * @return Shared pointer.
 */
template<typename T, typename... Args>
constexpr auto mkShared(Args&&... iArgs) -> shared<T> {
	return std::make_shared<T>(std::forward<Args>(iArgs)...);
}

/// Wrap to weak pointer.
template<typename T>
using weak = std::weak_ptr<T>;

}// namespace owl

#include "Assert.h"
