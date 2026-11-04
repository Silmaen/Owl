/**
 * @file Replay.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/Replay.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "input/InputNames.h"
#include "platform/AtomicFile.h"
#include "platform/FileUtils.h"

#include <array>
#include <bit>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <utility>

namespace owl::scene {

namespace {

constexpr std::array<core::MigrationStep, 0> g_replayMigrations{};
constexpr core::DocumentFormat g_replayFormat{.name = "Replay", .migrations = g_replayMigrations};

using Nanoseconds = std::chrono::duration<int64_t, std::nano>;

// Running state of the encoder and the decoder: what the previous frame held.
struct DeltaState {
	input::InputState input;
	bool cursorCaptured{false};
	std::optional<ReplayPointer> pointer;
};

auto hexHash(const uint64_t iHash) -> std::string { return std::format("{:#018x}", iHash); }

auto parseHash(const YAML::Node& iNode) -> uint64_t {
	const auto text = iNode.as<std::string>();
	return std::stoull(text, nullptr, 0);
}

void emitVec2(YAML::Emitter& ioOut, const math::vec2& iValue) {
	ioOut << YAML::Flow << YAML::BeginSeq << iValue.x() << iValue.y() << YAML::EndSeq;
}

auto readVec2(const YAML::Node& iNode) -> math::vec2 { return {iNode[0].as<float>(), iNode[1].as<float>()}; }

template<std::size_t Count, typename NameFunction>
void emitBits(YAML::Emitter& ioOut, const std::bitset<Count>& iBits, NameFunction iName) {
	ioOut << YAML::Flow << YAML::BeginSeq;
	for (std::size_t index = 0; index < Count; ++index) {
		if (!iBits[index])
			continue;
		const std::string name = iName(index);
		ioOut << (name.empty() ? std::format("#{}", index) : name);
	}
	ioOut << YAML::EndSeq;
}

template<std::size_t Count, typename ParseFunction>
auto readBits(const YAML::Node& iNode, ParseFunction iParse) -> std::bitset<Count> {
	std::bitset<Count> bits;
	for (const auto& item: iNode) {
		const auto text = item.as<std::string>();
		std::size_t index = Count;
		if (const auto parsed = iParse(text); parsed.has_value())
			index = static_cast<std::size_t>(*parsed);
		else if (text.starts_with('#'))
			index = std::stoul(text.substr(1));
		if (index >= Count)
			throw YAML::Exception(item.Mark(), std::format("unknown input code '{}'", text));
		bits.set(index);
	}
	return bits;
}

auto keyLabel(const std::size_t iIndex) -> std::string {
	const auto code = static_cast<input::KeyCode>(iIndex);
	const auto name = input::names::keyName(code);
	return input::names::parseKey(name) == code ? name : std::string{};
}

auto mouseLabel(const std::size_t iIndex) -> std::string {
	const auto code = static_cast<input::MouseCode>(iIndex);
	const auto name = input::names::mouseButtonName(code);
	return input::names::parseMouseButton(name) == code ? name : std::string{};
}

auto padButtonLabel(const std::size_t iIndex) -> std::string {
	const auto code = static_cast<input::GamepadButton>(iIndex);
	const auto name = input::names::gamepadButtonName(code);
	return input::names::parseGamepadButton(name) == code ? name : std::string{};
}

auto samePad(const input::GamepadState& iLeft, const input::GamepadState& iRight) -> bool {
	return iLeft.connected == iRight.connected && iLeft.buttons == iRight.buttons && iLeft.axes == iRight.axes;
}

void emitPads(YAML::Emitter& ioOut, const input::InputState& iPrevious, const input::InputState& iCurrent) {
	bool open = false;
	for (std::size_t slot = 0; slot < input::g_GamepadCount; ++slot) {
		const auto& pad = iCurrent.gamepads[slot];
		if (samePad(iPrevious.gamepads[slot], pad))
			continue;
		if (!open) {
			ioOut << YAML::Key << "pads" << YAML::Value << YAML::BeginSeq;
			open = true;
		}
		ioOut << YAML::Flow << YAML::BeginMap;
		ioOut << YAML::Key << "slot" << YAML::Value << slot;
		ioOut << YAML::Key << "connected" << YAML::Value << pad.connected;
		ioOut << YAML::Key << "buttons" << YAML::Value;
		emitBits(ioOut, pad.buttons, &padButtonLabel);
		ioOut << YAML::Key << "axes" << YAML::Value << YAML::Flow << YAML::BeginSeq;
		for (const float axis: pad.axes) ioOut << axis;
		ioOut << YAML::EndSeq << YAML::EndMap;
	}
	if (open)
		ioOut << YAML::EndSeq;
}

void emitPointer(YAML::Emitter& ioOut, const std::optional<ReplayPointer>& iPointer) {
	ioOut << YAML::Key << "pointer" << YAML::Value;
	if (!iPointer.has_value()) {
		ioOut << YAML::Null;
		return;
	}
	ioOut << YAML::Flow << YAML::BeginMap;
	ioOut << YAML::Key << "viewport" << YAML::Value << YAML::Flow << YAML::BeginSeq << iPointer->viewport.x()
		  << iPointer->viewport.y() << YAML::EndSeq;
	ioOut << YAML::Key << "position" << YAML::Value;
	emitVec2(ioOut, iPointer->position);
	ioOut << YAML::Key << "pressed" << YAML::Value << iPointer->pressed;
	ioOut << YAML::Key << "wheel" << YAML::Value;
	emitVec2(ioOut, iPointer->wheel);
	ioOut << YAML::Key << "dt" << YAML::Value << iPointer->deltaTime;
	ioOut << YAML::EndMap;
}

// Emit the fields of a frame that differ from the previous one, as map entries of an open map.
void emitChanges(YAML::Emitter& ioOut, const DeltaState& iPrevious, const DeltaState& iCurrent) {
	const auto& previous = iPrevious.input;
	const auto& current = iCurrent.input;
	if (previous.keys != current.keys) {
		ioOut << YAML::Key << "keys" << YAML::Value;
		emitBits(ioOut, current.keys, &keyLabel);
	}
	if (previous.mouseButtons != current.mouseButtons) {
		ioOut << YAML::Key << "mouse" << YAML::Value;
		emitBits(ioOut, current.mouseButtons, &mouseLabel);
	}
	if (previous.cursor != current.cursor) {
		ioOut << YAML::Key << "cursor" << YAML::Value;
		emitVec2(ioOut, current.cursor);
	}
	if (previous.wheel != current.wheel) {
		ioOut << YAML::Key << "wheel" << YAML::Value;
		emitVec2(ioOut, current.wheel);
	}
	emitPads(ioOut, previous, current);
	if (iPrevious.cursorCaptured != iCurrent.cursorCaptured)
		ioOut << YAML::Key << "captured" << YAML::Value << iCurrent.cursorCaptured;
	if (iPrevious.pointer != iCurrent.pointer)
		emitPointer(ioOut, iCurrent.pointer);
}

auto differs(const DeltaState& iPrevious, const DeltaState& iCurrent) -> bool {
	const auto& previous = iPrevious.input;
	const auto& current = iCurrent.input;
	if (previous.keys != current.keys || previous.mouseButtons != current.mouseButtons ||
		previous.cursor != current.cursor || previous.wheel != current.wheel ||
		iPrevious.cursorCaptured != iCurrent.cursorCaptured || iPrevious.pointer != iCurrent.pointer)
		return true;
	for (std::size_t slot = 0; slot < input::g_GamepadCount; ++slot)
		if (!samePad(previous.gamepads[slot], current.gamepads[slot]))
			return true;
	return false;
}

void readPads(const YAML::Node& iNode, input::InputState& ioState) {
	for (const auto& item: iNode) {
		const auto slot = item["slot"].as<std::size_t>();
		if (slot >= input::g_GamepadCount)
			throw YAML::Exception(item.Mark(), std::format("gamepad slot {} out of range", slot));
		auto& pad = ioState.gamepads[slot];
		pad.connected = item["connected"].as<bool>();
		pad.buttons = readBits<input::g_GamepadButtonCount>(
				item["buttons"], [](const std::string& iName) -> std::optional<input::GamepadButton> {
					return input::names::parseGamepadButton(iName);
				});
		const auto axes = item["axes"];
		if (!axes.IsSequence() || axes.size() != input::g_GamepadAxisCount)
			throw YAML::Exception(item.Mark(), std::format("a gamepad has {} axes", input::g_GamepadAxisCount));
		for (std::size_t axis = 0; axis < input::g_GamepadAxisCount; ++axis) pad.axes[axis] = axes[axis].as<float>();
	}
}

// Apply the changes of one event onto the running state.
void readChanges(const YAML::Node& iNode, DeltaState& ioState) {
	auto& state = ioState.input;
	if (const auto keys = iNode["keys"]; keys)
		state.keys = readBits<input::g_KeyCount>(keys, [](const std::string& iName) -> std::optional<input::KeyCode> {
			return input::names::parseKey(iName);
		});
	if (const auto mouse = iNode["mouse"]; mouse)
		state.mouseButtons = readBits<input::g_MouseButtonCount>(
				mouse, [](const std::string& iName) -> std::optional<input::MouseCode> {
					return input::names::parseMouseButton(iName);
				});
	if (const auto cursor = iNode["cursor"]; cursor)
		state.cursor = readVec2(cursor);
	if (const auto wheel = iNode["wheel"]; wheel)
		state.wheel = readVec2(wheel);
	if (const auto pads = iNode["pads"]; pads)
		readPads(pads, state);
	if (const auto captured = iNode["captured"]; captured)
		ioState.cursorCaptured = captured.as<bool>();
	if (const auto pointer = iNode["pointer"]; pointer) {
		if (pointer.IsNull()) {
			ioState.pointer.reset();
		} else {
			const auto viewport = pointer["viewport"];
			ioState.pointer = ReplayPointer{.viewport = {viewport[0].as<uint32_t>(), viewport[1].as<uint32_t>()},
											.position = readVec2(pointer["position"]),
											.pressed = pointer["pressed"].as<bool>(),
											.wheel = readVec2(pointer["wheel"]),
											.deltaTime = pointer["dt"].as<float>()};
		}
	}
}

void emitDurations(YAML::Emitter& ioOut, const std::vector<ReplayFrame>& iFrames) {
	ioOut << YAML::Key << "durations" << YAML::Value << YAML::BeginSeq;
	for (std::size_t index = 0; index < iFrames.size();) {
		const auto duration = iFrames[index].duration;
		std::size_t end = index + 1;
		while (end < iFrames.size() && iFrames[end].duration == duration) ++end;
		ioOut << YAML::Flow << YAML::BeginSeq << std::chrono::duration_cast<Nanoseconds>(duration).count()
			  << (end - index) << YAML::EndSeq;
		index = end;
	}
	ioOut << YAML::EndSeq;
}

void readDurations(const YAML::Node& iNode, std::vector<ReplayFrame>& oFrames) {
	for (const auto& run: iNode) {
		const auto duration = std::chrono::duration_cast<core::Timestep::duration>(Nanoseconds{run[0].as<int64_t>()});
		const auto count = run[1].as<std::size_t>();
		for (std::size_t index = 0; index < count; ++index)
			oFrames.push_back({.duration = duration, .input = {}, .cursorCaptured = false, .pointer = std::nullopt});
	}
}

}// namespace

auto ReplayPointer::operator==(const ReplayPointer& iOther) const -> bool {
	const auto bits = [](const float iValue) -> uint32_t { return std::bit_cast<uint32_t>(iValue); };
	const auto sameVec = [&bits](const math::vec2& iLeft, const math::vec2& iRight) -> bool {
		return bits(iLeft.x()) == bits(iRight.x()) && bits(iLeft.y()) == bits(iRight.y());
	};
	return viewport == iOther.viewport && sameVec(position, iOther.position) && pressed == iOther.pressed &&
		   sameVec(wheel, iOther.wheel) && bits(deltaTime) == bits(iOther.deltaTime);
}

auto Replay::format() -> const core::DocumentFormat& { return g_replayFormat; }

auto Replay::fromYaml(const std::string_view iText, const std::string_view iSourceName)
		-> expected<Replay, std::string> {
	try {
		YAML::Node root = YAML::Load(std::string{iText});
		if (!root.IsMap()) {
			OWL_CORE_WARN("Replay: '{}' is not a YAML map.", iSourceName)
			return unexpected{std::format("{}: the document is not a map", iSourceName)};
		}
		if (const auto upgraded = core::upgradeYamlDocument(g_replayFormat, root, iSourceName); !upgraded) {
			OWL_CORE_WARN("Replay: Cannot read '{}': {}.", iSourceName, describe(upgraded.error()))
			return unexpected{std::format("{}: {}", iSourceName, describe(upgraded.error()))};
		}
		Replay replay;
		replay.scene = root["scene"].as<std::string>("");
		replay.seed = root["seed"].as<uint64_t>(0);
		replay.tickRate = root["tick_rate"].as<float>(0.f);
		replay.maxTicksPerFrame = root["max_ticks_per_frame"].as<uint32_t>(0);
		if (const auto hash = root["start_hash"]; hash)
			replay.startHash = parseHash(hash);
		DeltaState state;
		if (const auto start = root["start"]; start)
			readChanges(start, state);
		replay.start = state.input;
		state.input.wheel = {0.f, 0.f};
		if (const auto durations = root["durations"]; durations)
			readDurations(durations, replay.frames);
		const auto events = root["events"];
		std::size_t next = 0;
		for (std::size_t index = 0; index < replay.frames.size(); ++index) {
			if (events && next < events.size() && events[next]["frame"].as<std::size_t>() == index)
				readChanges(events[next++], state);
			auto& frame = replay.frames[index];
			frame.input = state.input;
			frame.cursorCaptured = state.cursorCaptured;
			frame.pointer = state.pointer;
			// The wheel is the offset of one frame: an event sets it, the next frame starts from zero again.
			state.input.wheel = {0.f, 0.f};
		}
		if (events && next < events.size()) {
			OWL_CORE_WARN("Replay: In '{}', an event is out of order or past the last frame.", iSourceName)
			return unexpected{std::format("{}: event {} is out of order or past the last frame", iSourceName, next)};
		}
		if (const auto checkpoints = root["checkpoints"]; checkpoints)
			for (const auto& item: checkpoints)
				replay.checkpoints.push_back({.frame = item[0].as<uint64_t>(), .worldHash = parseHash(item[1])});
		return replay;
	} catch (const std::exception& iEx) {
		OWL_CORE_WARN("Replay: Cannot parse '{}': {}.", iSourceName, iEx.what())
		return unexpected{std::format("{}: {}", iSourceName, iEx.what())};
	}
}

auto Replay::loadFile(const std::filesystem::path& iFile) -> expected<Replay, std::string> {
	if (!exists(iFile)) {
		OWL_CORE_WARN("Replay: File '{}' not found.", iFile.string())
		return unexpected{std::format("{}: file not found", iFile.string())};
	}
	return fromYaml(platform::fileToString(iFile), iFile.string());
}

auto Replay::toYaml() const -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	core::emitFormatVersion(out, g_replayFormat);
	out << YAML::Key << "scene" << YAML::Value << scene;
	out << YAML::Key << "seed" << YAML::Value << seed;
	out << YAML::Key << "tick_rate" << YAML::Value << tickRate;
	out << YAML::Key << "max_ticks_per_frame" << YAML::Value << maxTicksPerFrame;
	out << YAML::Key << "start_hash" << YAML::Value << hexHash(startHash);
	DeltaState state;
	const DeltaState startState{.input = start, .cursorCaptured = false, .pointer = std::nullopt};
	out << YAML::Key << "start" << YAML::Value << YAML::BeginMap;
	emitChanges(out, state, startState);
	out << YAML::EndMap;
	state = startState;
	state.input.wheel = {0.f, 0.f};
	emitDurations(out, frames);
	out << YAML::Key << "events" << YAML::Value << YAML::BeginSeq;
	for (std::size_t index = 0; index < frames.size(); ++index) {
		const auto& frame = frames[index];
		const DeltaState current{.input = frame.input,
								 .cursorCaptured = frame.cursorCaptured,
								 .pointer = frame.pointer};
		if (differs(state, current)) {
			out << YAML::Flow << YAML::BeginMap << YAML::Key << "frame" << YAML::Value << index;
			emitChanges(out, state, current);
			out << YAML::EndMap;
		}
		state = current;
		state.input.wheel = {0.f, 0.f};
	}
	out << YAML::EndSeq;
	out << YAML::Key << "checkpoints" << YAML::Value << YAML::BeginSeq;
	for (const auto& [frame, worldHash]: checkpoints)
		out << YAML::Flow << YAML::BeginSeq << frame << hexHash(worldHash) << YAML::EndSeq;
	out << YAML::EndSeq << YAML::EndMap;
	return std::string{out.c_str()} + "\n";
}

auto Replay::saveFile(const std::filesystem::path& iFile) const -> bool {
	if (const auto written = platform::writeFileAtomic(iFile, toYaml()); !written) {
		OWL_CORE_ERROR("Replay: Cannot save '{}': {}.", iFile.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto Replay::getSeconds() const -> double {
	core::Timestep::duration total{};
	for (const auto& frame: frames) total += frame.duration;
	return std::chrono::duration<double>(total).count();
}

}// namespace owl::scene
