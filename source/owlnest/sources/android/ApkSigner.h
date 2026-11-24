/**
 * @file ApkSigner.h
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <core/expected.h>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace owl::nest::android {

/**
 * @brief
 *  Why an APK could not be signed.
 */
enum struct SigningError : uint8_t {
	/// The keystore could not be read, created or opened with its password.
	Keystore,
	/// The archive is not a zip (no end of central directory).
	Archive,
	/// OpenSSL failed to sign.
	Signature,
};

/**
 * @brief
 *  An RSA key and its certificate, read from a PKCS #12 keystore (`.p12`, `.pfx`).
 */
class SigningKey final {
public:
	SigningKey(const SigningKey&) = delete;

	SigningKey(SigningKey&& ioOther) noexcept;

	auto operator=(const SigningKey&) -> SigningKey& = delete;

	auto operator=(SigningKey&& ioOther) noexcept -> SigningKey&;

	/**
	 * @brief
	 *  Destructor, freeing the key.
	 */
	~SigningKey();

	/**
	 * @brief
	 *  Open a PKCS #12 keystore.
	 * @param[in] iFile The keystore.
	 * @param[in] iPassword Its password.
	 * @return The key, the error otherwise.
	 */
	[[nodiscard]] static auto load(const std::filesystem::path& iFile, const std::string& iPassword)
			-> expected<SigningKey, SigningError>;

	/**
	 * @brief
	 *  Open the debug keystore of Owl Nest, made at the first use (RSA 2048, `CN=Android Debug`, password `android`).
	 * @param[in] iFile The keystore, in the user folder of Owl Nest, never in a project.
	 * @return The key, the error otherwise.
	 */
	[[nodiscard]] static auto loadOrCreateDebug(const std::filesystem::path& iFile)
			-> expected<SigningKey, SigningError>;

	/**
	 * @brief
	 *  Sign data with RSASSA-PKCS1-v1_5 over SHA-256.
	 * @param[in] iData The data.
	 * @return The signature, empty on failure.
	 */
	[[nodiscard]] auto sign(std::span<const uint8_t> iData) const -> std::vector<uint8_t>;

	/**
	 * @brief
	 *  Get the certificate, DER encoded.
	 * @return The certificate.
	 */
	[[nodiscard]] auto getCertificate() const -> const std::vector<uint8_t>& { return m_certificate; }

	/**
	 * @brief
	 *  Get the public key, DER encoded `SubjectPublicKeyInfo`.
	 * @return The public key.
	 */
	[[nodiscard]] auto getPublicKey() const -> const std::vector<uint8_t>& { return m_publicKey; }

private:
	/**
	 * @brief
	 *  Constructor from an OpenSSL key and the DER encodings.
	 * @param[in] iKey The private key (owned).
	 * @param[in] iCertificate The certificate.
	 * @param[in] iPublicKey The public key.
	 */
	SigningKey(void* iKey, std::vector<uint8_t> iCertificate, std::vector<uint8_t> iPublicKey);

	/// The OpenSSL private key (`EVP_PKEY`), owned.
	void* mp_key = nullptr;
	/// Certificate, DER.
	std::vector<uint8_t> m_certificate;
	/// Public key, DER.
	std::vector<uint8_t> m_publicKey;
};

/**
 * @brief
 *  Sign a zip as an APK with the signature schemes v2 and v3 (Android 7 and later), the APK Signing Block inserted
 *  before the central directory.
 * @param[in] iZip The aligned, unsigned archive.
 * @param[in] iKey The key.
 * @return The signed APK, the error otherwise.
 */
[[nodiscard]] auto signApk(std::span<const uint8_t> iZip, const SigningKey& iKey)
		-> expected<std::vector<uint8_t>, SigningError>;

}// namespace owl::nest::android
