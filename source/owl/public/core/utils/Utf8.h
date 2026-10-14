/**
 * @file Utf8.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace owl::core::utils {

/// Code point substituted for every invalid UTF-8 sequence (U+FFFD REPLACEMENT CHARACTER).
inline constexpr char32_t g_ReplacementCharacter = U'�';

/**
 * @brief
 *  Decode the code point starting at a byte offset of a UTF-8 string.
 *
 * Invalid input (stray continuation byte, truncated sequence, overlong form, surrogate, value above U+10FFFF) gives
 * `g_ReplacementCharacter` and consumes the maximal invalid subpart only, as the Unicode standard recommends: the
 * next call resumes on the first byte that cannot continue the broken sequence.
 * @param[in] iText The UTF-8 text.
 * @param[in,out] ioOffset Byte offset of the sequence, moved past it; must be lower than `iText.size()`.
 * @return The decoded code point.
 */
[[nodiscard]] OWL_API auto decodeNextCodePoint(std::string_view iText, size_t& ioOffset) noexcept -> char32_t;

/**
 * @brief
 *  Decode a whole UTF-8 string into code points.
 * @param[in] iText The UTF-8 text.
 * @return The code points, invalid sequences replaced by `g_ReplacementCharacter`.
 */
[[nodiscard]] OWL_API auto decodeUtf8(std::string_view iText) -> std::u32string;

}// namespace owl::core::utils
