/**
 * @file ExtraDataContainer.cpp
 * @author Silmaen
 * @date 20/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/extradata/ExtraDataContainer.h"

namespace owl::data::extradata {

ExtraDataContainer::~ExtraDataContainer() { m_extraDataList.clear(); }

ExtraDataContainer::ExtraDataContainer(const ExtraDataContainer& iOther) { *this = iOther.clone(); }
ExtraDataContainer::ExtraDataContainer(ExtraDataContainer&& iOther) noexcept
	: m_edPid(iOther.m_edPid), m_extraDataList(std::move(iOther.m_extraDataList)) {}

auto ExtraDataContainer::operator=(const ExtraDataContainer& iOther) -> ExtraDataContainer& {
	*this = iOther.clone();
	return *this;
}
auto ExtraDataContainer::operator=(ExtraDataContainer&& iOther) noexcept -> ExtraDataContainer& {
	m_edPid = iOther.m_edPid;
	m_extraDataList = std::move(iOther.m_extraDataList);
	return *this;
}

ExtraDataContainer::ExtraDataContainer(const core::FactoryPid iEdPid, const size_t iInitialSize) : m_edPid(iEdPid) {
	OWL_ASSERT(iEdPid != core::INVALID_FACTORY_PID, "ExtraDataContainer::ExtraDataContainer: Invalid ExtraData pid")
	m_extraDataList.resize(iInitialSize);
	init();
}

void ExtraDataContainer::init() {
	const core::IFactory& gFactory = core::IFactory::getInstance();
	if (std::string pKey; gFactory.getKey(m_edPid, pKey)) {
		for (auto& ed: m_extraDataList) {
			if (!ed) {
				ed = shared<ExtraDataBase>(dynamic_cast<ExtraDataBase*>(gFactory.createProduct(pKey)));
			}
		}
	} else {
		OWL_CORE_ERROR("ExtraDataContainer::init: Unknown ExtraData pid {}", m_edPid)
	}
}

auto ExtraDataContainer::getExtraData(size_t iIndex) const -> shared<ExtraDataBase> {
	if (iIndex >= m_extraDataList.size()) {
		OWL_CORE_WARN("ExtraDataContainer::getExtraData: Index {} out of range (size={})", iIndex,
					  m_extraDataList.size())
		return nullptr;
	}
	return m_extraDataList.at(iIndex);
}

auto ExtraDataContainer::clone() const -> ExtraDataContainer {
	ExtraDataContainer newEdc(m_edPid, 0);
	const core::IFactory& gFactory = core::IFactory::getInstance();
	std::string pKey;
	if (!gFactory.getKey(m_edPid, pKey))
		return newEdc;
	newEdc.m_extraDataList.reserve(m_extraDataList.size());
	for (const auto& ed: m_extraDataList) {
		if (auto* newEd = dynamic_cast<ExtraDataBase*>(gFactory.createProduct(pKey))) {
			*newEd = *ed;
			newEdc.m_extraDataList.push_back(shared<ExtraDataBase>(newEd));
		}
	}
	return newEdc;
}

void ExtraDataContainer::removeExtraData(size_t iIndex) {
	if (iIndex >= m_extraDataList.size()) {
		OWL_CORE_WARN("ExtraDataContainer::removeExtraData: Index {} out of range (size={})", iIndex,
					  m_extraDataList.size())
		return;
	}
	m_extraDataList.erase(m_extraDataList.begin() +
						  static_cast<std::vector<shared<ExtraDataBase>>::difference_type>(iIndex));
}

void ExtraDataContainer::resize(const size_t iNewSize) {
	if (iNewSize == m_extraDataList.size())
		return;
	const size_t oldSize = m_extraDataList.size();
	m_extraDataList.resize(iNewSize);
	if (iNewSize > oldSize) {
		// Initialize new elements
		init();
	}
}

}// namespace owl::data::extradata
