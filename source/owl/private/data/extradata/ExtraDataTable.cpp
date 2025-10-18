/**
 * @file ExtraDataTable.cpp
 * @author Silmaen
 * @date 22/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/extradata/ExtraDataTable.h"
namespace owl::data::extradata {

ExtraDataTable::ExtraDataTable() = default;
ExtraDataTable::~ExtraDataTable() = default;

void ExtraDataTable::clear() {
	m_extraDataList.clear();
	m_size = 0;
}

void ExtraDataTable::resize(const size_t iNewSize) {
	if (iNewSize == m_size)
		return;
	m_size = iNewSize;
	for (auto& container: m_extraDataList) { container.resize(iNewSize); }
}

auto ExtraDataTable::clone() const -> ExtraDataTable {
	ExtraDataTable newTable;
	newTable.m_size = m_size;
	for (const auto& container: m_extraDataList) { newTable.m_extraDataList.push_back(container); }
	return newTable;
}

void ExtraDataTable::removeElement(const size_t iIndex) {
	if (iIndex >= m_size)
		return;
	--m_size;
	for (auto& container: m_extraDataList) { container.removeExtraData(iIndex); }
}

auto ExtraDataTable::isExtraDataDefined(core::FactoryPid iPid) const -> bool {
	return std::ranges::any_of(m_extraDataList,
							   [iPid](const ExtraDataContainer& iEdc) -> bool { return iEdc.getEdPid() == iPid; });
}

auto ExtraDataTable::addExtraData(core::FactoryPid iPid) -> bool {
	if (isExtraDataDefined(iPid))
		return false;
	m_extraDataList.emplace_back(iPid, m_size);
	return true;
}

auto ExtraDataTable::deleteExtraData(core::FactoryPid iPid) -> bool {
	if (!isExtraDataDefined(iPid))
		return false;
	m_extraDataList.erase(
			std::ranges::remove_if(m_extraDataList,
								   [iPid](const ExtraDataContainer& iEdc) -> bool { return iEdc.getEdPid() == iPid; })
					.begin(),
			m_extraDataList.end());
	return true;
}


auto ExtraDataTable::getExtraData(const core::FactoryPid iPid) const -> const ExtraDataContainer* {
	for (const auto& container: m_extraDataList) {
		if (container.getEdPid() == iPid)
			return &container;
	}
	return nullptr;
}


}// namespace owl::data::extradata
