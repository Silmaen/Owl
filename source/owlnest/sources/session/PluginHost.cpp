/**
 * @file PluginHost.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "PluginHost.h"

#include "../commands/DocumentCommands.h"
#include "../commands/PluginCommands.h"
#include "EditorModule.h"

#include <core/Log.h>
#include <data/assets/pack/ExtensionPack.h>
#include <platform/AtomicFile.h>
#include <platform/FileUtils.h>
#include <scene/ComponentRegistry.h>
#include <scene/SceneSerializer.h>
#include <scene/ScriptedComponents.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace owl::nest::session {

PluginUi::~PluginUi() = default;

struct PluginHost::State {
	script::ScriptConsole lua;
	uniq<EditorModule> editor;
	std::vector<commands::CommandSpec> commands;
	bool loading{false};
};

namespace {

using Fields = std::map<std::string, script::HostValue>;
using commands::CommandError;
using commands::CommandFailure;
using commands::CommandResult;

constexpr std::array<std::string_view, 5> g_hostCommands{"plugin.list", "plugin.reload", "plugin.show_panel",
														 "plugin.document_set", "asset.import"};

auto refuse(std::string iMessage) -> script::HostResult {
	return {.ok = false, .error = std::move(iMessage), .fields = {}};
}

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto textOf(const Fields& iArgs, const std::string& iName, std::string iDefault = {}) -> std::string {
	const auto found = iArgs.find(iName);
	if (found == iArgs.end())
		return iDefault;
	return std::visit(
			[&iDefault]<typename T>(const T& iItem) -> std::string {
				if constexpr (std::is_same_v<T, std::string>)
					return iItem;
				else if constexpr (std::is_same_v<T, bool>)
					return iItem ? "true" : "false";
				else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, double>)
					return std::format("{}", iItem);
				else
					return iDefault;
			},
			found->second);
}

auto numberOf(const Fields& iArgs, const std::string& iName) -> std::optional<double> {
	const auto found = iArgs.find(iName);
	if (found == iArgs.end())
		return std::nullopt;
	if (const auto* value = std::get_if<double>(&found->second))
		return *value;
	if (const auto* value = std::get_if<int64_t>(&found->second))
		return static_cast<double>(*value);
	return std::nullopt;
}

auto boolOf(const Fields& iArgs, const std::string& iName, const bool iDefault = false) -> bool {
	const auto found = iArgs.find(iName);
	if (found == iArgs.end())
		return iDefault;
	const auto* value = std::get_if<bool>(&found->second);
	return value != nullptr ? *value : iDefault;
}

auto isIdentifier(const std::string_view iName) -> bool {
	return !iName.empty() && std::isdigit(static_cast<unsigned char>(iName.front())) == 0 &&
		   std::ranges::all_of(iName, [](const char iChar) -> bool {
			   return std::isalnum(static_cast<unsigned char>(iChar)) != 0 || iChar == '_';
		   });
}

auto split(const std::string_view iList) -> std::vector<std::string> {
	std::vector<std::string> items;
	size_t start = 0;
	while (start <= iList.size()) {
		const auto comma = std::min(iList.find(',', start), iList.size());
		auto item = iList.substr(start, comma - start);
		while (!item.empty() && item.front() == ' ') item.remove_prefix(1);
		while (!item.empty() && item.back() == ' ') item.remove_suffix(1);
		if (!item.empty())
			items.emplace_back(item);
		start = comma + 1;
	}
	return items;
}

auto join(const std::vector<std::string>& iItems) -> std::string {
	std::string text;
	for (const auto& item: iItems) text += std::format("{}{}", text.empty() ? "" : ",", item);
	return text;
}

auto parseArgType(const std::string_view iName) -> std::optional<commands::ArgType> {
	constexpr std::array<std::pair<std::string_view, commands::ArgType>, 10> types{
			{{"bool", commands::ArgType::Bool},
			 {"integer", commands::ArgType::Integer},
			 {"number", commands::ArgType::Number},
			 {"text", commands::ArgType::Text},
			 {"entity", commands::ArgType::Entity},
			 {"vec2", commands::ArgType::Vec2},
			 {"vec3", commands::ArgType::Vec3},
			 {"vec4", commands::ArgType::Vec4},
			 {"color", commands::ArgType::Color},
			 {"enum", commands::ArgType::Enum}}};
	// NOLINTNEXTLINE(readability-qualified-auto): array iterator is a pointer on libstdc++, a class on Web libc++.
	const auto found = std::ranges::find(types, iName, &std::pair<std::string_view, commands::ArgType>::first);
	return found == types.end() ? std::nullopt : std::optional{found->second};
}

auto resized(const script::HostValue& iValue, const size_t iSize, const double iFill) -> std::vector<double> {
	std::vector<double> numbers;
	if (const auto* list = std::get_if<std::vector<double>>(&iValue))
		numbers = *list;
	numbers.resize(iSize, iFill);
	return numbers;
}

// The value of a widget in the type its kind edits, whatever the script gave.
auto widgetValue(const PluginWidgetKind iKind, const Fields& iArgs) -> script::HostValue {
	const auto found = iArgs.find("value");
	const script::HostValue given = found != iArgs.end() ? found->second : script::HostValue{};
	switch (iKind) {
		case PluginWidgetKind::Section:
		case PluginWidgetKind::Text:
		case PluginWidgetKind::Button:
			return {};
		case PluginWidgetKind::Checkbox:
			return boolOf(iArgs, "value");
		case PluginWidgetKind::Number:
			return numberOf(iArgs, "value").value_or(0.0);
		case PluginWidgetKind::Integer:
			return static_cast<int64_t>(numberOf(iArgs, "value").value_or(0.0));
		case PluginWidgetKind::Value:
		case PluginWidgetKind::Input:
		case PluginWidgetKind::Choice:
			return textOf(iArgs, "value");
		case PluginWidgetKind::Vec3:
			return resized(given, 3, 0.0);
		case PluginWidgetKind::Color:
			return resized(given, 4, 1.0);
	}
	return {};
}

auto toHostValue(const script::ScriptValue& iValue) -> script::HostValue {
	return std::visit(
			[]<typename T>(const T& iItem) -> script::HostValue {
				if constexpr (std::is_same_v<T, float>)
					return static_cast<double>(iItem);
				else if constexpr (std::is_same_v<T, uint64_t>)
					return static_cast<int64_t>(iItem);
				else if constexpr (std::is_same_v<T, std::vector<std::string>>)
					return join(iItem);
				else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, std::string> ||
								   std::is_same_v<T, bool> || std::is_same_v<T, std::vector<double>>)
					return iItem;
				else
					return std::vector<double>(iItem.begin(), iItem.end());
			},
			iValue);
}

auto toArgValue(const script::HostValue& iValue) -> std::optional<commands::ArgValue> {
	return std::visit(
			[]<typename T>(const T& iItem) -> std::optional<commands::ArgValue> {
				if constexpr (std::is_same_v<T, std::monostate>)
					return std::nullopt;
				else if constexpr (std::is_same_v<T, std::vector<std::string>>)
					return join(iItem);
				else if constexpr (std::is_same_v<T, std::vector<double>>) {
					const auto at = [&iItem](const size_t iIndex) -> float {
						return static_cast<float>(iItem[iIndex]);
					};
					if (iItem.size() == 2)
						return math::vec2{at(0), at(1)};
					if (iItem.size() == 3)
						return math::vec3{at(0), at(1), at(2)};
					if (iItem.size() == 4)
						return math::vec4{at(0), at(1), at(2), at(3)};
					return std::nullopt;
				} else
					return iItem;
			},
			iValue);
}

// A YAML scalar as the value it spells: a quoted one stays a text.
auto scalarToHost(const YAML::Node& iNode) -> script::HostValue {
	const auto& text = iNode.Scalar();
	if (iNode.Tag() == "!")
		return text;
	if (text == "true" || text == "false")
		return text == "true";
	const auto* const end = text.data() + text.size();
	if (int64_t whole = 0; !text.empty()) {
		if (const auto [ptr, ec] = std::from_chars(text.data(), end, whole); ec == std::errc{} && ptr == end)
			return whole;
	}
	if (double real = 0.0; !text.empty()) {
		if (const auto [ptr, ec] = std::from_chars(text.data(), end, real); ec == std::errc{} && ptr == end)
			return real;
	}
	return text;
}

// A YAML node as a Lua value: a scalar, a list of numbers or of texts, anything else as its YAML in one line.
auto yamlToHost(const YAML::Node& iNode) -> script::HostValue {
	if (iNode.IsScalar())
		return scalarToHost(iNode);
	if (iNode.IsSequence()) {
		std::vector<double> numbers;
		std::vector<std::string> texts;
		bool plain = true;
		for (const auto& item: iNode) {
			if (!item.IsScalar()) {
				plain = false;
				break;
			}
			const auto value = scalarToHost(item);
			if (const auto* whole = std::get_if<int64_t>(&value))
				numbers.push_back(static_cast<double>(*whole));
			else if (const auto* real = std::get_if<double>(&value))
				numbers.push_back(*real);
			else
				texts.push_back(item.Scalar());
		}
		if (plain && (texts.empty() || numbers.empty()))
			return texts.empty() ? script::HostValue{std::move(numbers)} : script::HostValue{std::move(texts)};
	}
	if (iNode.IsNull() || !iNode.IsDefined())
		return {};
	YAML::Emitter out;
	out << YAML::Flow << iNode;
	return std::string{out.c_str()};
}

// The fields of a component of an entity with their values, read through the serialisation of the scene.
auto componentValues(const scene::Entity& iEntity, const std::string& iKey)
		-> std::vector<std::pair<std::string, script::HostValue>> {
	std::vector<std::pair<std::string, script::HostValue>> values;
	const auto text = scene::SceneSerializer::serializeComponentToString(iEntity, iKey);
	if (text.empty())
		return values;
	try {
		const auto fields = YAML::Load(text)[iKey];
		if (!fields.IsMap())
			return values;
		for (const auto& item: fields) values.emplace_back(item.first.as<std::string>(), yamlToHost(item.second));
	} catch (const std::exception& e) { OWL_WARN("Plugins: Component '{}' not readable: {}.", iKey, e.what()) }
	return values;
}

// The entity a script names (`entity` is its id or its name) in the scene of a context.
auto lookUp(const commands::CommandContext& iContext, const Fields& iArgs) -> std::optional<scene::Entity> {
	const auto value = iArgs.find("entity");
	if (!iContext.scene || value == iArgs.end())
		return std::nullopt;
	std::optional<scene::Entity> entity;
	if (const auto id = toEntityId(value->second); id.has_value())
		entity = commands::CommandRegistry::resolveEntity(
				*iContext.scene, commands::ArgValue{static_cast<int64_t>(static_cast<uint64_t>(*id))});
	if (const auto* name = std::get_if<std::string>(&value->second); !entity && name != nullptr)
		entity = commands::CommandRegistry::resolveEntity(*iContext.scene, commands::ArgValue{*name});
	return entity;
}

auto resolveFile(const std::string& iPath, const std::filesystem::path& iBase) -> std::filesystem::path {
	std::filesystem::path path{iPath};
	if (path.is_relative() && !iBase.empty())
		path = iBase / path;
	return path.lexically_normal();
}

auto isInside(const std::filesystem::path& iPath, const std::filesystem::path& iFolder) -> bool {
	const auto relative = iPath.lexically_relative(iFolder.lexically_normal());
	return !relative.empty() && *relative.begin() != "..";
}

}// namespace

auto getContributionKindName(const PluginContributionKind iKind) -> std::string_view {
	switch (iKind) {
		case PluginContributionKind::Command:
			return "command";
		case PluginContributionKind::Panel:
			return "panel";
		case PluginContributionKind::Drawer:
			return "drawer";
		case PluginContributionKind::DocumentType:
			return "document";
		case PluginContributionKind::Importer:
			return "importer";
	}
	return "";
}

PluginHost::PluginHost(commands::CommandRegistry& ioRegistry) : m_registry{ioRegistry} { addHostCommands(); }

PluginHost::~PluginHost() {
	unload();
	for (const auto name: g_hostCommands) std::ignore = m_registry.remove(name);
}

void PluginHost::unload() {
	for (const auto& plugin: m_plugins)
		for (const auto& contribution: plugin.contributions)
			if (contribution.kind == PluginContributionKind::Command)
				std::ignore = m_registry.remove(contribution.id);
	m_plugins.clear();
	m_states.clear();
}

auto PluginHost::load(const data::assets::pack::ExtensionRegistry& iExtensions) -> size_t {
	mp_extensions = &iExtensions;
	if (m_depth > 0) {
		m_reloadPending = true;
		return 0;
	}
	std::vector<std::string> shown;
	for (const auto* panel: getContributions(PluginContributionKind::Panel))
		if (panel->visible)
			shown.push_back(panel->id);
	unload();
	size_t loaded = 0;
	for (const auto* pack: iExtensions.getPacks()) {
		if (pack->getManifest().editorScript.empty())
			continue;
		if (loadPlugin(*pack))
			++loaded;
	}
	for (const auto& id: shown) std::ignore = setPanelVisible(id, true);
	if (!m_plugins.empty())
		OWL_INFO("Plugins: {} Owl Nest plugin(s) loaded, {} with errors.", loaded, m_plugins.size() - loaded)
	return loaded;
}

auto PluginHost::loadPlugin(const data::assets::pack::ExtensionPack& iPack) -> bool {
	const auto& manifest = iPack.getManifest();
	const size_t index = m_plugins.size();
	m_plugins.push_back({.id = manifest.id,
						 .name = manifest.getDisplayName(),
						 .script = manifest.editorScript,
						 .source = iPack.getSource(),
						 .contributions = {},
						 .errors = {}});
	m_states.push_back(mkUniq<State>());
	auto& state = *m_states.back();
	const auto bytes = iPack.readEntry(manifest.editorScript);
	if (!bytes) {
		report(index,
			   std::format("the pack holds no `{}` (only a Script pack mounts its scripts)", manifest.editorScript));
		return false;
	}
	state.editor =
			mkUniq<EditorModule>(state.lua, m_registry,
								 [this](const std::string_view iName,
										const commands::CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
									 return executeFromScript(iName, iArgs);
								 });
	state.editor->sync();
	bindModules(index);
	state.loading = true;
	++m_depth;
	const auto result = state.lua.run(std::string_view{reinterpret_cast<const char*>(bytes->data()), bytes->size()},
									  manifest.editorScript);
	--m_depth;
	state.loading = false;
	if (result.status == script::ScriptConsole::Status::Error) {
		report(index, result.output.empty() ? std::string{"the entry script failed"} : result.output.back());
		m_plugins[index].contributions.clear();
		return false;
	}
	for (auto& contribution: m_plugins[index].contributions) {
		contribution.line = state.lua.getFunctionLine(contribution.handler);
		if (contribution.line == 0)
			report(index,
				   std::format("{} `{}`: the script defines no function `{}`",
							   getContributionKindName(contribution.kind), contribution.id, contribution.handler));
	}
	for (auto& spec: state.commands) m_registry.add(std::move(spec));
	state.commands.clear();
	return m_plugins[index].errors.empty();
}

void PluginHost::bindModules(const size_t iIndex) {
	auto& lua = m_states[iIndex]->lua;
	const auto declaration = [this, iIndex](const PluginContributionKind iKind) -> script::HostFunction {
		return [this, iIndex, iKind](const Fields& iArgs) -> script::HostResult {
			return declare(iIndex, iKind, iArgs);
		};
	};
	lua.addFunction("nest.command",
					"Add a command: name (starts with the pack id), description, handler, play (allowed in Play), "
					"scene (needs a scene, true by default).",
					declaration(PluginContributionKind::Command));
	lua.addFunction(
			"nest.argument",
			"Add an argument to a command: command, name, type (bool, integer, number, text, entity, vec2, "
			"vec3, vec4, color, enum), description, required, default, choices (`a,b,c`).",
			[this, iIndex](const Fields& iArgs) -> script::HostResult { return declareArgument(iIndex, iArgs); });
	lua.addFunction("nest.panel", "Add a panel: id (starts with the pack id), title, handler (draws it with ui.*).",
					declaration(PluginContributionKind::Panel));
	lua.addFunction("nest.drawer", "Draw a component in the inspector: component (its key), handler.",
					declaration(PluginContributionKind::Drawer));
	lua.addFunction("nest.document",
					"Add a document type: id (starts with the pack id), extension (`.owlnote`), title, handler.",
					declaration(PluginContributionKind::DocumentType));
	lua.addFunction("nest.importer",
					"Add an importer: id (starts with the pack id), extension of the source (`.txt`), description, "
					"handler (returns the path and the text of the asset).",
					declaration(PluginContributionKind::Importer));
	lua.addFunction("nest.entity",
					"Look an entity up by UUID or name: exists, entity, name; with component (a name or key), has.",
					[this](const Fields& iArgs) -> script::HostResult {
						script::HostResult result{.ok = true, .error = {}, .fields = {{"exists", false}}};
						if (const auto entity = lookUp(currentContext(), iArgs); entity) {
							result.fields["exists"] = true;
							result.fields["entity"] = toHostEntity(entity->getUUID());
							result.fields["name"] = entity->getName();
							if (const auto* component = scene::ComponentRegistry::find(textOf(iArgs, "component"));
								iArgs.contains("component"))
								result.fields["has"] = component != nullptr && component->has(*entity);
						}
						return result;
					});
	lua.addFunction("nest.entities",
					"List the entities of the scene, read only: count, ids, names (lists, at most max, 1000 by "
					"default) and, with a selection, selection and selection_name. With component (a name or key), "
					"only the entities that have it.",
					[this](const Fields& iArgs) -> script::HostResult { return listEntities(iArgs); });
	lua.addFunction("nest.components",
					"The components an entity has, read only: exists, keys (the keys of the component list).",
					[this](const Fields& iArgs) -> script::HostResult {
						script::HostResult result{.ok = true, .error = {}, .fields = {{"exists", false}}};
						if (const auto entity = lookUp(currentContext(), iArgs); entity) {
							std::vector<std::string> keys;
							for (const auto& descriptor: scene::ComponentRegistry::getAll())
								if (descriptor.has(*entity))
									keys.push_back(descriptor.key);
							result.fields["exists"] = true;
							result.fields["keys"] = std::move(keys);
						}
						return result;
					});
	lua.addFunction("nest.fields",
					"The fields of a component of an entity, read only: exists, has, names (their names, in the order "
					"of the component). Needs entity and component.",
					[this](const Fields& iArgs) -> script::HostResult { return readFields(iArgs, false); });
	lua.addFunction("nest.field",
					"The value of one field of a component of an entity, read only: exists, has, found, value. Needs "
					"entity, component and field; a vector or a colour is a list, a map or a list of maps its YAML.",
					[this](const Fields& iArgs) -> script::HostResult { return readFields(iArgs, true); });
	constexpr std::array<std::tuple<std::string_view, PluginWidgetKind, std::string_view>, 11> widgets{
			{{"ui.section", PluginWidgetKind::Section, "Start a section: label. Returns open."},
			 {"ui.text", PluginWidgetKind::Text, "A paragraph: text, dim."},
			 {"ui.value", PluginWidgetKind::Value, "A read-only row: label, value."},
			 {"ui.button", PluginWidgetKind::Button, "A button: label, tooltip, primary. Returns clicked."},
			 {"ui.checkbox", PluginWidgetKind::Checkbox, "A check box row: label, value. Returns value, changed."},
			 {"ui.number", PluginWidgetKind::Number,
			  "A number row: label, value, min, max, step. Returns value, changed."},
			 {"ui.integer", PluginWidgetKind::Integer,
			  "An integer row: label, value, min, max, step. Returns value, changed."},
			 {"ui.input", PluginWidgetKind::Input, "A text row: label, value. Returns value, changed."},
			 {"ui.vec3", PluginWidgetKind::Vec3, "An X / Y / Z row: label, value {x, y, z}. Returns value, changed."},
			 {"ui.color", PluginWidgetKind::Color, "A colour row: label, value {r, g, b, a}. Returns value, changed."},
			 {"ui.choice", PluginWidgetKind::Choice,
			  "A drop-down row: label, value, choices (`a,b,c`). Returns value, changed."}}};
	for (const auto& [path, kind, description]: widgets)
		lua.addFunction(path, description, [this, widget = kind](const Fields& iArgs) -> script::HostResult {
			return drawWidget(widget, iArgs);
		});
}

auto PluginHost::listEntities(const Fields& iArgs) const -> script::HostResult {
	script::HostResult result{.ok = true, .error = {}, .fields = {{"count", int64_t{0}}}};
	const auto context = currentContext();
	if (!context.scene)
		return result;
	const scene::ComponentDescriptor* filter = nullptr;
	if (iArgs.contains("component")) {
		filter = scene::ComponentRegistry::find(textOf(iArgs, "component"));
		if (filter == nullptr)
			return refuse(std::format("nest.entities: no component `{}`", textOf(iArgs, "component")));
	}
	const auto limit = static_cast<size_t>(std::max(0.0, numberOf(iArgs, "max").value_or(1000.0)));
	std::vector<std::string> ids;
	std::vector<std::string> names;
	int64_t count = 0;
	for (const auto& entity: context.scene->getAllEntities()) {
		if (filter != nullptr && !filter->has(entity))
			continue;
		++count;
		if (ids.size() >= limit)
			continue;
		ids.push_back(std::to_string(static_cast<uint64_t>(entity.getUUID())));
		names.push_back(entity.getName());
	}
	result.fields["count"] = count;
	result.fields["ids"] = std::move(ids);
	result.fields["names"] = std::move(names);
	if (const auto selected = m_selectionProvider ? m_selectionProvider() : core::UUID{0}; selected != core::UUID{0}) {
		if (const auto entity = context.scene->findEntityByUUID(selected); entity) {
			result.fields["selection"] = toHostEntity(selected);
			result.fields["selection_name"] = entity.getName();
		}
	}
	return result;
}

auto PluginHost::readFields(const Fields& iArgs, const bool iOneField) const -> script::HostResult {
	script::HostResult result{.ok = true, .error = {}, .fields = {{"exists", false}, {"has", false}}};
	const auto entity = lookUp(currentContext(), iArgs);
	if (!entity)
		return result;
	result.fields["exists"] = true;
	const auto* component = scene::ComponentRegistry::find(textOf(iArgs, "component"));
	if (component == nullptr || !component->has(*entity))
		return result;
	result.fields["has"] = true;
	const auto values = componentValues(*entity, component->key);
	if (!iOneField) {
		std::vector<std::string> names;
		names.reserve(values.size());
		for (const auto& [name, value]: values) names.push_back(name);
		result.fields["names"] = std::move(names);
		return result;
	}
	const auto wanted = textOf(iArgs, "field");
	const auto found = std::ranges::find(values, wanted, &std::pair<std::string, script::HostValue>::first);
	result.fields["found"] = found != values.end();
	if (found != values.end() && !std::holds_alternative<std::monostate>(found->second))
		result.fields["value"] = found->second;
	return result;
}

auto PluginHost::declare(const size_t iIndex, const PluginContributionKind iKind, const Fields& iArgs)
		-> script::HostResult {
	auto& plugin = m_plugins[iIndex];
	auto& state = *m_states[iIndex];
	const auto kindName = getContributionKindName(iKind);
	if (!state.loading)
		return refuse(std::format("nest.{}: declarations are read when the plugin loads only", kindName));
	std::string idField = "id";
	if (iKind == PluginContributionKind::Command)
		idField = "name";
	else if (iKind == PluginContributionKind::Drawer)
		idField = "component";
	PluginContribution contribution{.kind = iKind,
									.id = textOf(iArgs, idField),
									.title = textOf(iArgs, "title", textOf(iArgs, "description")),
									.extension = textOf(iArgs, "extension"),
									.handler = textOf(iArgs, "handler")};
	if (contribution.id.empty())
		return refuse(std::format("nest.{}: `{}` is missing", kindName, idField));
	if (iKind != PluginContributionKind::Drawer && !contribution.id.starts_with(plugin.id + "."))
		return refuse(std::format("nest.{}: `{}` must start with `{}.`", kindName, contribution.id, plugin.id));
	if (!isIdentifier(contribution.handler))
		return refuse(std::format("nest.{}: `handler` must name a global function", kindName));
	if (findContribution(iKind, contribution.id) != nullptr)
		return refuse(std::format("nest.{}: `{}` is already declared", kindName, contribution.id));
	const bool hasExtension =
			iKind == PluginContributionKind::DocumentType || iKind == PluginContributionKind::Importer;
	if (hasExtension && (contribution.extension.size() < 2 || contribution.extension.front() != '.'))
		return refuse(std::format("nest.{}: `extension` must be a file extension with its dot", kindName));
	if (iKind == PluginContributionKind::DocumentType)
		for (const auto* other: getContributions(iKind))
			if (other->extension == contribution.extension)
				return refuse(
						std::format("nest.document: `{}` already opens `{}` files", other->id, contribution.extension));
	if (contribution.title.empty())
		contribution.title = contribution.id;
	if (iKind == PluginContributionKind::Command) {
		if (m_registry.find(contribution.id) != nullptr)
			return refuse(std::format("nest.command: `{}` is already a command", contribution.id));
		state.commands.push_back(
				{.name = contribution.id,
				 .description = contribution.title,
				 .args = {},
				 .factory = {},
				 .run = [this, pluginId = plugin.id, name = contribution.id](
								const commands::CommandContext& iContext,
								const commands::CommandArgs& iCommandArgs) -> expected<CommandResult, CommandFailure> {
					 return runCommand(pluginId, name, iContext, iCommandArgs);
				 },
				 .allowedInPlay = boolOf(iArgs, "play"),
				 .needsScene = boolOf(iArgs, "scene", true)});
	}
	plugin.contributions.push_back(std::move(contribution));
	return {.ok = true, .error = {}, .fields = {}};
}

auto PluginHost::declareArgument(const size_t iIndex, const Fields& iArgs) -> script::HostResult {
	auto& state = *m_states[iIndex];
	if (!state.loading)
		return refuse("nest.argument: declarations are read when the plugin loads only");
	const auto command = std::ranges::find(state.commands, textOf(iArgs, "command"), &commands::CommandSpec::name);
	if (command == state.commands.end())
		return refuse(std::format("nest.argument: `{}` is no command of this plugin", textOf(iArgs, "command")));
	const auto type = parseArgType(textOf(iArgs, "type", "text"));
	if (!type)
		return refuse(std::format("nest.argument: `{}` is no argument type", textOf(iArgs, "type")));
	commands::ArgSpec spec{.name = textOf(iArgs, "name"),
						   .type = *type,
						   .required = boolOf(iArgs, "required"),
						   .description = textOf(iArgs, "description"),
						   .defaultValue = {},
						   .choices = split(textOf(iArgs, "choices")),
						   .projectPath = false};
	if (!isIdentifier(spec.name) || std::ranges::contains(command->args, spec.name, &commands::ArgSpec::name))
		return refuse(std::format("nest.argument: `{}` is no new argument name", spec.name));
	if (const auto value = iArgs.find("default"); value != iArgs.end()) {
		auto converted = session::toArgValue(spec, value->second);
		if (!converted)
			return refuse(std::format("nest.argument: default of {}", converted.error()));
		spec.defaultValue = std::move(*converted);
	}
	command->args.push_back(std::move(spec));
	return {.ok = true, .error = {}, .fields = {}};
}

auto PluginHost::drawWidget(const PluginWidgetKind iKind, const Fields& iArgs) -> script::HostResult {
	if (mp_ui == nullptr)
		return refuse("ui.*: widgets are drawn from the handler of a panel, a drawer or a document only");
	const PluginWidget widget{.kind = iKind,
							  .label = textOf(iArgs, iKind == PluginWidgetKind::Text ? "text" : "label"),
							  .tooltip = textOf(iArgs, "tooltip"),
							  .value = widgetValue(iKind, iArgs),
							  .min = numberOf(iArgs, "min"),
							  .max = numberOf(iArgs, "max"),
							  .step = numberOf(iArgs, "step"),
							  .choices = split(textOf(iArgs, "choices")),
							  .emphasis = boolOf(iArgs, "dim") || boolOf(iArgs, "primary")};
	script::HostResult out{.ok = true, .error = {}, .fields = {}};
	if (iKind == PluginWidgetKind::Section) {
		m_sectionOpen = mp_ui->draw(widget).changed;
		out.fields.emplace("open", m_sectionOpen);
		return out;
	}
	const auto result =
			m_sectionOpen ? mp_ui->draw(widget) : PluginWidgetResult{.value = widget.value, .changed = false};
	if (!std::holds_alternative<std::monostate>(result.value))
		out.fields.emplace("value", result.value);
	out.fields.emplace(iKind == PluginWidgetKind::Button ? "clicked" : "changed", result.changed);
	return out;
}

auto PluginHost::invoke(const size_t iIndex, const std::string& iHandler, const Fields& iArgs,
						const commands::CommandContext* iContext, PluginUi* ioUi) -> script::ScriptConsole::CallResult {
	auto& state = *m_states[iIndex];
	const auto* outerContext = mp_context;
	auto* outerUi = mp_ui;
	const bool outerSection = m_sectionOpen;
	mp_context = iContext;
	mp_ui = ioUi;
	m_sectionOpen = true;
	++m_depth;
	state.editor->sync();
	auto result = state.lua.call(iHandler, iArgs);
	--m_depth;
	mp_context = outerContext;
	mp_ui = outerUi;
	m_sectionOpen = outerSection;
	if (!result.ok)
		report(iIndex, std::format("{}: {}", iHandler, result.error));
	if (m_depth == 0 && m_reloadPending && mp_extensions != nullptr) {
		m_reloadPending = false;
		std::ignore = load(*mp_extensions);
	}
	return result;
}

auto PluginHost::runCommand(const std::string& iPlugin, const std::string& iCommand,
							const commands::CommandContext& iContext, const commands::CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const auto [index, contribution] = locate(PluginContributionKind::Command, iCommand);
	if (contribution == nullptr || m_plugins[index].id != iPlugin) {
		OWL_WARN("Plugins: Command '{}' has no loaded plugin.", iCommand)
		return fail(CommandError::NotApplicable, std::format("the plugin `{}` is not loaded", iPlugin));
	}
	Fields fields;
	for (const auto& [name, value]: iArgs.getValues()) fields.emplace(name, session::toHostValue(value));
	CommandResult out{.entity = core::UUID{0}, .description = contribution->title, .data = {}};
	const std::string handler = contribution->handler;
	if (iContext.undo != nullptr)
		iContext.undo->beginGroup(out.description);
	const auto result = invoke(index, handler, fields, &iContext, nullptr);
	if (!result.ok) {
		if (iContext.undo != nullptr && iContext.scene)
			iContext.undo->cancelGroup(*iContext.scene);
		OWL_WARN("Plugins: Command '{}' failed: {}.", iCommand, result.error)
		return fail(CommandError::NotApplicable, result.error);
	}
	if (iContext.undo != nullptr)
		iContext.undo->endGroup();
	for (const auto& [name, value]: result.fields) {
		if (name == "description" && std::holds_alternative<std::string>(value)) {
			out.description = std::get<std::string>(value);
			continue;
		}
		if (const auto id = name == "entity" ? toEntityId(value) : std::nullopt; id.has_value()) {
			out.entity = *id;
			out.data.insert_or_assign(name, static_cast<int64_t>(static_cast<uint64_t>(*id)));
			continue;
		}
		if (auto converted = toArgValue(value); converted)
			out.data.insert_or_assign(name, std::move(*converted));
	}
	return out;
}

auto PluginHost::currentContext() const -> commands::CommandContext {
	commands::CommandContext context;
	if (mp_context != nullptr)
		context = *mp_context;
	else if (m_contextProvider)
		context = m_contextProvider();
	context.source = commands::CommandSource::Script;
	return context;
}

auto PluginHost::executeFromScript(const std::string_view iName, const commands::CommandArgs& iArgs) const
		-> expected<CommandResult, CommandFailure> {
	if (std::ranges::any_of(m_states, [](const uniq<State>& iState) -> bool { return iState->loading; })) {
		OWL_WARN("Plugins: Command '{}' refused while a plugin loads.", iName)
		return fail(CommandError::NotApplicable, "a plugin runs no command while it loads");
	}
	return m_registry.execute(iName, iArgs, currentContext());
}

void PluginHost::report(const size_t iIndex, const std::string& iMessage) {
	auto& errors = m_plugins[iIndex].errors;
	if (std::ranges::contains(errors, iMessage))
		return;
	OWL_WARN("Plugins: '{}': {}.", m_plugins[iIndex].id, iMessage)
	if (errors.size() < maxErrors)
		errors.push_back(iMessage);
}

auto PluginHost::find(const std::string_view iId) const -> const PluginInfo* {
	const auto found = std::ranges::find(m_plugins, iId, &PluginInfo::id);
	return found == m_plugins.end() ? nullptr : &*found;
}

auto PluginHost::locate(const PluginContributionKind iKind, const std::string_view iId)
		-> std::pair<size_t, PluginContribution*> {
	for (size_t index = 0; index < m_plugins.size(); ++index)
		for (auto& contribution: m_plugins[index].contributions)
			if (contribution.kind == iKind && contribution.id == iId)
				return {index, &contribution};
	return {0, nullptr};
}

auto PluginHost::findContribution(const PluginContributionKind iKind, const std::string_view iId) const
		-> const PluginContribution* {
	for (const auto& plugin: m_plugins)
		for (const auto& contribution: plugin.contributions)
			if (contribution.kind == iKind && contribution.id == iId)
				return &contribution;
	return nullptr;
}

auto PluginHost::getContributions(const PluginContributionKind iKind) const -> std::vector<const PluginContribution*> {
	std::vector<const PluginContribution*> found;
	for (const auto& plugin: m_plugins)
		for (const auto& contribution: plugin.contributions)
			if (contribution.kind == iKind)
				found.push_back(&contribution);
	return found;
}

auto PluginHost::setPanelVisible(const std::string_view iId, const bool iVisible) -> bool {
	auto* panel = locate(PluginContributionKind::Panel, iId).second;
	if (panel == nullptr)
		return false;
	panel->visible = iVisible;
	return true;
}

auto PluginHost::drawPanel(const std::string_view iId, PluginUi& ioUi) -> bool {
	const auto [index, panel] = locate(PluginContributionKind::Panel, iId);
	if (panel == nullptr)
		return false;
	const auto context = currentContext();
	Fields args{{"playing", context.playing},
				{"entity_count", static_cast<int64_t>(context.scene ? context.scene->getEntityCount() : 0)}};
	if (const auto selected = m_selectionProvider ? m_selectionProvider() : core::UUID{0};
		context.scene && selected != core::UUID{0}) {
		if (const auto entity = context.scene->findEntityByUUID(selected); entity) {
			args.emplace("selection", toHostEntity(selected));
			args.emplace("selection_name", entity.getName());
		}
	}
	const auto result = invoke(index, std::string{panel->handler}, args, nullptr, &ioUi);
	if (!result.ok)
		std::ignore = ioUi.draw({.kind = PluginWidgetKind::Text,
								 .label = std::format("The plugin failed: {}", result.error),
								 .emphasis = true});
	return result.ok;
}

auto PluginHost::drawComponent(const scene::Entity& iEntity, const std::string& iKey, PluginUi& ioUi) -> bool {
	const auto [index, drawer] = locate(PluginContributionKind::Drawer, iKey);
	if (drawer == nullptr)
		return false;
	Fields args{{"entity", toHostEntity(iEntity.getUUID())}, {"name", iEntity.getName()}, {"component", iKey}};
	// The fields of the component, built-in or declared in Lua; the three keys above keep their meaning.
	if (const auto* scripted = scene::ScriptedComponents::get(iEntity, iKey); scripted != nullptr)
		for (const auto& field: scripted->fields) args.emplace(field.name, toHostValue(field.value));
	else
		for (auto& [name, value]: componentValues(iEntity, iKey))
			if (!std::holds_alternative<std::monostate>(value))
				args.emplace(name, std::move(value));
	if (const auto result = invoke(index, std::string{drawer->handler}, args, nullptr, &ioUi); !result.ok)
		std::ignore = ioUi.draw({.kind = PluginWidgetKind::Text,
								 .label = std::format("The plugin failed: {}", result.error),
								 .emphasis = true});
	return true;
}

auto PluginHost::findDocumentType(const std::filesystem::path& iPath) const -> const PluginContribution* {
	const auto extension = iPath.extension().string();
	for (const auto* type: getContributions(PluginContributionKind::DocumentType))
		if (type->extension == extension)
			return type;
	return nullptr;
}

auto PluginHost::drawDocument(const std::string_view iType, const std::filesystem::path& iPath,
							  const std::string& iText, PluginUi& ioUi) -> bool {
	const auto [index, type] = locate(PluginContributionKind::DocumentType, iType);
	if (type == nullptr)
		return false;
	const auto result =
			invoke(index, std::string{type->handler},
				   {{"path", iPath.string()}, {"name", iPath.stem().string()}, {"text", iText}}, nullptr, &ioUi);
	if (!result.ok)
		std::ignore = ioUi.draw({.kind = PluginWidgetKind::Text,
								 .label = std::format("The plugin failed: {}", result.error),
								 .emphasis = true});
	return result.ok;
}

auto PluginHost::findImporter(const std::filesystem::path& iPath, const std::string_view iId) const
		-> const PluginContribution* {
	const auto extension = iPath.extension().string();
	for (const auto* importer: getContributions(PluginContributionKind::Importer))
		if (importer->extension == extension && (iId.empty() || importer->id == iId))
			return importer;
	return nullptr;
}

auto PluginHost::runImporter(const std::string_view iId, const std::filesystem::path& iSource, const std::string& iText)
		-> expected<PluginImport, std::string> {
	const auto [index, importer] = locate(PluginContributionKind::Importer, iId);
	if (importer == nullptr) {
		OWL_WARN("Plugins: No importer '{}'.", iId)
		return unexpected<std::string>{std::format("no importer `{}`", iId)};
	}
	const auto result =
			invoke(index, std::string{importer->handler},
				   {{"path", iSource.string()}, {"name", iSource.stem().string()}, {"text", iText}}, nullptr, nullptr);
	if (!result.ok) {
		OWL_WARN("Plugins: Importer '{}' failed: {}.", iId, result.error)
		return unexpected<std::string>{result.error};
	}
	PluginImport output{.path = textOf(result.fields, "path"), .text = textOf(result.fields, "text")};
	if (output.path.empty()) {
		OWL_WARN("Plugins: Importer '{}' returned no path.", iId)
		return unexpected<std::string>{"the importer returned no `path`"};
	}
	return output;
}

void PluginHost::addHostCommands() {
	m_registry.add({.name = "plugin.list",
					.description = "List the Owl Nest plugins loaded and what each kind adds.",
					.args = {},
					.factory = {},
					.run = [this](const commands::CommandContext&,
								  const commands::CommandArgs&) -> expected<CommandResult, CommandFailure> {
						std::vector<std::string> ids;
						int64_t errors = 0;
						for (const auto& plugin: m_plugins) {
							ids.push_back(plugin.id);
							errors += static_cast<int64_t>(plugin.errors.size());
						}
						CommandResult result{.entity = core::UUID{0},
											 .description = std::format("{} Owl Nest plugin(s)", ids.size()),
											 .data = {{"count", static_cast<int64_t>(ids.size())},
													  {"ids", join(ids)},
													  {"errors", errors}}};
						for (const auto kind: {PluginContributionKind::Command, PluginContributionKind::Panel,
											   PluginContributionKind::Drawer, PluginContributionKind::DocumentType,
											   PluginContributionKind::Importer}) {
							std::vector<std::string> names;
							for (const auto* contribution: getContributions(kind)) names.push_back(contribution->id);
							result.data.emplace(std::format("{}s", getContributionKindName(kind)), join(names));
						}
						return result;
					},
					.allowedInPlay = true,
					.needsScene = false});
	m_registry.add({.name = "plugin.reload",
					.description = "Load the Owl Nest plugins of the mounted packs again, after a change of a script.",
					.args = {},
					.factory = {},
					.run = [this](const commands::CommandContext&,
								  const commands::CommandArgs&) -> expected<CommandResult, CommandFailure> {
						if (mp_extensions == nullptr) {
							OWL_WARN("Plugins: Nothing to reload, no pack was loaded.")
							return fail(CommandError::NotApplicable, "no extension pack is mounted");
						}
						const auto loaded = load(*mp_extensions);
						return CommandResult{.entity = core::UUID{0},
											 .description = std::format("Reload {} Owl Nest plugin(s)", loaded),
											 .data = {{"count", static_cast<int64_t>(loaded)}}};
					},
					.allowedInPlay = false,
					.needsScene = false});
	m_registry.add({.name = "plugin.show_panel",
					.description = "Show or hide a panel of an Owl Nest plugin.",
					.args = {{.name = "id",
							  .type = commands::ArgType::Text,
							  .required = true,
							  .description = "Id of the panel."},
							 {.name = "visible",
							  .type = commands::ArgType::Bool,
							  .description = "True to show it, false to hide it.",
							  .defaultValue = true}},
					.factory = {},
					.run = [this](const commands::CommandContext&,
								  const commands::CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
						const auto id = iArgs.getText("id");
						const bool visible = iArgs.getBool("visible", true);
						if (!setPanelVisible(id, visible)) {
							OWL_WARN("Plugins: No panel '{}'.", id)
							return fail(CommandError::InvalidArgument, std::format("no plugin has a panel `{}`", id));
						}
						return CommandResult{.entity = core::UUID{0},
											 .description =
													 std::format("{} the panel `{}`", visible ? "Show" : "Hide", id),
											 .data = {{"id", id}, {"visible", visible}}};
					},
					.allowedInPlay = true,
					.needsScene = false});
	m_registry.add({.name = "plugin.document_set",
					.description = "Replace the text of a document of a plugin document type, open in a tab "
								   "(undoable in the tab).",
					.args = {{.name = "path",
							  .type = commands::ArgType::Text,
							  .required = true,
							  .description = "The file of the document.",
							  .projectPath = true},
							 {.name = "text",
							  .type = commands::ArgType::Text,
							  .required = true,
							  .description = "The whole text of the document."}},
					.factory = {},
					.run = [](const commands::CommandContext& iContext,
							  const commands::CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
						const auto path = resolveFile(iArgs.getText("path"), iContext.projectDirectory);
						auto* editor =
								iContext.documents != nullptr ? iContext.documents->openPluginDocument(path) : nullptr;
						if (editor == nullptr) {
							OWL_WARN("Plugins: Cannot open '{}' in a plugin document.", path.string())
							return fail(CommandError::NotApplicable,
										std::format("cannot open {} in a plugin document", path.string()));
						}
						auto description = std::format("Edit `{}`", path.filename().string());
						if (!editor->setText(iArgs.getText("text"), description)) {
							OWL_WARN("Plugins: '{}' is unchanged.", path.string())
							return fail(CommandError::NotApplicable, "the text is unchanged");
						}
						return CommandResult{.entity = core::UUID{0},
											 .description = std::move(description),
											 .data = {{"path", path.generic_string()}}};
					},
					.allowedInPlay = true,
					.needsScene = false});
	m_registry.add(
			{.name = "asset.import",
			 .description = "Turn a source file into an asset with an importer of an Owl Nest plugin (undoable).",
			 .args = {{.name = "path",
					   .type = commands::ArgType::Text,
					   .required = true,
					   .description = "The source file: absolute, or relative to the project.",
					   .projectPath = true},
					  {.name = "importer",
					   .type = commands::ArgType::Text,
					   .description = "Id of the importer (the first one taking the extension of the file when "
									  "absent)."}},
			 .factory = {},
			 .run = [this](const commands::CommandContext& iContext,
						   const commands::CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
				 const auto source = resolveFile(iArgs.getText("path"), iContext.projectDirectory);
				 std::error_code error;
				 if (!std::filesystem::is_regular_file(source, error)) {
					 OWL_WARN("Plugins: No file '{}' to import.", source.string())
					 return fail(CommandError::InvalidArgument, std::format("no file `{}`", source.string()));
				 }
				 const auto* importer = findImporter(source, iArgs.getText("importer"));
				 if (importer == nullptr) {
					 OWL_WARN("Plugins: No importer for '{}'.", source.string())
					 return fail(CommandError::InvalidArgument,
								 std::format("no plugin importer takes `{}`", source.filename().string()));
				 }
				 const std::string importerId = importer->id;
				 const auto imported = runImporter(importerId, source, platform::fileToString(source));
				 if (!imported)
					 return fail(CommandError::NotApplicable, imported.error());
				 const auto target = resolveFile(imported->path, source.parent_path());
				 if (target == source ||
					 (!iContext.projectDirectory.empty() && !isInside(target, iContext.projectDirectory))) {
					 OWL_WARN("Plugins: Importer '{}' may not write '{}'.", importerId, target.string())
					 return fail(CommandError::NotApplicable,
								 std::format("the importer may not write `{}`", target.string()));
				 }
				 std::optional<std::string> before;
				 if (std::filesystem::is_regular_file(target, error))
					 before = platform::fileToString(target);
				 std::filesystem::create_directories(target.parent_path(), error);
				 if (const auto written = platform::writeFileAtomic(target, imported->text); !written) {
					 OWL_WARN("Plugins: Cannot write '{}': {}.", target.string(), platform::describe(written.error()))
					 return fail(CommandError::NotApplicable, std::format("cannot write `{}`", target.string()));
				 }
				 iContext.undo->push(mkUniq<commands::WriteTextFileCommand>(target, std::move(before), imported->text));
				 return CommandResult{.entity = core::UUID{0},
									  .description = std::format("Import `{}` as `{}`", source.filename().string(),
																 target.filename().string()),
									  .data = {{"path", target.generic_string()}, {"importer", importerId}}};
			 },
			 .allowedInPlay = false,
			 .needsScene = true});
}

}// namespace owl::nest::session
