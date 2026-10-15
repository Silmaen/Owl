/**
 * @file ActionFrame.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "input/ActionMap.h"
#include "input/InputFrame.h"
#include "math/vectors.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace owl::input {

/// Reading from which an `Axis` or `Vector` action counts as held (its edges are the crossings of this level).
inline constexpr float g_actionThreshold = 0.5f;

/**
 * @brief
 *  Value of one action at one sampling point.
 */
struct OWL_API ActionState {
	/// Whether the action is held (a button down, an axis or vector past `g_actionThreshold`).
	bool held{false};
	/// Whether the action went from released to held since the previous sample.
	bool pressed{false};
	/// Whether the action went from held to released since the previous sample.
	bool released{false};
	/// Reading of a button (0 or 1) or of an axis, in [-1, 1].
	float value{0.f};
	/// Reading of a vector action (x right, y down), in the unit disc.
	math::vec2 vector{0.f, 0.f};
};

/**
 * @brief
 *  Reading of an action from one input state, without edges.
 *
 * Pure function: the bindings of the action are read, scaled and added, then clamped to the unit range.
 * @param[in] iAction The action.
 * @param[in] iState The input state.
 * @return The reading: `held`, `value` and `vector` are set, `pressed` and `released` stay false.
 */
[[nodiscard]] OWL_API auto sampleAction(const ActionDefinition& iAction, const InputState& iState) -> ActionState;

/**
 * @brief
 *  Reading of an action between two input samples, with its edges.
 * @param[in] iAction The action.
 * @param[in] iInput The previous and the current input state.
 * @return The reading at the current sample; `pressed` / `released` on the edges between the samples.
 */
[[nodiscard]] OWL_API auto evaluateAction(const ActionDefinition& iAction, const InputFrame& iInput) -> ActionState;

/**
 * @brief
 *  The state of every action of a map at one sampling point: the actions counterpart of `InputFrame`.
 *
 * A scene keeps one per simulation tick and one per frame next to its input frames; plain data, ready to be recorded
 * by a replay. An unknown action reads as released, zero.
 */
class OWL_API ActionFrame {
public:
	/**
	 * @brief
	 *  Evaluate every action of a map.
	 * @param[in] iMap The action map (kept by the frame).
	 * @param[in] iInput The previous and the current input state.
	 */
	void update(const shared<const ActionMap>& iMap, const InputFrame& iInput);

	/**
	 * @brief
	 *  The map the frame was evaluated with.
	 * @return The map, null before the first `update`.
	 */
	[[nodiscard]] auto getMap() const -> const shared<const ActionMap>& { return m_map; }

	/**
	 * @brief
	 *  States, in the order of `ActionMap::getActions()`.
	 * @return The states.
	 */
	[[nodiscard]] auto getStates() const -> const std::vector<ActionState>& { return m_states; }

	/**
	 * @brief
	 *  Whether the map defines an action.
	 * @param[in] iName The action name.
	 * @return True if the action exists.
	 */
	[[nodiscard]] auto has(std::string_view iName) const -> bool;

	/**
	 * @brief
	 *  State of an action.
	 * @param[in] iName The action name.
	 * @return The state, a released one for an unknown action.
	 */
	[[nodiscard]] auto get(std::string_view iName) const -> const ActionState&;

	/**
	 * @brief
	 *  Whether an action is held at the current sample.
	 * @param[in] iName The action name.
	 * @return True if held.
	 */
	[[nodiscard]] auto isHeld(const std::string_view iName) const -> bool { return get(iName).held; }

	/**
	 * @brief
	 *  Whether an action went down between the previous and the current sample.
	 * @param[in] iName The action name.
	 * @return True on the press edge.
	 */
	[[nodiscard]] auto isJustPressed(const std::string_view iName) const -> bool { return get(iName).pressed; }

	/**
	 * @brief
	 *  Whether an action went up between the previous and the current sample.
	 * @param[in] iName The action name.
	 * @return True on the release edge.
	 */
	[[nodiscard]] auto isJustReleased(const std::string_view iName) const -> bool { return get(iName).released; }

	/**
	 * @brief
	 *  Reading of a button or an axis action.
	 * @param[in] iName The action name.
	 * @return The value, 0 for an unknown action.
	 */
	[[nodiscard]] auto getValue(const std::string_view iName) const -> float { return get(iName).value; }

	/**
	 * @brief
	 *  Reading of a vector action.
	 * @param[in] iName The action name.
	 * @return The vector (x right, y down), zero for an unknown action.
	 */
	[[nodiscard]] auto getVector(const std::string_view iName) const -> math::vec2 { return get(iName).vector; }

private:
	/// Map of the last update.
	shared<const ActionMap> m_map;
	/// State of each action of the map.
	std::vector<ActionState> m_states;
};

}// namespace owl::input
