/**
 * @file Factory.h
 * @author Silmaen
 * @date 19/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/IFactory.h"

namespace owl::core {

/**
 * @brief
 *  Implementation of a factory class.
 */
class OWL_API Factory : public IFactory {
public:
	Factory(const Factory&) = delete;
	Factory(Factory&&) = delete;
	auto operator=(const Factory&) -> Factory& = delete;
	auto operator=(Factory&&) -> Factory = delete;

	/**
	 * @brief
	 *  Default constructor
	 */
	Factory();

	/**
	 * @brief
	 *  Destructor
	 */
	~Factory() override;

	/**
	 * @brief
	 *  Register a new product into the factory
	 * @param[in] iType          Type index of the product
	 * @param[in] iDataKey      Key identifier of the product
	 * @param[in] iDataAllocator Allocator functions for the product
	 */
	void registerObject(std::type_index iType, const std::string& iDataKey,
						const ProductAllocator& iDataAllocator) override;
	/**
	 * @brief
	 *  Remove a product from the factory.
	 * @param[in] iType          Type index of the product
	 * @param[in] iDataKey      Key identifier of the product
	 */
	void unregisterObject(std::type_index iType, const std::string& iDataKey);

	/**
	 * @brief
	 *  Check if the product key is present in the factory
	 * @param[in] iDataKey Key to check
	 * @return Return true if the key is registered in the factory, false otherwise
	 */
	[[nodiscard]] auto isRegistered(const std::string& iDataKey) const -> bool override;

	/**
	 * @brief
	 *  Check if a PID is present in the factory.
	 * @param[in] iPid PID to check.
	 * @return Return true if the PID is registered in the factory, false otherwise.
	 */
	[[nodiscard]] auto isRegistered(FactoryPid iPid) const -> bool override;

	/**
	 * @brief
	 *  Get the key string from the PID
	 * @param[in]  iPid        Product identifier
	 * @param[out] oDataKey    Key identifier of the product
	 * @return Return true if there are no error , false if the Product was not found.
	 */
	auto getKey(FactoryPid iPid, std::string& oDataKey) const -> bool override;

	/**
	 * @brief
	 *  Create a new instance of a class contained into the factory
	 * @param[in] iDataKey Key identifier of the product
	 * @return The product created through the allocator
	 */
	[[nodiscard]] auto createProduct(const std::string& iDataKey) const -> FactoryProduct* override;

	/**
	 * @brief
	 *  Create n instances of a class contained into the factory
	 * @param[in]  iDataKey Key identifier of the product
	 * @param[in]  iNumber Number of elements to instantiate
	 * @param[out] oElements Products created by the function.
	 * @return  This function returns true if success or false if error
	 */
	auto createProducts(const std::string& iDataKey, uint32_t iNumber, std::vector<FactoryProduct*>& oElements) const
			-> bool override;

	/**
	 * @brief
	 *  From a type index of a product, find the corresponding PID. The product must be registered before.
	 * @param[in] iType The type index of a product.
	 * @return The pid corresponding to the product.
	 */
	auto getPid(std::type_index iType) -> FactoryPid override;

	/**
	 * @brief
	 *  From a data key of a product, find the corresponding PID. The product must be registered before.
	 * @param[in]  iDataKey Key identifier of the product
	 * @return The pid corresponding to the product.
	 */
	auto getPid(const std::string& iDataKey) -> FactoryPid override;

private:
	using FactoryMap = std::map<std::string, std::pair<FactoryPid, ProductAllocator>>;
	using FactoryPidMap = std::map<FactoryPid, std::string>;
	using FactoryTypeMap = std::map<std::type_index, FactoryPid>;

	/// Map containing the correspondence between key strings and allocators
	FactoryMap m_products;
	/// Map containing the correspondence between PID and key strings
	FactoryPidMap m_pids;
	/// Map containing the correspondence between Type and PID.
	FactoryTypeMap m_types;
	/// next available PID
	FactoryPid m_nextPid{0};
};

}// namespace owl::core
