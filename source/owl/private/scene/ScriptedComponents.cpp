/**
 * @file ScriptedComponents.cpp
 * @author Silmaen
 * @date 16/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/ScriptedComponents.h"

#include "component/ScriptValueYaml.h"
#include "core/SerializerImpl.h"
#include "data/assets/pack/ExtensionRegistry.h"
#include "scene/ComponentRegistry.h"
#include "scene/Scene.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

namespace owl::scene {

namespace component {

auto ScriptedComponent::find(const std::string& iName) -> script::ScriptProperty* {
	const auto found = std::ranges::find(fields, iName, &script::ScriptProperty::name);
	return found != fields.end() ? &*found : nullptr;
}

auto ScriptedComponent::find(const std::string& iName) const -> const script::ScriptProperty* {
	const auto found = std::ranges::find(fields, iName, &script::ScriptProperty::name);
	return found != fields.end() ? &*found : nullptr;
}

}// namespace component

namespace {

using Storage = entt::storage_for_t<component::ScriptedComponent>;

auto types() -> std::vector<ScriptedComponentType>& {
	static std::vector<ScriptedComponentType> list;
	return list;
}

auto storageId(const std::string& iKey) -> entt::id_type {
	return entt::hashed_string::value(iKey.c_str(), iKey.size());
}

auto storageOf(entt::registry& ioRegistry, const std::string& iKey) -> Storage& {
	return ioRegistry.storage<component::ScriptedComponent>(storageId(iKey));
}

auto storageOf(const entt::registry& iRegistry, const std::string& iKey) -> const Storage* {
	return iRegistry.storage<component::ScriptedComponent>(storageId(iKey));
}

auto defaultsOf(const std::string& iKey) -> component::ScriptedComponent {
	const auto* type = ScriptedComponents::findType(iKey);
	return type != nullptr ? component::ScriptedComponent{.fields = type->fields} : component::ScriptedComponent{};
}

auto describeScripted(const std::string& iKey) -> ComponentDescriptor {
	ComponentDescriptor desc;
	desc.key = iKey;
	desc.name = iKey;
	desc.has = [iKey](const Entity& iEntity) -> bool {
		const auto* storage = storageOf(std::as_const(iEntity.getScene()->registry), iKey);
		return storage != nullptr && storage->contains(static_cast<entt::entity>(iEntity));
	};
	desc.add = [iKey](Entity& ioEntity) -> void {
		storageOf(ioEntity.getScene()->registry, iKey).remove(static_cast<entt::entity>(ioEntity));
		storageOf(ioEntity.getScene()->registry, iKey).emplace(static_cast<entt::entity>(ioEntity), defaultsOf(iKey));
	};
	desc.remove = [iKey](Entity& ioEntity) -> void {
		storageOf(ioEntity.getScene()->registry, iKey).remove(static_cast<entt::entity>(ioEntity));
	};
	desc.serialize = [iKey](const Entity& iEntity, const core::Serializer& iOut) -> void {
		const auto* values = ScriptedComponents::get(iEntity, iKey);
		if (values == nullptr)
			return;
		auto& out = iOut.getImpl()->emitter;
		out << YAML::Key << iKey << YAML::Value << YAML::BeginMap;
		for (const auto& field: values->fields) {
			out << YAML::Key << field.name << YAML::Value;
			component::writeScriptValue(out, field);
		}
		out << YAML::EndMap;
	};
	desc.deserialize = [iKey](Entity& ioEntity, const core::Serializer& iNode) -> void {
		auto values = defaultsOf(iKey);
		if (const auto& node = iNode.getImpl()->node; node.isMap()) {
			for (auto& field: values.fields)
				if (const auto value = node[field.name]; value)
					field.value = component::readScriptValue(value, field.type);
		}
		auto& storage = storageOf(ioEntity.getScene()->registry, iKey);
		storage.remove(static_cast<entt::entity>(ioEntity));
		storage.emplace(static_cast<entt::entity>(ioEntity), std::move(values));
	};
	desc.copy = [iKey](Entity& oDst, const Entity& iSrc) -> void {
		const auto* values = ScriptedComponents::get(iSrc, iKey);
		if (values == nullptr)
			return;
		auto copy = *values;
		auto& storage = storageOf(oDst.getScene()->registry, iKey);
		storage.remove(static_cast<entt::entity>(oDst));
		storage.emplace(static_cast<entt::entity>(oDst), std::move(copy));
	};
	desc.copyAll = [iKey](entt::registry& oDst, const entt::registry& iSrc, const EntityRemap& iRemap) -> void {
		const auto* source = storageOf(iSrc, iKey);
		if (source == nullptr)
			return;
		auto& target = storageOf(oDst, iKey);
		for (const auto entity: std::views::reverse(static_cast<const entt::sparse_set&>(*source))) {
			const auto* uuid = iSrc.try_get<component::ID>(entity);
			if (uuid == nullptr)
				continue;
			if (const auto found = iRemap.find(uuid->id); found != iRemap.end()) {
				target.remove(found->second);
				target.emplace(found->second, source->get(entity));
			}
		}
	};
	return desc;
}

}// namespace

auto ScriptedComponents::registerType(const ScriptedComponentType& iType) -> bool {
	if (!iType.key.starts_with(iType.extensionId + ".") || iType.key.size() <= iType.extensionId.size() + 1) {
		OWL_CORE_WARN("Extensions: Component '{}' of '{}' is not registered: its key must start with '{}.'.", iType.key,
					  iType.extensionId, iType.extensionId)
		return false;
	}
	if (auto found = std::ranges::find(types(), iType.key, &ScriptedComponentType::key); found != types().end()) {
		if (found->extensionId != iType.extensionId)
			return false;
		found->fields = iType.fields;
		return true;
	}
	if (!ComponentRegistry::registerDescriptor(describeScripted(iType.key))) {
		OWL_CORE_WARN("Extensions: Component '{}' of '{}' is not registered: the key is taken.", iType.key,
					  iType.extensionId)
		return false;
	}
	types().push_back(iType);
	return true;
}

auto ScriptedComponents::unregisterType(const std::string& iKey) -> bool {
	const auto found = std::ranges::find(types(), iKey, &ScriptedComponentType::key);
	if (found == types().end())
		return false;
	std::ignore = ComponentRegistry::unregisterComponent(iKey);
	types().erase(found);
	return true;
}

auto ScriptedComponents::sync(const data::assets::pack::ExtensionRegistry& iExtensions) -> size_t {
	std::vector<std::string> declared;
	for (const auto* pack: iExtensions.getPacks()) {
		const auto& manifest = pack->getManifest();
		if (manifest.kind != data::assets::pack::ExtensionKind::Script || manifest.script.empty())
			continue;
		const auto source = iExtensions.readEntry(manifest.script);
		if (!source)
			continue;
		for (auto& declaration: script::ScriptEngine::extractComponentsFromBuffer(*source, manifest.script)) {
			if (registerType({.key = declaration.key, .extensionId = manifest.id, .fields = declaration.fields}))
				declared.push_back(std::move(declaration.key));
		}
	}
	const auto stale = types() | std::views::filter([&declared](const ScriptedComponentType& iType) -> bool {
						   return !std::ranges::contains(declared, iType.key);
					   }) |
					   std::views::transform(&ScriptedComponentType::key) | std::ranges::to<std::vector>();
	for (const auto& key: stale) std::ignore = unregisterType(key);
	return types().size();
}

void ScriptedComponents::clear() {
	while (!types().empty()) std::ignore = unregisterType(types().back().key);
}

auto ScriptedComponents::getTypes() -> const std::vector<ScriptedComponentType>& { return types(); }

auto ScriptedComponents::findType(const std::string& iKey) -> const ScriptedComponentType* {
	const auto found = std::ranges::find(types(), iKey, &ScriptedComponentType::key);
	return found != types().end() ? &*found : nullptr;
}

auto ScriptedComponents::get(const Entity& iEntity, const std::string& iKey) -> component::ScriptedComponent* {
	if (!iEntity || findType(iKey) == nullptr)
		return nullptr;
	auto& storage = storageOf(iEntity.getScene()->registry, iKey);
	const auto handle = static_cast<entt::entity>(iEntity);
	return storage.contains(handle) ? &storage.get(handle) : nullptr;
}

auto ScriptedComponents::add(const Entity& iEntity, const std::string& iKey) -> component::ScriptedComponent* {
	if (!iEntity || findType(iKey) == nullptr)
		return nullptr;
	auto& storage = storageOf(iEntity.getScene()->registry, iKey);
	const auto handle = static_cast<entt::entity>(iEntity);
	if (!storage.contains(handle))
		storage.emplace(handle, defaultsOf(iKey));
	return &storage.get(handle);
}

}// namespace owl::scene
