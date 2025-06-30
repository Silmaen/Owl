/**
 * @file Window.cpp
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include <stb_image.h>

#include "Window.h"
#include "core/Log.h"
#include "debug/Profiler.h"
#include "event/AppEvent.h"
#include "event/KeyEvent.h"
#include "event/MouseEvent.h"
#include "renderer/RenderAPI.h"
#include "renderer/RenderCommand.h"

namespace owl::window::glfw {

namespace {
uint8_t g_GlfwWindowCount = 0;

void glfwErrorCallback(int iError, const char* iDescription) {
	OWL_CORE_ERROR("GLFW Error ({}): {}", iError, iDescription)
}
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
	OWL_SCOPE_UNTRACK

	// Initializations
	{
		m_windowData.title = iProps.title;
		m_windowData.size = {iProps.width, iProps.height};

		OWL_CORE_INFO("Creating window {} ({}, {})", iProps.title, iProps.width, iProps.height)

		if (g_GlfwWindowCount == 0) {
			OWL_PROFILE_SCOPE("glfwInit")
			[[maybe_unused]] const int success = glfwInit();
			OWL_CORE_ASSERT(success, "Could not initialize GLFW!")
			glfwSetErrorCallback(glfwErrorCallback);
		}
	}
	// window creation.
	{
		OWL_PROFILE_SCOPE("glfwCreateWindow")
		const auto api = renderer::RenderCommand::getApi();
		if (api == renderer::RenderAPI::Type::Vulkan) {
			if (glfwVulkanSupported() == GLFW_FALSE) {
				OWL_CORE_CRITICAL("No Vulkan support for glfw.")
				return;
			}
		}
#if defined(OWL_DEBUG)
		if (api == renderer::RenderAPI::Type::OpenGL)
			glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
		if (api == renderer::RenderAPI::Type::Vulkan)
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		{
			mp_glfwWindow = glfwCreateWindow(static_cast<int>(iProps.width), static_cast<int>(iProps.height),
											 m_windowData.title.c_str(), nullptr, nullptr);
		}
		++g_GlfwWindowCount;
	}
	// Set icon
	{
		GLFWimage icon;
		int channels = 0;
		if (!iProps.iconPath.empty()) {
			icon.pixels = stbi_load(iProps.iconPath.c_str(), &icon.width, &icon.height, &channels, 4);
			glfwSetWindowIcon(mp_glfwWindow, 1, &icon);
			stbi_image_free(icon.pixels);
		}
	}
	// Graph context
	{
		m_context = renderer::GraphContext::create(mp_glfwWindow);
		m_context->init();

		glfwSetWindowUserPointer(mp_glfwWindow, &m_windowData);
		setVSync(true);
	}
	// Set GLFW callbacks
	{
		glfwSetWindowSizeCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const int iWidth, const int iHeight) {
			auto* const data = static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow));
			data->size.x() = static_cast<uint32_t>(iWidth);
			data->size.y() = static_cast<uint32_t>(iHeight);

			event::WindowResizeEvent event(data->size);
			data->eventCallback(event);
		});

		glfwSetWindowCloseCallback(mp_glfwWindow, [](GLFWwindow* iWindow) {
			event::WindowCloseEvent event;
			static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
		});
		glfwSetKeyCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const int iKey, [[maybe_unused]] int iScancode,
											 const int iAction, [[maybe_unused]] int iMods) {
			const auto cKey = static_cast<input::KeyCode>(iKey);
			switch (iAction) {
				case GLFW_PRESS:
					{
						event::KeyPressedEvent event(cKey, 0u);
						static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
						break;
					}
				case GLFW_RELEASE:
					{
						event::KeyReleasedEvent event(cKey);
						static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
						break;
					}
				case GLFW_REPEAT:
					{
						event::KeyPressedEvent event(cKey, 1u);
						static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
						break;
					}
				default:
					break;
			}
		});

		glfwSetCharCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const unsigned int iKeycode) {
			event::KeyTypedEvent event(static_cast<input::KeyCode>(iKeycode));
			static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
		});

		glfwSetMouseButtonCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const int iButton, const int iAction,
													 [[maybe_unused]] const int iMods) {
			switch (iAction) {
				case GLFW_PRESS:
					{
						event::MouseButtonPressedEvent event(static_cast<input::MouseCode>(iButton));
						static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
						break;
					}
				case GLFW_RELEASE:
					{
						event::MouseButtonReleasedEvent event(static_cast<input::MouseCode>(iButton));
						static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
						break;
					}
				default:
					break;
			}
		});

		glfwSetScrollCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const double iXOffset, const double iYOffset) {
			event::MouseScrolledEvent event(static_cast<float>(iXOffset), static_cast<float>(iYOffset));
			static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
		});

		glfwSetCursorPosCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const double iX, const double iY) {
			event::MouseMovedEvent event(static_cast<float>(iX), static_cast<float>(iY));
			static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
		});
	}
}

void Window::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (mp_glfwWindow == nullptr)
		return;
	m_context->waitIdle();
	glfwDestroyWindow(mp_glfwWindow);
	--g_GlfwWindowCount;
	mp_glfwWindow = nullptr;

	if (g_GlfwWindowCount == 0) {
		glfwTerminate();
	}
}


void Window::onUpdate() {
	OWL_PROFILE_FUNCTION()

	glfwPollEvents();
	m_context->swapBuffers();
}

void Window::setVSync(const bool iEnabled) {
	OWL_PROFILE_FUNCTION()

	if (const auto api = renderer::RenderCommand::getApi(); api == renderer::RenderAPI::Type::OpenGL) {
		if (iEnabled)
			glfwSwapInterval(1);
		else
			glfwSwapInterval(0);
	}

	m_windowData.vSync = iEnabled;
}

auto Window::isVSync() const -> bool { return m_windowData.vSync; }

}// namespace owl::window::glfw
