/**
 * @file RenderLayerFactory.cpp
 * @author Silmaen
 * @date 30/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RenderLayerFactory.h"

#include <algorithm>
#include <unordered_map>

namespace owl::renderer {

namespace {
auto registry() -> std::unordered_map<std::string, RenderLayerFactory::CreateFn>& {
	static std::unordered_map<std::string, RenderLayerFactory::CreateFn> s_registry;
	return s_registry;
}

}// namespace

void RenderLayerFactory::registerType(const std::string& iTypeKey, CreateFn iFactory) {
	auto& reg = registry();
	if (reg.contains(iTypeKey)) {
		OWL_CORE_WARN("RenderLayerFactory: type '{}' already registered, replacing.", iTypeKey)
	}
	reg[iTypeKey] = std::move(iFactory);
}

auto RenderLayerFactory::unregisterType(const std::string& iTypeKey) -> bool { return registry().erase(iTypeKey) > 0; }

auto RenderLayerFactory::hasType(const std::string& iTypeKey) -> bool { return registry().contains(iTypeKey); }

auto RenderLayerFactory::create(const std::string& iTypeKey, const std::string& iName) -> shared<RenderLayer> {
	const auto& reg = registry();
	const auto it = reg.find(iTypeKey);
	if (it == reg.end()) {
		OWL_CORE_ERROR("RenderLayerFactory: unknown type '{}'.", iTypeKey)
		return nullptr;
	}
	return it->second(iName);
}

auto RenderLayerFactory::registeredTypes() -> std::vector<std::string> {
	const auto& reg = registry();
	std::vector<std::string> keys;
	keys.reserve(reg.size());
	for (const auto& key: reg | std::views::keys)
		keys.push_back(key);
	std::ranges::sort(keys);
	return keys;
}

void RenderLayerFactory::clear() { registry().clear(); }

}// namespace owl::renderer
