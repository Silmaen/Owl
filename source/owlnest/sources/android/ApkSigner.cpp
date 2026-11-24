/**
 * @file ApkSigner.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ApkSigner.h"

#include <core/Log.h>

#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace owl::nest::android {

namespace {

constexpr uint32_t g_rsaSha256 = 0x0103;
constexpr uint32_t g_v2BlockId = 0x7109871a;
constexpr uint32_t g_v3BlockId = 0xf05368c0;
// Attribute of the v2 signed data telling a verifier a v3 signature exists (stripping protection).
constexpr uint32_t g_strippingProtection = 0xbeeff00d;
constexpr uint32_t g_v3MinSdk = 28;
constexpr uint32_t g_maxSdk = 0x7fffffff;
constexpr size_t g_chunk = size_t{1} << 20u;
constexpr std::array<uint8_t, 16> g_blockMagic = {'A', 'P', 'K', ' ', 'S', 'i', 'g', ' ',
												  'B', 'l', 'o', 'c', 'k', ' ', '4', '2'};

using Bytes = std::vector<uint8_t>;

void putU32(Bytes& ioOut, const uint32_t iValue) {
	for (uint32_t i = 0; i < 4; ++i) ioOut.push_back(static_cast<uint8_t>(iValue >> (8 * i)));
}

void putU64(Bytes& ioOut, const uint64_t iValue) {
	for (uint32_t i = 0; i < 8; ++i) ioOut.push_back(static_cast<uint8_t>(iValue >> (8 * i)));
}

void putPrefixed(Bytes& ioOut, const std::span<const uint8_t> iData) {
	putU32(ioOut, static_cast<uint32_t>(iData.size()));
	ioOut.insert(ioOut.end(), iData.begin(), iData.end());
}

auto readU32(const std::span<const uint8_t> iData, const size_t iOffset) -> uint32_t {
	return static_cast<uint32_t>(iData[iOffset]) | static_cast<uint32_t>(iData[iOffset + 1]) << 8u |
		   static_cast<uint32_t>(iData[iOffset + 2]) << 16u | static_cast<uint32_t>(iData[iOffset + 3]) << 24u;
}

auto sha256(const std::span<const uint8_t> iPrefix, const std::span<const uint8_t> iData) -> Bytes {
	Bytes digest(32);
	auto* context = EVP_MD_CTX_new();
	EVP_DigestInit_ex(context, EVP_sha256(), nullptr);
	EVP_DigestUpdate(context, iPrefix.data(), iPrefix.size());
	EVP_DigestUpdate(context, iData.data(), iData.size());
	unsigned int length = 0;
	EVP_DigestFinal_ex(context, digest.data(), &length);
	EVP_MD_CTX_free(context);
	return digest;
}

// The v2 / v3 content digest: SHA-256 of the 1 MiB chunks of the entries, the central directory and the end record.
auto contentDigest(const std::array<std::span<const uint8_t>, 3>& iSections) -> Bytes {
	Bytes chunks;
	uint32_t count = 0;
	for (const auto& section: iSections) {
		for (size_t offset = 0; offset < section.size(); offset += g_chunk) {
			const auto chunk = section.subspan(offset, std::min(g_chunk, section.size() - offset));
			Bytes prefix{0xa5};
			putU32(prefix, static_cast<uint32_t>(chunk.size()));
			const auto digest = sha256(prefix, chunk);
			chunks.insert(chunks.end(), digest.begin(), digest.end());
			++count;
		}
	}
	Bytes prefix{0x5a};
	putU32(prefix, count);
	return sha256(prefix, chunks);
}

// One signer of a v2 (`iV3` false) or v3 signature block.
auto makeSigner(const Bytes& iDigest, const SigningKey& iKey, const bool iV3) -> Bytes {
	Bytes digests;
	{
		Bytes digest;
		putU32(digest, g_rsaSha256);
		putPrefixed(digest, iDigest);
		putPrefixed(digests, digest);
	}
	Bytes certificates;
	putPrefixed(certificates, iKey.getCertificate());
	Bytes attributes;
	if (!iV3) {
		Bytes attribute;
		putU32(attribute, g_strippingProtection);
		putU32(attribute, 3);
		putPrefixed(attributes, attribute);
	}
	Bytes signedData;
	putPrefixed(signedData, digests);
	putPrefixed(signedData, certificates);
	if (iV3) {
		putU32(signedData, g_v3MinSdk);
		putU32(signedData, g_maxSdk);
	}
	putPrefixed(signedData, attributes);
	const auto signature = iKey.sign(signedData);
	if (signature.empty())
		return {};
	Bytes signatures;
	{
		Bytes entry;
		putU32(entry, g_rsaSha256);
		putPrefixed(entry, signature);
		putPrefixed(signatures, entry);
	}
	Bytes signer;
	putPrefixed(signer, signedData);
	if (iV3) {
		putU32(signer, g_v3MinSdk);
		putU32(signer, g_maxSdk);
	}
	putPrefixed(signer, signatures);
	putPrefixed(signer, iKey.getPublicKey());
	Bytes signers;
	putPrefixed(signers, signer);
	Bytes value;
	putPrefixed(value, signers);
	return value;
}

auto derOf(X509* iCertificate) -> Bytes {
	const int length = i2d_X509(iCertificate, nullptr);
	Bytes der(static_cast<size_t>(std::max(length, 0)));
	auto* out = der.data();
	i2d_X509(iCertificate, &out);
	return der;
}

auto publicDerOf(EVP_PKEY* iKey) -> Bytes {
	const int length = i2d_PUBKEY(iKey, nullptr);
	Bytes der(static_cast<size_t>(std::max(length, 0)));
	auto* out = der.data();
	i2d_PUBKEY(iKey, &out);
	return der;
}

auto readFile(const std::filesystem::path& iFile) -> Bytes {
	std::ifstream in(iFile, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// A debug keystore as the Android tools make one: RSA 2048, self-signed for 30 years.
auto createDebugKeystore(const std::filesystem::path& iFile) -> bool {
	auto* key = EVP_RSA_gen(2048);
	auto* certificate = X509_new();
	if (key == nullptr || certificate == nullptr) {
		EVP_PKEY_free(key);
		X509_free(certificate);
		return false;
	}
	X509_set_version(certificate, 2);
	ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1);
	X509_gmtime_adj(X509_getm_notBefore(certificate), 0);
	X509_gmtime_adj(X509_getm_notAfter(certificate), 30L * 365 * 24 * 3600);
	X509_set_pubkey(certificate, key);
	auto* name = X509_NAME_new();
	X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("US"), -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("Android"), -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("Android Debug"), -1,
							   -1, 0);
	X509_set_subject_name(certificate, name);
	X509_set_issuer_name(certificate, name);
	X509_NAME_free(name);
	bool ok = X509_sign(certificate, key, EVP_sha256()) > 0;
	auto* store = ok ? PKCS12_create("android", "androiddebugkey", key, certificate, nullptr, 0, 0, 0, 0, 0) : nullptr;
	ok = store != nullptr;
	if (ok) {
		std::error_code ec;
		std::filesystem::create_directories(iFile.parent_path(), ec);
		const int length = i2d_PKCS12(store, nullptr);
		Bytes der(static_cast<size_t>(std::max(length, 0)));
		auto* out = der.data();
		i2d_PKCS12(store, &out);
		std::ofstream file(iFile, std::ios::binary);
		file.write(reinterpret_cast<const char*>(der.data()), static_cast<std::streamsize>(der.size()));
		ok = file.good();
	}
	PKCS12_free(store);
	X509_free(certificate);
	EVP_PKEY_free(key);
	return ok;
}

}// namespace

SigningKey::SigningKey(void* iKey, std::vector<uint8_t> iCertificate, std::vector<uint8_t> iPublicKey)
	: mp_key{iKey}, m_certificate{std::move(iCertificate)}, m_publicKey{std::move(iPublicKey)} {}

SigningKey::SigningKey(SigningKey&& ioOther) noexcept
	: mp_key{std::exchange(ioOther.mp_key, nullptr)}, m_certificate{std::move(ioOther.m_certificate)},
	  m_publicKey{std::move(ioOther.m_publicKey)} {}

auto SigningKey::operator=(SigningKey&& ioOther) noexcept -> SigningKey& {
	if (this != &ioOther) {
		EVP_PKEY_free(static_cast<EVP_PKEY*>(mp_key));
		mp_key = std::exchange(ioOther.mp_key, nullptr);
		m_certificate = std::move(ioOther.m_certificate);
		m_publicKey = std::move(ioOther.m_publicKey);
	}
	return *this;
}

SigningKey::~SigningKey() { EVP_PKEY_free(static_cast<EVP_PKEY*>(mp_key)); }

auto SigningKey::load(const std::filesystem::path& iFile, const std::string& iPassword)
		-> expected<SigningKey, SigningError> {
	const auto content = readFile(iFile);
	const auto* in = content.data();
	auto* store = content.empty() ? nullptr : d2i_PKCS12(nullptr, &in, static_cast<long>(content.size()));
	EVP_PKEY* key = nullptr;
	X509* certificate = nullptr;
	const bool ok = store != nullptr && PKCS12_parse(store, iPassword.c_str(), &key, &certificate, nullptr) == 1 &&
					key != nullptr && certificate != nullptr && EVP_PKEY_get_base_id(key) == EVP_PKEY_RSA;
	PKCS12_free(store);
	if (!ok) {
		EVP_PKEY_free(key);
		X509_free(certificate);
		OWL_CORE_WARN("ApkSigner: Cannot open the RSA key of the keystore '{}'.", iFile.string())
		return unexpected{SigningError::Keystore};
	}
	auto certificateDer = derOf(certificate);
	X509_free(certificate);
	auto publicDer = publicDerOf(key);
	return SigningKey{key, std::move(certificateDer), std::move(publicDer)};
}

auto SigningKey::loadOrCreateDebug(const std::filesystem::path& iFile) -> expected<SigningKey, SigningError> {
	if (!exists(iFile) && !createDebugKeystore(iFile)) {
		OWL_CORE_WARN("ApkSigner: Cannot create the debug keystore '{}'.", iFile.string())
		return unexpected{SigningError::Keystore};
	}
	return load(iFile, "android");
}

auto SigningKey::sign(const std::span<const uint8_t> iData) const -> std::vector<uint8_t> {
	auto* context = EVP_MD_CTX_new();
	size_t length = 0;
	std::vector<uint8_t> signature;
	if (EVP_DigestSignInit(context, nullptr, EVP_sha256(), nullptr, static_cast<EVP_PKEY*>(mp_key)) == 1 &&
		EVP_DigestSign(context, nullptr, &length, iData.data(), iData.size()) == 1) {
		signature.resize(length);
		if (EVP_DigestSign(context, signature.data(), &length, iData.data(), iData.size()) == 1)
			signature.resize(length);
		else
			signature.clear();
	}
	EVP_MD_CTX_free(context);
	return signature;
}

auto signApk(const std::span<const uint8_t> iZip, const SigningKey& iKey)
		-> expected<std::vector<uint8_t>, SigningError> {
	constexpr size_t endRecordSize = 22;
	if (iZip.size() < endRecordSize || readU32(iZip, iZip.size() - endRecordSize) != 0x06054b50) {
		OWL_CORE_WARN("ApkSigner: No end of central directory (an archive comment is not supported).")
		return unexpected{SigningError::Archive};
	}
	const size_t endRecord = iZip.size() - endRecordSize;
	const size_t centralDirectory = readU32(iZip, endRecord + 16);
	if (centralDirectory > endRecord) {
		OWL_CORE_WARN("ApkSigner: Central directory out of the archive.")
		return unexpected{SigningError::Archive};
	}
	const auto digest =
			contentDigest({iZip.subspan(0, centralDirectory),
						   iZip.subspan(centralDirectory, endRecord - centralDirectory), iZip.subspan(endRecord)});
	const auto v2 = makeSigner(digest, iKey, false);
	const auto v3 = makeSigner(digest, iKey, true);
	if (v2.empty() || v3.empty()) {
		OWL_CORE_WARN("ApkSigner: OpenSSL could not sign the APK.")
		return unexpected{SigningError::Signature};
	}
	Bytes pairs;
	for (const auto& [id, value]: {std::pair{g_v2BlockId, &v2}, std::pair{g_v3BlockId, &v3}}) {
		putU64(pairs, value->size() + 4);
		putU32(pairs, id);
		pairs.insert(pairs.end(), value->begin(), value->end());
	}
	const uint64_t blockSize = pairs.size() + 8 + g_blockMagic.size();
	Bytes block;
	putU64(block, blockSize);
	block.insert(block.end(), pairs.begin(), pairs.end());
	putU64(block, blockSize);
	block.insert(block.end(), g_blockMagic.begin(), g_blockMagic.end());

	std::vector<uint8_t> apk(iZip.begin(), iZip.begin() + static_cast<std::ptrdiff_t>(centralDirectory));
	apk.insert(apk.end(), block.begin(), block.end());
	apk.insert(apk.end(), iZip.begin() + static_cast<std::ptrdiff_t>(centralDirectory), iZip.end());
	const auto movedDirectory = static_cast<uint32_t>(centralDirectory + block.size());
	const size_t patchedEnd = apk.size() - endRecordSize + 16;
	for (uint32_t i = 0; i < 4; ++i) apk[patchedEnd + i] = static_cast<uint8_t>(movedDirectory >> (8 * i));
	return apk;
}

}// namespace owl::nest::android
