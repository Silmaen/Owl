/**
 * @file AndroidApk.h
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "ApkSigner.h"

#include <core/expected.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::android {

/**
 * @brief
 *  Why an APK could not be written.
 */
enum struct ApkError : uint8_t {
	/// No exported game folder.
	NoGame,
	/// No library of the Android runner (`lib/arm64-v8a/libOwlGameRunner.so`).
	NoRunner,
	/// The signature failed (keystore, OpenSSL).
	Signing,
	/// The APK file could not be written.
	Write,
};

/**
 * @brief
 *  Text of an error.
 * @param[in] iError The error.
 * @return A short sentence.
 */
[[nodiscard]] auto getErrorMessage(ApkError iError) -> std::string_view;

/**
 * @brief
 *  What the manifest of a game's APK says.
 */
struct ApkManifest {
	/// Package name, the identity of the game on the device.
	std::string package;
	/// Name shown by the launcher.
	std::string label;
	/// Version shown to the user.
	std::string versionName;
	/// Version compared by the store and the installer, growing with each release.
	uint32_t versionCode = 1;
	/// Lowest Android API level (33: the first loader exporting Vulkan 1.3).
	uint32_t minSdk = 33;
	/// API level the game is made for.
	uint32_t targetSdk = 36;
	/// Whether the game can be debugged (signed with the debug key).
	bool debuggable = true;
	/// Whether `resources.arsc` holds a launcher icon (`res/mipmap/icon.png`).
	bool hasIcon = false;
};

/**
 * @brief
 *  Write the binary `AndroidManifest.xml` of a game: a `NativeActivity` loading `libOwlGameRunner.so`, Vulkan 1.3
 *  required, landscape, full screen.
 * @param[in] iManifest The values of the manifest.
 * @return The binary XML, as `aapt2` compiles it.
 */
[[nodiscard]] auto makeBinaryManifest(const ApkManifest& iManifest) -> std::vector<uint8_t>;

/**
 * @brief
 *  Write the `resources.arsc` holding the launcher icon (resource `mipmap/icon`, `0x7f010000`).
 * @param[in] iPackage The package name.
 * @return The resource table.
 */
[[nodiscard]] auto makeIconResources(std::string_view iPackage) -> std::vector<uint8_t>;

/**
 * @brief
 *  Version code of a version name: `1.2.3` gives 10203.
 * @param[in] iVersion The version name.
 * @return The code, at least 1.
 */
[[nodiscard]] auto getVersionCode(std::string_view iVersion) -> uint32_t;

/**
 * @brief
 *  Write the signed APK of a game exported for Android.
 * @param[in] iGameDirectory The folder `GameExporter` wrote for `ExportTarget::Android`: the assets of the APK.
 * @param[in] iRunnerDirectory The Android runner: its `lib/arm64-v8a/` libraries.
 * @param[in] iIcon PNG of the launcher icon, empty for none.
 * @param[in] iManifest The values of the manifest.
 * @param[in] iKey The signing key.
 * @param[in] iOutput The APK to write.
 * @return Nothing on success, the error otherwise.
 */
auto writeApk(const std::filesystem::path& iGameDirectory, const std::filesystem::path& iRunnerDirectory,
			  const std::filesystem::path& iIcon, const ApkManifest& iManifest, const SigningKey& iKey,
			  const std::filesystem::path& iOutput) -> expected<void, ApkError>;

}// namespace owl::nest::android
