/**
 * @file GameProtocol.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "GameProtocol.h"

#include <bit>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace owl::network::protocol {

namespace {

enum struct ValueKind : uint8_t { Nil, Boolean, Integer, Number, String };

}// namespace

void writeF64(ByteWriter& ioWriter, const double iValue) { ioWriter.writeU64(std::bit_cast<uint64_t>(iValue)); }

auto readF64(ByteReader& ioReader) -> std::optional<double> {
	const auto bits = ioReader.readU64();
	if (!bits.has_value())
		return std::nullopt;
	return std::bit_cast<double>(*bits);
}

void writeValue(ByteWriter& ioWriter, const script::CallValue& iValue) {
	std::visit(
			[&ioWriter]<typename T>(const T& iHeld) -> void {
				if constexpr (std::is_same_v<T, std::monostate>) {
					ioWriter.writeU8(static_cast<uint8_t>(ValueKind::Nil));
				} else if constexpr (std::is_same_v<T, bool>) {
					ioWriter.writeU8(static_cast<uint8_t>(ValueKind::Boolean));
					ioWriter.writeU8(iHeld ? 1 : 0);
				} else if constexpr (std::is_same_v<T, int64_t>) {
					ioWriter.writeU8(static_cast<uint8_t>(ValueKind::Integer));
					ioWriter.writeU64(std::bit_cast<uint64_t>(iHeld));
				} else if constexpr (std::is_same_v<T, double>) {
					ioWriter.writeU8(static_cast<uint8_t>(ValueKind::Number));
					writeF64(ioWriter, iHeld);
				} else {
					ioWriter.writeU8(static_cast<uint8_t>(ValueKind::String));
					ioWriter.writeString(iHeld);
				}
			},
			iValue);
}

auto readValue(ByteReader& ioReader) -> std::optional<script::CallValue> {
	const auto kind = ioReader.readU8();
	if (!kind.has_value())
		return std::nullopt;
	switch (static_cast<ValueKind>(*kind)) {
		case ValueKind::Nil:
			return script::CallValue{};
		case ValueKind::Boolean:
			if (const auto value = ioReader.readU8(); value.has_value())
				return script::CallValue{*value != 0};
			return std::nullopt;
		case ValueKind::Integer:
			if (const auto value = ioReader.readU64(); value.has_value())
				return script::CallValue{std::bit_cast<int64_t>(*value)};
			return std::nullopt;
		case ValueKind::Number:
			if (const auto value = readF64(ioReader); value.has_value())
				return script::CallValue{*value};
			return std::nullopt;
		case ValueKind::String:
			if (auto value = ioReader.readString(); value.has_value())
				return script::CallValue{std::move(*value)};
			return std::nullopt;
	}
	return std::nullopt;
}

}// namespace owl::network::protocol
