/**
 * @file StateMachine.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/animation/StateMachine.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>

namespace owl::data::animation {

namespace {

template<typename T>
auto findByName(const std::vector<T>& iItems, const std::string_view iName) -> std::optional<size_t> {
	const auto found = std::ranges::find(iItems, iName, &T::name);
	if (found == iItems.end())
		return std::nullopt;
	return static_cast<size_t>(std::distance(iItems.begin(), found));
}

auto findClip(const std::span<const SkeletalClip> iClips, const std::string_view iName) -> int32_t {
	if (iName.empty())
		return -1;
	const auto found = std::ranges::find(iClips, iName, &SkeletalClip::name);
	return found == iClips.end() ? -1 : static_cast<int32_t>(std::distance(iClips.begin(), found));
}

}// namespace

auto StateMachine::findState(const std::string_view iName) const -> std::optional<size_t> {
	return findByName(states, iName);
}

auto StateMachine::findParameter(const std::string_view iName) const -> std::optional<size_t> {
	return findByName(parameters, iName);
}

auto StateMachine::getEntryState() const -> std::optional<size_t> {
	if (states.empty())
		return std::nullopt;
	if (const auto entry = findState(entryState); entry)
		return entry;
	return size_t{0};
}

void StateMachinePlayer::reset(const StateMachine& iMachine, const std::span<const SkeletalClip> iClips) {
	m_values.clear();
	for (const auto& parameter: iMachine.parameters)
		m_values.push_back(parameter.type == ParameterType::Trigger ? 0.f : parameter.value);
	m_stateClips.clear();
	for (const auto& state: iMachine.states) {
		const int32_t clip = findClip(iClips, state.clip);
		if (clip < 0 && !state.clip.empty())
			OWL_CORE_WARN("Animator: Clip {} of state {} not found, the state holds the rest pose.", state.clip,
						  state.name)
		m_stateClips.push_back(clip);
	}
	const auto entry = iMachine.getEntryState();
	m_current = entry ? static_cast<int32_t>(*entry) : -1;
	m_previous = -1;
	m_time = 0.f;
	m_previousTime = 0.f;
	m_blendElapsed = 0.f;
	m_blendDuration = 0.f;
	m_ready = true;
}

auto StateMachinePlayer::setParameter(const StateMachine& iMachine, const std::string_view iName, const float iValue)
		-> bool {
	const auto index = iMachine.findParameter(iName);
	if (!index || *index >= m_values.size()) {
		OWL_CORE_WARN("Animator: No parameter {}.", iName)
		return false;
	}
	const bool flag = iMachine.parameters[*index].type != ParameterType::Float;
	const float flagValue = std::abs(iValue) >= 0.5f ? 1.f : 0.f;
	m_values[*index] = flag ? flagValue : iValue;
	return true;
}

auto StateMachinePlayer::getParameter(const StateMachine& iMachine, const std::string_view iName) const
		-> std::optional<float> {
	const auto index = iMachine.findParameter(iName);
	if (!index || *index >= m_values.size())
		return std::nullopt;
	return m_values[*index];
}

auto StateMachinePlayer::play(const StateMachine& iMachine, const std::string_view iState, const float iDuration)
		-> bool {
	const auto state = iMachine.findState(iState);
	if (!state) {
		OWL_CORE_WARN("Animator: No state {}.", iState)
		return false;
	}
	enter(*state, iDuration);
	return true;
}

auto StateMachinePlayer::holds(const StateMachine& iMachine, const StateTransition& iTransition,
							   const std::span<const SkeletalClip> iClips) const -> bool {
	if (iTransition.exitTime >= 0.f && getNormalizedTime(iClips) < iTransition.exitTime)
		return false;
	if (iTransition.exitTime < 0.f && iTransition.conditions.empty())
		return getNormalizedTime(iClips) >= 1.f;
	for (const auto& condition: iTransition.conditions) {
		const auto index = iMachine.findParameter(condition.parameter);
		if (!index || *index >= m_values.size())
			return false;
		const float value = m_values[*index];
		switch (condition.op) {
			case ConditionOp::Greater:
				if (!(value > condition.threshold))
					return false;
				break;
			case ConditionOp::Less:
				if (!(value < condition.threshold))
					return false;
				break;
			case ConditionOp::IsTrue:
				if (value < 0.5f)
					return false;
				break;
			case ConditionOp::IsFalse:
				if (value >= 0.5f)
					return false;
				break;
		}
	}
	return true;
}

void StateMachinePlayer::enter(const size_t iState, const float iDuration) {
	if (iDuration > 0.f && m_current >= 0) {
		m_previous = m_current;
		m_previousTime = m_time;
		m_blendDuration = iDuration;
	} else {
		m_previous = -1;
		m_blendDuration = 0.f;
	}
	m_blendElapsed = 0.f;
	m_current = static_cast<int32_t>(iState);
	m_time = 0.f;
}

void StateMachinePlayer::update(const StateMachine& iMachine, const std::span<const SkeletalClip> iClips,
								const float iSeconds) {
	if (!m_ready || m_current < 0 || static_cast<size_t>(m_current) >= iMachine.states.size())
		return;
	m_time += iSeconds * iMachine.states[static_cast<size_t>(m_current)].speed;
	if (m_previous >= 0) {
		if (static_cast<size_t>(m_previous) < iMachine.states.size())
			m_previousTime += iSeconds * iMachine.states[static_cast<size_t>(m_previous)].speed;
		m_blendElapsed += iSeconds;
		if (m_blendElapsed >= m_blendDuration)
			m_previous = -1;
	}
	const auto& current = iMachine.states[static_cast<size_t>(m_current)];
	for (const auto& transition: iMachine.transitions) {
		if (transition.from != StateMachine::g_anyState && transition.from != current.name)
			continue;
		const auto target = iMachine.findState(transition.to);
		if (!target || static_cast<int32_t>(*target) == m_current || !holds(iMachine, transition, iClips))
			continue;
		for (const auto& condition: transition.conditions) {
			if (const auto index = iMachine.findParameter(condition.parameter);
				index && iMachine.parameters[*index].type == ParameterType::Trigger)
				m_values[*index] = 0.f;
		}
		enter(*target, transition.duration);
		break;
	}
}

void StateMachinePlayer::sampleState(const StateMachine& iMachine, const size_t iState, const float iTime,
									 const std::span<const SkeletalClip> iClips, Pose& ioPose) const {
	if (iState >= m_stateClips.size() || iState >= iMachine.states.size())
		return;
	const int32_t clip = m_stateClips[iState];
	if (clip < 0 || static_cast<size_t>(clip) >= iClips.size())
		return;
	iClips[static_cast<size_t>(clip)].sample(iTime, iMachine.states[iState].loop, ioPose);
}

void StateMachinePlayer::evaluate(const StateMachine& iMachine, const Skeleton& iSkeleton,
								  const std::span<const SkeletalClip> iClips, Pose& oResult) {
	const auto& bones = iSkeleton.getBones();
	oResult.resize(bones.size());
	for (size_t index = 0; index < bones.size(); ++index) oResult[index] = bones[index].rest;
	if (m_current < 0)
		return;
	sampleState(iMachine, static_cast<size_t>(m_current), m_time, iClips, oResult);
	if (m_previous < 0)
		return;
	m_previousPose.resize(bones.size());
	for (size_t index = 0; index < bones.size(); ++index) m_previousPose[index] = bones[index].rest;
	sampleState(iMachine, static_cast<size_t>(m_previous), m_previousTime, iClips, m_previousPose);
	blendPoses(m_previousPose, oResult, getBlendWeight(), oResult);
}

auto StateMachinePlayer::getCurrentState() const -> std::optional<size_t> {
	if (!m_ready || m_current < 0)
		return std::nullopt;
	return static_cast<size_t>(m_current);
}

auto StateMachinePlayer::getBlendWeight() const -> float {
	if (m_previous < 0 || m_blendDuration <= 0.f)
		return 1.f;
	return std::clamp(m_blendElapsed / m_blendDuration, 0.f, 1.f);
}

auto StateMachinePlayer::getNormalizedTime(const std::span<const SkeletalClip> iClips) const -> float {
	if (m_current < 0 || static_cast<size_t>(m_current) >= m_stateClips.size())
		return 0.f;
	const int32_t clip = m_stateClips[static_cast<size_t>(m_current)];
	if (clip < 0 || static_cast<size_t>(clip) >= iClips.size() || iClips[static_cast<size_t>(clip)].duration <= 0.f)
		return 0.f;
	return m_time / iClips[static_cast<size_t>(clip)].duration;
}

}// namespace owl::data::animation
