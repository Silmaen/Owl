/**
 * @file ByteStream_test.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <network/ByteStream.h>

#include <array>
#include <cstdint>
#include <vector>

using namespace owl::network;

TEST(ByteStream, RoundTrip) {
	ByteWriter writer;
	writer.writeU8(0xab);
	writer.writeU16(0x1234);
	writer.writeU32(0xdeadbeef);
	writer.writeU64(0x0102030405060708);
	writer.writeF32(-1.5f);
	writer.writeString("owl");
	const std::array<uint8_t, 2> raw{7, 9};
	writer.writeBytes(raw);

	ByteReader reader{writer.getBytes()};
	EXPECT_EQ(reader.readU8(), 0xab);
	EXPECT_EQ(reader.readU16(), 0x1234);
	EXPECT_EQ(reader.readU32(), 0xdeadbeef);
	EXPECT_EQ(reader.readU64(), 0x0102030405060708u);
	EXPECT_EQ(reader.readF32(), -1.5f);
	EXPECT_EQ(reader.readString(), "owl");
	const auto rest = reader.readRemaining();
	ASSERT_EQ(rest.size(), 2u);
	EXPECT_EQ(rest[1], 9);
	EXPECT_EQ(reader.getRemaining(), 0u);
	EXPECT_FALSE(reader.hasFailed());
}

TEST(ByteStream, LittleEndian) {
	ByteWriter writer;
	writer.writeU32(0x11223344);
	const auto bytes = writer.getBytes();
	ASSERT_EQ(bytes.size(), 4u);
	EXPECT_EQ(bytes[0], 0x44);
	EXPECT_EQ(bytes[3], 0x11);
	writer.clear();
	EXPECT_TRUE(writer.getBytes().empty());
}

TEST(ByteStream, ReadPastEndFails) {
	const std::vector<uint8_t> bytes{1, 2, 3};
	ByteReader reader{bytes};
	EXPECT_FALSE(reader.readU32().has_value());
	EXPECT_TRUE(reader.hasFailed());
	// Once failed, the reader stays failed: a message is either whole or rejected.
	EXPECT_FALSE(reader.readU8().has_value());
}

TEST(ByteStream, StringLongerThanMessageFails) {
	ByteWriter writer;
	writer.writeU32(1000);
	writer.writeU8('a');
	ByteReader reader{writer.getBytes()};
	EXPECT_FALSE(reader.readString().has_value());
	EXPECT_TRUE(reader.hasFailed());
}
