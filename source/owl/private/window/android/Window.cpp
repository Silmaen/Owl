/**
 * @file Window.cpp
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "Window.h"

#include "core/Log.h"
#include "debug/Profiler.h"
#include "event/AppEvent.h"
#include "event/KeyEvent.h"
#include "event/MouseEvent.h"
#include "input/android/Input.h"
#include "platform/AndroidApp.h"
#include "renderer/gpu/RenderCommand.h"
#if OWL_WITH_GUI
#include "core/external/imgui.h"
#endif

#include <android/configuration.h>
#include <android/input.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

namespace owl::window::android {

Window::Window(const Properties& iProps) : mp_app{platform::android::getApp()}, m_title{iProps.title} {
	OWL_PROFILE_FUNCTION()

	if (mp_app == nullptr) {
		OWL_CORE_ERROR("Android: No native activity registered, the window cannot open.")
		return;
	}
	mp_app->userData = this;
	mp_app->onAppCmd = [](android_app* ioApp, const int32_t iCommand) {
		static_cast<Window*>(ioApp->userData)->handleCommand(iCommand);
	};
	mp_app->onInputEvent = [](android_app* ioApp, AInputEvent* iEvent) -> int32_t {
		return static_cast<Window*>(ioApp->userData)->handleInput(iEvent) ? 1 : 0;
	};
	// The activity hands its window over asynchronously: wait for it, or for the end of the activity.
	while (mp_app->window == nullptr && mp_app->destroyRequested == 0) pollEvents(-1);
	if (mp_app->window == nullptr) {
		OWL_CORE_WARN("Android: The activity ended before its window was ready.")
		requestClose();
		return;
	}
	if (mp_app->config != nullptr) {
		if (const int32_t density = AConfiguration_getDensity(mp_app->config);
			density != ACONFIGURATION_DENSITY_DEFAULT && density != ACONFIGURATION_DENSITY_NONE)
			m_contentScale = static_cast<float>(density) / static_cast<float>(ACONFIGURATION_DENSITY_MEDIUM);
	}
	m_size = {static_cast<uint32_t>(ANativeWindow_getWidth(mp_app->window)),
			  static_cast<uint32_t>(ANativeWindow_getHeight(mp_app->window))};
	OWL_CORE_DEBUG("Android: Window '{}' ready ({}x{}, content scale {}).", m_title, m_size.x(), m_size.y(),
				   m_contentScale)
	m_context = renderer::gpu::GraphContext::create(mp_app->window);
	m_context->init();
	setVSync(true);
}

Window::~Window() {
	OWL_PROFILE_FUNCTION()

	shutdown();
}

void Window::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (m_context)
		m_context->waitIdle();
	if (mp_app != nullptr && mp_app->userData == this) {
		mp_app->onAppCmd = nullptr;
		mp_app->onInputEvent = nullptr;
		mp_app->userData = nullptr;
	}
}

void Window::onUpdate() {
	OWL_PROFILE_FUNCTION()

	if (mp_app == nullptr)
		return;
	if (m_context && mp_app->window != nullptr) {
		m_context->swapBuffers();
		++m_presentedFrames;
	}
	if (mp_app->window == nullptr || mp_app->destroyRequested != 0)
		requestClose();
	pollEvents(0);
}

void Window::pollEvents(const int iTimeoutMs) {
	int timeout = iTimeoutMs;
	android_poll_source* source = nullptr;
	while (ALooper_pollOnce(timeout, nullptr, nullptr, reinterpret_cast<void**>(&source)) >= 0) {
		if (source != nullptr)
			source->process(mp_app, source);
		if (mp_app->destroyRequested != 0) {
			requestClose();
			return;
		}
		// Only the first wait blocks; the rest of the queue is drained at once.
		timeout = 0;
	}
}

void Window::waitEvents(const std::chrono::milliseconds iTimeout) {
	if (mp_app != nullptr)
		pollEvents(static_cast<int>(iTimeout.count()));
}

void Window::wakeUp() {
	if (mp_app != nullptr && mp_app->looper != nullptr)
		ALooper_wake(mp_app->looper);
}

auto Window::getNativeWindow() const -> void* { return mp_app != nullptr ? mp_app->window : nullptr; }

void Window::setVSync(const bool iEnabled) {
	m_vSync = iEnabled;
	renderer::gpu::RenderCommand::setVSync(iEnabled);
	renderer::gpu::RenderCommand::setLowLatencyPresent(m_lowLatencyPresent);
}

void Window::setLowLatencyPresent(const bool iEnabled) {
	m_lowLatencyPresent = iEnabled;
	setVSync(m_vSync);
}

void Window::handleCommand(const int32_t iCommand) {
	switch (iCommand) {
		case APP_CMD_WINDOW_RESIZED:
		case APP_CMD_CONFIG_CHANGED:
			updateSize();
			break;
		case APP_CMD_GAINED_FOCUS:
			m_focused = true;
			break;
		case APP_CMD_LOST_FOCUS:
			m_focused = false;
			break;
		// The surface cannot outlive its window: the game ends with it.
		case APP_CMD_TERM_WINDOW:
		case APP_CMD_DESTROY:
			requestClose();
			break;
		default:
			break;
	}
}

auto Window::handleInput(const AInputEvent* iEvent) -> bool {
#if OWL_WITH_GUI
	// The UI layer reads the touches and the keys through the ImGui Android backend.
	if (ImGui::GetCurrentContext() != nullptr)
		ImGui_ImplAndroid_HandleInputEvent(iEvent);
#endif
	auto& state = input::android::Input::deviceState();
	if (AInputEvent_getType(iEvent) == AINPUT_EVENT_TYPE_KEY) {
		const auto key = input::android::toKeyCode(AKeyEvent_getKeyCode(iEvent));
		if (!key.has_value())
			return false;
		const int32_t action = AKeyEvent_getAction(iEvent);
		if (action == AKEY_EVENT_ACTION_DOWN) {
			state.setKey(*key, true);
			event::KeyPressedEvent event(*key, static_cast<uint16_t>(AKeyEvent_getRepeatCount(iEvent)));
			if (m_eventCallback)
				m_eventCallback(event);
		} else if (action == AKEY_EVENT_ACTION_UP) {
			state.setKey(*key, false);
			event::KeyReleasedEvent event(*key);
			if (m_eventCallback)
				m_eventCallback(event);
		}
		return true;
	}
	if (AInputEvent_getType(iEvent) != AINPUT_EVENT_TYPE_MOTION)
		return false;
	// The first finger drives the cursor and the left button; the other fingers wait for the gesture support.
	const int32_t action = AMotionEvent_getAction(iEvent) & AMOTION_EVENT_ACTION_MASK;
	const math::vec2 position{AMotionEvent_getX(iEvent, 0), AMotionEvent_getY(iEvent, 0)};
	if (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_MOVE ||
		action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
		state.cursor = position;
		event::MouseMovedEvent moved(position.x(), position.y());
		if (m_eventCallback)
			m_eventCallback(moved);
	}
	if (action == AMOTION_EVENT_ACTION_DOWN) {
		state.setMouseButton(input::mouse::ButtonLeft, true);
		event::MouseButtonPressedEvent pressed(input::mouse::ButtonLeft);
		if (m_eventCallback)
			m_eventCallback(pressed);
	} else if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
		state.setMouseButton(input::mouse::ButtonLeft, false);
		event::MouseButtonReleasedEvent released(input::mouse::ButtonLeft);
		if (m_eventCallback)
			m_eventCallback(released);
	}
	return true;
}

void Window::updateSize() {
	if (mp_app->window == nullptr)
		return;
	const math::vec2ui size{static_cast<uint32_t>(ANativeWindow_getWidth(mp_app->window)),
							static_cast<uint32_t>(ANativeWindow_getHeight(mp_app->window))};
	if (size == m_size)
		return;
	m_size = size;
	event::WindowResizeEvent event(m_size);
	if (m_eventCallback)
		m_eventCallback(event);
}

void Window::requestClose() {
	// Before the application sets its callback, the request waits for the next poll.
	if (m_closeSent || !m_eventCallback)
		return;
	m_closeSent = true;
	OWL_CORE_INFO("Android: The activity window is gone, closing the application.")
	event::WindowCloseEvent event;
	m_eventCallback(event);
}

}// namespace owl::window::android
