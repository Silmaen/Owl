/**
 * @file CommandRegistry.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "CommandRegistry.h"

#include "../EntitySnapshot.h"
#include "CommandJournal.h"
#include "ComponentCommands.h"
#include "EntityCommands.h"
#include "HierarchyCommands.h"
#include "Macro.h"
#include "PrefabCommands.h"

#include <scene/ComponentRegistry.h>
#include <scene/PrefabSerializer.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <exception>
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
		case ArgType::Vec2:
			return "two numbers";
		case ArgType::Vec4:
			return "four numbers";
		case ArgType::Color:
			return "a colour (three or four numbers, or #rrggbb[aa])";
		case ArgType::Enum:
			return "one of the declared choices";
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
		case ArgType::Vec2:
			return std::holds_alternative<math::vec2>(iValue);
		case ArgType::Vec4:
		case ArgType::Color:
			return std::holds_alternative<math::vec4>(iValue);
		case ArgType::Enum:
			return std::holds_alternative<std::string>(iValue);
	}
	return false;
}

auto joinChoices(const std::vector<std::string>& iChoices) -> std::string {
	std::string joined;
	for (const auto& choice: iChoices) joined += joined.empty() ? choice : std::format(", {}", choice);
	return joined;
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

auto toArg(const core::UUID iUuid) -> ArgValue { return static_cast<int64_t>(static_cast<uint64_t>(iUuid)); }

void mergeYaml(const YAML::Node& ioTarget, const YAML::Node& iPatch) {
	std::vector<std::pair<YAML::Node, YAML::Node>> pending{{ioTarget, iPatch}};
	while (!pending.empty()) {
		auto [target, patch] = pending.back();
		pending.pop_back();
		for (const auto& item: patch) {
			const auto key = item.first.as<std::string>();
			if (auto field = target[key]; field && field.IsMap() && item.second.IsMap())
				pending.emplace_back(field, item.second);
			else
				target[key] = YAML::Clone(item.second);
		}
	}
}

auto unknownField(const YAML::Node& iBefore, const YAML::Node& iAfter, const YAML::Node& iPatch)
		-> std::optional<std::string> {
	for (const auto& item: iPatch) {
		auto key = item.first.as<std::string>();
		if (!iBefore[key] && !iAfter[key])
			return key;
	}
	return std::nullopt;
}

auto mergedComponentYaml(const std::string& iComponentYaml, const std::string& iKey, const std::string& iValues,
						 const bool iReplace) -> expected<std::string, CommandFailure> {
	try {
		const auto patch = YAML::Load(iValues);
		if (!patch.IsMap())
			return fail(CommandError::InvalidArgument, "`values` must be a YAML or JSON map of fields");
		auto root = YAML::Load(iComponentYaml);
		auto fields = root[iKey];
		if (!fields.IsMap())
			return fail(CommandError::NotApplicable, std::format("`{}` has no editable fields", iKey));
		if (iReplace)
			root[iKey] = patch;
		else
			mergeYaml(fields, patch);
		YAML::Emitter out;
		out << root;
		return std::string{out.c_str()};
	} catch (const std::exception& e) {
		return fail(CommandError::InvalidArgument, std::format("`values` is not valid YAML: {}", e.what()));
	}
}

auto setComponent(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = entityArg(*ioScene, iArgs, "entity");
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto name = iArgs.getText("component");
	const auto* desc = scene::ComponentRegistry::find(name);
	if (desc == nullptr || !desc->has(*entity))
		return fail(CommandError::InvalidArgument, std::format("the entity has no component `{}`", name));
	const auto componentYaml = scene::SceneSerializer::serializeComponentToString(*entity, desc->key);
	if (componentYaml.empty())
		return fail(CommandError::NotApplicable, std::format("`{}` is not serializable", desc->key));
	const auto merged =
			mergedComponentYaml(componentYaml, desc->key, iArgs.getText("values"), iArgs.getBool("replace"));
	if (!merged)
		return unexpected<CommandFailure>{merged.error()};
	const auto beforeYaml = scene::SceneSerializer::serializeEntityToString(*entity);
	const auto entityYaml = scene::SceneSerializer::replaceComponentInString(beforeYaml, desc->key, *merged);
	if (entityYaml.empty() || !scene::SceneSerializer::applyEntityFromString(*entity, entityYaml)) {
		std::ignore = scene::SceneSerializer::applyEntityFromString(*entity, beforeYaml);
		return fail(CommandError::InvalidArgument, std::format("`{}` refused the values", desc->key));
	}
	const auto afterYaml = scene::SceneSerializer::serializeComponentToString(*entity, desc->key);
	const auto patch = YAML::Load(iArgs.getText("values"));
	if (const auto field = unknownField(YAML::Load(componentYaml)[desc->key], YAML::Load(afterYaml)[desc->key], patch);
		field) {
		std::ignore = scene::SceneSerializer::applyEntityFromString(*entity, beforeYaml);
		return fail(CommandError::InvalidArgument, std::format("`{}` has no field `{}`", desc->key, *field));
	}
	auto command = mkUniq<ModifyEntityCommand>(entity->getUUID(), EntitySnapshot{entity->getUUID(), beforeYaml},
											   std::format("Edit {} of '{}'", desc->name, entity->getName()));
	command->setPrefabOverrides(PrefabOverrideChange::record(*entity, *ioScene, beforeYaml));
	command->captureAfter(*entity);
	return PreparedCommand{.command = std::move(command),
						   .applied = true,
						   .entity = entity->getUUID(),
						   .data = {{"component", desc->key}}};
}

auto saveScene(const CommandContext& iContext, const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
	const std::filesystem::path path =
			iArgs.has("path") ? std::filesystem::path{iArgs.getText("path")} : iContext.scenePath;
	if (path.empty())
		return fail(CommandError::MissingArgument, "the scene was never saved, give `path`");
	if (const scene::SceneSerializer serializer{iContext.scene}; !serializer.serialize(path))
		return fail(CommandError::NotApplicable, std::format("cannot write {}", path.string()));
	iContext.undo->markSaved();
	if (iContext.onSaved)
		iContext.onSaved(path);
	return CommandResult{.entity = core::UUID{0},
						 .description = std::format("Save {}", path.filename().string()),
						 .data = {{"path", path.generic_string()}}};
}

auto historyResult(const SceneUndoManager& iUndo, std::string iDescription) -> CommandResult {
	CommandResult result{.entity = iUndo.lastSelectionHint(), .description = std::move(iDescription), .data = {}};
	result.data = {{"can_undo", iUndo.canUndo()}, {"can_redo", iUndo.canRedo()}};
	if (result.entity != core::UUID{0})
		result.data.emplace("entity", toArg(result.entity));
	return result;
}

auto undoEdit(const CommandContext& iContext, const CommandArgs& /*iArgs*/) -> expected<CommandResult, CommandFailure> {
	if (!iContext.undo->canUndo())
		return fail(CommandError::NotApplicable, "nothing to undo");
	auto description = iContext.undo->undoDescription();
	iContext.undo->undo(*iContext.scene);
	return historyResult(*iContext.undo, std::move(description));
}

auto redoEdit(const CommandContext& iContext, const CommandArgs& /*iArgs*/) -> expected<CommandResult, CommandFailure> {
	if (!iContext.undo->canRedo())
		return fail(CommandError::NotApplicable, "nothing to redo");
	auto description = iContext.undo->redoDescription();
	iContext.undo->redo(*iContext.scene);
	return historyResult(*iContext.undo, std::move(description));
}

auto parseNumbers(const std::string& iText) -> std::optional<std::vector<float>> {
	std::string spaced = iText;
	std::ranges::replace(spaced, ',', ' ');
	std::istringstream stream{spaced};
	std::vector<float> numbers;
	float value = 0.f;
	while (stream >> value) numbers.push_back(value);
	if (!stream.eof())
		return std::nullopt;
	return numbers;
}

auto parseHexColor(const std::string& iText) -> std::optional<math::vec4> {
	const auto digits = std::string_view{iText}.substr(1);
	if (digits.size() != 6 && digits.size() != 8)
		return std::nullopt;
	math::vec4 color{0.f, 0.f, 0.f, 1.f};
	for (size_t i = 0; i < digits.size() / 2; ++i) {
		uint32_t channel = 0;
		const auto* const first = digits.data() + (2 * i);
		if (const auto [ptr, ec] = std::from_chars(first, first + 2, channel, 16);
			ec != std::errc{} || ptr != first + 2)
			return std::nullopt;
		color[i] = static_cast<float>(channel) / 255.f;
	}
	return color;
}

auto entitySpec(std::string iDescription) -> ArgSpec {
	return {.name = "entity", .type = ArgType::Entity, .required = true, .description = std::move(iDescription)};
}

}// namespace

auto getSourceName(const CommandSource iSource) -> std::string_view {
	switch (iSource) {
		case CommandSource::Gesture:
			return "gesture";
		case CommandSource::Console:
			return "console";
		case CommandSource::Palette:
			return "palette";
		case CommandSource::Mcp:
			return "mcp";
		case CommandSource::Replay:
			return "replay";
		case CommandSource::Scenario:
			return "scenario";
	}
	return "";
}

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
		 .args = {{.name = "name",
				   .type = ArgType::Text,
				   .description = "Tag of the entity.",
				   .defaultValue = std::string{"Empty Entity"}},
				  {.name = "parent", .type = ArgType::Entity, .description = "Parent entity (root when absent)."}},
		 .factory = createEntity});
	add({.name = "entity.delete",
		 .description = "Delete an entity; its children move to its parent unless `children` is true.",
		 .args = {entitySpec("Entity to delete."),
				  {.name = "children",
				   .type = ArgType::Bool,
				   .description = "Delete the children too.",
				   .defaultValue = false}},
		 .factory = deleteEntity});
	add({.name = "entity.duplicate",
		 .description = "Duplicate an entity next to it, with its children when `children` is true.",
		 .args = {entitySpec("Entity to duplicate."),
				  {.name = "children",
				   .type = ArgType::Bool,
				   .description = "Duplicate the children too.",
				   .defaultValue = false}},
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
	add({.name = "component.set",
		 .description = "Change fields of a component of an entity: the values are merged into its current state.",
		 .args = {entitySpec("Entity changed."),
				  {.name = "component",
				   .type = ArgType::Text,
				   .required = true,
				   .description = "Component name or file key."},
				  {.name = "values",
				   .type = ArgType::Text,
				   .required = true,
				   .description = "YAML or JSON map of the fields to change, as in the scene file "
								  "(`{color: [1, 0, 0, 1]}`)."},
				  {.name = "replace",
				   .type = ArgType::Bool,
				   .description = "Replace every field by `values` instead of merging them (the inspector edits).",
				   .defaultValue = false}},
		 .factory = setComponent});
	add({.name = "scene.save",
		 .description = "Save the scene to its file, or to `path`.",
		 .args = {{.name = "path", .type = ArgType::Text, .description = "File written (the scene file when absent)."}},
		 .factory = {},
		 .run = saveScene});
	add({.name = "edit.undo",
		 .description = "Undo the last change of the scene.",
		 .args = {},
		 .factory = {},
		 .run = undoEdit});
	add({.name = "edit.redo",
		 .description = "Redo the last change undone.",
		 .args = {},
		 .factory = {},
		 .run = redoEdit});
	add(getMacroRunSpec());
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
		if (spec->type == ArgType::Enum && !std::ranges::contains(spec->choices, std::get<std::string>(value)))
			return CommandFailure{.error = CommandError::InvalidArgument,
								  .message = std::format("{}: `{}` must be one of: {}", iSpec.name, name,
														 joinChoices(spec->choices))};
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
	return execute(iName, iArgs, CommandContext{.scene = ioScene, .undo = &ioUndo, .playing = false});
}

auto CommandRegistry::execute(const std::string_view iName, const CommandArgs& iArgs,
							  const CommandContext& iContext) const -> expected<CommandResult, CommandFailure> {
	CommandContext context = iContext;
	context.registry = this;
	auto result = run(iName, iArgs, context);
	if (mp_journal != nullptr)
		mp_journal->record(find(iName), iName, iArgs, iContext.source, result);
	return result;
}

auto CommandRegistry::run(const std::string_view iName, const CommandArgs& iArgs, const CommandContext& iContext) const
		-> expected<CommandResult, CommandFailure> {
	const auto* spec = find(iName);
	if (spec == nullptr) {
		OWL_WARN("Command: Unknown command '{}'.", iName)
		return fail(CommandError::UnknownCommand, std::format("unknown command `{}`", iName));
	}
	if (iContext.playing && !spec->allowedInPlay) {
		OWL_WARN("Command: {} refused, the scene is playing.", spec->name)
		return fail(CommandError::RefusedDuringPlay,
					std::format("{}: the scene is playing, stop it to edit the scene", spec->name));
	}
	if (const auto failure = validate(*spec, iArgs); failure) {
		OWL_WARN("Command: {}.", failure->message)
		return unexpected<CommandFailure>{*failure};
	}
	if (!iContext.scene || iContext.undo == nullptr) {
		OWL_WARN("Command: {} needs a scene and its undo history.", spec->name)
		return fail(CommandError::NotApplicable, std::format("{}: no scene or undo history", spec->name));
	}
	CommandArgs args = iArgs;
	for (const auto& arg: spec->args) {
		if (arg.defaultValue && !args.has(arg.name))
			args.set(arg.name, *arg.defaultValue);
	}
	const auto withPrefix = [&spec](CommandFailure iFailure) -> unexpected<CommandFailure> {
		iFailure.message = std::format("{}: {}", spec->name, iFailure.message);
		OWL_WARN("Command: {}.", iFailure.message)
		return unexpected<CommandFailure>{std::move(iFailure)};
	};
	if (!spec->factory) {
		if (!spec->run)
			return withPrefix({.error = CommandError::NotApplicable, .message = "the command does nothing"});
		auto result = spec->run(iContext, args);
		if (!result)
			return withPrefix(result.error());
		return result;
	}
	auto prepared = spec->factory(iContext.scene, args);
	if (!prepared)
		return withPrefix(prepared.error());
	CommandResult result{.entity = prepared->entity,
						 .description = prepared->command->description(),
						 .data = std::move(prepared->data)};
	if (result.entity != core::UUID{0})
		result.data.insert_or_assign("entity", toArg(result.entity));
	if (prepared->applied)
		iContext.undo->push(std::move(prepared->command));
	else
		iContext.undo->execute(std::move(prepared->command), *iContext.scene);
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
		case ArgType::Enum:
			return iText;
		case ArgType::Vec2:
		case ArgType::Vec3:
		case ArgType::Vec4:
		case ArgType::Color:
			break;
	}
	if (iType == ArgType::Color && iText.starts_with('#')) {
		if (const auto color = parseHexColor(iText); color)
			return *color;
		return std::nullopt;
	}
	const auto numbers = parseNumbers(iText);
	if (!numbers)
		return std::nullopt;
	const auto& n = *numbers;
	if (iType == ArgType::Vec2 && n.size() == 2)
		return math::vec2{n[0], n[1]};
	if (iType == ArgType::Vec3 && n.size() == 3)
		return math::vec3{n[0], n[1], n[2]};
	if (iType == ArgType::Vec4 && n.size() == 4)
		return math::vec4{n[0], n[1], n[2], n[3]};
	if (iType == ArgType::Color && (n.size() == 3 || n.size() == 4))
		return math::vec4{n[0], n[1], n[2], n.size() == 4 ? n[3] : 1.f};
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
	return registry->execute(iName, iArgs,
							 CommandContext{.scene = scene, .undo = undo, .playing = false, .source = source});
}

}// namespace owl::nest::commands
