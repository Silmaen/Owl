/**
 * @file InputFrame.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "input/InputFrame.h"

namespace owl::input {

void InputSampler::reset(const InputState& iState) {
	m_latest = iState;
	m_latest.wheel = {0.f, 0.f};
	m_pending = m_latest;
	m_observedSinceTick = false;
}

void InputSampler::observe(const InputState& iFrame) {
	if (m_observedSinceTick) {
		m_pending.keys |= iFrame.keys;
		m_pending.mouseButtons |= iFrame.mouseButtons;
		m_pending.cursor = iFrame.cursor;
		m_pending.wheel += iFrame.wheel;
	} else {
		m_pending = iFrame;
	}
	m_latest = iFrame;
	m_observedSinceTick = true;
}

auto InputSampler::take() -> InputState {
	const InputState tick = m_pending;
	m_pending = m_latest;
	m_pending.wheel = {0.f, 0.f};
	m_observedSinceTick = false;
	return tick;
}

}// namespace owl::input
