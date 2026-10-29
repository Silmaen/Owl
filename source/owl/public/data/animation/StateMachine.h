/**
 * @file StateMachine.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/animation/SkeletalClip.h"
#include "data/animation/Skeleton.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::data::animation {

/**
 * @brief
 *  Type of a state machine parameter.
 */
enum struct ParameterType : uint8_t {
	Float,///< A number.
	Bool,///< A flag (0 or 1).
	Trigger///< A flag set by the game and cleared by the transition it fires.
};

/**
 * @brief
 *  A value the game sets and the transitions read.
 */
struct OWL_API StateParameter {
	/// Name of the parameter.
	std::string name;
	/// Its type.
	ParameterType type = ParameterType::Float;
	/// Default value (0 or 1 for a flag).
	float value = 0.f;
};

/**
 * @brief
 *  Test of a condition on a parameter.
 */
enum struct ConditionOp : uint8_t {
	Greater,///< The value is above the threshold.
	Less,///< The value is below the threshold.
	IsTrue,///< The flag is set (a trigger: it was fired).
	IsFalse///< The flag is clear.
};

/**
 * @brief
 *  One condition of a transition.
 */
struct OWL_API TransitionCondition {
	/// Name of the parameter tested.
	std::string parameter;
	/// The test.
	ConditionOp op = ConditionOp::Greater;
	/// Threshold of `Greater` and `Less`.
	float threshold = 0.f;
};

/**
 * @brief
 *  A state: a clip played at a speed.
 */
struct OWL_API AnimationState {
	/// Name of the state.
	std::string name;
	/// Name of the clip played (a clip of the mesh); empty holds the rest pose.
	std::string clip;
	/// Playback speed (1 is the authored speed).
	float speed = 1.f;
	/// True to loop the clip, false to hold its last key.
	bool loop = true;
};

/**
 * @brief
 *  A cross-fade from one state to another when its conditions hold.
 */
struct OWL_API StateTransition {
	/// Name of the source state, `*` for any state.
	std::string from;
	/// Name of the target state.
	std::string to;
	/// Length of the cross-fade, in seconds (0 cuts).
	float duration = 0.2f;
	/// Normalized time of the source state the transition waits for (1 = the end of the clip); negative for none.
	float exitTime = -1.f;
	/// Conditions that must all hold.
	std::vector<TransitionCondition> conditions;
};

/**
 * @brief
 *  The authored animation graph of a character: states, transitions and parameters.
 */
struct OWL_API StateMachine {
	/// Wildcard source of a transition leaving any state.
	static constexpr std::string_view g_anyState{"*"};

	/// The states.
	std::vector<AnimationState> states;
	/// The transitions, tested in order (the first one that holds fires).
	std::vector<StateTransition> transitions;
	/// The parameters.
	std::vector<StateParameter> parameters;
	/// Name of the state entered first; empty for the first state.
	std::string entryState;

	/**
	 * @brief
	 *  Find a state by its name.
	 * @param[in] iName The name.
	 * @return The index of the state, nothing when absent.
	 */
	[[nodiscard]] auto findState(std::string_view iName) const -> std::optional<size_t>;

	/**
	 * @brief
	 *  Find a parameter by its name.
	 * @param[in] iName The name.
	 * @return The index of the parameter, nothing when absent.
	 */
	[[nodiscard]] auto findParameter(std::string_view iName) const -> std::optional<size_t>;

	/**
	 * @brief
	 *  Get the state entered first.
	 * @return Its index, nothing without state.
	 */
	[[nodiscard]] auto getEntryState() const -> std::optional<size_t>;
};

/**
 * @brief
 *  Plays a state machine: parameter values, the current state, the cross-fade, and the resulting pose.
 */
class OWL_API StateMachinePlayer final {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	StateMachinePlayer() = default;

	/**
	 * @brief
	 *  Default destructor.
	 */
	~StateMachinePlayer() = default;

	/**
	 * @brief
	 *  Default copy constructor.
	 */
	StateMachinePlayer(const StateMachinePlayer&) = default;

	/**
	 * @brief
	 *  Default move constructor.
	 */
	StateMachinePlayer(StateMachinePlayer&&) noexcept = default;

	/**
	 * @brief
	 *  Default copy assignment.
	 * @return This player.
	 */
	auto operator=(const StateMachinePlayer&) -> StateMachinePlayer& = default;

	/**
	 * @brief
	 *  Default move assignment.
	 * @return This player.
	 */
	auto operator=(StateMachinePlayer&&) noexcept -> StateMachinePlayer& = default;

	/**
	 * @brief
	 *  Start the machine in its entry state with the default parameter values, the clips resolved by name.
	 * @param[in] iMachine The machine.
	 * @param[in] iClips The clips the states name.
	 */
	void reset(const StateMachine& iMachine, std::span<const SkeletalClip> iClips);

	/**
	 * @brief
	 *  Check if the player was reset.
	 * @return True once `reset` ran.
	 */
	[[nodiscard]] auto isReady() const -> bool { return m_ready; }

	/**
	 * @brief
	 *  Forget the machine, so that the next update resets it.
	 */
	void invalidate() { m_ready = false; }

	/**
	 * @brief
	 *  Set a parameter.
	 * @param[in] iMachine The machine.
	 * @param[in] iName Name of the parameter.
	 * @param[in] iValue The value (a magnitude of 0.5 or more sets a flag).
	 * @return False when the machine has no such parameter.
	 */
	auto setParameter(const StateMachine& iMachine, std::string_view iName, float iValue) -> bool;

	/**
	 * @brief
	 *  Get a parameter.
	 * @param[in] iMachine The machine.
	 * @param[in] iName Name of the parameter.
	 * @return Its value, nothing when the machine has no such parameter.
	 */
	[[nodiscard]] auto getParameter(const StateMachine& iMachine, std::string_view iName) const -> std::optional<float>;

	/**
	 * @brief
	 *  Cross-fade to a state, whatever the transitions.
	 * @param[in] iMachine The machine.
	 * @param[in] iState Name of the state.
	 * @param[in] iDuration Length of the cross-fade, in seconds.
	 * @return False when the machine has no such state.
	 */
	auto play(const StateMachine& iMachine, std::string_view iState, float iDuration) -> bool;

	/**
	 * @brief
	 *  Advance the time and fire the first transition that holds.
	 * @param[in] iMachine The machine.
	 * @param[in] iClips The clips given to `reset`.
	 * @param[in] iSeconds Elapsed time.
	 */
	void update(const StateMachine& iMachine, std::span<const SkeletalClip> iClips, float iSeconds);

	/**
	 * @brief
	 *  Compute the pose of the current state, cross-faded with the previous one.
	 * @param[in] iMachine The machine.
	 * @param[in] iSkeleton The skeleton the clips animate.
	 * @param[in] iClips The clips given to `reset`.
	 * @param[out] oResult The pose.
	 */
	void evaluate(const StateMachine& iMachine, const Skeleton& iSkeleton, std::span<const SkeletalClip> iClips,
				  Pose& oResult);

	/**
	 * @brief
	 *  Get the current state.
	 * @return Its index, nothing before a reset or without state.
	 */
	[[nodiscard]] auto getCurrentState() const -> std::optional<size_t>;

	/**
	 * @brief
	 *  Get the time spent in the current state, scaled by its speed.
	 * @return The time, in seconds.
	 */
	[[nodiscard]] auto getStateTime() const -> float { return m_time; }

	/**
	 * @brief
	 *  Get the weight of the current state in the cross-fade.
	 * @return 1 outside a cross-fade.
	 */
	[[nodiscard]] auto getBlendWeight() const -> float;

	/**
	 * @brief
	 *  Get the normalized time of the current state (1 at the end of its clip, above 1 after a loop).
	 * @param[in] iClips The clips given to `reset`.
	 * @return The normalized time, 0 without clip.
	 */
	[[nodiscard]] auto getNormalizedTime(std::span<const SkeletalClip> iClips) const -> float;

private:
	/**
	 * @brief
	 *  Check the conditions of a transition.
	 * @param[in] iMachine The machine.
	 * @param[in] iTransition The transition.
	 * @param[in] iClips The clips.
	 * @return True when it fires.
	 */
	[[nodiscard]] auto holds(const StateMachine& iMachine, const StateTransition& iTransition,
							 std::span<const SkeletalClip> iClips) const -> bool;

	/**
	 * @brief
	 *  Enter a state with a cross-fade.
	 * @param[in] iState Index of the state.
	 * @param[in] iDuration Length of the cross-fade.
	 */
	void enter(size_t iState, float iDuration);

	/**
	 * @brief
	 *  Sample the clip of a state into a pose.
	 * @param[in] iMachine The machine.
	 * @param[in] iState Index of the state.
	 * @param[in] iTime Time in the state.
	 * @param[in] iClips The clips.
	 * @param[in,out] ioPose The pose, at rest on entry.
	 */
	void sampleState(const StateMachine& iMachine, size_t iState, float iTime, std::span<const SkeletalClip> iClips,
					 Pose& ioPose) const;

	/// Current value of every parameter, in the machine order.
	std::vector<float> m_values;
	/// Clip of every state (index in the clips), -1 without one.
	std::vector<int32_t> m_stateClips;
	/// Current state, -1 without one.
	int32_t m_current = -1;
	/// State faded out, -1 outside a cross-fade.
	int32_t m_previous = -1;
	/// Time in the current state.
	float m_time = 0.f;
	/// Time in the state faded out.
	float m_previousTime = 0.f;
	/// Time elapsed in the cross-fade.
	float m_blendElapsed = 0.f;
	/// Length of the cross-fade.
	float m_blendDuration = 0.f;
	/// Pose of the state faded out, kept between frames to avoid allocations.
	Pose m_previousPose;
	/// True once reset.
	bool m_ready = false;
};

}// namespace owl::data::animation
