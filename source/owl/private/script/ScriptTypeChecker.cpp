/**
 * @file ScriptTypeChecker.cpp
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "script/ScriptTypeChecker.h"

#include "LuaBindings.h"
#include "core/external/luau_analysis.h"
#include "data/assets/AssetSearchPaths.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::script {

namespace {

// Name of the checked script in the frontend; its modules are the `require` names.
constexpr std::string_view g_mainModule = "@owl/script";

// Prefix of the declaration of a table that extends a table of the standard library (`math`, `table`).
constexpr std::string_view g_extensionPrefix = "__owl_";

// Globals the engine sets in every state, beside the tables of the registry.
constexpr std::string_view g_engineGlobals = R"(declare entity_id: number
declare _clicked_entity: number
declare _list_entity: number
declare _focused_entity: number
declare function tr(key: string, ...: any): string
declare _slider_value: number
declare _selected_index: number
declare _row_index: number
declare function load(chunk: string, chunkname: string?, mode: string?, env: { [any]: any }?): (any, string?)
declare function collectgarbage(option: string?): number
)";

auto typeName(const LuaType iType) -> std::string_view {
	switch (iType) {
		case LuaType::Boolean:
			return "boolean";
		case LuaType::Integer:
		case LuaType::Number:
		case LuaType::Entity:
			return "number";
		case LuaType::String:
			return "string";
		case LuaType::Table:
			return "{ [any]: any }";
		case LuaType::Any:
			return "any";
	}
	return "any";
}

auto functionType(const LuaBinding& iBinding) -> std::string {
	std::string params;
	for (const auto& param: iBinding.params) {
		if (!params.empty())
			params += ", ";
		params += std::format("{}: {}{}", param.name, typeName(param.type), param.optional ? "?" : "");
	}
	std::string returns;
	for (const auto& value: iBinding.returns) {
		if (!returns.empty())
			returns += ", ";
		returns += typeName(value.type);
	}
	return std::format("({}) -> ({})", params, returns);
}

// The tables of the registry that also exist in the standard library: their functions join the standard ones.
auto isStandardTable(const std::string_view iName) -> bool { return iName == "math" || iName == "table"; }

// The names a script declares in `properties = { { name = "..." }, ... }`, set as globals by the engine.
auto declaredProperties(const std::string_view iSource) -> std::set<std::string> {
	std::set<std::string> names;
	Luau::Allocator allocator;
	Luau::AstNameTable table{allocator};
	const auto result = Luau::Parser::parse(iSource.data(), iSource.size(), table, allocator);
	if (result.root == nullptr)
		return names;
	for (auto* statement: result.root->body) {
		const auto* assign = statement->as<Luau::AstStatAssign>();
		if (assign == nullptr || assign->vars.size != 1 || assign->values.size != 1)
			continue;
		const auto* global = assign->vars.data[0]->as<Luau::AstExprGlobal>();
		const auto* list = assign->values.data[0]->as<Luau::AstExprTable>();
		if (global == nullptr || list == nullptr || std::string_view{global->name.value} != "properties")
			continue;
		for (const auto& item: list->items) {
			const auto* entry = item.value->as<Luau::AstExprTable>();
			if (entry == nullptr)
				continue;
			if (const auto name = entry->getRecord("name"); name.has_value()) {
				if (const auto* text = (*name)->as<Luau::AstExprConstantString>(); text != nullptr)
					names.emplace(text->value.data, text->value.size);
			}
		}
	}
	return names;
}

// The script being checked, and the modules it requires read as the sandbox reads them (`a.b` is `a/b.lua`).
class SourceResolver final : public Luau::FileResolver {
public:
	auto readSource(const Luau::ModuleName& iName) -> std::optional<Luau::SourceCode> override {
		if (iName == g_mainModule)
			return Luau::SourceCode{.source = source, .type = Luau::SourceCode::Script};
		if (auto text = data::assets::readAssetText(iName); text.has_value())
			return Luau::SourceCode{.source = std::move(*text), .type = Luau::SourceCode::Module};
		for (const auto& folder: folders) {
			if (auto text = data::assets::readAssetText(folder / iName); text.has_value())
				return Luau::SourceCode{.source = std::move(*text), .type = Luau::SourceCode::Module};
		}
		return std::nullopt;
	}

	auto resolveModule([[maybe_unused]] const Luau::ModuleInfo* iContext, Luau::AstExpr* iExpr,
					   [[maybe_unused]] const Luau::TypeCheckLimits& iLimits)
			-> std::optional<Luau::ModuleInfo> override {
		const auto* text = iExpr->as<Luau::AstExprConstantString>();
		if (text == nullptr)
			return std::nullopt;
		std::string path{text->value.data, text->value.size};
		if (path.empty() ||
			path.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.") !=
					std::string::npos)
			return std::nullopt;
		std::ranges::replace(path, '.', '/');
		return Luau::ModuleInfo{.name = path + ".lua", .optional = false};
	}

	[[nodiscard]] auto getHumanReadableModuleName(const Luau::ModuleName& iName) const -> std::string override {
		return iName == g_mainModule ? chunkName : iName;
	}

	// Text of the checked script.
	std::string source;
	// Name of the checked script in the messages.
	std::string chunkName;
	// Folders searched for the required modules after the asset folders.
	std::vector<std::filesystem::path> folders;
};

// Levenshtein distance, for the near misses of a global name.
auto editDistance(const std::string_view iA, const std::string_view iB) -> size_t {
	std::vector<size_t> row(iB.size() + 1);
	for (size_t j = 0; j <= iB.size(); ++j) row[j] = j;
	for (size_t i = 1; i <= iA.size(); ++i) {
		size_t diagonal = row[0];
		row[0] = i;
		for (size_t j = 1; j <= iB.size(); ++j) {
			const size_t above = row[j];
			row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (iA[i - 1] == iB[j - 1] ? 0 : 1)});
			diagonal = above;
		}
	}
	return row[iB.size()];
}

auto lintingOptions() -> Luau::FrontendOptions {
	Luau::FrontendOptions options;
	options.runLintChecks = true;
	return options;
}

auto toDiagnostic(const Luau::Location& iLocation, std::string iMessage, const ScriptDiagnostic::Severity iSeverity)
		-> ScriptDiagnostic {
	return {.severity = iSeverity,
			.line = iLocation.begin.line + 1,
			.column = iLocation.begin.column + 1,
			.endLine = iLocation.end.line + 1,
			.endColumn = iLocation.end.column + 1,
			.message = std::move(iMessage)};
}

}// namespace

auto getScriptDefinitions() -> std::string {
	std::map<std::string_view, std::vector<const LuaBinding*>> tables;
	for (const auto& binding: getLuaBindings()) tables[binding.table].push_back(&binding);
	std::string out{g_engineGlobals};
	for (const auto& [table, bindings]: tables) {
		out += std::format("declare {}{}: {{\n", isStandardTable(table) ? g_extensionPrefix : "", table);
		for (const auto* binding: bindings) out += std::format("\t{}: {},\n", binding->name, functionType(*binding));
		out += "}\n";
	}
	return out;
}

struct ScriptTypeChecker::Impl {
	SourceResolver resolver;
	Luau::NullConfigResolver config;
	Luau::Frontend frontend{Luau::SolverMode::Old, &resolver, &config, lintingOptions()};
	bool ready = false;
	// Table types of the globals (engine and standard library): a missing key there is a real error.
	std::set<Luau::TypeId> declaredTables;
	// Names of the declared globals, to tell a typo from a global another script sets.
	std::vector<std::string> globalNames;

	Impl() {
		auto& globals = frontend.globals;
		Luau::registerBuiltinGlobals(frontend, globals);
		Luau::unfreeze(globals.globalTypes);
		const auto definitions = getScriptDefinitions();
		const auto loaded = frontend.loadDefinitionFile(globals, globals.globalScope, definitions, "@owl", false);
		if (!loaded.success) {
			for (const auto& error: loaded.parseResult.errors)
				OWL_CORE_ERROR("ScriptTypeChecker: Engine declarations line {}: {}.",
							   error.getLocation().begin.line + 1, error.getMessage())
			for (const auto& error: loaded.module ? loaded.module->errors : std::vector<Luau::TypeError>{})
				OWL_CORE_ERROR("ScriptTypeChecker: Engine declarations: {}.", Luau::toString(error))
			Luau::freeze(globals.globalTypes);
			return;
		}
		mergeStandardExtensions();
		Luau::freeze(globals.globalTypes);
		for (const auto& [symbol, binding]: globals.globalScope->bindings) {
			if (symbol.global.value == nullptr)
				continue;
			globalNames.emplace_back(symbol.global.value);
			if (const auto* const type = Luau::follow(binding.typeId); Luau::get<Luau::TableType>(type) != nullptr)
				declaredTables.insert(type);
		}
		ready = true;
	}

	// How a type error of the main script is reported: dropped, a warning or an error.
	[[nodiscard]] auto severityOf(const Luau::TypeError& iError, const std::set<std::string>& iProperties) const
			-> std::optional<ScriptDiagnostic::Severity> {
		if (const auto* unknown = Luau::get_if<Luau::UnknownSymbol>(&iError.data); unknown != nullptr) {
			if (iProperties.contains(unknown->name))
				return std::nullopt;
			// A global another script or the requiring script sets is legal: only a near miss of a known one is an error.
			return isNearMiss(unknown->name) ? ScriptDiagnostic::Severity::Error : ScriptDiagnostic::Severity::Warning;
		}
		if (const auto* property = Luau::get_if<Luau::UnknownProperty>(&iError.data); property != nullptr)
			return declaredTables.contains(Luau::follow(property->table))
						   ? std::optional{ScriptDiagnostic::Severity::Error}
						   : std::nullopt;
		return ScriptDiagnostic::Severity::Error;
	}

	[[nodiscard]] auto isNearMiss(const std::string_view iName) const -> bool {
		return std::ranges::any_of(globalNames, [iName](const std::string& iKnown) -> bool {
			return iKnown != iName && editDistance(iKnown, iName) <= 2 && iKnown.size() > 3;
		});
	}

	// Moves the functions of `__owl_math` into the standard `math` table type, and the same for `table`.
	void mergeStandardExtensions() {
		auto& globals = frontend.globals;
		auto& names = *globals.globalNames.names;
		for (const std::string_view table: {"math", "table"}) {
			const auto extension = std::format("{}{}", g_extensionPrefix, table);
			const Luau::Symbol standardSymbol{names.getOrAdd(std::string{table}.c_str())};
			const Luau::Symbol extensionSymbol{names.getOrAdd(extension.c_str())};
			auto& bindings = globals.globalScope->bindings;
			const auto standard = bindings.find(standardSymbol);
			const auto added = bindings.find(extensionSymbol);
			if (standard == bindings.end() || added == bindings.end())
				continue;
			auto* target = Luau::getMutable<Luau::TableType>(Luau::follow(standard->second.typeId));
			const auto* source = Luau::get<Luau::TableType>(Luau::follow(added->second.typeId));
			if (target != nullptr && source != nullptr) {
				for (const auto& [name, property]: source->props) target->props[name] = property;
			}
			bindings.erase(added);
		}
	}
};

ScriptTypeChecker::ScriptTypeChecker() : mp_impl{mkUniq<Impl>()} {}

ScriptTypeChecker::~ScriptTypeChecker() = default;

auto ScriptTypeChecker::isReady() const -> bool { return mp_impl->ready; }

void ScriptTypeChecker::addModuleFolder(const std::filesystem::path& iFolder) {
	mp_impl->resolver.folders.push_back(iFolder);
	mp_impl->frontend.clear();
}

auto ScriptTypeChecker::check(const std::string_view iSource, const std::string_view iChunkName)
		-> std::vector<ScriptDiagnostic> {
	OWL_PROFILE_FUNCTION()

	auto& impl = *mp_impl;
	impl.resolver.source = std::string{iSource};
	impl.resolver.chunkName = std::string{iChunkName};
	impl.frontend.markDirty(std::string{g_mainModule});
	const auto result = impl.frontend.check(std::string{g_mainModule});
	const auto properties = declaredProperties(iSource);
	std::vector<ScriptDiagnostic> diagnostics;
	for (const auto& error: result.errors) {
		if (error.moduleName != g_mainModule)
			continue;
		const auto severity = impl.severityOf(error, properties);
		if (!severity.has_value())
			continue;
		Luau::TypeErrorToStringOptions options;
		options.fileResolver = &impl.resolver;
		diagnostics.push_back(toDiagnostic(error.location, Luau::toString(error, options), *severity));
	}
	for (const auto& lint: result.lintResult.errors)
		diagnostics.push_back(toDiagnostic(lint.location, lint.text, ScriptDiagnostic::Severity::Error));
	for (const auto& lint: result.lintResult.warnings)
		diagnostics.push_back(toDiagnostic(lint.location, lint.text, ScriptDiagnostic::Severity::Warning));
	std::ranges::sort(diagnostics, [](const ScriptDiagnostic& iA, const ScriptDiagnostic& iB) -> bool {
		return std::tie(iA.line, iA.column) < std::tie(iB.line, iB.column);
	});
	return diagnostics;
}

}// namespace owl::script
