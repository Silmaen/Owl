/**
 * @file AuthenticodeSigner_test.cpp
 * @author Silmaen
 * @date 25/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "GamePackager.h"
#include "signing/AuthenticodeSigner.h"

#include <openssl/bio.h>

#include <openssl/pkcs7.h>
#include <openssl/x509.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::signing;

namespace {

constexpr size_t g_peOffset = 0x40;
constexpr size_t g_optional = g_peOffset + 4 + 20;
constexpr size_t g_headers = 0x200;
constexpr size_t g_sectionData = 0x200;

void put16(std::vector<uint8_t>& ioData, const size_t iOffset, const uint32_t iValue) {
	ioData[iOffset] = static_cast<uint8_t>(iValue);
	ioData[iOffset + 1] = static_cast<uint8_t>(iValue >> 8u);
}

void put32(std::vector<uint8_t>& ioData, const size_t iOffset, const uint32_t iValue) {
	put16(ioData, iOffset, iValue & 0xffffu);
	put16(ioData, iOffset + 2, iValue >> 16u);
}

auto get32(const std::vector<uint8_t>& iData, const size_t iOffset) -> uint32_t {
	return static_cast<uint32_t>(iData[iOffset]) | static_cast<uint32_t>(iData[iOffset + 1]) << 8u |
		   static_cast<uint32_t>(iData[iOffset + 2]) << 16u | static_cast<uint32_t>(iData[iOffset + 3]) << 24u;
}

// A PE32+ with 16 data directories and one 0x300 bytes section.
auto makePe() -> std::vector<uint8_t> {
	std::vector<uint8_t> pe(g_headers + 0x300 + 13, 0);
	pe[0] = 'M';
	pe[1] = 'Z';
	put32(pe, 0x3C, g_peOffset);
	put32(pe, g_peOffset, 0x00004550);
	put16(pe, g_peOffset + 4 + 2, 1);
	put16(pe, g_peOffset + 4 + 16, 240);
	put16(pe, g_optional, 0x20b);
	put32(pe, g_optional + 60, g_headers);
	put32(pe, g_optional + 108, 16);
	const size_t section = g_optional + 240;
	put32(pe, section + 16, 0x300);
	put32(pe, section + 20, g_sectionData);
	for (size_t i = g_sectionData; i < pe.size(); ++i) pe[i] = static_cast<uint8_t>(i * 7);
	return pe;
}

auto readFile(const std::filesystem::path& iFile) -> std::vector<uint8_t> {
	std::ifstream in(iFile, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void writeFile(const std::filesystem::path& iFile, const std::vector<uint8_t>& iContent) {
	std::filesystem::create_directories(iFile.parent_path());
	std::ofstream out(iFile, std::ios::binary);
	out.write(reinterpret_cast<const char*>(iContent.data()), static_cast<std::streamsize>(iContent.size()));
}

// Body of the first DER element at `iPosition`: where it starts and its length.
auto derBody(const std::vector<uint8_t>& iData, const size_t iPosition) -> std::pair<size_t, size_t> {
	size_t cursor = iPosition + 1;
	size_t length = iData[cursor++];
	if (length >= 0x80) {
		const size_t count = length & 0x7fu;
		length = 0;
		for (size_t i = 0; i < count; ++i) length = length << 8u | iData[cursor++];
	}
	return {cursor, length};
}

// The SpcIndirectDataContent body the signature covers: after the first indirect-data OID, in an `A0` then a sequence.
auto indirectDataBody(const std::vector<uint8_t>& iBlob) -> std::vector<uint8_t> {
	constexpr std::array<uint8_t, 12> oid{0x06, 0x0A, 0x2B, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x02, 0x01, 0x04};
	const auto found = std::ranges::search(iBlob, oid);
	const auto explicitTag = static_cast<size_t>(found.end() - iBlob.begin());
	const auto [sequence, sequenceLength] = derBody(iBlob, explicitTag);
	const auto [body, bodyLength] = derBody(iBlob, sequence);
	static_cast<void>(sequenceLength);
	return {iBlob.begin() + static_cast<std::ptrdiff_t>(body),
			iBlob.begin() + static_cast<std::ptrdiff_t>(body + bodyLength)};
}

class AuthenticodeFixture : public ::testing::Test {
protected:
	void SetUp() override {
		m_root = std::filesystem::temp_directory_path() / "owl_authenticode";
		std::filesystem::remove_all(m_root);
		writeFile(m_root / "game.exe", makePe());
	}

	void TearDown() override { std::filesystem::remove_all(m_root); }

	std::filesystem::path m_root;
};

}// namespace

TEST_F(AuthenticodeFixture, TheDigestSkipsTheChecksumAndTheCertificateEntry) {
	auto pe = makePe();
	const auto digest = getAuthenticodeDigest(pe);
	ASSERT_EQ(digest.size(), 32u);
	put32(pe, g_optional + 64, 0x12345678);
	put32(pe, g_optional + 112 + 4 * 8, 0);
	EXPECT_EQ(getAuthenticodeDigest(pe), digest);
	pe[g_sectionData + 5] ^= 1;
	EXPECT_NE(getAuthenticodeDigest(pe), digest);
}

TEST_F(AuthenticodeFixture, ANonExecutableHasNoDigest) {
	EXPECT_TRUE(getAuthenticodeDigest(std::vector<uint8_t>(300, 1)).empty());
}

TEST_F(AuthenticodeFixture, TheSignatureIsAppendedAndVerifies) {
	const auto key = android::SigningKey::loadOrCreateDebug(m_root / "key.p12");
	ASSERT_TRUE(key.has_value());
	ASSERT_TRUE(signExecutable(m_root / "game.exe", *key).has_value());

	const auto signedPe = readFile(m_root / "game.exe");
	const auto offset = get32(signedPe, g_optional + 112 + 4 * 8);
	const auto size = get32(signedPe, g_optional + 112 + 4 * 8 + 4);
	ASSERT_GT(size, 0u);
	EXPECT_EQ(offset % 8, 0u);
	EXPECT_EQ(offset + size, signedPe.size());
	EXPECT_EQ(signedPe[offset + 5], 0x02);
	EXPECT_EQ(signedPe[offset + 6], 0x02);

	const auto length = get32(signedPe, offset) - 8;
	const auto* in = signedPe.data() + offset + 8;
	auto* p7 = d2i_PKCS7(nullptr, &in, static_cast<long>(length));
	ASSERT_NE(p7, nullptr);
	EXPECT_TRUE(PKCS7_type_is_signed(p7));
	auto* signers = PKCS7_get_signer_info(p7);
	ASSERT_NE(signers, nullptr);
	ASSERT_EQ(OPENSSL_sk_num(reinterpret_cast<const OPENSSL_STACK*>(signers)), 1);
	auto* signer =
			static_cast<PKCS7_SIGNER_INFO*>(OPENSSL_sk_value(reinterpret_cast<const OPENSSL_STACK*>(signers), 0));
	const auto* certificateDer = key->getCertificate().data();
	auto* certificate = d2i_X509(nullptr, &certificateDer, static_cast<long>(key->getCertificate().size()));
	ASSERT_NE(certificate, nullptr);

	// The signed content: the image digest inside the SpcIndirectDataContent, which the signature covers.
	const auto digest = getAuthenticodeDigest(signedPe);
	const std::vector<uint8_t> blob(signedPe.begin() + offset + 8, signedPe.begin() + offset + 8 + length);
	EXPECT_FALSE(std::ranges::search(blob, digest).empty());
	const auto body = indirectDataBody(blob);
	auto* source = BIO_new_mem_buf(body.data(), static_cast<int>(body.size()));
	auto* chain = PKCS7_dataInit(p7, source);
	ASSERT_NE(chain, nullptr);
	std::array<char, 512> sink{};
	while (BIO_read(chain, sink.data(), static_cast<int>(sink.size())) > 0) {}
	EXPECT_EQ(PKCS7_signatureVerify(chain, p7, signer, certificate), 1);
	BIO_free_all(chain);
	PKCS7_free(p7);
	X509_free(certificate);
}

TEST_F(AuthenticodeFixture, TheChecksumIsTheOneOfTheWholeFile) {
	const auto key = android::SigningKey::loadOrCreateDebug(m_root / "key.p12");
	ASSERT_TRUE(key.has_value());
	ASSERT_TRUE(signExecutable(m_root / "game.exe", *key).has_value());
	auto signedPe = readFile(m_root / "game.exe");
	const auto stored = get32(signedPe, g_optional + 64);
	put32(signedPe, g_optional + 64, 0);
	uint64_t sum = 0;
	for (size_t i = 0; i < signedPe.size(); i += 2) {
		sum += static_cast<uint64_t>(signedPe[i]) | (i + 1 < signedPe.size() ? uint64_t{signedPe[i + 1]} << 8u : 0u);
		sum = (sum & 0xffffffffu) + (sum >> 32u);
	}
	sum = (sum & 0xffffu) + (sum >> 16u);
	sum += sum >> 16u;
	EXPECT_EQ(stored, static_cast<uint32_t>((sum & 0xffffu) + signedPe.size()));
}

TEST_F(AuthenticodeFixture, SigningAgainReplacesTheSignature) {
	const auto key = android::SigningKey::loadOrCreateDebug(m_root / "key.p12");
	ASSERT_TRUE(key.has_value());
	ASSERT_TRUE(signExecutable(m_root / "game.exe", *key).has_value());
	const auto first = readFile(m_root / "game.exe");
	ASSERT_TRUE(signExecutable(m_root / "game.exe", *key).has_value());
	const auto second = readFile(m_root / "game.exe");
	EXPECT_EQ(first.size(), second.size());
	EXPECT_EQ(getAuthenticodeDigest(first), getAuthenticodeDigest(second));
}

TEST_F(AuthenticodeFixture, ANonExecutableIsRefused) {
	const auto key = android::SigningKey::loadOrCreateDebug(m_root / "key.p12");
	ASSERT_TRUE(key.has_value());
	writeFile(m_root / "text.exe", std::vector<uint8_t>(300, 'a'));
	const auto result = signExecutable(m_root / "text.exe", *key);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), AuthenticodeError::NotExecutable);
}

TEST_F(AuthenticodeFixture, ACustomCommandSucceedsOrFails) {
	EXPECT_TRUE(signExecutableWithCommand(m_root / "game.exe", "exit 0").has_value());
	const auto failed = signExecutableWithCommand(m_root / "game.exe", "exit 3");
	ASSERT_FALSE(failed.has_value());
	EXPECT_EQ(failed.error(), AuthenticodeError::Command);
	EXPECT_FALSE(signExecutableWithCommand(m_root / "game.exe", "").has_value());
}

TEST_F(AuthenticodeFixture, TheWindowsExportIsSignedByTheCertificateTheCommandOrNot) {
	const auto exe = m_root / "game.exe";
	const auto before = readFile(exe);
	const auto none = GamePackager::signWindowsGame({}, exe);
	ASSERT_TRUE(none.has_value());
	EXPECT_TRUE(none->empty());
	EXPECT_EQ(readFile(exe), before);

	ExportSigning signing;
	signing.windowsCommand = "exit 0";
	const auto byCommand = GamePackager::signWindowsGame(signing, exe);
	ASSERT_TRUE(byCommand.has_value());
	EXPECT_FALSE(byCommand->empty());
	signing.windowsCommand = "exit 2";
	EXPECT_FALSE(GamePackager::signWindowsGame(signing, exe).has_value());

	ASSERT_TRUE(android::SigningKey::loadOrCreateDebug(m_root / "studio.pfx").has_value());
	signing.windowsCommand.clear();
	signing.windowsPfx = (m_root / "studio.pfx").string();
	signing.windowsPassword = "nope";
	ASSERT_FALSE(GamePackager::signWindowsGame(signing, exe).has_value());
	EXPECT_EQ(readFile(exe), before);
	signing.windowsPassword = "android";
	ASSERT_TRUE(GamePackager::signWindowsGame(signing, exe).has_value());
	EXPECT_NE(readFile(exe), before);
}
