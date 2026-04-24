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
			// Opt-in: force X11 (via XWayland on a Wayland session) when OWL_FORCE_X11=1.
			// Useful to get glfwSetWindowIcon working at dev time without installing a
			// system-wide .desktop file. Default path stays on the native platform (Wayland
			// when available) to preserve high-refresh-rate presentation.
			if (const char* forceX11 = std::getenv("OWL_FORCE_X11"); (forceX11 != nullptr) && forceX11[0] == '1')
				glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
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
	// Set icon — skipped on Wayland since the protocol doesn't support glfwSetWindowIcon
	// (the compositor derives the icon from a .desktop file matched via app_id).
	if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND) {
		GLFWimage icon;
		int channels = 0;
		if (!iProps.iconPath.empty()) {
			icon.pixels = stbi_load(iProps.iconPath.c_str(), &icon.width, &icon.height, &channels, 4);
			if (icon.pixels != nullptr) {
				glfwSetWindowIcon(mp_glfwWindow, 1, &icon);
				stbi_image_free(icon.pixels);
			} else {
				OWL_CORE_WARN("Failed to load window icon: {}", iProps.iconPath)
			}
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
		glfwSetWindowSizeCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const int iWidth, const int iHeight) -> void {
			auto* const data = static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow));
			data->size.x() = static_cast<uint32_t>(iWidth);
			data->size.y() = static_cast<uint32_t>(iHeight);

			event::WindowResizeEvent event(data->size);
			data->eventCallback(event);
		});

		glfwSetWindowCloseCallback(mp_glfwWindow, [](GLFWwindow* iWindow) -> void {
			event::WindowCloseEvent event;
			static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
		});
		glfwSetKeyCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const int iKey, [[maybe_unused]] int iScancode,
											 const int iAction, [[maybe_unused]] int iMods) -> void {
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

		glfwSetCharCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const unsigned int iKeycode) -> void {
			event::KeyTypedEvent event(static_cast<input::KeyCode>(iKeycode));
			static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
		});

		glfwSetMouseButtonCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const int iButton, const int iAction,
													 [[maybe_unused]] const int iMods) -> void {
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

		glfwSetScrollCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const double iXOffset, const double iYOffset) -> void {
			event::MouseScrolledEvent event(static_cast<float>(iXOffset), static_cast<float>(iYOffset));
			static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
		});

		glfwSetCursorPosCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const double iX, const double iY) -> void {
			event::MouseMovedEvent event(static_cast<float>(iX), static_cast<float>(iY));
			static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
		});

		glfwSetDropCallback(mp_glfwWindow, [](GLFWwindow* iWindow, const int iCount, const char** iPaths) -> void {
			std::vector<std::filesystem::path> paths;
			paths.reserve(static_cast<size_t>(iCount));
			for (int i = 0; i < iCount; ++i)
				paths.emplace_back(iPaths[i]);
			event::FileDropEvent event(std::move(paths));
			static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
		});
	}
}

void Window::setTitle(const std::string& iTitle) {
	m_windowData.title = iTitle;
	if (mp_glfwWindow != nullptr)
		glfwSetWindowTitle(mp_glfwWindow, iTitle.c_str());
}

void Window::setFullscreen(const bool iFullscreen) {
	if (m_windowData.fullscreen == iFullscreen || mp_glfwWindow == nullptr)
		return;
	m_windowData.fullscreen = iFullscreen;
	if (iFullscreen) {
		m_windowData.windowedSize = m_windowData.size;
		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		glfwSetWindowMonitor(mp_glfwWindow, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
	} else {
		glfwSetWindowMonitor(mp_glfwWindow, nullptr, 100, 100,
							 static_cast<int>(m_windowData.windowedSize.x()),
							 static_cast<int>(m_windowData.windowedSize.y()), 0);
	}
}

auto Window::isFullscreen() const -> bool { return m_windowData.fullscreen; }

void Window::setResizable(const bool iResizable) {
	if (mp_glfwWindow != nullptr)
		glfwSetWindowAttrib(mp_glfwWindow, GLFW_RESIZABLE, iResizable ? GLFW_TRUE : GLFW_FALSE);
}

void Window::setSize(const uint32_t iWidth, const uint32_t iHeight) {
	m_windowData.size = {iWidth, iHeight};
	if (mp_glfwWindow != nullptr)
		glfwSetWindowSize(mp_glfwWindow, static_cast<int>(iWidth), static_cast<int>(iHeight));
}

void Window::setIcon(const std::filesystem::path& iIconPath) {
	if (mp_glfwWindow == nullptr)
		return;
	if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
		return;// Wayland compositor picks the icon from a .desktop file, not the app.
	if (!exists(iIconPath)) {
		OWL_CORE_WARN("Window icon not found: {}", iIconPath.string())
		return;
	}
	GLFWimage icon;
	int channels = 0;
	icon.pixels = stbi_load(iIconPath.string().c_str(), &icon.width, &icon.height, &channels, 4);
	if (icon.pixels != nullptr) {
		glfwSetWindowIcon(mp_glfwWindow, 1, &icon);
		stbi_image_free(icon.pixels);
	} else {
		OWL_CORE_WARN("Failed to load window icon: {}", iIconPath.string())
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
