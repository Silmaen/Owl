/**
 * @file ComponentRegistry.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/ComponentRegistry.h"

#include "scene/component/components.h"

#include <algorithm>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace owl::scene {

namespace {

template<typename T, typename Tuple>
struct isInTuple;

template<typename T, typename... Ts>
struct isInTuple<T, std::tuple<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

template<typename... Components>
void describeEngineComponents(std::vector<ComponentDescriptor>& oList, const std::tuple<Components...>& /*iList*/) {
	(oList.push_back([]() -> ComponentDescriptor {
		auto desc =
				ComponentRegistry::describe<Components>(isInTuple<Components, component::OptionalComponents>::value,
														isInTuple<Components, component::CopiableComponents>::value);
		desc.builtin = true;
		return desc;
	}()),
	 ...);
}

auto storage() -> std::vector<ComponentDescriptor>& {
	static std::vector<ComponentDescriptor> descriptors = []() -> std::vector<ComponentDescriptor> {
		std::vector<ComponentDescriptor> list;
		describeEngineComponents(list, component::SerializableComponents{});
		return list;
	}();
	return descriptors;
}

auto isComplete(const ComponentDescriptor& iDesc) -> bool {
	return !iDesc.key.empty() && !iDesc.name.empty() && iDesc.has && iDesc.add && iDesc.remove && iDesc.serialize &&
		   iDesc.deserialize && iDesc.copy && iDesc.copyAll;
}

}// namespace

auto ComponentRegistry::registerDescriptor(ComponentDescriptor iDescriptor) -> bool {
	if (!isComplete(iDescriptor)) {
		OWL_CORE_WARN("ComponentRegistry: Component '{}' not registered, its descriptor is incomplete.",
					  iDescriptor.key)
		return false;
	}
	if (find(iDescriptor.key) != nullptr || find(iDescriptor.name) != nullptr) {
		OWL_CORE_WARN("ComponentRegistry: Component '{}' ({}) not registered, its key or name is already used.",
					  iDescriptor.key, iDescriptor.name)
		return false;
	}
	iDescriptor.builtin = false;
	storage().push_back(std::move(iDescriptor));
	return true;
}

auto ComponentRegistry::unregisterComponent(const std::string& iKey) -> bool {
	auto& list = storage();
	const auto it = std::ranges::find(list, iKey, &ComponentDescriptor::key);
	if (it == list.end() || it->builtin) {
		OWL_CORE_WARN("ComponentRegistry: Component '{}' not unregistered (unknown, or an engine component).", iKey)
		return false;
	}
	list.erase(it);
	return true;
}

auto ComponentRegistry::getAll() -> const std::vector<ComponentDescriptor>& { return storage(); }

auto ComponentRegistry::find(const std::string& iKeyOrName) -> const ComponentDescriptor* {
	const auto& list = storage();
	const auto it = std::ranges::find_if(list, [&iKeyOrName](const ComponentDescriptor& iDesc) -> bool {
		return iDesc.key == iKeyOrName || iDesc.name == iKeyOrName;
	});
	return it != list.end() ? &*it : nullptr;
}

}// namespace owl::scene
