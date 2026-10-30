/**
 * @file DataTableCommands.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "DataTableCommands.h"

#include "CommandRegistry.h"
#include "DocumentCommands.h"


#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest::commands {

DataTableEditor::~DataTableEditor() = default;

namespace {

using data::table::Column;
using data::table::ColumnType;
using data::table::DataTable;

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto unchanged() -> unexpected<CommandFailure> { return fail(CommandError::NotApplicable, "nothing changes"); }

auto trimmed(std::string_view iText) -> std::string_view {
	while (!iText.empty() && iText.front() == ' ') iText.remove_prefix(1);
	while (!iText.empty() && iText.back() == ' ') iText.remove_suffix(1);
	return iText;
}

auto splitList(const std::string_view iText) -> std::vector<std::string> {
	std::vector<std::string> items;
	for (const auto part: iText | std::views::split(',')) {
		if (const auto item = trimmed({part.begin(), part.end()}); !item.empty())
			items.emplace_back(item);
	}
	return items;
}

// Find the document of `path`, then edit it; a function pointer fits std::function's buffer.
using TableEdit = auto (*)(DataTableEditor&, const CommandArgs&) -> expected<std::string, CommandFailure>;

auto tableRunner(TableEdit iEdit) -> CommandSpec::Runner {
	return [edit = iEdit](const CommandContext& iContext,
						  const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		const std::filesystem::path path{iArgs.getText("path")};
		if (!DataTable::isTable(path))
			return fail(CommandError::InvalidArgument, "`path` must end with `.owltable`");
		auto* editor = iContext.documents != nullptr ? iContext.documents->openDataTable(path) : nullptr;
		if (editor == nullptr)
			return fail(CommandError::NotApplicable, std::format("cannot open {} in an editor tab", path.string()));
		auto description = edit(*editor, iArgs);
		if (!description)
			return unexpected<CommandFailure>{description.error()};
		return CommandResult{.entity = core::UUID{0},
							 .description = std::move(*description),
							 .data = {{"path", path.generic_string()}}};
	};
}

auto rowIndex(const DataTableEditor& iEditor, const CommandArgs& iArgs, const std::string& iName)
		-> expected<size_t, CommandFailure> {
	const auto key = iArgs.getText(iName);
	if (const auto index = iEditor.table().findRow(key); index)
		return *index;
	return fail(CommandError::InvalidArgument, std::format("no row `{}`", key));
}

auto rowIndices(const DataTableEditor& iEditor, const CommandArgs& iArgs)
		-> expected<std::vector<size_t>, CommandFailure> {
	std::vector<size_t> indices;
	for (const auto& key: splitList(iArgs.getText("keys"))) {
		const auto index = iEditor.table().findRow(key);
		if (!index)
			return fail(CommandError::InvalidArgument, std::format("no row `{}`", key));
		indices.push_back(*index);
	}
	if (indices.empty())
		return fail(CommandError::InvalidArgument, "`keys` names no row");
	return indices;
}

auto columnIndex(const DataTableEditor& iEditor, const CommandArgs& iArgs) -> expected<size_t, CommandFailure> {
	const auto name = iArgs.getText("column");
	if (const auto index = iEditor.table().findColumn(name); index)
		return *index;
	return fail(CommandError::InvalidArgument, std::format("no column `{}`", name));
}

auto columnTypes() -> std::vector<std::string> {
	std::vector<std::string> names;
	for (const auto type: data::table::getColumnTypes()) names.emplace_back(data::table::getColumnTypeName(type));
	return names;
}

// The column described by the arguments, starting from `iBase`.
auto columnFrom(Column iBase, const CommandArgs& iArgs) -> Column {
	iBase.name = iArgs.getText("name", iBase.name);
	if (iArgs.has("type")) {
		if (const auto type = data::table::parseColumnType(iArgs.getText("type")); type)
			iBase.type = *type;
	}
	if (iArgs.has("choices"))
		iBase.choices = splitList(iArgs.getText("choices"));
	if (iArgs.has("table"))
		iBase.table = iArgs.getText("table");
	return iBase;
}

auto addRow(DataTableEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	size_t at = ioEditor.table().rows.size();
	if (iArgs.has("after")) {
		const auto after = rowIndex(ioEditor, iArgs, "after");
		if (!after)
			return unexpected<CommandFailure>{after.error()};
		at = *after + 1;
	}
	if (!ioEditor.addRow(iArgs.getText("key"), at))
		return unchanged();
	return "Add row";
}

auto removeRows(DataTableEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	const auto indices = rowIndices(ioEditor, iArgs);
	if (!indices)
		return unexpected<CommandFailure>{indices.error()};
	if (!ioEditor.removeRows(*indices))
		return unchanged();
	return indices->size() > 1 ? "Remove rows" : "Remove row";
}

auto duplicateRows(DataTableEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	const auto indices = rowIndices(ioEditor, iArgs);
	if (!indices)
		return unexpected<CommandFailure>{indices.error()};
	if (!ioEditor.duplicateRows(*indices))
		return unchanged();
	return "Duplicate rows";
}

auto moveRows(DataTableEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	const auto indices = rowIndices(ioEditor, iArgs);
	if (!indices)
		return unexpected<CommandFailure>{indices.error()};
	if (!ioEditor.moveRows(*indices, iArgs.getBool("down")))
		return unchanged();
	return "Move rows";
}

auto renameRow(DataTableEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	const auto index = rowIndex(ioEditor, iArgs, "key");
	if (!index)
		return unexpected<CommandFailure>{index.error()};
	if (!ioEditor.setRowKey(*index, iArgs.getText("new_key")))
		return fail(CommandError::InvalidArgument, "`new_key` is empty, taken or the same");
	return "Rename row";
}

auto setCell(DataTableEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	const auto row = rowIndex(ioEditor, iArgs, "key");
	if (!row)
		return unexpected<CommandFailure>{row.error()};
	const auto column = columnIndex(ioEditor, iArgs);
	if (!column)
		return unexpected<CommandFailure>{column.error()};
	const auto& definition = ioEditor.table().columns[*column];
	const auto value = data::table::parseValue(iArgs.getText("value"), definition);
	if (!value)
		return fail(CommandError::InvalidArgument,
					std::format("`{}` does not fit the {} column `{}`", iArgs.getText("value"),
								data::table::getColumnTypeName(definition.type), definition.name));
	if (!ioEditor.setCell(*row, *column, *value))
		return unchanged();
	return "Edit cell";
}

auto addColumn(DataTableEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	size_t at = ioEditor.table().columns.size();
	if (iArgs.has("after")) {
		const auto name = iArgs.getText("after");
		const auto after = ioEditor.table().findColumn(name);
		if (!after)
			return fail(CommandError::InvalidArgument, std::format("no column `{}`", name));
		at = *after + 1;
	}
	const auto column = columnFrom({}, iArgs);
	if (!ioEditor.addColumn(column, at))
		return fail(CommandError::InvalidArgument, std::format("`{}` is empty, `key` or taken", column.name));
	return "Add column";
}

auto removeColumn(DataTableEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	const auto column = columnIndex(ioEditor, iArgs);
	if (!column)
		return unexpected<CommandFailure>{column.error()};
	if (!ioEditor.removeColumn(*column))
		return unchanged();
	return "Remove column";
}

auto moveColumn(DataTableEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	const auto column = columnIndex(ioEditor, iArgs);
	if (!column)
		return unexpected<CommandFailure>{column.error()};
	const bool right = iArgs.getBool("right");
	if ((!right && *column == 0) || (right && *column + 1 >= ioEditor.table().columns.size()))
		return unchanged();
	if (!ioEditor.moveColumn(*column, right ? *column + 1 : *column - 1))
		return unchanged();
	return "Move column";
}

auto setColumn(DataTableEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	const auto column = columnIndex(ioEditor, iArgs);
	if (!column)
		return unexpected<CommandFailure>{column.error()};
	const auto& before = ioEditor.table().columns[*column];
	const auto after = columnFrom(before, iArgs);
	if (after == before)
		return unchanged();
	if (!ioEditor.setColumn(*column, after))
		return fail(CommandError::InvalidArgument, std::format("`{}` is empty, `key` or taken", after.name));
	return "Edit column";
}

auto createTable(const CommandContext& /*iContext*/, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const std::filesystem::path path{iArgs.getText("path")};
	if (!DataTable::isTable(path))
		return fail(CommandError::InvalidArgument, "`path` must end with `.owltable`");
	if (exists(path))
		return fail(CommandError::InvalidArgument, std::format("{} exists already", path.string()));
	DataTable table;
	std::ignore = table.addColumn({.name = "name", .type = ColumnType::Text, .choices = {}, .table = {}}, 0);
	std::ignore = table.addRow("row", 0);
	if (!table.saveToFile(path))
		return fail(CommandError::NotApplicable, std::format("cannot write {}", path.string()));
	return CommandResult{.entity = core::UUID{0},
						 .description = "Create data table",
						 .data = {{"path", path.generic_string()}}};
}

auto pathSpec() -> ArgSpec {
	return {.name = "path",
			.type = ArgType::Text,
			.required = true,
			.description = "The `.owltable` file, opened in a tab when it is not.",
			.projectPath = true};
}

auto text(const char* iName, const char* iDescription, const bool iRequired = false) -> ArgSpec {
	return {.name = iName, .type = ArgType::Text, .required = iRequired, .description = iDescription};
}

auto typeSpec(const bool iRequired) -> ArgSpec {
	return {.name = "type",
			.type = ArgType::Enum,
			.required = iRequired,
			.description = "Type of the cells.",
			.choices = columnTypes()};
}

}// namespace

void addDataTableCommands(CommandRegistry& ioRegistry) {
	const auto add = [&ioRegistry](const char* iName, const char* iDescription, std::vector<ArgSpec> iArgs,
								   TableEdit iEdit) -> void {
		iArgs.insert(iArgs.begin(), pathSpec());
		ioRegistry.add({.name = iName,
						.description = iDescription,
						.args = std::move(iArgs),
						.factory = {},
						.run = tableRunner(iEdit),
						.allowedInPlay = true,
						.needsScene = false});
	};
	ioRegistry.add({.name = "table.create",
					.description = "Create a `.owltable` data table with a text column and a row.",
					.args = {{.name = "path",
							  .type = ArgType::Text,
							  .required = true,
							  .description = "Data table file to write (`.owltable`, must not exist).",
							  .projectPath = true}},
					.factory = {},
					.run = createTable,
					.allowedInPlay = true,
					.needsScene = false});
	add("table.add_row", "Insert a row of default cells into a data table.",
		{text("key", "Its key; a number is added when it is taken.", true),
		 text("after", "Key of the row it follows (appended when absent).")},
		addRow);
	add("table.remove_rows", "Remove rows of a data table.", {text("keys", "Keys of the rows (`a,b`).", true)},
		removeRows);
	add("table.duplicate_rows", "Copy rows of a data table after the last of them, under unique keys.",
		{text("keys", "Keys of the rows (`a,b`).", true)}, duplicateRows);
	add("table.move_rows", "Move rows of a data table one place up or down.",
		{text("keys", "Keys of the rows (`a,b`).", true),
		 {.name = "down",
		  .type = ArgType::Bool,
		  .description = "Move down instead of up.",
		  .defaultValue = ArgValue{false}}},
		moveRows);
	add("table.rename_row", "Rename the key of a row of a data table.",
		{text("key", "The key.", true), text("new_key", "The new key, not taken.", true)}, renameRow);
	add("table.set_cell", "Set a cell of a data table, written as text (`true`, `12`, `1.5`, a choice, a path, a key).",
		{text("key", "Key of the row.", true), text("column", "Name of the column.", true),
		 text("value", "The value.", true)},
		setCell);
	add("table.add_column", "Add a column of default cells to a data table.",
		{text("name", "Its name, not taken and not `key`.", true), typeSpec(true),
		 text("choices", "Choices of an Enum column (`a,b`)."),
		 text("table", "Asset path of the table a Reference column points into (empty: this table)."),
		 text("after", "Name of the column it follows (appended when absent).")},
		addColumn);
	add("table.remove_column", "Remove a column of a data table and its cells.",
		{text("column", "Name of the column.", true)}, removeColumn);
	add("table.move_column", "Move a column of a data table one place left or right.",
		{text("column", "Name of the column.", true),
		 {.name = "right",
		  .type = ArgType::Bool,
		  .description = "Move right instead of left.",
		  .defaultValue = ArgValue{false}}},
		moveColumn);
	add("table.set_column",
		"Rename a column of a data table or change its type (the cells are converted), its choices or its table.",
		{text("column", "Name of the column.", true), text("name", "Its new name."), typeSpec(false),
		 text("choices", "Choices of an Enum column (`a,b`)."),
		 text("table", "Asset path of the table a Reference column points into (empty: this table).")},
		setColumn);
}

}// namespace owl::nest::commands
