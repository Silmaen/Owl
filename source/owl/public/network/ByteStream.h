/**
 * @file ByteStream.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace owl::network {

/**
 * @brief
 *  Writes values as little-endian bytes, the byte order of every Owl network message.
 */
class OWL_API ByteWriter {
public:
	/**
	 * @brief
	 *  Append one byte.
	 * @param[in] iValue The value.
	 */
	void writeU8(uint8_t iValue);

	/**
	 * @brief
	 *  Append a 16-bit unsigned integer.
	 * @param[in] iValue The value.
	 */
	void writeU16(uint16_t iValue);

	/**
	 * @brief
	 *  Append a 32-bit unsigned integer.
	 * @param[in] iValue The value.
	 */
	void writeU32(uint32_t iValue);

	/**
	 * @brief
	 *  Append a 64-bit unsigned integer.
	 * @param[in] iValue The value.
	 */
	void writeU64(uint64_t iValue);

	/**
	 * @brief
	 *  Append a 32-bit float, bit for bit.
	 * @param[in] iValue The value.
	 */
	void writeF32(float iValue);

	/**
	 * @brief
	 *  Append a string: its byte length on 32 bits, then its bytes.
	 * @param[in] iValue The string.
	 */
	void writeString(const std::string& iValue);

	/**
	 * @brief
	 *  Append raw bytes, without length.
	 * @param[in] iBytes The bytes.
	 */
	void writeBytes(std::span<const uint8_t> iBytes);

	/**
	 * @brief
	 *  Get the bytes written.
	 * @return The bytes.
	 */
	[[nodiscard]] auto getBytes() const -> std::span<const uint8_t> { return m_bytes; }

	/**
	 * @brief
	 *  Forget the bytes written, keeping the memory.
	 */
	void clear() { m_bytes.clear(); }

private:
	/// The bytes written.
	std::vector<uint8_t> m_bytes;
};

/**
 * @brief
 *  Reads the little-endian values a `ByteWriter` wrote. A read past the end returns nothing and marks the reader as
 *  failed, so a truncated or hostile message never reads out of bounds.
 */
class OWL_API ByteReader {
public:
	/**
	 * @brief
	 *  Read a sequence of bytes, which must outlive the reader.
	 * @param[in] iBytes The bytes.
	 */
	explicit ByteReader(const std::span<const uint8_t> iBytes) : m_bytes{iBytes} {}

	/**
	 * @brief
	 *  Read one byte.
	 * @return The value, or nothing past the end.
	 */
	auto readU8() -> std::optional<uint8_t>;

	/**
	 * @brief
	 *  Read a 16-bit unsigned integer.
	 * @return The value, or nothing past the end.
	 */
	auto readU16() -> std::optional<uint16_t>;

	/**
	 * @brief
	 *  Read a 32-bit unsigned integer.
	 * @return The value, or nothing past the end.
	 */
	auto readU32() -> std::optional<uint32_t>;

	/**
	 * @brief
	 *  Read a 64-bit unsigned integer.
	 * @return The value, or nothing past the end.
	 */
	auto readU64() -> std::optional<uint64_t>;

	/**
	 * @brief
	 *  Read a 32-bit float.
	 * @return The value, or nothing past the end.
	 */
	auto readF32() -> std::optional<float>;

	/**
	 * @brief
	 *  Read a string written by `ByteWriter::writeString`.
	 * @return The string, or nothing when its length goes past the end.
	 */
	auto readString() -> std::optional<std::string>;

	/**
	 * @brief
	 *  Get the bytes not read yet, consuming them.
	 * @return The remaining bytes.
	 */
	auto readRemaining() -> std::span<const uint8_t>;

	/**
	 * @brief
	 *  Get the number of bytes not read yet.
	 * @return The count.
	 */
	[[nodiscard]] auto getRemaining() const -> size_t { return m_bytes.size() - m_offset; }

	/**
	 * @brief
	 *  Check whether a read went past the end.
	 * @return True after a failed read.
	 */
	[[nodiscard]] auto hasFailed() const -> bool { return m_failed; }

private:
	/**
	 * @brief
	 *  Read an unsigned little-endian integer of a given width.
	 * @param[in] iWidth Number of bytes.
	 * @return The value, or nothing past the end.
	 */
	auto readUnsigned(size_t iWidth) -> std::optional<uint64_t>;

	/// The bytes read.
	std::span<const uint8_t> m_bytes;
	/// Offset of the next byte.
	size_t m_offset = 0;
	/// Whether a read went past the end.
	bool m_failed = false;
};

}// namespace owl::network
