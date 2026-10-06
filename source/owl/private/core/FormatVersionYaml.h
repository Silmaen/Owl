/**
 * @file FormatVersionYaml.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/FormatVersion.h"
#include "core/external/yaml.h"

#include <cstdint>
#include <string_view>

namespace owl::core {

/**
 * @brief
 *  Emit the `FormatVersion` key of a format into a map being written.
 * @param[in,out] ioEmitter Emitter positioned inside the root map.
 * @param[in] iFormat The format whose current version is written.
 */
OWL_API void emitFormatVersion(YAML::Emitter& ioEmitter, const DocumentFormat& iFormat);

/**
 * @brief
 *  `upgradeDocument` on a raw YAML root.
 * @param[in] iFormat The format of the document.
 * @param[in,out] ioRoot The document root, migrated in place.
 * @param[in] iSourceName File or buffer name for the log messages.
 * @return The version the document was written with, or the failure reason.
 */
[[nodiscard]] OWL_API auto upgradeYamlDocument(const DocumentFormat& iFormat, YAML::Node& ioRoot,
											   std::string_view iSourceName) -> expected<uint32_t, FormatError>;

}// namespace owl::core
