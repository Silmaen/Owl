/**
 * @file Factory.cpp
 * @author Silmaen
 * @date 19/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/Factory.h"

namespace owl::core {

ProductAllocator::ProductAllocator(const SingleAllocator iSingleAlloc, const MultipleAllocator iMultipleAlloc)
	: singleAllocator(iSingleAlloc), multipleAllocator(iMultipleAlloc) {}

//----------------------------------------------------------------------------------------------------------------------
IFactory::IFactory() = default;

//----------------------------------------------------------------------------------------------------------------------
IFactory::~IFactory() = default;

//----------------------------------------------------------------------------------------------------------------------
auto IFactory::getInstance() -> IFactory& {
	static Factory singletonInstance;

	return singletonInstance;
}

//----------------------------------------------------------------------------------------------------------------------
Factory::Factory() = default;

//----------------------------------------------------------------------------------------------------------------------
Factory::~Factory() = default;

//----------------------------------------------------------------------------------------------------------------------
void Factory::registerObject(const std::type_index iType, const std::string& iDataKey,
							 const ProductAllocator& iDataAllocator) {

	// Check if the key is not present
	if (const auto itMap = m_products.find(iDataKey); itMap == m_products.end()) {
		// Get the next free PID
		const FactoryPid curPid = m_nextPid++;
		// Update maps
		m_products[iDataKey] = std::pair<FactoryPid, ProductAllocator>(curPid, iDataAllocator);
		m_pids[curPid] = iDataKey;
		m_types[iType] = curPid;
	}
}

//----------------------------------------------------------------------------------------------------------------------
void Factory::unregisterObject(const std::type_index iType, const std::string& iDataKey) {
	const auto itMap = m_products.find(iDataKey);
	// Check if the key is not present
	if (itMap == m_products.end())
		return;

	const FactoryPid pid = itMap->second.first;
	m_products.erase(itMap);
	m_pids.erase(pid);
	m_types.erase(iType);
}

//----------------------------------------------------------------------------------------------------------------------
auto Factory::isRegistered(const std::string& iDataKey) const -> bool { return m_products.contains(iDataKey); }

//----------------------------------------------------------------------------------------------------------------------
auto Factory::isRegistered(const FactoryPid iPid) const -> bool { return m_pids.contains(iPid); }

//----------------------------------------------------------------------------------------------------------------------
auto Factory::getKey(const FactoryPid iPid, std::string& oDataKey) const -> bool {

	// Check if the PID is present in the map
	if (const auto itMap = m_pids.find(iPid); itMap != m_pids.end()) {
		oDataKey = itMap->second;
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
auto Factory::createProduct(const std::string& iDataKey) const -> FactoryProduct* {
	// Check if the key is present in the map
	if (const auto itMap = m_products.find(iDataKey);
		itMap != m_products.end() && itMap->second.second.singleAllocator != nullptr)
		return itMap->second.second.singleAllocator();
	return nullptr;
}

//----------------------------------------------------------------------------------------------------------------------
auto Factory::createProducts(const std::string& iDataKey, const uint32_t iNumber,
							 std::vector<FactoryProduct*>& oElements) const -> bool {
	// Check if the key is present in the map
	if (const auto itMap = m_products.find(iDataKey);
		itMap != m_products.end() && itMap->second.second.multipleAllocator != nullptr)
		return itMap->second.second.multipleAllocator(iNumber, oElements);
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
auto Factory::getPid(const std::type_index iType) -> FactoryPid {

	if (const auto itMap = m_types.find(iType); itMap != m_types.end()) {
		return itMap->second;
	}
	return INVALID_FACTORY_PID;
}

//----------------------------------------------------------------------------------------------------------------------
auto Factory::getPid(const std::string& iDataKey) -> FactoryPid {

	if (const auto itMap = m_products.find(iDataKey); itMap != m_products.end()) {
		return itMap->second.first;
	}
	return INVALID_FACTORY_PID;
}

}// namespace owl::core
