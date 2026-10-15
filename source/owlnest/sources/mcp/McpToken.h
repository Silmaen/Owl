/**
 * @file McpToken.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace owl::nest::mcp {

/// Bytes of a bearer token (256 bits).
constexpr size_t g_tokenBytes = 32;

/**
 * @brief
 *  Draw random bytes from the operating system CSPRNG, as lowercase hexadecimal.
 * @param[in] iBytes Number of random bytes.
 * @return The `2 * iBytes` hexadecimal characters, nothing when the system generator fails.
 */
[[nodiscard]] auto generateRandomHex(size_t iBytes) -> std::optional<std::string>;

/**
 * @brief
 *  Generate a bearer token: 256 bits from the operating system CSPRNG.
 * @return The 64 hexadecimal characters, nothing when the system generator fails.
 */
[[nodiscard]] auto generateToken() -> std::optional<std::string>;

/**
 * @brief
 *  Compare two tokens in a time that depends on their lengths only.
 * @param[in] iExpected The token of the server.
 * @param[in] iCandidate The token a client sent.
 * @return True when they are equal.
 */
[[nodiscard]] auto isSameToken(std::string_view iExpected, std::string_view iCandidate) -> bool;

/**
 * @brief
 *  Default file of the token: `$XDG_RUNTIME_DIR/owlnest/mcp_token`, else the user configuration directory
 *  (`$XDG_CONFIG_HOME` or `~/.config` on Linux, `%LOCALAPPDATA%` on Windows) under `owlnest`.
 * @return The file path (its directory may not exist yet).
 */
[[nodiscard]] auto getDefaultTokenFile() -> std::filesystem::path;

/**
 * @brief
 *  Write a token to a file only its owner can read and write (`0600`, directory `0700` when created).
 * @param[in] iFile The file.
 * @param[in] iToken The token.
 * @return True on success.
 */
auto writeTokenFile(const std::filesystem::path& iFile, std::string_view iToken) -> bool;

/**
 * @brief
 *  Remove a token file, when it exists.
 * @param[in] iFile The file.
 */
void removeTokenFile(const std::filesystem::path& iFile);

}// namespace owl::nest::mcp
