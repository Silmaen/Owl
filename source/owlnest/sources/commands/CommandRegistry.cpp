/**
 * @file CommandRegistry.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "CommandRegistry.h"

#include "../EntitySnapshot.h"
#include "ComponentCommands.h"
#include "EntityCommands.h"
#include "HierarchyCommands.h"
#include "PrefabCommands.h"

#include <scene/ComponentRegistry.h>
#include <scene/PrefabSerializer.h>
#include <scene/component/components.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <filesystem>
#include <format>
#include <sstream>
#include <tuple>

namespace owl::nest::commands {

namespace {

using Prepared = expected<PreparedCommand, CommandFailure>;

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto typeName(const ArgType iType) -> std::string_view {
	switch (iType) {
		case ArgType::Bool:
			return "a boolean";
		case ArgType::Integer:
			return "an integer";
		case ArgType::Number:
			return "a number";
		case ArgType::Text:
			return "a text";
		case ArgType::Entity:
			return "an entity UUID or tag";
		case ArgType::Vec3:
			return "three numbers";
	}
	return "";
}

auto fits(const ArgType iType, const ArgValue& iValue) -> bool {
	switch (iType) {
		case ArgType::Bool:
			return std::holds_alternative<bool>(iValue);
		case ArgType::Integer:
			return std::holds_alternative<int64_t>(iValue);
		case ArgType::Number:
			return std::holds_alternative<double>(iValue) || std::holds_alternative<int64_t>(iValue);
		case ArgType::Text:
			return std::holds_alternative<std::string>(iValue);
		case ArgType::Entity:
			return std::holds_alternative<int64_t>(iValue) || std::holds_alternative<std::string>(iValue);
		case ArgType::Vec3:
			return std::holds_alternative<math::vec3>(iValue);
	}
	return false;
}

auto entityArg(const scene::Scene& iScene, const CommandArgs& iArgs, const std::string& iName)
		-> expected<scene::Entity, CommandFailure> {
	const auto& values = iArgs.getValues();
	const auto found = values.find(iName);
	if (found == values.end())
		return fail(CommandError::MissingArgument, std::format("argument `{}` is missing", iName));
	if (auto entity = CommandRegistry::resolveEntity(iScene, found->second); entity)
		return *entity;
	return fail(CommandError::UnknownEntity, std::format("no entity matches `{}`", iName));
}

auto optionalComponent(const std::string& iName) -> const scene::ComponentDescriptor* {
	const auto* desc = scene::ComponentRegistry::find(iName);
	return desc != nullptr && desc->optional ? desc : nullptr;
}

auto addByName(scene::Entity& ioEntity, const std::string& iName) -> std::optional<std::string> {
	const auto* desc = optionalComponent(iName);
	if (desc == nullptr || desc->has(ioEntity))
		return std::nullopt;
	desc->add(ioEntity);
	return desc->name;
}

auto removeByName(scene::Entity& ioEntity, const std::string& iName) -> std::optional<std::string> {
	const auto* desc = optionalComponent(iName);
	if (desc == nullptr || !desc->has(ioEntity))
		return std::nullopt;
	desc->remove(ioEntity);
	return desc->name;
}

auto createEntity(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	std::optional<scene::Entity> parent;
	if (iArgs.has("parent")) {
		auto found = entityArg(*ioScene, iArgs, "parent");
		if (!found)
			return unexpected<CommandFailure>{found.error()};
		parent = *found;
	}
	auto entity = ioScene->createEntity(iArgs.getText("name", "Empty Entity"));
	if (parent)
		ioScene->setParent(entity, *parent);
	return PreparedCommand{.command = mkUniq<CreateEntityCommand>(entity), .applied = true, .entity = entity.getUUID()};
}

auto deleteEntity(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = entityArg(*ioScene, iArgs, "entity");
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	if (iArgs.getBool("children"))
		return PreparedCommand{.command = mkUniq<DeleteSubtreeCommand>(*entity, *ioScene), .applied = false};
	return PreparedCommand{.command = mkUniq<DeleteEntityCommand>(*entity, *ioScene), .applied = false};
}

auto duplicateEntity(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = entityArg(*ioScene, iArgs, "entity");
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	if (iArgs.getBool("children")) {
		const auto copy = ioScene->duplicateSubtree(*entity);
		return PreparedCommand{.command = mkUniq<DuplicateSubtreeCommand>(*entity, copy, *ioScene),
							   .applied = true,
							   .entity = copy.getUUID()};
	}
	const auto copy = ioScene->duplicateEntity(*entity);
	return PreparedCommand{.command = mkUniq<DuplicateEntityCommand>(*entity, copy),
						   .applied = true,
						   .entity = copy.getUUID()};
}

auto reparentEntity(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = entityArg(*ioScene, iArgs, "entity");
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	if (!iArgs.has("parent"))
		return PreparedCommand{.command = mkUniq<UnparentCommand>(*entity, *ioScene),
							   .applied = false,
							   .entity = entity->getUUID()};
	const auto parent = entityArg(*ioScene, iArgs, "parent");
	if (!parent)
		return unexpected<CommandFailure>{parent.error()};
	if (*parent == *entity)
		return fail(CommandError::NotApplicable, "an entity cannot be its own parent");
	return PreparedCommand{.command = mkUniq<ReparentCommand>(*entity, parent->getUUID(), *ioScene),
						   .applied = false,
						   .entity = entity->getUUID()};
}

template<typename Edit>
auto modifyEntity(const shared<scene::Scene>& ioScene, scene::Entity iEntity, const std::string& iDescription,
				  Edit&& iEdit) -> Prepared {
	auto before = EntitySnapshot::capture(iEntity);
	const auto beforeYaml = before.yamlData;
	std::forward<Edit>(iEdit)(iEntity);
	auto command = mkUniq<ModifyEntityCommand>(iEntity.getUUID(), std::move(before), iDescription);
	command->setPrefabOverrides(PrefabOverrideChange::record(iEntity, *ioScene, beforeYaml));
	command->captureAfter(iEntity);
	return PreparedCommand{.command = std::move(command), .applied = true, .entity = iEntity.getUUID()};
}

auto renameEntity(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = entityArg(*ioScene, iArgs, "entity");
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto name = iArgs.getText("name");
	return modifyEntity(
			ioScene, *entity, std::format("Rename '{}'", entity->getName()),
			[&name](scene::Entity& ioEntity) -> void { ioEntity.getComponent<scene::component::Tag>().tag = name; });
}

auto setTransform(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = entityArg(*ioScene, iArgs, "entity");
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto translation = iArgs.getVec3("translation");
	const auto rotation = iArgs.getVec3("rotation");
	const auto scale = iArgs.getVec3("scale");
	if (!translation && !rotation && !scale)
		return fail(CommandError::MissingArgument, "give `translation`, `rotation` or `scale`");
	return modifyEntity(ioScene, *entity, std::format("Move '{}'", entity->getName()),
						[&](scene::Entity& ioEntity) -> void {
							auto& transform = ioEntity.getComponent<scene::component::Transform>().transform;
							if (translation)
								transform.translation() = *translation;
							if (rotation)
								transform.rotation() = *rotation;
							if (scale)
								transform.scale() = *scale;
						});
}

auto addComponent(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	auto entity = entityArg(*ioScene, iArgs, "entity");
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto name = iArgs.getText("component");
	auto before = EntitySnapshot::capture(*entity);
	const auto added = addByName(*entity, name);
	if (!added)
		return fail(CommandError::InvalidArgument,
					std::format("`{}` is not an optional component, or the entity already has it", name));
	auto overrides = PrefabOverrideChange::record(*entity, *ioScene, before.yamlData);
	auto command = mkUniq<AddComponentCommand>(std::move(before), EntitySnapshot::capture(*entity), *added);
	command->setPrefabOverrides(std::move(overrides));
	return PreparedCommand{.command = std::move(command), .applied = true, .entity = entity->getUUID()};
}

auto removeComponent(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	auto entity = entityArg(*ioScene, iArgs, "entity");
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto name = iArgs.getText("component");
	auto before = EntitySnapshot::capture(*entity);
	const auto removed = removeByName(*entity, name);
	if (!removed)
		return fail(CommandError::InvalidArgument,
					std::format("`{}` is not an optional component of the entity", name));
	auto overrides = PrefabOverrideChange::record(*entity, *ioScene, before.yamlData);
	auto command = mkUniq<RemoveComponentCommand>(std::move(before), EntitySnapshot::capture(*entity), *removed);
	command->setPrefabOverrides(std::move(overrides));
	return PreparedCommand{.command = std::move(command), .applied = true, .entity = entity->getUUID()};
}

auto assetRelativePath(const std::filesystem::path& iPath) -> std::string {
	if (!app::Application::instanced())
		return {};
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (const auto rel = iPath.lexically_relative(assetsPath); !rel.empty() && *rel.begin() != "..")
			return rel.generic_string();
	}
	return {};
}

auto instantiatePrefab(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const std::filesystem::path path = iArgs.getText("path");
	if (!exists(path))
		return fail(CommandError::InvalidArgument, std::format("prefab {} not found", path.string()));
	const auto assetPath = iArgs.has("asset_path") ? iArgs.getText("asset_path") : assetRelativePath(path);
	const auto root = scene::PrefabSerializer::instantiate(path, ioScene, assetPath);
	if (!root)
		return fail(CommandError::NotApplicable, std::format("prefab {} could not be instantiated", path.string()));
	const auto info = scene::PrefabSerializer::readInfo(path);
	const auto name = info.has_value() ? info->name : path.stem().string();
	return PreparedCommand{.command = mkUniq<InstantiatePrefabCommand>(root, *ioScene, name),
						   .applied = true,
						   .entity = root.getUUID()};
}

auto entitySpec(std::string iDescription) -> ArgSpec {
	return {.name = "entity", .type = ArgType::Entity, .required = true, .description = std::move(iDescription)};
}

}// namespace

auto CommandArgs::getBool(const std::string& iName, const bool iDefault) const -> bool {
	const auto found = m_values.find(iName);
	if (found == m_values.end())
		return iDefault;
	const auto* value = std::get_if<bool>(&found->second);
	return value != nullptr ? *value : iDefault;
}

auto CommandArgs::getText(const std::string& iName, const std::string& iDefault) const -> std::string {
	const auto found = m_values.find(iName);
	if (found == m_values.end())
		return iDefault;
	const auto* value = std::get_if<std::string>(&found->second);
	return value != nullptr ? *value : iDefault;
}

auto CommandArgs::getVec3(const std::string& iName) const -> std::optional<math::vec3> {
	const auto found = m_values.find(iName);
	if (found == m_values.end())
		return std::nullopt;
	const auto* value = std::get_if<math::vec3>(&found->second);
	return value != nullptr ? std::optional{*value} : std::nullopt;
}

CommandRegistry::CommandRegistry() {
	add({.name = "entity.create",
		 .description = "Create an empty entity, at the root or under a parent.",
		 .args = {{.name = "name", .type = ArgType::Text, .description = "Tag of the entity (`Empty Entity`)."},
				  {.name = "parent", .type = ArgType::Entity, .description = "Parent entity (root when absent)."}},
		 .factory = createEntity});
	add({.name = "entity.delete",
		 .description = "Delete an entity; its children move to its parent unless `children` is true.",
		 .args = {entitySpec("Entity to delete."),
				  {.name = "children", .type = ArgType::Bool, .description = "Delete the children too."}},
		 .factory = deleteEntity});
	add({.name = "entity.duplicate",
		 .description = "Duplicate an entity next to it, with its children when `children` is true.",
		 .args = {entitySpec("Entity to duplicate."),
				  {.name = "children", .type = ArgType::Bool, .description = "Duplicate the children too."}},
		 .factory = duplicateEntity});
	add({.name = "entity.reparent",
		 .description = "Move an entity under another one, or to the root without `parent`; the world position "
						"is kept.",
		 .args = {entitySpec("Entity to move."),
				  {.name = "parent", .type = ArgType::Entity, .description = "New parent (root when absent)."}},
		 .factory = reparentEntity});
	add({.name = "entity.rename",
		 .description = "Change the tag of an entity.",
		 .args = {entitySpec("Entity to rename."),
				  {.name = "name", .type = ArgType::Text, .required = true, .description = "New tag."}},
		 .factory = renameEntity});
	add({.name = "entity.set_transform",
		 .description = "Set the local translation, rotation (radians) or scale of an entity.",
		 .args = {entitySpec("Entity to move."),
				  {.name = "translation", .type = ArgType::Vec3, .description = "Local translation."},
				  {.name = "rotation", .type = ArgType::Vec3, .description = "Local rotation, in radians."},
				  {.name = "scale", .type = ArgType::Vec3, .description = "Local scale."}},
		 .factory = setTransform});
	add({.name = "component.add",
		 .description = "Add an optional component, with its default values, to an entity.",
		 .args = {entitySpec("Entity changed."),
				  {.name = "component",
				   .type = ArgType::Text,
				   .required = true,
				   .description = "Component name (`Sprite Renderer`) or file key (`SpriteRenderer`)."}},
		 .factory = addComponent});
	add({.name = "component.remove",
		 .description = "Remove an optional component from an entity.",
		 .args = {entitySpec("Entity changed."),
				  {.name = "component",
				   .type = ArgType::Text,
				   .required = true,
				   .description = "Component name or file key."}},
		 .factory = removeComponent});
	add({.name = "prefab.instantiate",
		 .description = "Instantiate a prefab file in the scene.",
		 .args = {{.name = "path", .type = ArgType::Text, .required = true, .description = "Prefab file."},
				  {.name = "asset_path",
				   .type = ArgType::Text,
				   .description = "Path recorded in the instance link (relative to the asset folder holding the "
								  "file when absent)."}},
		 .factory = instantiatePrefab});
}

void CommandRegistry::add(CommandSpec iSpec) {
	if (auto found = std::ranges::find(m_commands, iSpec.name, &CommandSpec::name); found != m_commands.end()) {
		*found = std::move(iSpec);
		return;
	}
	m_commands.push_back(std::move(iSpec));
}

auto CommandRegistry::find(const std::string_view iName) const -> const CommandSpec* {
	const auto found = std::ranges::find(m_commands, iName, &CommandSpec::name);
	return found == m_commands.end() ? nullptr : &*found;
}

auto CommandRegistry::validate(const CommandSpec& iSpec, const CommandArgs& iArgs) -> std::optional<CommandFailure> {
	for (const auto& [name, value]: iArgs.getValues()) {
		const auto spec = std::ranges::find(iSpec.args, name, &ArgSpec::name);
		if (spec == iSpec.args.end())
			return CommandFailure{.error = CommandError::UnknownArgument,
								  .message = std::format("{}: unknown argument `{}`", iSpec.name, name)};
		if (!fits(spec->type, value))
			return CommandFailure{.error = CommandError::WrongArgumentType,
								  .message =
										  std::format("{}: `{}` must be {}", iSpec.name, name, typeName(spec->type))};
	}
	for (const auto& arg: iSpec.args) {
		if (arg.required && !iArgs.has(arg.name))
			return CommandFailure{.error = CommandError::MissingArgument,
								  .message = std::format("{}: argument `{}` is missing", iSpec.name, arg.name)};
	}
	return std::nullopt;
}

auto CommandRegistry::execute(const std::string_view iName, const CommandArgs& iArgs,
							  const shared<scene::Scene>& ioScene, SceneUndoManager& ioUndo) const
		-> expected<CommandResult, CommandFailure> {
	const auto* spec = find(iName);
	if (spec == nullptr) {
		OWL_WARN("Command: Unknown command '{}'.", iName)
		return fail(CommandError::UnknownCommand, std::format("unknown command `{}`", iName));
	}
	if (const auto failure = validate(*spec, iArgs); failure) {
		OWL_WARN("Command: {}.", failure->message)
		return unexpected<CommandFailure>{*failure};
	}
	if (!ioScene) {
		OWL_WARN("Command: {} needs a scene.", spec->name)
		return fail(CommandError::NotApplicable, std::format("{}: no scene", spec->name));
	}
	auto prepared = spec->factory(ioScene, iArgs);
	if (!prepared) {
		auto failure = prepared.error();
		failure.message = std::format("{}: {}", spec->name, failure.message);
		OWL_WARN("Command: {}.", failure.message)
		return unexpected<CommandFailure>{std::move(failure)};
	}
	CommandResult result{.entity = prepared->entity, .description = prepared->command->description()};
	if (prepared->applied)
		ioUndo.push(std::move(prepared->command));
	else
		ioUndo.execute(std::move(prepared->command), *ioScene);
	return result;
}

auto CommandRegistry::parseArg(const ArgType iType, const std::string& iText) -> std::optional<ArgValue> {
	const auto toInteger = [&iText]() -> std::optional<int64_t> {
		int64_t value = 0;
		const auto* const end = iText.data() + iText.size();
		if (const auto [ptr, ec] = std::from_chars(iText.data(), end, value); ec == std::errc{} && ptr == end)
			return value;
		return std::nullopt;
	};
	switch (iType) {
		case ArgType::Bool:
			if (iText == "true" || iText == "false")
				return iText == "true";
			return std::nullopt;
		case ArgType::Integer:
			if (const auto value = toInteger(); value)
				return *value;
			return std::nullopt;
		case ArgType::Number:
			{
				double value = 0.0;
				const auto* const end = iText.data() + iText.size();
				if (const auto [ptr, ec] = std::from_chars(iText.data(), end, value); ec == std::errc{} && ptr == end)
					return value;
				return std::nullopt;
			}
		case ArgType::Text:
			return iText;
		case ArgType::Entity:
			if (const auto value = toInteger(); value)
				return *value;
			return iText;
		case ArgType::Vec3:
			{
				std::string spaced = iText;
				std::ranges::replace(spaced, ',', ' ');
				std::istringstream stream{spaced};
				math::vec3 value;
				if (stream >> value.x() >> value.y() >> value.z())
					return value;
				return std::nullopt;
			}
	}
	return std::nullopt;
}

auto CommandRegistry::resolveEntity(const scene::Scene& iScene, const ArgValue& iValue)
		-> std::optional<scene::Entity> {
	if (const auto* uuid = std::get_if<int64_t>(&iValue); uuid != nullptr) {
		if (auto entity = iScene.findEntityByUUID(core::UUID{static_cast<uint64_t>(*uuid)}); entity)
			return entity;
		return std::nullopt;
	}
	const auto* tag = std::get_if<std::string>(&iValue);
	if (tag == nullptr)
		return std::nullopt;
	for (const auto view = iScene.registry.view<scene::component::Tag>(); const auto handle: view) {
		if (view.get<scene::component::Tag>(handle).tag == *tag)
			return scene::Entity{handle, const_cast<scene::Scene*>(&iScene)};
	}
	return std::nullopt;
}

auto CommandTarget::execute(const std::string_view iName, const CommandArgs& iArgs) const
		-> expected<CommandResult, CommandFailure> {
	if (!isValid()) {
		OWL_WARN("Command: {} ignored, no scene or undo history to run it on.", iName)
		return fail(CommandError::NotApplicable, std::format("{}: no scene or undo history", iName));
	}
	return registry->execute(iName, iArgs, scene, *undo);
}

}// namespace owl::nest::commands
