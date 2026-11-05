/**
 * @file Replay.h
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/FormatVersion.h"
#include "core/Timestep.h"
#include "core/expected.h"
#include "input/InputFrame.h"
#include "math/vectors.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::scene {

/**
 * @brief
 *  The mouse handed to the HUD in one frame (`UiInputSystem::update`), in viewport pixels.
 */
struct OWL_API ReplayPointer {
	/// Size of the viewport the HUD was laid out in.
	math::vec2ui viewport{0, 0};
	/// Mouse position, Y down from the top left of the viewport.
	math::vec2 position{0.f, 0.f};
	/// Whether the left button was held on the game.
	bool pressed{false};
	/// Wheel offset of the frame over the game.
	math::vec2 wheel{0.f, 0.f};
	/// Seconds handed to the HUD with the mouse (hover and repeat timers).
	float deltaTime{0.f};

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther The other pointer.
	 * @return True if every field holds the same bits.
	 */
	auto operator==(const ReplayPointer& iOther) const -> bool;
};

/**
 * @brief
 *  Everything a runtime frame of a scene reads from outside: its duration and its input.
 */
struct OWL_API ReplayFrame {
	/// Duration of the frame given to `Scene::onUpdateRuntime`, before the time scale.
	core::Timestep::duration duration{};
	/// State of the devices read at the start of the frame.
	input::InputState input;
	/// Whether the window cursor was captured (mouse look of the first-person and ship controllers).
	bool cursorCaptured{false};
	/// The mouse handed to the HUD before the frame, none when the HUD was not updated.
	std::optional<ReplayPointer> pointer;
};

/**
 * @brief
 *  World hash of the scene at the end of a recorded frame.
 */
struct OWL_API ReplayCheckpoint {
	/// Frames run when the hash was taken (1 after the first runtime frame).
	uint64_t frame{0};
	/// `computeWorldHash` at the end of that frame.
	uint64_t worldHash{0};

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther The other checkpoint.
	 * @return True if both fields are equal.
	 */
	auto operator==(const ReplayCheckpoint& iOther) const -> bool = default;
};

/**
 * @brief
 *  The input log of one Play session of a scene, replayed frame-exact by `Scene::setPlaybackReplay`.
 *
 * The scene, its seed, its simulation settings and every frame (duration, device state, HUD mouse) are enough to
 * replay the session bit for bit on the bit-exact simulation; the world hashes of the checkpoints tell where a replay
 * diverges. Written as `.owlreplay` YAML: frame durations as runs, input as the changes from the previous frame.
 */
class OWL_API Replay {
public:
	/// Simulation ticks between two checkpoints of a recording.
	static constexpr uint64_t checkpointTicks = 60;

	/// Scene the session played, as the recorder named it (a path relative to the project for the editor and runner).
	std::string scene;
	/// Seed of the random streams at the start.
	uint64_t seed{0};
	/// Fixed ticks per second of the simulation clock.
	float tickRate{0.f};
	/// Bound of the ticks run by one frame.
	uint32_t maxTicksPerFrame{0};
	/// Device state held when the runtime started.
	input::InputState start;
	/// World hash at the end of `onStartRuntime`.
	uint64_t startHash{0};
	/// The frames, in order.
	std::vector<ReplayFrame> frames;
	/// World hashes recorded every `checkpointTicks` ticks, in frame order.
	std::vector<ReplayCheckpoint> checkpoints;

	/**
	 * @brief
	 *  Format descriptor of the `.owlreplay` file.
	 * @return The descriptor.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  Parse a replay from YAML text.
	 * @param[in] iText The YAML text.
	 * @param[in] iSourceName File or buffer name for the messages.
	 * @return The replay, or the reason it cannot be read.
	 */
	[[nodiscard]] static auto fromYaml(std::string_view iText, std::string_view iSourceName = "replay")
			-> expected<Replay, std::string>;

	/**
	 * @brief
	 *  Read a replay file.
	 * @param[in] iFile The `.owlreplay` file.
	 * @return The replay, or the reason it cannot be read.
	 */
	[[nodiscard]] static auto loadFile(const std::filesystem::path& iFile) -> expected<Replay, std::string>;

	/**
	 * @brief
	 *  Write the replay as YAML text (with its `FormatVersion`).
	 * @return The YAML text.
	 */
	[[nodiscard]] auto toYaml() const -> std::string;

	/**
	 * @brief
	 *  Write the replay to a file, atomically.
	 * @param[in] iFile The file to write.
	 * @return True if the file was written.
	 */
	[[nodiscard]] auto saveFile(const std::filesystem::path& iFile) const -> bool;

	/**
	 * @brief
	 *  Duration of the session, the sum of the frame durations.
	 * @return The duration in seconds.
	 */
	[[nodiscard]] auto getSeconds() const -> double;
};

/**
 * @brief
 *  Where a replay of a scene stands (`Scene::getReplayProgress`).
 */
struct OWL_API ReplayProgress {
	/// Frames already replayed.
	uint64_t frame{0};
	/// Frames of the replay.
	uint64_t frameCount{0};
	/// Checkpoints compared so far.
	uint64_t checkedCount{0};
	/// Frames run at the first world hash differing from the recording, 0 when the start state already differs.
	std::optional<uint64_t> divergedFrame;
	/// Whether every frame was replayed (the scene reads the devices again).
	bool finished{false};
};

}// namespace owl::scene
