/**
 * @file DocumentCommands.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "DocumentCommands.h"

#include "CommandRegistry.h"

#include <charconv>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest::commands {

SoundCueEditor::~SoundCueEditor() = default;

LocalisationEditor::~LocalisationEditor() = default;

DocumentHost::~DocumentHost() = default;

namespace {

auto trimmed(std::string_view iText) -> std::string_view {
	while (!iText.empty() && iText.front() == ' ') iText.remove_prefix(1);
	while (!iText.empty() && iText.back() == ' ') iText.remove_suffix(1);
	return iText;
}

}// namespace

auto parseIndices(const std::string_view iText) -> std::optional<std::vector<size_t>> {
	std::vector<size_t> indices;
	for (const auto part: iText | std::views::split(',')) {
		const auto item = trimmed({part.begin(), part.end()});
		if (item.empty())
			continue;
		size_t value = 0;
		if (const auto [end, error] = std::from_chars(item.data(), item.data() + item.size(), value);
			error != std::errc{} || end != item.data() + item.size())
			return std::nullopt;
		indices.push_back(value);
	}
	return indices;
}

auto formatIndices(const std::span<const size_t> iIndices) -> std::string {
	std::string text;
	for (const auto index: iIndices) text += text.empty() ? std::format("{}", index) : std::format(",{}", index);
	return text;
}

namespace {

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto getNumber(const CommandArgs& iArgs, const std::string& iName) -> std::optional<float> {
	if (const auto value = iArgs.get<double>(iName); value)
		return static_cast<float>(*value);
	if (const auto value = iArgs.get<int64_t>(iName); value)
		return static_cast<float>(*value);
	return std::nullopt;
}

auto done(const std::filesystem::path& iPath, std::string iDescription) -> CommandResult {
	return {.entity = core::UUID{0},
			.description = std::move(iDescription),
			.data = {{"path", iPath.generic_string()}}};
}

// Find the document of `path`, then edit it; a function pointer fits std::function's buffer (false NewDeleteLeaks).
using SoundEdit = auto (*)(SoundCueEditor&, const CommandArgs&) -> expected<std::string, CommandFailure>;
using LocaleEdit = auto (*)(LocalisationEditor&, const CommandArgs&) -> expected<std::string, CommandFailure>;

auto soundRunner(SoundEdit iEdit) -> CommandSpec::Runner {
	return [edit = iEdit](const CommandContext& iContext,
						  const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		const std::filesystem::path path{iArgs.getText("path")};
		if (path.extension() != sound::SoundCue::fileExtension())
			return fail(CommandError::InvalidArgument, "`path` must end with `.owlsound`");
		auto* editor = iContext.documents != nullptr ? iContext.documents->openSoundCue(path) : nullptr;
		if (editor == nullptr)
			return fail(CommandError::NotApplicable, std::format("cannot open {} in an editor tab", path.string()));
		auto description = edit(*editor, iArgs);
		if (!description)
			return unexpected<CommandFailure>{description.error()};
		return done(path, std::move(*description));
	};
}

auto localeRunner(LocaleEdit iEdit) -> CommandSpec::Runner {
	return [edit = iEdit](const CommandContext& iContext,
						  const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		const std::filesystem::path path{iArgs.getText("path")};
		if (path.filename() != data::localisation::Localisation::fileName)
			return fail(CommandError::InvalidArgument, "`path` must be a `localisation.yml`");
		auto* editor = iContext.documents != nullptr ? iContext.documents->openLocalisation(path) : nullptr;
		if (editor == nullptr)
			return fail(CommandError::NotApplicable, std::format("cannot open {} in an editor tab", path.string()));
		auto description = edit(*editor, iArgs);
		if (!description)
			return unexpected<CommandFailure>{description.error()};
		return done(path, std::move(*description));
	};
}

auto unchanged() -> unexpected<CommandFailure> { return fail(CommandError::NotApplicable, "nothing changes"); }

auto takeIndices(const SoundCueEditor& iEditor, const CommandArgs& iArgs)
		-> expected<std::vector<size_t>, CommandFailure> {
	const auto indices = parseIndices(iArgs.getText("takes"));
	if (!indices)
		return fail(CommandError::InvalidArgument, "`takes` must be indices separated by commas");
	for (const auto index: *indices) {
		if (index >= iEditor.cue().variants.size())
			return fail(CommandError::InvalidArgument, std::format("no take {}", index));
	}
	return *indices;
}

auto stringIndex(const LocalisationEditor& iEditor, const std::string_view iKey) -> std::optional<size_t> {
	const auto& strings = iEditor.localisation().strings;
	for (size_t i = 0; i < strings.size(); ++i) {
		if (strings[i].key == iKey)
			return i;
	}
	return std::nullopt;
}

auto stringIndices(const LocalisationEditor& iEditor, const CommandArgs& iArgs)
		-> expected<std::vector<size_t>, CommandFailure> {
	std::vector<size_t> indices;
	for (const auto part: iArgs.getText("keys") | std::views::split(',')) {
		const auto key = trimmed({part.begin(), part.end()});
		if (key.empty())
			continue;
		const auto index = stringIndex(iEditor, key);
		if (!index)
			return fail(CommandError::InvalidArgument, std::format("no string `{}`", key));
		indices.push_back(*index);
	}
	return indices;
}

auto setCue(SoundCueEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	auto next = ioEditor.cue();
	next.volume = getNumber(iArgs, "volume").value_or(next.volume);
	next.volumeJitter = getNumber(iArgs, "volume_change").value_or(next.volumeJitter);
	next.pitch = getNumber(iArgs, "pitch").value_or(next.pitch);
	next.pitchJitter = getNumber(iArgs, "pitch_change").value_or(next.pitchJitter);
	next.loop = iArgs.get<bool>("loop").value_or(next.loop);
	if (!ioEditor.setProperties(next))
		return unchanged();
	return "Edit sound";
}

auto setTake(SoundCueEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	const auto index = iArgs.get<int64_t>("take").value_or(-1);
	if (index < 0 || std::cmp_greater_equal(index, ioEditor.cue().variants.size()))
		return fail(CommandError::InvalidArgument, std::format("no take {}", index));
	auto next = ioEditor.cue().variants[static_cast<size_t>(index)];
	next.file = iArgs.getText("file", next.file);
	next.trimStart = getNumber(iArgs, "trim_start").value_or(next.trimStart);
	next.trimEnd = getNumber(iArgs, "trim_end").value_or(next.trimEnd);
	next.loopStart = getNumber(iArgs, "loop_start").value_or(next.loopStart);
	next.loopEnd = getNumber(iArgs, "loop_end").value_or(next.loopEnd);
	next.weight = getNumber(iArgs, "weight").value_or(next.weight);
	if (!ioEditor.setVariant(static_cast<size_t>(index), next))
		return unchanged();
	return "Edit take";
}

auto setLanguage(LocalisationEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	const auto code = iArgs.getText("code");
	const auto* language = ioEditor.localisation().findLanguage(code);
	if (language == nullptr)
		return fail(CommandError::InvalidArgument, std::format("no language `{}`", code));
	auto next = *language;
	next.name = iArgs.getText("name", next.name);
	next.font = iArgs.getText("font", next.font);
	next.exported = iArgs.get<bool>("exported").value_or(next.exported);
	if (!ioEditor.setLanguage(next))
		return unchanged();
	return "Edit language";
}

auto setValue(LocalisationEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
	const auto key = iArgs.getText("key");
	const auto index = stringIndex(ioEditor, key);
	if (!index)
		return fail(CommandError::InvalidArgument, std::format("no string `{}`", key));
	const auto language = iArgs.getText("language");
	if (ioEditor.localisation().findLanguage(language) == nullptr)
		return fail(CommandError::InvalidArgument, std::format("no language `{}`", language));
	if (!ioEditor.setValue(*index, language, iArgs.getText("value")))
		return unchanged();
	return "Edit text";
}

auto soundPath() -> ArgSpec {
	return {.name = "path",
			.type = ArgType::Text,
			.required = true,
			.description = "The `.owlsound` file, opened in a tab when it is not.",
			.projectPath = true};
}

auto localePath() -> ArgSpec {
	return {.name = "path",
			.type = ArgType::Text,
			.required = true,
			.description = "The `localisation.yml` file, opened in a tab when it is not.",
			.projectPath = true};
}

auto takesSpec(const char* iDescription) -> ArgSpec {
	return {.name = "takes", .type = ArgType::Text, .required = true, .description = iDescription};
}

auto keysSpec(const char* iDescription) -> ArgSpec {
	return {.name = "keys", .type = ArgType::Text, .required = true, .description = iDescription};
}

auto number(const char* iName, const char* iDescription) -> ArgSpec {
	return {.name = iName, .type = ArgType::Number, .description = iDescription};
}

auto text(const char* iName, const char* iDescription, const bool iRequired = false) -> ArgSpec {
	return {.name = iName, .type = ArgType::Text, .required = iRequired, .description = iDescription};
}

void addSoundCommands(CommandRegistry& ioRegistry) {
	const auto add = [&ioRegistry](const char* iName, const char* iDescription, std::vector<ArgSpec> iArgs,
								   SoundEdit iEdit) -> void {
		iArgs.insert(iArgs.begin(), soundPath());
		ioRegistry.add({.name = iName,
						.description = iDescription,
						.args = std::move(iArgs),
						.factory = {},
						.run = soundRunner(iEdit),
						.allowedInPlay = true,
						.needsScene = false});
	};
	add("sound.set", "Set the volume, the pitch, their random change at each play and the loop flag of a sound cue.",
		{number("volume", "Base volume multiplier of every take."),
		 number("volume_change", "Random change of the volume at each play, from 0 to 1."),
		 number("pitch", "Base pitch multiplier of every take."),
		 number("pitch_change", "Random change of the pitch at each play, from 0 to 0.9."),
		 {.name = "loop", .type = ArgType::Bool, .description = "Whether the take loops."}},
		setCue);
	add("sound.add_take", "Append a take playing a whole audio file to a sound cue.",
		{text("file", "Asset path of the audio file.", true)},
		[](SoundCueEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
			if (!ioEditor.addVariant(iArgs.getText("file")))
				return fail(CommandError::InvalidArgument, "`file` must be an audio file");
			return "Add take";
		});
	add("sound.set_take", "Change the file, the trim and loop points (seconds, 0 = end) or the weight of a take.",
		{{.name = "take", .type = ArgType::Integer, .required = true, .description = "Index of the take, from 0."},
		 text("file", "Asset path of its audio file."),
		 number("trim_start", "First played second."),
		 number("trim_end", "Last played second; 0 plays to the end."),
		 number("loop_start", "Start of the part a looping cue repeats."),
		 number("loop_end", "End of the repeated part; 0 is the end."),
		 number("weight", "Relative chance of the take being picked.")},
		setTake);
	add("sound.remove_takes", "Remove takes of a sound cue.", {takesSpec("Indices of the takes (`0,2`).")},
		[](SoundCueEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
			const auto indices = takeIndices(ioEditor, iArgs);
			if (!indices)
				return unexpected<CommandFailure>{indices.error()};
			if (!ioEditor.removeVariants(*indices))
				return unchanged();
			return indices->size() > 1 ? "Remove takes" : "Remove take";
		});
	add("sound.duplicate_takes", "Copy takes of a sound cue after the last of them.",
		{takesSpec("Indices of the takes (`0,2`).")},
		[](SoundCueEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
			const auto indices = takeIndices(ioEditor, iArgs);
			if (!indices)
				return unexpected<CommandFailure>{indices.error()};
			if (!ioEditor.duplicateVariants(*indices))
				return unchanged();
			return "Duplicate takes";
		});
	add("sound.move_takes", "Move takes of a sound cue one row up or down.",
		{takesSpec("Indices of the takes (`0,2`)."),
		 {.name = "down",
		  .type = ArgType::Bool,
		  .description = "Move down instead of up.",
		  .defaultValue = ArgValue{false}}},
		[](SoundCueEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
			const auto indices = takeIndices(ioEditor, iArgs);
			if (!indices)
				return unexpected<CommandFailure>{indices.error()};
			if (!ioEditor.moveVariants(*indices, iArgs.getBool("down")))
				return unchanged();
			return "Move takes";
		});
}

void addLocaleCommands(CommandRegistry& ioRegistry) {
	const auto add = [&ioRegistry](const char* iName, const char* iDescription, std::vector<ArgSpec> iArgs,
								   LocaleEdit iEdit) -> void {
		iArgs.insert(iArgs.begin(), localePath());
		ioRegistry.add({.name = iName,
						.description = iDescription,
						.args = std::move(iArgs),
						.factory = {},
						.run = localeRunner(iEdit),
						.allowedInPlay = true,
						.needsScene = false});
	};
	add("locale.add_language", "Add a language to a localisation.",
		{text("code", "Its code (`fr`), unique.", true), text("name", "Its name (the code when absent).")},
		[](LocalisationEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
			const auto code = iArgs.getText("code");
			if (!ioEditor.addLanguage(code, iArgs.getText("name", code)))
				return fail(CommandError::InvalidArgument, std::format("`{}` is empty or taken", code));
			return "Add language";
		});
	add("locale.remove_language", "Remove a language and its texts from a localisation.",
		{text("code", "Its code.", true)},
		[](LocalisationEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
			if (!ioEditor.removeLanguage(iArgs.getText("code")))
				return fail(CommandError::InvalidArgument, std::format("no language `{}`", iArgs.getText("code")));
			return "Remove language";
		});
	add("locale.set_language", "Change the name, the font or the export flag of a language.",
		{text("code", "Its code.", true),
		 text("name", "Its name."),
		 text("font", "Font of its HUD texts (empty keeps each text's font)."),
		 {.name = "exported", .type = ArgType::Bool, .description = "Whether the exported game ships it."}},
		setLanguage);
	add("locale.set_fallback", "Choose the language a missing text falls back to.",
		{text("code", "A language code of the localisation.", true)},
		[](LocalisationEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
			if (ioEditor.localisation().findLanguage(iArgs.getText("code")) == nullptr)
				return fail(CommandError::InvalidArgument, std::format("no language `{}`", iArgs.getText("code")));
			if (!ioEditor.setFallback(iArgs.getText("code")))
				return unchanged();
			return "Set fallback";
		});
	add("locale.add_string", "Append a string, under a unique key.",
		{text("key", "Its key; a number is added when it is taken.", true), text("table", "Its string table.")},
		[](LocalisationEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
			std::ignore = ioEditor.addString(iArgs.getText("key"), iArgs.getText("table"));
			return "Add string";
		});
	add("locale.rename_key", "Rename the key of a string.",
		{text("key", "The key.", true), text("new_key", "The new key, not taken.", true)},
		[](LocalisationEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
			const auto index = stringIndex(ioEditor, iArgs.getText("key"));
			if (!index)
				return fail(CommandError::InvalidArgument, std::format("no string `{}`", iArgs.getText("key")));
			if (!ioEditor.setKey(*index, iArgs.getText("new_key")))
				return fail(CommandError::InvalidArgument, "`new_key` is empty, taken or the same");
			return "Rename key";
		});
	add("locale.set_value", "Set the text of a string in a language.",
		{text("key", "The key.", true), text("language", "The language code.", true),
		 text("value", "The text (empty: the fallback shows).", true)},
		setValue);
	add("locale.set_table", "Move strings to a string table.",
		{keysSpec("Keys of the strings (`a,b`)."), text("table", "The table.", true)},
		[](LocalisationEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
			const auto indices = stringIndices(ioEditor, iArgs);
			if (!indices)
				return unexpected<CommandFailure>{indices.error()};
			if (!ioEditor.setTable(*indices, iArgs.getText("table")))
				return unchanged();
			return "Set table";
		});
	add("locale.remove_strings", "Remove strings.", {keysSpec("Keys of the strings (`a,b`).")},
		[](LocalisationEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
			const auto indices = stringIndices(ioEditor, iArgs);
			if (!indices)
				return unexpected<CommandFailure>{indices.error()};
			if (!ioEditor.removeStrings(*indices))
				return unchanged();
			return "Remove strings";
		});
	add("locale.duplicate_strings", "Copy strings after the last of them, under unique keys.",
		{keysSpec("Keys of the strings (`a,b`).")},
		[](LocalisationEditor& ioEditor, const CommandArgs& iArgs) -> expected<std::string, CommandFailure> {
			const auto indices = stringIndices(ioEditor, iArgs);
			if (!indices)
				return unexpected<CommandFailure>{indices.error()};
			if (!ioEditor.duplicateStrings(*indices))
				return unchanged();
			return "Duplicate strings";
		});
}

}// namespace

void addDocumentCommands(CommandRegistry& ioRegistry) {
	addSoundCommands(ioRegistry);
	addLocaleCommands(ioRegistry);
}

}// namespace owl::nest::commands
