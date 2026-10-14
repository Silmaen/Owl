/**
 * @file Utf8.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/utils/Utf8.h"

#include <cstdint>

namespace owl::core::utils {

namespace {
// Shape of a sequence given its lead byte: continuation count and the allowed range of the second byte.
struct LeadInfo {
	uint8_t continuations = 0;
	uint8_t secondMin = 0x80;
	uint8_t secondMax = 0xBF;
	char32_t payload = 0;
};

constexpr auto leadInfo(const uint8_t iLead) noexcept -> LeadInfo {
	if (iLead >= 0xC2 && iLead <= 0xDF)
		return {.continuations = 1, .payload = static_cast<char32_t>(iLead & 0x1Fu)};
	if (iLead >= 0xE0 && iLead <= 0xEF) {
		// E0 would be overlong below A0, ED would encode a surrogate from A0.
		const uint8_t secondMin = iLead == 0xE0 ? 0xA0 : 0x80;
		const uint8_t secondMax = iLead == 0xED ? 0x9F : 0xBF;
		return {.continuations = 2,
				.secondMin = secondMin,
				.secondMax = secondMax,
				.payload = static_cast<char32_t>(iLead & 0x0Fu)};
	}
	if (iLead >= 0xF0 && iLead <= 0xF4) {
		// F0 would be overlong below 90, F4 would exceed U+10FFFF from 90.
		const uint8_t secondMin = iLead == 0xF0 ? 0x90 : 0x80;
		const uint8_t secondMax = iLead == 0xF4 ? 0x8F : 0xBF;
		return {.continuations = 3,
				.secondMin = secondMin,
				.secondMax = secondMax,
				.payload = static_cast<char32_t>(iLead & 0x07u)};
	}
	return {.continuations = 0, .payload = 0};
}
}// namespace

auto decodeNextCodePoint(const std::string_view iText, size_t& ioOffset) noexcept -> char32_t {
	const auto lead = static_cast<uint8_t>(iText[ioOffset++]);
	if (lead < 0x80)
		return lead;
	const auto info = leadInfo(lead);
	if (info.continuations == 0)
		return g_ReplacementCharacter;
	char32_t codePoint = info.payload;
	for (uint8_t index = 0; index < info.continuations; ++index) {
		if (ioOffset >= iText.size())
			return g_ReplacementCharacter;
		const auto byte = static_cast<uint8_t>(iText[ioOffset]);
		const uint8_t low = index == 0 ? info.secondMin : uint8_t{0x80};
		const uint8_t high = index == 0 ? info.secondMax : uint8_t{0xBF};
		if (byte < low || byte > high)
			return g_ReplacementCharacter;
		codePoint = (codePoint << 6u) | static_cast<char32_t>(byte & 0x3Fu);
		++ioOffset;
	}
	return codePoint;
}

auto decodeUtf8(const std::string_view iText) -> std::u32string {
	std::u32string result;
	result.reserve(iText.size());
	size_t offset = 0;
	while (offset < iText.size()) result.push_back(decodeNextCodePoint(iText, offset));
	return result;
}

}// namespace owl::core::utils
