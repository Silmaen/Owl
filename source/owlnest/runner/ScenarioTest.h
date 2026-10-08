/**
 * @file ScenarioTest.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <core/expected.h>
#include <owl.h>
#include <scene/Scene.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief
 *  Runner modes and tools.
 */
namespace owl::nest::runner {

/**
 * @brief
 *  One comparison of an expectation: `equals`, `greater` or `less` on a number, or `equals` on a text or a boolean.
 */
struct ScenarioCheck {
	/// Comparison operator.
	enum struct Op : uint8_t {
		Equals,///< Equal (numbers within `tolerance`).
		Greater,///< Strictly greater.
		Less///< Strictly less.
	};
	/// What is compared: `translation.x`, `rotation.z`, `scale.y`, `exists`, or the game-state value.
	std::string field;
	/// The operator.
	Op op{Op::Equals};
	/// Expected number (numbers and booleans).
	double number{0.0};
	/// Expected text, when the expected value is not a number.
	std::optional<std::string> text;
	/// Tolerance of a numeric `equals`.
	double tolerance{1e-4};
};

/**
 * @brief
 *  One step of a scenario: play frames (holding inputs), or check the world.
 */
struct ScenarioStep {
	/// Frames to play; 0 for an expectation.
	uint32_t frames{0};
	/// Keys held while the frames play.
	std::vector<uint16_t> keys;
	/// Mouse buttons held while the frames play.
	std::vector<uint8_t> mouseButtons;
	/// Mouse position while the frames play, if set.
	std::optional<math::vec2> mousePos;
	/// Entity checked, by tag; empty with `player` or `gameState`.
	std::string entity;
	/// Check the primary player instead of a named entity.
	bool player{false};
	/// Game-state key checked, empty for an entity check.
	std::string gameState;
	/// Comparisons of an expectation.
	std::vector<ScenarioCheck> checks;
	/// Line of the step in the file, for the messages.
	int line{0};
};

/**
 * @brief
 *  Scripted headless run: a scene, then steps that play frames with injected inputs and check the world.
 * The `.owltest` YAML format is described in `doc/pages/editor.md` (Scripted headless runs).
 */
class ScenarioTest final {
public:
	/**
	 * @brief
	 *  Read a scenario file.
	 * @param[in] iFile The `.owltest` file.
	 * @return The scenario, or the reason it cannot be read.
	 */
	[[nodiscard]] static auto load(const std::filesystem::path& iFile) -> expected<ScenarioTest, std::string>;

	/**
	 * @brief
	 *  Parse a key name (`A`..`Z`, `D0`..`D9`, `Space`, `Enter`, `Escape`, `Tab`, `Left`, `Right`, `Up`, `Down`,
	 *  `LeftShift`, `LeftControl`) or a numeric key code.
	 * @param[in] iName The name.
	 * @return The key code, or nothing for an unknown name.
	 */
	[[nodiscard]] static auto parseKey(const std::string& iName) -> std::optional<uint16_t>;

	/**
	 * @brief
	 *  Get the scene to load.
	 * @return The scene file (absolute).
	 */
	[[nodiscard]] auto getScene() const -> const std::filesystem::path& { return m_scene; }

	/**
	 * @brief
	 *  Get the project the scene belongs to.
	 * @return The project directory, empty when none was found.
	 */
	[[nodiscard]] auto getProject() const -> const std::filesystem::path& { return m_project; }

	/**
	 * @brief
	 *  Get the fixed time step.
	 * @return The time step of every frame.
	 */
	[[nodiscard]] auto getTimeStep() const -> const core::Timestep& { return m_step; }

	/**
	 * @brief
	 *  Get the steps.
	 * @return The steps, in order.
	 */
	[[nodiscard]] auto getSteps() const -> const std::vector<ScenarioStep>& { return m_steps; }

	/**
	 * @brief
	 *  Advance before a frame: run the expectations due now on the state left by the previous frames, then set the
	 *  inputs of the frame to play.
	 * @param[in] iScene The running scene.
	 * @return True when the scenario is over (no frame left to play).
	 */
	auto beginFrame(const scene::Scene& iScene) -> bool;

	/**
	 * @brief
	 *  Check one expectation step against a scene.
	 * @param[in] iStep The expectation.
	 * @param[in] iScene The scene.
	 * @return An empty string when it holds, the failure message otherwise.
	 */
	[[nodiscard]] static auto check(const ScenarioStep& iStep, const scene::Scene& iScene) -> std::string;

	/**
	 * @brief
	 *  Get the number of expectations that failed so far.
	 * @return The failure count.
	 */
	[[nodiscard]] auto getFailureCount() const -> uint32_t { return m_failures; }

	/**
	 * @brief
	 *  Get the number of expectations checked so far.
	 * @return The check count.
	 */
	[[nodiscard]] auto getCheckCount() const -> uint32_t { return m_checks; }

private:
	/// Scene to load.
	std::filesystem::path m_scene;
	/// Project of the scene.
	std::filesystem::path m_project;
	/// Fixed time step.
	core::Timestep m_step;
	/// Steps, in order.
	std::vector<ScenarioStep> m_steps;
	/// Index of the current step.
	size_t m_index{0};
	/// Frames left in the current play step.
	uint32_t m_framesLeft{0};
	/// Expectations checked.
	uint32_t m_checks{0};
	/// Expectations failed.
	uint32_t m_failures{0};
};

}// namespace owl::nest::runner
