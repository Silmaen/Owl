/**
 * @file AndroidApp.cpp
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "platform/AndroidApp.h"

#include "core/Log.h"

#include <android/asset_manager.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace owl::platform::android {

namespace {

// Name of the file list in the APK assets and in the internal storage.
constexpr auto g_AssetList = "owl_assets.txt";

auto getAppSlot() -> android_app*& {
	static android_app* sApp = nullptr;
	return sApp;
}

auto readAsset(AAssetManager* iManager, const std::string& iPath) -> std::optional<std::string> {
	AAsset* asset = AAssetManager_open(iManager, iPath.c_str(), AASSET_MODE_STREAMING);
	if (asset == nullptr)
		return std::nullopt;
	std::string content(static_cast<size_t>(AAsset_getLength64(asset)), '\0');
	const auto read = AAsset_read(asset, content.data(), content.size());
	AAsset_close(asset);
	if (read < 0 || static_cast<size_t>(read) != content.size())
		return std::nullopt;
	return content;
}

auto readFile(const std::filesystem::path& iPath) -> std::string {
	const std::ifstream file(iPath, std::ios::binary);
	std::ostringstream content;
	content << file.rdbuf();
	return content.str();
}

auto writeFile(const std::filesystem::path& iPath, const std::string& iContent) -> bool {
	std::error_code error;
	std::filesystem::create_directories(iPath.parent_path(), error);
	std::ofstream file(iPath, std::ios::binary | std::ios::trunc);
	file.write(iContent.data(), static_cast<std::streamsize>(iContent.size()));
	return file.good();
}

}// namespace

void setApp(android_app* ioApp) { getAppSlot() = ioApp; }

auto getApp() -> android_app* { return getAppSlot(); }

void finishActivity() {
	auto* app = getApp();
	if (app == nullptr)
		return;
	ANativeActivity_finish(app->activity);
	while (app->destroyRequested == 0) {
		android_poll_source* source = nullptr;
		if (ALooper_pollOnce(-1, nullptr, nullptr, reinterpret_cast<void**>(&source)) >= 0 && source != nullptr)
			source->process(app, source);
	}
	setApp(nullptr);
}

auto getDataDirectory() -> std::filesystem::path {
	const auto* app = getApp();
	if (app == nullptr || app->activity == nullptr || app->activity->internalDataPath == nullptr)
		return {};
	return app->activity->internalDataPath;
}

auto extractAssets() -> std::filesystem::path {
	const auto dataDir = getDataDirectory();
	if (dataDir.empty())
		return {};
	AAssetManager* manager = getApp()->activity->assetManager;
	const auto list = readAsset(manager, g_AssetList);
	if (!list.has_value()) {
		OWL_CORE_WARN("Android: The APK has no {}, nothing to extract.", g_AssetList)
		return dataDir;
	}
	if (const auto installed = dataDir / g_AssetList; exists(installed) && readFile(installed) == *list) {
		OWL_CORE_INFO("Android: Assets already extracted in '{}'.", dataDir.string())
		return dataDir;
	}
	std::istringstream lines(*list);
	std::string line;
	// First line: the build identifier, only compared as part of the list.
	std::getline(lines, line);
	size_t count = 0;
	while (std::getline(lines, line)) {
		if (line.empty())
			continue;
		const auto content = readAsset(manager, line);
		if (!content.has_value() || !writeFile(dataDir / line, *content)) {
			OWL_CORE_ERROR("Android: Cannot extract the asset '{}'.", line)
			continue;
		}
		++count;
	}
	// Written last: an interrupted extraction starts again at the next launch.
	if (!writeFile(dataDir / g_AssetList, *list))
		OWL_CORE_WARN("Android: Cannot write the asset list in '{}'.", dataDir.string())
	OWL_CORE_INFO("Android: {} asset(s) extracted in '{}'.", count, dataDir.string())
	return dataDir;
}

}// namespace owl::platform::android
