/**
 * @file AssetSearchPaths.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <filesystem>
#include <vector>

/**
 * @brief
 *  Asset management.
 */
namespace owl::data::assets {

/**
 * @brief
 *  Directories the asset libraries search, in priority order.
 * The asset directories of the running application, or the working directory when there is none. Defined in the
 * engine library, so the `data` module does not depend on `app`.
 * @return The directories.
 */
[[nodiscard]] OWL_API auto getAssetSearchPaths() -> std::vector<std::filesystem::path>;

}// namespace owl::data::assets
