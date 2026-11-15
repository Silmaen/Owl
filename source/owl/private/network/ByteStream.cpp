/**
 * @file ByteStream.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "network/ByteStream.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace owl::network {

namespace {

// Append the low bytes of an integer, least significant first.
void appendLittleEndian(std::vector<uint8_t>& ioBytes, const uint64_t iValue, const size_t iWidth) {
	for (size_t i = 0; i < iWidth; ++i) ioBytes.push_back(static_cast<uint8_t>((iValue >> (8 * i)) & 0xffu));
}

}// namespace

void ByteWriter::writeU8(const uint8_t iValue) { m_bytes.push_back(iValue); }

void ByteWriter::writeU16(const uint16_t iValue) { appendLittleEndian(m_bytes, iValue, 2); }

void ByteWriter::writeU32(const uint32_t iValue) { appendLittleEndian(m_bytes, iValue, 4); }

void ByteWriter::writeU64(const uint64_t iValue) { appendLittleEndian(m_bytes, iValue, 8); }

void ByteWriter::writeF32(const float iValue) { writeU32(std::bit_cast<uint32_t>(iValue)); }

void ByteWriter::writeString(const std::string& iValue) {
	writeU32(static_cast<uint32_t>(iValue.size()));
	m_bytes.insert(m_bytes.end(), iValue.begin(), iValue.end());
}

void ByteWriter::writeBytes(const std::span<const uint8_t> iBytes) {
	m_bytes.insert(m_bytes.end(), iBytes.begin(), iBytes.end());
}

auto ByteReader::readUnsigned(const size_t iWidth) -> std::optional<uint64_t> {
	if (m_failed || getRemaining() < iWidth) {
		m_failed = true;
		return std::nullopt;
	}
	uint64_t value = 0;
	for (size_t i = 0; i < iWidth; ++i) value |= static_cast<uint64_t>(m_bytes[m_offset + i]) << (8 * i);
	m_offset += iWidth;
	return value;
}

auto ByteReader::readU8() -> std::optional<uint8_t> {
	return readUnsigned(1).transform([](const uint64_t iValue) -> uint8_t { return static_cast<uint8_t>(iValue); });
}

auto ByteReader::readU16() -> std::optional<uint16_t> {
	return readUnsigned(2).transform([](const uint64_t iValue) -> uint16_t { return static_cast<uint16_t>(iValue); });
}

auto ByteReader::readU32() -> std::optional<uint32_t> {
	return readUnsigned(4).transform([](const uint64_t iValue) -> uint32_t { return static_cast<uint32_t>(iValue); });
}

auto ByteReader::readU64() -> std::optional<uint64_t> { return readUnsigned(8); }

auto ByteReader::readF32() -> std::optional<float> {
	return readU32().transform([](const uint32_t iValue) -> float { return std::bit_cast<float>(iValue); });
}

auto ByteReader::readString() -> std::optional<std::string> {
	const auto length = readU32();
	if (!length || getRemaining() < *length) {
		m_failed = true;
		return std::nullopt;
	}
	std::string value(m_bytes.begin() + static_cast<std::ptrdiff_t>(m_offset),
					  m_bytes.begin() + static_cast<std::ptrdiff_t>(m_offset + *length));
	m_offset += *length;
	return value;
}

auto ByteReader::readRemaining() -> std::span<const uint8_t> {
	const auto rest = m_bytes.subspan(m_offset);
	m_offset = m_bytes.size();
	return rest;
}

}// namespace owl::network
