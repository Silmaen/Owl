/**
 * @file AtomicFile.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"
#include "core/expected.h"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace owl::platform {

/**
 * @brief
 *  Step of an atomic write that failed.
 */
enum struct WriteError : uint8_t {
	CreateFailed,///< The temporary file could not be created next to the target.
	WriteFailed,///< The content could not be fully written to the temporary file.
	FlushFailed,///< The temporary file could not be flushed to the storage device.
	RenameFailed,///< The temporary file could not replace the target.
};

/**
 * @brief
 *  Human-readable description of an atomic write error.
 * @param[in] iError The error.
 * @return A short sentence fragment describing the error.
 */
[[nodiscard]] OWL_API auto describe(WriteError iError) -> std::string_view;

/**
 * @brief
 *  Write a whole file so that readers see either the old content or the new one, never a truncated mix.
 *
 * The content goes to a temporary file in the target's directory, is flushed to the device (`fsync` /
 * `_commit`), then replaces the target with an atomic rename. On any failure the temporary file is
 * removed, the previous target is left untouched and the cause is logged. The data is written as is
 * (binary mode, no newline translation). The parent directory must exist.
 * @param[in] iPath The file to create or replace.
 * @param[in] iContent The complete new content.
 * @return Nothing on success, the failed step otherwise.
 */
[[nodiscard]] OWL_API auto writeFileAtomic(const std::filesystem::path& iPath, std::string_view iContent)
		-> expected<void, WriteError>;

}// namespace owl::platform
