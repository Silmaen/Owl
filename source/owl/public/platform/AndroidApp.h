/**
 * @file AndroidApp.h
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"

#include <filesystem>

#ifdef OWL_PLATFORM_ANDROID
struct android_app;

/**
 * @brief
 *  Entry point of the native activity, called by the native app glue (defined by `app/EntryPoint.h`).
 * @param[in,out] ioApp The activity.
 */
extern "C" void android_main(android_app* ioApp);

/**
 * @brief
 *  Android native activity of the running application.
 *
 * The entry point (`app/EntryPoint.h`) registers the activity before the application starts; the window, the input
 * and the asset extraction read it.
 */
namespace owl::platform::android {

/**
 * @brief
 *  Register the native activity of the running application.
 * @param[in] ioApp The activity given to `android_main` by the native app glue.
 */
OWL_API void setApp(android_app* ioApp);

/**
 * @brief
 *  Native activity of the running application.
 * @return The activity registered by setApp(), nullptr before.
 */
[[nodiscard]] OWL_API auto getApp() -> android_app*;

/**
 * @brief
 *  Copy the files packed in the APK `assets/` folder to the internal storage of the application.
 *
 * The APK lists its files in `assets/owl_assets.txt`, one relative path per line after a first line identifying the
 * build; the copy is skipped when the internal storage already holds the list of the same build.
 * @return The internal storage directory, the working directory of the game; empty when no activity is registered.
 */
[[nodiscard]] OWL_API auto extractAssets() -> std::filesystem::path;

/**
 * @brief
 *  Close the activity once the application ended by itself, and wait until the system destroys it.
 */
OWL_API void finishActivity();

/**
 * @brief
 *  Internal storage directory of the application (`internalDataPath` of the activity).
 * @return The directory, empty when no activity is registered.
 */
[[nodiscard]] OWL_API auto getDataDirectory() -> std::filesystem::path;

}// namespace owl::platform::android
#endif
