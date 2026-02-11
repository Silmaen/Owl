/**
 * @file IFactory.h
 * @author Silmaen
 * @date 18/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include <string>
#include <typeindex>

namespace owl::core {

using FactoryPid = int32_t;
constexpr FactoryPid INVALID_FACTORY_PID = -1;

struct ProductAllocator;
class FactoryProduct;

/**
 * @brief
 *  Interface for a factory class.
 */
class OWL_API IFactory {
protected:
	/**
	 * @brief
	 *  Default constructor
	 */
	IFactory();

public:
	/**
	 * @brief
	 *  Destructor
	 */
	virtual ~IFactory();
	IFactory(const IFactory&) = delete;
	IFactory(IFactory&&) = delete;
	auto operator=(const IFactory&) -> IFactory& = delete;
	auto operator=(IFactory&&) -> IFactory& = delete;
	/**
	 * @brief
	 *  Allocate the unique instance of the class if needed and return it
	 */
	static auto getInstance() -> IFactory&;

	/**
	 * @brief
	 *  Register a new product into the factory
	 * @param[in] iType          Type index of the product
	 * @param[in] iDataKey      Key identifier of the product
	 * @param[in] iDataAllocator Allocator functions for the product
	 */
	virtual void registerObject(std::type_index iType, const std::string& iDataKey,
								const ProductAllocator& iDataAllocator) = 0;

	/**
	 * @brief
	 *  Check if the product key is present in the factory
	 * @param[in] iDataKey Key to check
	 * @return Return true if the key is registered in the factory, false otherwise
	 */
	[[nodiscard]] virtual auto isRegistered(const std::string& iDataKey) const -> bool = 0;

	/**
	 * @brief
	 *  Check if a PID is present in the factory.
	 * @param[in] iPid PID to check.
	 * @return Return true if the PID is registered in the factory, false otherwise.
	 */
	[[nodiscard]] virtual auto isRegistered(FactoryPid iPid) const -> bool = 0;

	/**
	 * @brief
	 *  Get the key string from the PID
	 * @param[in]  iPid        Product identifier
	 * @param[out] oDataKey    Key identifier of the product
	 * @return Return true if there are no error , false if the Product was not found.
	 */
	virtual auto getKey(FactoryPid iPid, std::string& oDataKey) const -> bool = 0;

	/**
	 * @brief
	 *  Create a new instance of a class contained into the factory
	 * @param[in] iDataKey Key identifier of the product
	 * @return The product created through the allocator
	 */
	[[nodiscard]] virtual auto createProduct(const std::string& iDataKey) const -> FactoryProduct* = 0;

	/**
	 * @brief
	 *  Create n instances of a class contained into the factory
	 * @param[in]  iDataKey Key identifier of the product
	 * @param[in]  iNumber Number of elements to instantiate
	 * @param[out] oElements Products created by the function.
	 * @return  This function returns true if success or false if error
	 */
	virtual auto createProducts(const std::string& iDataKey, unsigned int iNumber,
								std::vector<FactoryProduct*>& oElements) const -> bool = 0;

	/**
	 * @brief
	 *  From a type index of a product, find the corresponding PID. The product must be registered before.
	 * @param iType The type index of a product.
	 * @return The pid corresponding to the product.
	 */
	virtual auto getPid(std::type_index iType) -> FactoryPid = 0;
	/**
	 * @brief
	 *  From a data key of a product, find the corresponding PID. The product must be registered before.
	 * @param[in]  iDataKey Key identifier of the product
	 * @return The pid corresponding to the product.
	 */
	virtual auto getPid(const std::string& iDataKey) -> FactoryPid = 0;
};

/**
 * @brief
 *  Base class for all products created by the factory.
 */
OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class OWL_API FactoryProduct {
public:
	/**
	 * @brief
	 *  Default constructor
	 */
	FactoryProduct() = default;
	/**
	 * @brief
	 * Default Destructor
	 */
	virtual ~FactoryProduct() = default;
	/**
	 * @brief
	 * Default copy constructor
	 */
	FactoryProduct(const FactoryProduct& iOther) = default;
	/**
	 * @brief
	 * Default move constructor
	 */
	FactoryProduct(FactoryProduct&& iOther) = default;
	/**
	 * @brief
	 * Default copy operator
	 */
	auto operator=(const FactoryProduct& iOther) -> FactoryProduct& = default;
	/**
	 * @brief
	 * Default move operator
	 */
	auto operator=(FactoryProduct&& iOther) -> FactoryProduct& = default;
	/**
	 * @brief
	 *  Return the product identifier
	 * @note
	 *  This id is unique
	 */
	[[nodiscard]] virtual auto getPid() const -> FactoryPid = 0;
};
OWL_DIAG_POP

/**
 * @brief
 *  Gets the PID of the given factory product.
 * @tparam FactoryProductType Type of the extra data to get the ID for, a derived class of IFactoryProduct.
 * @return Given factory product's PID.
 */
template<typename FactoryProductType>
auto getFactoryPid() -> FactoryPid {
	static_assert(std::is_base_of_v<FactoryProduct, FactoryProductType>,
				  "Error: Fetched extra data must be derived from IFactoryProduct.");
	static_assert(!std::is_same_v<FactoryProduct, FactoryProductType>,
				  "Error: Fetched extra data must not be of specific type 'IFactoryProduct'.");

	const auto& tid = typeid(FactoryProductType);
	const FactoryPid id = IFactory::getInstance().getPid(tid);
	return id;
}

/**
 * @brief
 *  Check if a PID is present in the factory.
 * @param[in] iPid PID to check.
 * @return Return true if the PID is registered in the factory, false otherwise.
 */
[[maybe_unused]] static auto hasFactoryPid(const FactoryPid iPid) -> bool {
	return IFactory::getInstance().isRegistered(iPid);
}

/**
 * @brief
 *  Structure containing function pointers to allocate products.
 */
struct OWL_API ProductAllocator {
	using SingleAllocator = FactoryProduct* (*) ();
	using MultipleAllocator = bool (*)(unsigned int, std::vector<FactoryProduct*>&);
	/// Function pointer of single allocator
	SingleAllocator singleAllocator;
	/// Function pointer of multiple allocator
	MultipleAllocator multipleAllocator;
	/**
	 * @brief
	 *  Constructor
	 * @param iSingleAlloc Function to call for a single allocation
	 * @param iMultipleAlloc Function to call for multiple allocations
	 */
	explicit ProductAllocator(SingleAllocator iSingleAlloc = nullptr, MultipleAllocator iMultipleAlloc = nullptr);
};

template<typename T>
auto factorySingleProductAllocator() -> FactoryProduct* {
	return new T();
}

template<typename T>
auto factoryMultipleProductAllocator(const unsigned int iNumber, std::vector<FactoryProduct*>& iTable) -> bool {
	iTable.resize(iNumber);
	for (unsigned int ii = 0; ii < iNumber; ++ii) iTable[ii] = new T;
	return iTable.size() == iNumber;
}

template<typename T>
auto factoryRegisterType() -> FactoryPid {
	const std::string type = T::getStaticType();
	const ProductAllocator pAlloc(factorySingleProductAllocator<T>, factoryMultipleProductAllocator<T>);
	IFactory::getInstance().registerObject(typeid(T), type, pAlloc);
	const FactoryPid curPid = getFactoryPid<T>();
	return curPid;
}

}// namespace owl::core
