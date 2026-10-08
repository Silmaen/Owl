/**
 * @file ExtraDataRegistry.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/extradata/ExtraDataRegistry.h"

#include "data/extradata/ExtraDataBase.h"

#include <typeindex>
#include <unordered_map>
#include <vector>

namespace owl::data::extradata {

namespace {

struct Registry {
	std::unordered_map<std::type_index, ExtraDataPid> pids;
	std::vector<ExtraDataRegistry::Creator> creators;
};

// Function-local: the engine types register from static initialisers of other translation units.
auto registry() -> Registry& {
	static Registry instance;
	return instance;
}

}// namespace

auto ExtraDataRegistry::registerType(const std::type_index iType, const Creator iCreator) -> ExtraDataPid {
	if (iCreator == nullptr) {
		OWL_CORE_WARN("ExtraDataRegistry: No creator given for {}.", iType.name())
		return g_invalidExtraDataPid;
	}
	auto& reg = registry();
	if (const auto found = reg.pids.find(iType); found != reg.pids.end())
		return found->second;
	const auto pid = static_cast<ExtraDataPid>(reg.creators.size());
	reg.creators.push_back(iCreator);
	reg.pids.emplace(iType, pid);
	return pid;
}

auto ExtraDataRegistry::getPid(const std::type_index iType) -> ExtraDataPid {
	const auto& reg = registry();
	if (const auto found = reg.pids.find(iType); found != reg.pids.end())
		return found->second;
	return g_invalidExtraDataPid;
}

auto ExtraDataRegistry::isRegistered(const ExtraDataPid iPid) -> bool {
	return iPid >= 0 && static_cast<size_t>(iPid) < registry().creators.size();
}

auto ExtraDataRegistry::create(const ExtraDataPid iPid) -> uniq<ExtraDataBase> {
	if (!isRegistered(iPid)) {
		OWL_CORE_WARN("ExtraDataRegistry: Unknown extra data pid {}.", iPid)
		return nullptr;
	}
	return registry().creators[static_cast<size_t>(iPid)]();
}

}// namespace owl::data::extradata
