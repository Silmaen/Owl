/**
 * @file AuthenticodeSigner.cpp
 * @author Silmaen
 * @date 25/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "AuthenticodeSigner.h"

#include <core/Log.h>

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest::signing {

namespace {

using Bytes = std::vector<uint8_t>;

constexpr std::array<uint8_t, 9> g_oidSha256{0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01};
constexpr std::array<uint8_t, 9> g_oidRsa{0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01};
constexpr std::array<uint8_t, 9> g_oidSignedData{0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x07, 0x02};
constexpr std::array<uint8_t, 9> g_oidContentType{0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x09, 0x03};
constexpr std::array<uint8_t, 9> g_oidMessageDigest{0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x09, 0x04};
constexpr std::array<uint8_t, 10> g_oidIndirectData{0x2B, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x02, 0x01, 0x04};
constexpr std::array<uint8_t, 10> g_oidPeImageData{0x2B, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x02, 0x01, 0x0F};
constexpr std::array<uint8_t, 10> g_oidOpusInfo{0x2B, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x02, 0x01, 0x0C};

constexpr uint16_t g_certificateRevision = 0x0200;
constexpr uint16_t g_certificatePkcs = 0x0002;
constexpr size_t g_certificateDirectory = 4;
constexpr size_t g_alignment = 8;

auto getU16(const std::span<const uint8_t> iData, const size_t iOffset) -> uint32_t {
	return static_cast<uint32_t>(iData[iOffset]) | static_cast<uint32_t>(iData[iOffset + 1]) << 8u;
}

auto getU32(const std::span<const uint8_t> iData, const size_t iOffset) -> uint32_t {
	return getU16(iData, iOffset) | getU16(iData, iOffset + 2) << 16u;
}

void setU32(Bytes& ioData, const size_t iOffset, const uint32_t iValue) {
	for (uint32_t i = 0; i < 4; ++i) ioData[iOffset + i] = static_cast<uint8_t>(iValue >> (8 * i));
}

void append(Bytes& ioOut, const std::span<const uint8_t> iData) {
	ioOut.insert(ioOut.end(), iData.begin(), iData.end());
}

auto der(const uint8_t iTag, const std::span<const uint8_t> iContent) -> Bytes {
	Bytes out{iTag};
	if (iContent.size() < 0x80) {
		out.push_back(static_cast<uint8_t>(iContent.size()));
	} else {
		Bytes length;
		for (size_t value = iContent.size(); value > 0; value >>= 8u) length.push_back(static_cast<uint8_t>(value));
		out.push_back(static_cast<uint8_t>(0x80 | length.size()));
		out.insert(out.end(), length.rbegin(), length.rend());
	}
	append(out, iContent);
	return out;
}

auto concat(const std::initializer_list<Bytes> iParts) -> Bytes {
	Bytes out;
	for (const auto& part: iParts) append(out, part);
	return out;
}

auto oid(const std::span<const uint8_t> iEncoded) -> Bytes { return der(0x06, iEncoded); }

auto algorithm(const std::span<const uint8_t> iOid) -> Bytes {
	return der(0x30, concat({oid(iOid), Bytes{0x05, 0x00}}));
}

auto sha256Of(const std::span<const uint8_t> iData) -> Bytes {
	Bytes digest(32);
	unsigned int length = 0;
	EVP_Digest(iData.data(), iData.size(), digest.data(), &length, EVP_sha256(), nullptr);
	return digest;
}

// Where a PE keeps what the Authenticode digest skips.
struct PeLayout {
	size_t checksum = 0;
	size_t certificateEntry = 0;
	size_t headersSize = 0;
	uint32_t certificateOffset = 0;
	uint32_t certificateSize = 0;
	std::vector<std::pair<size_t, size_t>> sections;
};

auto parsePe(const std::span<const uint8_t> iImage) -> std::optional<PeLayout> {
	constexpr size_t dosHeader = 64;
	if (iImage.size() < dosHeader || iImage[0] != 'M' || iImage[1] != 'Z')
		return std::nullopt;
	const size_t coff = static_cast<size_t>(getU32(iImage, 0x3C)) + 4;
	if (coff + 20 > iImage.size() || getU32(iImage, coff - 4) != 0x00004550)
		return std::nullopt;
	const size_t sectionCount = getU16(iImage, coff + 2);
	const size_t optionalSize = getU16(iImage, coff + 16);
	const size_t optional = coff + 20;
	if (optional + optionalSize > iImage.size() || optionalSize < 96)
		return std::nullopt;
	const auto magic = getU16(iImage, optional);
	if (magic != 0x10b && magic != 0x20b)
		return std::nullopt;
	const size_t directories = optional + (magic == 0x20b ? 112 : 96);
	if (directories + (g_certificateDirectory + 1) * 8 > optional + optionalSize)
		return std::nullopt;
	PeLayout layout;
	layout.checksum = optional + 64;
	layout.headersSize = getU32(iImage, optional + 60);
	layout.certificateEntry = directories + g_certificateDirectory * 8;
	layout.certificateOffset = getU32(iImage, layout.certificateEntry);
	layout.certificateSize = getU32(iImage, layout.certificateEntry + 4);
	const size_t table = optional + optionalSize;
	if (table + sectionCount * 40 > iImage.size() || layout.headersSize > iImage.size())
		return std::nullopt;
	for (size_t i = 0; i < sectionCount; ++i) {
		const size_t entry = table + i * 40;
		const size_t size = getU32(iImage, entry + 16);
		const size_t pointer = getU32(iImage, entry + 20);
		if (size == 0)
			continue;
		if (pointer + size > iImage.size())
			return std::nullopt;
		layout.sections.emplace_back(pointer, size);
	}
	std::ranges::sort(layout.sections);
	return layout;
}

auto stripSignature(const std::span<const uint8_t> iImage, const PeLayout& iLayout) -> size_t {
	if (iLayout.certificateSize == 0)
		return iImage.size();
	return iLayout.certificateOffset;
}

auto digestOf(const std::span<const uint8_t> iImage, const PeLayout& iLayout) -> Bytes {
	const size_t end = stripSignature(iImage, iLayout);
	auto* context = EVP_MD_CTX_new();
	EVP_DigestInit_ex(context, EVP_sha256(), nullptr);
	const auto feed = [&](const size_t iFrom, const size_t iTo) -> void {
		if (iTo > iFrom)
			EVP_DigestUpdate(context, iImage.data() + iFrom, iTo - iFrom);
	};
	feed(0, iLayout.checksum);
	feed(iLayout.checksum + 4, iLayout.certificateEntry);
	feed(iLayout.certificateEntry + 8, iLayout.headersSize);
	size_t covered = iLayout.headersSize;
	for (const auto& [pointer, size]: iLayout.sections) {
		feed(pointer, std::min(pointer + size, end));
		covered = std::max(covered, pointer + size);
	}
	feed(covered, end);
	Bytes digest(32);
	unsigned int length = 0;
	EVP_DigestFinal_ex(context, digest.data(), &length);
	EVP_MD_CTX_free(context);
	return digest;
}

// The checksum Windows computes for an image, its checksum field read as zero.
auto checksumOf(const std::span<const uint8_t> iImage, const size_t iChecksumOffset) -> uint32_t {
	uint64_t sum = 0;
	for (size_t i = 0; i < iImage.size(); i += 2) {
		if (i == iChecksumOffset || i == iChecksumOffset + 2)
			continue;
		const uint64_t word = i + 1 < iImage.size() ? getU16(iImage, i) : iImage[i];
		sum += word;
		sum = (sum & 0xffffffffu) + (sum >> 32u);
	}
	sum = (sum & 0xffffu) + (sum >> 16u);
	sum += sum >> 16u;
	return static_cast<uint32_t>((sum & 0xffffu) + iImage.size());
}

// What the signature covers besides the image digest: the SpcIndirectDataContent, without its outer header.
auto indirectDataBody(const Bytes& iDigest) -> Bytes {
	constexpr std::string_view obsolete = "<<<Obsolete>>>";
	Bytes name;
	for (const char c: obsolete) {
		name.push_back(0);
		name.push_back(static_cast<uint8_t>(c));
	}
	const auto link = der(0xA0, der(0xA2, der(0x80, name)));
	const auto peData = der(0x30, concat({Bytes{0x03, 0x01, 0x00}, link}));
	const auto typeAndValue = der(0x30, concat({oid(g_oidPeImageData), peData}));
	const auto digestInfo = der(0x30, concat({algorithm(g_oidSha256), der(0x04, iDigest)}));
	return concat({typeAndValue, digestInfo});
}

auto derOfName(X509* iCertificate) -> Bytes {
	const int length = i2d_X509_NAME(X509_get_issuer_name(iCertificate), nullptr);
	Bytes name(static_cast<size_t>(std::max(length, 0)));
	auto* out = name.data();
	i2d_X509_NAME(X509_get_issuer_name(iCertificate), &out);
	return name;
}

auto derOfSerial(X509* iCertificate) -> Bytes {
	const int length = i2d_ASN1_INTEGER(X509_get_serialNumber(iCertificate), nullptr);
	Bytes serial(static_cast<size_t>(std::max(length, 0)));
	auto* out = serial.data();
	i2d_ASN1_INTEGER(X509_get_serialNumber(iCertificate), &out);
	return serial;
}

auto attribute(const std::span<const uint8_t> iOid, const Bytes& iValue) -> Bytes {
	return der(0x30, concat({oid(iOid), der(0x31, iValue)}));
}

// The PKCS #7 SignedData of an Authenticode signature, empty when OpenSSL fails.
auto makeSignedData(const Bytes& iDigest, const android::SigningKey& iKey) -> Bytes {
	const auto& certificate = iKey.getCertificate();
	const auto* in = certificate.data();
	auto* x509 = d2i_X509(nullptr, &in, static_cast<long>(certificate.size()));
	if (x509 == nullptr)
		return {};
	const auto issuerAndSerial = der(0x30, concat({derOfName(x509), derOfSerial(x509)}));
	X509_free(x509);

	const auto body = indirectDataBody(iDigest);
	std::vector<Bytes> attributes{attribute(g_oidContentType, oid(g_oidIndirectData)),
								  attribute(g_oidMessageDigest, der(0x04, sha256Of(body))),
								  attribute(g_oidOpusInfo, der(0x30, Bytes{}))};
	std::ranges::sort(attributes);
	Bytes attributeBytes;
	for (const auto& entry: attributes) append(attributeBytes, entry);
	const auto signature = iKey.sign(der(0x31, attributeBytes));
	if (signature.empty())
		return {};

	const auto signerInfo = der(0x30, concat({Bytes{0x02, 0x01, 0x01}, issuerAndSerial, algorithm(g_oidSha256),
											  der(0xA0, attributeBytes), algorithm(g_oidRsa), der(0x04, signature)}));
	const auto content = der(0x30, concat({oid(g_oidIndirectData), der(0xA0, der(0x30, body))}));
	const auto signedData = der(0x30, concat({Bytes{0x02, 0x01, 0x01}, der(0x31, algorithm(g_oidSha256)), content,
											  der(0xA0, certificate), der(0x31, signerInfo)}));
	return der(0x30, concat({oid(g_oidSignedData), der(0xA0, signedData)}));
}

auto readFile(const std::filesystem::path& iFile) -> Bytes {
	std::ifstream in(iFile, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}// namespace

auto getErrorMessage(const AuthenticodeError iError) -> std::string_view {
	switch (iError) {
		case AuthenticodeError::File:
			return "the executable cannot be read or written";
		case AuthenticodeError::NotExecutable:
			return "the file is not a Windows executable";
		case AuthenticodeError::Signature:
			return "OpenSSL could not sign the executable";
		case AuthenticodeError::Command:
			return "the signing command failed";
	}
	return "unknown signing error";
}

auto getAuthenticodeDigest(const std::span<const uint8_t> iImage) -> std::vector<uint8_t> {
	const auto layout = parsePe(iImage);
	if (!layout)
		return {};
	return digestOf(iImage, *layout);
}

auto signExecutable(const std::filesystem::path& iExecutable, const android::SigningKey& iKey)
		-> expected<void, AuthenticodeError> {
	auto image = readFile(iExecutable);
	const auto layout = parsePe(image);
	if (!layout) {
		OWL_CORE_WARN("Authenticode: '{}' is not a Windows executable.", iExecutable.string())
		return unexpected{AuthenticodeError::NotExecutable};
	}
	if (layout->certificateSize != 0 && layout->certificateOffset + layout->certificateSize != image.size()) {
		OWL_CORE_WARN("Authenticode: The signature of '{}' is not at the end of the file.", iExecutable.string())
		return unexpected{AuthenticodeError::NotExecutable};
	}
	image.resize(stripSignature(image, *layout));
	image.resize((image.size() + g_alignment - 1) / g_alignment * g_alignment);
	auto stripped = *layout;
	stripped.certificateSize = 0;
	const auto signedData = makeSignedData(digestOf(image, stripped), iKey);
	if (signedData.empty()) {
		OWL_CORE_WARN("Authenticode: OpenSSL could not sign '{}'.", iExecutable.string())
		return unexpected{AuthenticodeError::Signature};
	}
	const auto tableSize = (signedData.size() + 8 + g_alignment - 1) / g_alignment * g_alignment;
	const auto tableOffset = image.size();
	image.resize(tableOffset + tableSize);
	setU32(image, tableOffset, static_cast<uint32_t>(signedData.size() + 8));
	image[tableOffset + 4] = static_cast<uint8_t>(g_certificateRevision & 0xffu);
	image[tableOffset + 5] = static_cast<uint8_t>(g_certificateRevision >> 8u);
	image[tableOffset + 6] = static_cast<uint8_t>(g_certificatePkcs & 0xffu);
	image[tableOffset + 7] = static_cast<uint8_t>(g_certificatePkcs >> 8u);
	std::ranges::copy(signedData, image.begin() + static_cast<std::ptrdiff_t>(tableOffset + 8));
	setU32(image, layout->certificateEntry, static_cast<uint32_t>(tableOffset));
	setU32(image, layout->certificateEntry + 4, static_cast<uint32_t>(tableSize));
	setU32(image, layout->checksum, 0);
	setU32(image, layout->checksum, checksumOf(image, layout->checksum));

	std::ofstream out(iExecutable, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
	if (!out.good()) {
		OWL_CORE_WARN("Authenticode: Cannot write '{}'.", iExecutable.string())
		return unexpected{AuthenticodeError::File};
	}
	return {};
}

auto signExecutableWithCommand(const std::filesystem::path& iExecutable, const std::string& iCommand)
		-> expected<void, AuthenticodeError> {
	const auto file = iExecutable.string();
	if (file.contains('"') || iCommand.empty()) {
		OWL_CORE_WARN("Authenticode: No usable signing command for '{}'.", file)
		return unexpected{AuthenticodeError::Command};
	}
	const std::string quoted = "\"" + file + "\"";
	std::string command = iCommand;
	if (const auto place = command.find("{file}"); place != std::string::npos)
		command.replace(place, 6, quoted);
	else
		command += " " + quoted;
	// NOLINTNEXTLINE(bugprone-command-processor): the command is the developer's own, run as typed in the editor settings.
	if (const int status = std::system(command.c_str()); status != 0) {
		OWL_CORE_WARN("Authenticode: The signing command returned {}.", status)
		return unexpected{AuthenticodeError::Command};
	}
	return {};
}

}// namespace owl::nest::signing
