/**
 * @file Utf8_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/utils/Utf8.h>

#include <cstddef>
#include <string>
#include <string_view>

using namespace owl::core::utils;

namespace {
constexpr char32_t g_fffd = g_ReplacementCharacter;
}// namespace

TEST(Utf8, asciiIsUnchanged) { EXPECT_EQ(decodeUtf8("Hello\n"), U"Hello\n"); }

TEST(Utf8, emptyString) { EXPECT_TRUE(decodeUtf8("").empty()); }

TEST(Utf8, multiByteSequences) {
	// é (2 bytes), € (3 bytes), 😀 U+1F600 (4 bytes), œ U+0153 (2 bytes).
	EXPECT_EQ(decodeUtf8("\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xC5\x93"), U"é€\U0001F600œ");
}

TEST(Utf8, boundaries) {
	EXPECT_EQ(decodeUtf8("\x7F"), U"\x7F");
	EXPECT_EQ(decodeUtf8("\xC2\x80"), U"\u0080");
	EXPECT_EQ(decodeUtf8("\xDF\xBF"), U"߿");
	EXPECT_EQ(decodeUtf8("\xE0\xA0\x80"), U"ࠀ");
	EXPECT_EQ(decodeUtf8("\xEF\xBF\xBF"), U"￿");
	EXPECT_EQ(decodeUtf8("\xF0\x90\x80\x80"), U"\U00010000");
	EXPECT_EQ(decodeUtf8("\xF4\x8F\xBF\xBF"), U"\U0010FFFF");
}

TEST(Utf8, nextCodePointAdvancesOffset) {
	const std::string_view text = "a\xE2\x82\xAC"
								  "b";
	size_t offset = 0;
	EXPECT_EQ(decodeNextCodePoint(text, offset), U'a');
	EXPECT_EQ(offset, 1u);
	EXPECT_EQ(decodeNextCodePoint(text, offset), U'€');
	EXPECT_EQ(offset, 4u);
	EXPECT_EQ(decodeNextCodePoint(text, offset), U'b');
	EXPECT_EQ(offset, 5u);
}

TEST(Utf8, strayContinuationAndInvalidLeads) {
	EXPECT_EQ(decodeUtf8("a\x80"
						 "b"),
			  std::u32string({U'a', g_fffd, U'b'}));
	EXPECT_EQ(decodeUtf8("\xC0\xAF"), std::u32string({g_fffd, g_fffd}));
	EXPECT_EQ(decodeUtf8("\xC1\xBF"), std::u32string({g_fffd, g_fffd}));
	EXPECT_EQ(decodeUtf8("\xF5\x80"), std::u32string({g_fffd, g_fffd}));
	EXPECT_EQ(decodeUtf8("\xFF"), std::u32string({g_fffd}));
}

TEST(Utf8, truncatedSequences) {
	EXPECT_EQ(decodeUtf8("\xC3"), std::u32string({g_fffd}));
	EXPECT_EQ(decodeUtf8("\xE2\x82"), std::u32string({g_fffd}));
	EXPECT_EQ(decodeUtf8("\xF0\x9F\x98"), std::u32string({g_fffd}));
	// A broken sequence consumes only its valid prefix: the next byte starts again.
	EXPECT_EQ(decodeUtf8("\xE2\x82"
						 "A"),
			  std::u32string({g_fffd, U'A'}));
	EXPECT_EQ(decodeUtf8("\xC3\xC3\xA9"), std::u32string({g_fffd, U'é'}));
}

TEST(Utf8, overlongSurrogateAndOutOfRange) {
	// Overlong forms of '/' and of U+0800.
	EXPECT_EQ(decodeUtf8("\xE0\x80\xAF"), std::u32string({g_fffd, g_fffd, g_fffd}));
	EXPECT_EQ(decodeUtf8("\xF0\x80\x80\xAF"), std::u32string({g_fffd, g_fffd, g_fffd, g_fffd}));
	// Surrogate U+D800.
	EXPECT_EQ(decodeUtf8("\xED\xA0\x80"), std::u32string({g_fffd, g_fffd, g_fffd}));
	// U+110000.
	EXPECT_EQ(decodeUtf8("\xF4\x90\x80\x80"), std::u32string({g_fffd, g_fffd, g_fffd, g_fffd}));
}
