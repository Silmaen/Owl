/**
 * @file AuthenticodeSigner.h
 * @author Silmaen
 * @date 25/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "android/ApkSigner.h"

#include <core/expected.h>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::signing {

/**
 * @brief
 *  Why an executable could not be signed.
 */
enum struct AuthenticodeError : uint8_t {
	/// The file could not be read or written.
	File,
	/// The file is not a 64-bit or 32-bit Windows executable.
	NotExecutable,
	/// OpenSSL failed to sign.
	Signature,
	/// The custom signing command failed to start or returned an error.
	Command,
};

/**
 * @brief
 *  Get the sentence describing a signing error.
 * @param[in] iError The error.
 * @return A short message.
 */
[[nodiscard]] auto getErrorMessage(AuthenticodeError iError) -> std::string_view;

/**
 * @brief
 *  Authenticode digest of a PE image: SHA-256 of the file without its checksum, its certificate table entry and
 *  its certificate table.
 * @param[in] iImage The file content, its size a multiple of 8 when a signature is to be appended.
 * @return The digest, empty when the image is not a PE.
 */
[[nodiscard]] auto getAuthenticodeDigest(std::span<const uint8_t> iImage) -> std::vector<uint8_t>;

/**
 * @brief
 *  Sign a Windows executable with Authenticode (SHA-256, RSA), replacing a signature it already has. The signature
 *  is not time-stamped.
 * @param[in] iExecutable The executable, rewritten in place.
 * @param[in] iKey The key and its certificate.
 * @return Nothing on success, the error otherwise.
 */
[[nodiscard]] auto signExecutable(const std::filesystem::path& iExecutable, const android::SigningKey& iKey)
		-> expected<void, AuthenticodeError>;

/**
 * @brief
 *  Sign an executable with a command of the developer (a cloud signing service, a hardware token).
 * @param[in] iExecutable The executable.
 * @param[in] iCommand The command line; `{file}` is replaced by the quoted path of the executable, which is appended
 *  when the command has no `{file}`.
 * @return Nothing on success, the error otherwise.
 */
[[nodiscard]] auto signExecutableWithCommand(const std::filesystem::path& iExecutable, const std::string& iCommand)
		-> expected<void, AuthenticodeError>;

}// namespace owl::nest::signing
