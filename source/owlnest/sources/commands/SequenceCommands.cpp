/**
 * @file SequenceCommands.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SequenceCommands.h"

#include "CommandRegistry.h"
#include "DocumentCommands.h"

#include <math/trigonometry.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest::commands {

SequenceEditor::~SequenceEditor() = default;

namespace {

using data::sequence::CameraKey;
using data::sequence::CameraTrack;
using data::sequence::EventKey;
using data::sequence::EventTrack;
using data::sequence::Sequence;

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto unchanged() -> unexpected<CommandFailure> { return fail(CommandError::NotApplicable, "nothing changes"); }

// What a command does to the sequence: the description of its undo entry, or why it cannot.
struct Edit {
	// Description of the undo entry.
	std::string description;
	// Whether it merges with the previous entry of the same description.
	bool merge{false};
	// Fields given back to the caller (`track`, `key`).
	ResultData data{};// NOLINT(readability-redundant-member-init) most edits give nothing back
};

using SequenceEdit = auto (*)(Sequence&, const CommandArgs&) -> expected<Edit, CommandFailure>;

// Find the document of `path`, edit a copy of its sequence, then hand it the result as one undo entry.
auto sequenceRunner(SequenceEdit iEdit) -> CommandSpec::Runner {
	return [edit = iEdit](const CommandContext& iContext,
						  const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		const std::filesystem::path path{iArgs.getText("path")};
		if (!Sequence::isSequence(path))
			return fail(CommandError::InvalidArgument, "`path` must end with `.owlseq`");
		auto* editor = iContext.documents != nullptr ? iContext.documents->openSequence(path) : nullptr;
		if (editor == nullptr)
			return fail(CommandError::NotApplicable, std::format("cannot open {} in an editor tab", path.string()));
		auto next = editor->sequence();
		auto done = edit(next, iArgs);
		if (!done)
			return unexpected<CommandFailure>{done.error()};
		if (!editor->apply(std::move(next), done->description, done->merge))
			return unchanged();
		done->data.emplace("path", path.generic_string());
		return CommandResult{.entity = core::UUID{0}, .description = done->description, .data = std::move(done->data)};
	};
}

auto number(const CommandArgs& iArgs, const std::string& iName) -> std::optional<float> {
	if (const auto value = iArgs.get<double>(iName); value)
		return static_cast<float>(*value);
	if (const auto value = iArgs.get<int64_t>(iName); value)
		return static_cast<float>(*value);
	return std::nullopt;
}

auto isCamera(const CommandArgs& iArgs) -> bool { return iArgs.getText("kind", "Camera") == "Camera"; }

auto trackCount(const Sequence& iSequence, const bool iCamera) -> size_t {
	return iCamera ? iSequence.cameraTracks.size() : iSequence.eventTracks.size();
}

auto trackIndex(const Sequence& iSequence, const CommandArgs& iArgs) -> expected<size_t, CommandFailure> {
	const auto index = iArgs.get<int64_t>("track").value_or(-1);
	if (index < 0 || static_cast<size_t>(index) >= trackCount(iSequence, isCamera(iArgs)))
		return fail(CommandError::InvalidArgument, std::format("no {} track {}", iArgs.getText("kind"), index));
	return static_cast<size_t>(index);
}

auto keyIndex(const size_t iCount, const CommandArgs& iArgs) -> expected<size_t, CommandFailure> {
	const auto index = iArgs.get<int64_t>("key").value_or(-1);
	if (index < 0 || static_cast<size_t>(index) >= iCount)
		return fail(CommandError::InvalidArgument, std::format("no key {}", index));
	return static_cast<size_t>(index);
}

auto keyIndices(const size_t iCount, const CommandArgs& iArgs) -> expected<std::vector<size_t>, CommandFailure> {
	std::vector<size_t> indices;
	for (const auto part: std::string_view{iArgs.getText("keys")} | std::views::split(',')) {
		std::string_view text{part.begin(), part.end()};
		while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
		while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
		if (text.empty())
			continue;
		size_t index = 0;
		if (const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), index);
			error != std::errc{} || end != text.data() + text.size() || index >= iCount)
			return fail(CommandError::InvalidArgument, std::format("no key `{}`", text));
		indices.push_back(index);
	}
	std::ranges::sort(indices);
	const auto [first, last] = std::ranges::unique(indices);
	indices.erase(first, last);
	if (indices.empty())
		return fail(CommandError::InvalidArgument, "`keys` names no key");
	return indices;
}

// Fields of a camera key given by the arguments, the others kept.
void readCameraKey(CameraKey& ioKey, const CommandArgs& iArgs) {
	if (const auto position = iArgs.getVec3("position"); position)
		ioKey.position = *position;
	if (const auto rotation = iArgs.getVec3("rotation"); rotation)
		ioKey.rotation = math::radians(*rotation);
	if (const auto fov = number(iArgs, "fov"); fov)
		ioKey.fov = math::radians(std::clamp(*fov, 1.f, 179.f));
	if (const auto size = number(iArgs, "size"); size)
		ioKey.size = std::max(0.01f, *size);
	if (iArgs.has("easing")) {
		if (const auto easing = data::sequence::parseEasing(iArgs.getText("easing")); easing)
			ioKey.easing = *easing;
	}
}

template<typename Key>
auto insertSorted(std::vector<Key>& ioKeys, Key iKey) -> size_t {
	const auto at = std::ranges::upper_bound(ioKeys, iKey.time, {}, &Key::time);
	const auto index = static_cast<size_t>(std::distance(ioKeys.begin(), at));
	ioKeys.insert(at, std::move(iKey));
	return index;
}

auto keyData(const size_t iTrack, const size_t iKey) -> ResultData {
	return {{"track", static_cast<int64_t>(iTrack)}, {"key", static_cast<int64_t>(iKey)}};
}

auto createSequence(const CommandContext& /*iContext*/, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const std::filesystem::path path{iArgs.getText("path")};
	if (!Sequence::isSequence(path))
		return fail(CommandError::InvalidArgument, "`path` must end with `.owlseq`");
	if (exists(path))
		return fail(CommandError::InvalidArgument, std::format("{} exists already", path.string()));
	if (!Sequence::makeDefault().saveToFile(path))
		return fail(CommandError::NotApplicable, std::format("cannot write {}", path.string()));
	return CommandResult{.entity = core::UUID{0},
						 .description = "Create sequence",
						 .data = {{"path", path.generic_string()}}};
}

auto setDuration(Sequence& ioSequence, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	ioSequence.duration = std::max(0.f, number(iArgs, "duration").value_or(ioSequence.duration));
	return Edit{.description = "Set sequence duration", .merge = iArgs.getBool("merge")};
}

auto addTrack(Sequence& ioSequence, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const bool camera = isCamera(iArgs);
	const size_t index = trackCount(ioSequence, camera);
	const auto name = iArgs.getText("name", std::format("{} {}", camera ? "Shot" : "Events", index + 1));
	if (camera) {
		CameraTrack track{.name = name, .keys = {}, .muted = false};
		if (!ioSequence.cameraTracks.empty() && !ioSequence.cameraTracks.back().keys.empty()) {
			auto key = ioSequence.cameraTracks.back().keys.back();
			track.keys.push_back(key);
			key.time = std::min(ioSequence.duration, key.time + 1.f);
			track.keys.push_back(key);
		} else {
			track.keys.push_back({});
		}
		ioSequence.cameraTracks.push_back(std::move(track));
	} else {
		ioSequence.eventTracks.push_back({.name = name, .keys = {}, .muted = false});
	}
	return Edit{.description = "Add track", .merge = false, .data = {{"track", static_cast<int64_t>(index)}}};
}

auto removeTrack(Sequence& ioSequence, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto track = trackIndex(ioSequence, iArgs);
	if (!track)
		return unexpected<CommandFailure>{track.error()};
	const auto offset = static_cast<std::ptrdiff_t>(*track);
	if (isCamera(iArgs))
		ioSequence.cameraTracks.erase(ioSequence.cameraTracks.begin() + offset);
	else
		ioSequence.eventTracks.erase(ioSequence.eventTracks.begin() + offset);
	return Edit{.description = "Remove track"};
}

auto setTrack(Sequence& ioSequence, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto track = trackIndex(ioSequence, iArgs);
	if (!track)
		return unexpected<CommandFailure>{track.error()};
	auto& name = isCamera(iArgs) ? ioSequence.cameraTracks[*track].name : ioSequence.eventTracks[*track].name;
	auto& muted = isCamera(iArgs) ? ioSequence.cameraTracks[*track].muted : ioSequence.eventTracks[*track].muted;
	name = iArgs.getText("name", name);
	muted = iArgs.getBool("muted", muted);
	return Edit{.description = "Edit track", .merge = iArgs.getBool("merge")};
}

auto moveTrack(Sequence& ioSequence, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto track = trackIndex(ioSequence, iArgs);
	if (!track)
		return unexpected<CommandFailure>{track.error()};
	const bool down = iArgs.getBool("down");
	const size_t count = trackCount(ioSequence, isCamera(iArgs));
	if ((down && *track + 1 >= count) || (!down && *track == 0))
		return unchanged();
	const size_t other = down ? *track + 1 : *track - 1;
	if (isCamera(iArgs))
		std::swap(ioSequence.cameraTracks[*track], ioSequence.cameraTracks[other]);
	else
		std::swap(ioSequence.eventTracks[*track], ioSequence.eventTracks[other]);
	return Edit{.description = "Move track", .merge = false, .data = {{"track", static_cast<int64_t>(other)}}};
}

auto addKey(Sequence& ioSequence, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto track = trackIndex(ioSequence, iArgs);
	if (!track)
		return unexpected<CommandFailure>{track.error()};
	const float time = std::max(0.f, number(iArgs, "time").value_or(0.f));
	if (isCamera(iArgs)) {
		auto& keys = ioSequence.cameraTracks[*track].keys;
		CameraKey key = ioSequence.cameraTracks[*track].sample(time).value_or(CameraKey{});
		key.time = time;
		readCameraKey(key, iArgs);
		const size_t index = insertSorted(keys, key);
		return Edit{.description = "Add key", .merge = false, .data = keyData(*track, index)};
	}
	const size_t index = insertSorted(ioSequence.eventTracks[*track].keys,
									  EventKey{.time = time, .function = iArgs.getText("function")});
	return Edit{.description = "Add key", .merge = false, .data = keyData(*track, index)};
}

auto setKey(Sequence& ioSequence, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto track = trackIndex(ioSequence, iArgs);
	if (!track)
		return unexpected<CommandFailure>{track.error()};
	const Edit edit{.description = "Edit key", .merge = iArgs.getBool("merge"), .data = {}};
	if (isCamera(iArgs)) {
		auto& keys = ioSequence.cameraTracks[*track].keys;
		const auto key = keyIndex(keys.size(), iArgs);
		if (!key)
			return unexpected<CommandFailure>{key.error()};
		CameraKey value = keys[*key];
		value.time = std::max(0.f, number(iArgs, "time").value_or(value.time));
		readCameraKey(value, iArgs);
		keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(*key));
		const size_t index = insertSorted(keys, value);
		return Edit{.description = edit.description, .merge = edit.merge, .data = keyData(*track, index)};
	}
	auto& keys = ioSequence.eventTracks[*track].keys;
	const auto key = keyIndex(keys.size(), iArgs);
	if (!key)
		return unexpected<CommandFailure>{key.error()};
	EventKey value = keys[*key];
	value.time = std::max(0.f, number(iArgs, "time").value_or(value.time));
	value.function = iArgs.getText("function", value.function);
	keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(*key));
	const size_t index = insertSorted(keys, value);
	return Edit{.description = edit.description, .merge = edit.merge, .data = keyData(*track, index)};
}

template<typename Key>
auto eraseKeys(std::vector<Key>& ioKeys, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto indices = keyIndices(ioKeys.size(), iArgs);
	if (!indices)
		return unexpected<CommandFailure>{indices.error()};
	for (const auto index: *indices | std::views::reverse)
		ioKeys.erase(ioKeys.begin() + static_cast<std::ptrdiff_t>(index));
	return Edit{.description = "Remove keys"};
}

auto removeKeys(Sequence& ioSequence, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto track = trackIndex(ioSequence, iArgs);
	if (!track)
		return unexpected<CommandFailure>{track.error()};
	if (isCamera(iArgs))
		return eraseKeys(ioSequence.cameraTracks[*track].keys, iArgs);
	return eraseKeys(ioSequence.eventTracks[*track].keys, iArgs);
}

template<typename Key>
auto shiftKeys(std::vector<Key>& ioKeys, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto indices = keyIndices(ioKeys.size(), iArgs);
	if (!indices)
		return unexpected<CommandFailure>{indices.error()};
	const float offset = number(iArgs, "offset").value_or(0.f);
	for (const auto index: *indices) ioKeys[index].time = std::max(0.f, ioKeys[index].time + offset);
	std::ranges::stable_sort(ioKeys, {}, &Key::time);
	return Edit{.description = "Move keys", .merge = iArgs.getBool("merge")};
}

auto moveKeys(Sequence& ioSequence, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto track = trackIndex(ioSequence, iArgs);
	if (!track)
		return unexpected<CommandFailure>{track.error()};
	if (isCamera(iArgs))
		return shiftKeys(ioSequence.cameraTracks[*track].keys, iArgs);
	return shiftKeys(ioSequence.eventTracks[*track].keys, iArgs);
}

auto pathSpec() -> ArgSpec {
	return {.name = "path",
			.type = ArgType::Text,
			.required = true,
			.description = "The `.owlseq` file, opened in a tab when it is not.",
			.projectPath = true};
}

auto kindSpec() -> ArgSpec {
	return {.name = "kind",
			.type = ArgType::Enum,
			.description = "Kind of track: Camera (shots) or Event (Lua calls).",
			.defaultValue = ArgValue{std::string{"Camera"}},
			.choices = {"Camera", "Event"}};
}

auto text(const char* iName, const char* iDescription, const bool iRequired = false) -> ArgSpec {
	return {.name = iName, .type = ArgType::Text, .required = iRequired, .description = iDescription};
}

auto integer(const char* iName, const char* iDescription, const bool iRequired = false) -> ArgSpec {
	return {.name = iName, .type = ArgType::Integer, .required = iRequired, .description = iDescription};
}

auto real(const char* iName, const char* iDescription, const bool iRequired = false) -> ArgSpec {
	return {.name = iName, .type = ArgType::Number, .required = iRequired, .description = iDescription};
}

auto flag(const char* iName, const char* iDescription) -> ArgSpec {
	return {.name = iName, .type = ArgType::Bool, .description = iDescription, .defaultValue = ArgValue{false}};
}

auto easings() -> std::vector<std::string> {
	std::vector<std::string> names;
	for (const auto easing: data::sequence::getEasings()) names.emplace_back(data::sequence::getEasingName(easing));
	return names;
}

auto cameraFields() -> std::vector<ArgSpec> {
	return {{.name = "position", .type = ArgType::Vec3, .description = "Camera position, in the player's space."},
			{.name = "rotation", .type = ArgType::Vec3, .description = "Camera Euler angles, in degrees."},
			real("fov", "Vertical field of view of a perspective camera, in degrees."),
			real("size", "Size of an orthographic camera."),
			{.name = "easing",
			 .type = ArgType::Enum,
			 .description = "How the camera moves to the next key.",
			 .choices = easings()}};
}

}// namespace

void addSequenceCommands(CommandRegistry& ioRegistry) {
	const auto add = [&ioRegistry](const char* iName, const char* iDescription, std::vector<ArgSpec> iArgs,
								   SequenceEdit iEdit) -> void {
		iArgs.insert(iArgs.begin(), pathSpec());
		ioRegistry.add({.name = iName,
						.description = iDescription,
						.args = std::move(iArgs),
						.factory = {},
						.run = sequenceRunner(iEdit),
						.allowedInPlay = true,
						.needsScene = false});
	};
	const auto withCamera = [](std::vector<ArgSpec> iArgs) -> std::vector<ArgSpec> {
		std::ranges::move(cameraFields(), std::back_inserter(iArgs));
		return iArgs;
	};
	ioRegistry.add({.name = "sequence.create",
					.description = "Create a `.owlseq` sequence holding one camera track of two keys.",
					.args = {{.name = "path",
							  .type = ArgType::Text,
							  .required = true,
							  .description = "Sequence file to write (`.owlseq`, must not exist).",
							  .projectPath = true}},
					.factory = {},
					.run = createSequence,
					.allowedInPlay = true,
					.needsScene = false});
	add("sequence.set_duration", "Set the length of a sequence, in seconds.",
		{real("duration", "Length in seconds.", true), flag("merge", "Merge with the previous duration edit.")},
		setDuration);
	add("sequence.add_track", "Add a camera track (a shot) or an event track at the end of a sequence.",
		{kindSpec(), text("name", "Name of the track.")}, addTrack);
	add("sequence.remove_track", "Remove a track of a sequence and its keys.",
		{kindSpec(), integer("track", "Index of the track, from 0.", true)}, removeTrack);
	add("sequence.set_track", "Rename or mute a track of a sequence; absent fields are kept.",
		{kindSpec(),
		 integer("track", "Index of the track, from 0.", true),
		 text("name", "Name of the track."),
		 {.name = "muted", .type = ArgType::Bool, .description = "A muted track drives no camera and calls nothing."},
		 flag("merge", "Merge with the previous edit of the track (typing).")},
		setTrack);
	add("sequence.move_track", "Move a track one place up or down; a lower camera track wins over the ones above.",
		{kindSpec(), integer("track", "Index of the track, from 0.", true), flag("down", "Move down instead of up.")},
		moveTrack);
	add("sequence.add_key",
		"Add a key to a track; a camera key takes the pose of the track at its time for the fields not given.",
		withCamera({kindSpec(), integer("track", "Index of the track, from 0.", true),
					real("time", "Time of the key, in seconds."),
					text("function", "Lua function an event key calls.")}),
		addKey);
	add("sequence.set_key", "Edit a key of a track; absent fields are kept, a new time sorts the key again.",
		withCamera({kindSpec(), integer("track", "Index of the track, from 0.", true),
					integer("key", "Index of the key in its track, from 0.", true),
					real("time", "Time of the key, in seconds."), text("function", "Lua function an event key calls."),
					flag("merge", "Merge with the previous edit of the key (typing, dragging).")}),
		setKey);
	add("sequence.remove_keys", "Remove keys of a track.",
		{kindSpec(), integer("track", "Index of the track, from 0.", true),
		 text("keys", "Indices of the keys (`0,2`).", true)},
		removeKeys);
	add("sequence.move_keys", "Shift keys of a track in time (never before 0).",
		{kindSpec(), integer("track", "Index of the track, from 0.", true),
		 text("keys", "Indices of the keys (`0,2`).", true),
		 real("offset", "Shift in seconds, negative earlier.", true),
		 flag("merge", "Merge with the previous move (dragging).")},
		moveKeys);
}

}// namespace owl::nest::commands
