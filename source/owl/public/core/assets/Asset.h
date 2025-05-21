/**
 * @file Asset.h
 * @author Silmaen
 * @date 1/9/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::core::assets {

/**
 * @brief Concept to ensure DataType meets specific requirements.
 *
 * This concept checks that the DataType has:
 * - A static function `create` that takes a `std::filesystem::path`
 *   and returns a `shared<DataType>`.
 * - An internal class `Specification`.
 * - A static function `create` that takes a `DataType::Specification`
 *   and returns a `shared<DataType>`.
 * - A static function that gives the possible file extensions.
 */
template<typename DataType>
concept assetDataType = requires {
	// Check for existence of static function create with std::filesystem::path.
	{ DataType::create(std::declval<const std::filesystem::path&>()) } -> std::same_as<shared<DataType>>;

	// Check for existence of internal class Specification.
	typename DataType::Specification;

	// Check for existence of static function create with DataType::Specification.
	{ DataType::create(std::declval<const typename DataType::Specification&>()) } -> std::same_as<shared<DataType>>;

	// check existence of a static function that gives the possible file extensions.
	{ DataType::extension() } -> std::same_as<std::vector<std::string>>;
};

/**
 * @brief Class for managing the assets.
 * @tparam DataType the underlying data structure.
 */
template<assetDataType DataType>
class Asset final {
public:
	/**
	 * @brief Default constructor.
	 */
	Asset() = delete;
	/**
	 * @brief Default destructor.
	 */
	~Asset() = default;
	/**
	 * @brief Default copy constructor.
	 */
	Asset(const Asset&) = default;
	/**
	 * @brief Default move constructor.
	 */
	Asset(Asset&&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const Asset&) -> Asset& = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(Asset&&) -> Asset& = default;
	/**
	 * @brief Default constructor.
	 * @param[in] iData The internal data.
	 */
	explicit Asset(const shared<DataType>& iData) : m_asset{iData} {}

	/**
	 * @brief Access to the data.
	 * @return The data;
	 */
	[[nodiscard]] auto get() const -> const shared<DataType>& { return m_asset; }

	/**
	 * @brief Get the list of supported file extensions for this asset type.
	 * @return List of extensions.
	 */
	static auto extensions() -> std::vector<std::string> { return DataType::extension(); }

private:
	/// the real asset data;
	shared<DataType> m_asset;
};

}// namespace owl::core::assets
