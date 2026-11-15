/**
 * @file SessionProtocol.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "SessionProtocol.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace owl::network::protocol {

namespace {

// Copy the written bytes out of a writer.
auto toVector(const ByteWriter& iWriter) -> std::vector<uint8_t> {
	const auto bytes = iWriter.getBytes();
	return {bytes.begin(), bytes.end()};
}

}// namespace

auto encodeHello(const Hello& iHello) -> std::vector<uint8_t> {
	ByteWriter writer;
	writer.writeU8(static_cast<uint8_t>(MessageKind::Hello));
	writer.writeU32(g_Magic);
	writer.writeU16(iHello.protocol);
	writer.writeString(iHello.gameId);
	writer.writeU32(iHello.gameVersion);
	writer.writeU64(iHello.token);
	return toVector(writer);
}

auto decodeHello(const std::span<const uint8_t> iBytes) -> std::optional<Hello> {
	ByteReader reader{iBytes};
	if (reader.readU8() != static_cast<uint8_t>(MessageKind::Hello) || reader.readU32() != g_Magic)
		return std::nullopt;
	Hello hello;
	hello.protocol = reader.readU16().value_or(0);
	// A greeting of another protocol version may lay out the rest differently: stop at the version.
	if (hello.protocol != g_SessionProtocolVersion)
		return reader.hasFailed() ? std::nullopt : std::optional{hello};
	hello.gameId = reader.readString().value_or("");
	hello.gameVersion = reader.readU32().value_or(0);
	hello.token = reader.readU64().value_or(0);
	if (reader.hasFailed())
		return std::nullopt;
	return hello;
}

auto encodeWelcome(const ClientId iClient, const uint64_t iToken) -> std::vector<uint8_t> {
	ByteWriter writer;
	writer.writeU8(static_cast<uint8_t>(MessageKind::Welcome));
	writer.writeU32(iClient);
	writer.writeU64(iToken);
	return toVector(writer);
}

auto encodeReason(const MessageKind iKind, const uint8_t iReason) -> std::vector<uint8_t> {
	return {static_cast<uint8_t>(iKind), iReason};
}

void encodeData(const std::span<const uint8_t> iPayload, std::vector<uint8_t>& oFrame) {
	oFrame.clear();
	oFrame.reserve(iPayload.size() + 1);
	oFrame.push_back(static_cast<uint8_t>(MessageKind::Data));
	oFrame.insert(oFrame.end(), iPayload.begin(), iPayload.end());
}

}// namespace owl::network::protocol
