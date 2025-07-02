/**
 * @file Window.cpp
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "Window.h"
#include "core/Log.h"
#include "debug/Profiler.h"

namespace owl::window::null {
namespace {
uint8_t s_winCount = 0;
}// namespace

Window::Window(const Properties& iProps) {
	OWL_PROFILE_FUNCTION()
	init(iProps);
}

Window::~Window() {
	OWL_PROFILE_FUNCTION()

	shutdown();
}

void Window::init(const Properties& iProps) {
	OWL_PROFILE_FUNCTION()
	m_windowData.title = iProps.title;
	m_windowData.size = {iProps.width, iProps.height};

	OWL_CORE_INFO("Creating window {} ({}, {})", iProps.title, iProps.width, iProps.height)
	++s_winCount;
	setVSync(true);
}

void Window::shutdown() {
	OWL_PROFILE_FUNCTION()
	--s_winCount;
}


void Window::onUpdate() { OWL_PROFILE_FUNCTION() }

void Window::setVSync(bool iEnabled) {
	OWL_PROFILE_FUNCTION()
	m_windowData.vSync = iEnabled;
}

auto Window::isVSync() const -> bool { return m_windowData.vSync; }

}// namespace owl::window::null
