/**
 * @file HudThemeCommands.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "HudThemeCommands.h"

#include "CommandRegistry.h"
#include "DocumentCommands.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace owl::nest::commands {

HudThemeEditor::~HudThemeEditor() = default;

namespace {

using scene::HudStyle;
using scene::HudTheme;

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto unchanged() -> unexpected<CommandFailure> { return fail(CommandError::NotApplicable, "nothing changes"); }

// The theme after the edit, and the name of the step.
struct ThemeEdit {
	HudTheme theme;
	std::string description;
};

// Compute an edit of the theme; a function pointer fits std::function's buffer.
using ThemeChange = auto (*)(const HudTheme&, const CommandArgs&) -> expected<ThemeEdit, CommandFailure>;

auto themeRunner(ThemeChange iChange) -> CommandSpec::Runner {
	return [change = iChange](const CommandContext& iContext,
							  const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		const std::filesystem::path path{iArgs.getText("path")};
		if (!HudTheme::isTheme(path))
			return fail(CommandError::InvalidArgument, "`path` must end with `.owlhudtheme`");
		auto* editor = iContext.documents != nullptr ? iContext.documents->openHudTheme(path) : nullptr;
		if (editor == nullptr)
			return fail(CommandError::NotApplicable, std::format("cannot open {} in an editor tab", path.string()));
		auto edit = change(editor->theme(), iArgs);
		if (!edit)
			return unexpected<CommandFailure>{edit.error()};
		if (!editor->setTheme(edit->theme, edit->description))
			return unchanged();
		return CommandResult{.entity = core::UUID{0},
							 .description = std::move(edit->description),
							 .data = {{"path", path.generic_string()}}};
	};
}

auto styleIndex(const HudTheme& iTheme, const std::string& iName) -> expected<size_t, CommandFailure> {
	if (const auto index = iTheme.findStyleIndex(iName); index)
		return *index;
	return fail(CommandError::InvalidArgument, std::format("no style `{}`", iName));
}

// A name not taken yet: the name itself, else with a number.
auto freeName(const HudTheme& iTheme, const std::string& iName) -> std::string {
	std::string name = iName.empty() ? std::string{"style"} : iName;
	for (uint32_t counter = 2; iTheme.findStyle(name) != nullptr; ++counter)
		name = std::format("{} {}", iName.empty() ? "style" : iName, counter);
	return name;
}

auto addStyle(const HudTheme& iTheme, const CommandArgs& iArgs) -> expected<ThemeEdit, CommandFailure> {
	ThemeEdit edit{.theme = iTheme, .description = "Add style"};
	HudStyle style;
	if (iArgs.has("from")) {
		const auto from = styleIndex(iTheme, iArgs.getText("from"));
		if (!from)
			return unexpected<CommandFailure>{from.error()};
		style = iTheme.styles[*from];
		edit.description = "Duplicate style";
	}
	style.name = freeName(iTheme, iArgs.getText("name", style.name));
	edit.theme.styles.push_back(std::move(style));
	return edit;
}

auto removeStyle(const HudTheme& iTheme, const CommandArgs& iArgs) -> expected<ThemeEdit, CommandFailure> {
	const auto index = styleIndex(iTheme, iArgs.getText("name"));
	if (!index)
		return unexpected<CommandFailure>{index.error()};
	ThemeEdit edit{.theme = iTheme, .description = "Remove style"};
	edit.theme.styles.erase(edit.theme.styles.begin() + static_cast<std::ptrdiff_t>(*index));
	return edit;
}

auto renameStyle(const HudTheme& iTheme, const CommandArgs& iArgs) -> expected<ThemeEdit, CommandFailure> {
	const auto index = styleIndex(iTheme, iArgs.getText("name"));
	if (!index)
		return unexpected<CommandFailure>{index.error()};
	const auto newName = iArgs.getText("new_name");
	if (newName.empty() || iTheme.findStyle(newName) != nullptr)
		return fail(CommandError::InvalidArgument, "`new_name` is empty or taken");
	ThemeEdit edit{.theme = iTheme, .description = "Rename style"};
	edit.theme.styles[*index].name = newName;
	return edit;
}

auto moveStyle(const HudTheme& iTheme, const CommandArgs& iArgs) -> expected<ThemeEdit, CommandFailure> {
	const auto index = styleIndex(iTheme, iArgs.getText("name"));
	if (!index)
		return unexpected<CommandFailure>{index.error()};
	const bool down = iArgs.getBool("down");
	if ((down && *index + 1 >= iTheme.styles.size()) || (!down && *index == 0))
		return unchanged();
	ThemeEdit edit{.theme = iTheme, .description = "Move style"};
	std::swap(edit.theme.styles[*index], edit.theme.styles[down ? *index + 1 : *index - 1]);
	return edit;
}

auto setStyle(const HudTheme& iTheme, const CommandArgs& iArgs) -> expected<ThemeEdit, CommandFailure> {
	const auto index = styleIndex(iTheme, iArgs.getText("name"));
	if (!index)
		return unexpected<CommandFailure>{index.error()};
	auto style = HudTheme::deserializeStyle(iArgs.getText("style"));
	if (!style)
		return fail(CommandError::InvalidArgument, "`style` is not a YAML style map");
	style->name = iTheme.styles[*index].name;
	ThemeEdit edit{.theme = iTheme, .description = "Edit style"};
	edit.theme.styles[*index] = std::move(*style);
	return edit;
}

auto setName(const HudTheme& iTheme, const CommandArgs& iArgs) -> expected<ThemeEdit, CommandFailure> {
	ThemeEdit edit{.theme = iTheme, .description = "Rename theme"};
	edit.theme.name = iArgs.getText("theme_name");
	return edit;
}

auto createTheme(const CommandContext& /*iContext*/, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const std::filesystem::path path{iArgs.getText("path")};
	if (!HudTheme::isTheme(path))
		return fail(CommandError::InvalidArgument, "`path` must end with `.owlhudtheme`");
	if (exists(path))
		return fail(CommandError::InvalidArgument, std::format("{} exists already", path.string()));
	auto theme = HudTheme::makeDefault();
	theme.name = path.stem().string();
	if (!theme.saveToFile(path))
		return fail(CommandError::NotApplicable, std::format("cannot write {}", path.string()));
	return CommandResult{.entity = core::UUID{0},
						 .description = "Create HUD theme",
						 .data = {{"path", path.generic_string()}}};
}

auto text(const char* iName, const char* iDescription, const bool iRequired = false) -> ArgSpec {
	return {.name = iName, .type = ArgType::Text, .required = iRequired, .description = iDescription};
}

}// namespace

void addHudThemeCommands(CommandRegistry& ioRegistry) {
	const auto add = [&ioRegistry](const char* iName, const char* iDescription, std::vector<ArgSpec> iArgs,
								   ThemeChange iChange) -> void {
		iArgs.insert(iArgs.begin(), {.name = "path",
									 .type = ArgType::Text,
									 .required = true,
									 .description = "The `.owlhudtheme` file, opened in a tab when it is not.",
									 .projectPath = true});
		ioRegistry.add({.name = iName,
						.description = iDescription,
						.args = std::move(iArgs),
						.factory = {},
						.run = themeRunner(iChange),
						.allowedInPlay = true,
						.needsScene = false});
	};
	ioRegistry.add({.name = "theme.create",
					.description = "Create a `.owlhudtheme` HUD theme with a starting set of styles (panel, button, "
								   "title, text, slider, bar, list, window).",
					.args = {{.name = "path",
							  .type = ArgType::Text,
							  .required = true,
							  .description = "Theme file to write (`.owlhudtheme`, must not exist).",
							  .projectPath = true}},
					.factory = {},
					.run = createTheme,
					.allowedInPlay = true,
					.needsScene = false});
	add("theme.add_style", "Append a style to a HUD theme, empty or a copy of another one.",
		{text("name", "Its name; a number is added when it is taken."),
		 text("from", "Style copied (an empty style when absent).")},
		addStyle);
	add("theme.remove_style", "Remove a style of a HUD theme.", {text("name", "The style.", true)}, removeStyle);
	add("theme.rename_style", "Rename a style of a HUD theme (the widgets naming the old name lose it).",
		{text("name", "The style.", true), text("new_name", "The new name, not taken.", true)}, renameStyle);
	add("theme.move_style", "Move a style of a HUD theme one place up or down.",
		{text("name", "The style.", true),
		 {.name = "down",
		  .type = ArgType::Bool,
		  .description = "Move down instead of up.",
		  .defaultValue = ArgValue{false}}},
		moveStyle);
	add("theme.set_style", "Replace the properties of a style of a HUD theme (its name is kept).",
		{text("name", "The style.", true),
		 text("style",
			  "YAML map of the properties the style sets, as in the file (`{background: [0.2, 0.2, 0.3, 1], "
			  "cornerRadius: 8}`).",
			  true)},
		setStyle);
	add("theme.set_name", "Rename a HUD theme.", {text("theme_name", "The new name.", true)}, setName);
}

}// namespace owl::nest::commands
