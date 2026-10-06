/**
 * @file Window.cpp
 * @author Silmaen
 * @date 04/12/2022
 * Copyright (c) 2022 All rights reserved.
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
#include "platform/DesktopEntry.h"
#include "renderer/gpu/RenderAPI.h"
#include "renderer/gpu/RenderCommand.h"

#include <thread>

namespace owl::window::glfw {

namespace {
uint8_t g_GlfwWindowCount = 0;
Platform g_GlfwPlatform = Platform::None;

void glfwErrorCallback(int iError, const char* iDescription) {
	if (iError == GLFW_FEATURE_UNAVAILABLE) {
		OWL_CORE_TRACE("GLFW: Feature unavailable on this platform ({}).", iDescription)
		return;
	}
	OWL_CORE_ERROR("GLFW Error ({}): {}.", iError, iDescription)
}

[[nodiscard]] auto toGlfwPlatform(const Platform iPlatform) -> int {
	switch (iPlatform) {
		case Platform::Wayland:
			return GLFW_PLATFORM_WAYLAND;
		case Platform::X11:
			return GLFW_PLATFORM_X11;
		case Platform::Win32:
			return GLFW_PLATFORM_WIN32;
		case Platform::None:
			return GLFW_PLATFORM_NULL;
		case Platform::Auto:
			return GLFW_ANY_PLATFORM;
	}
	return GLFW_ANY_PLATFORM;
}

[[nodiscard]] auto fromGlfwPlatform(const int iPlatform) -> Platform {
	switch (iPlatform) {
		case GLFW_PLATFORM_WAYLAND:
			return Platform::Wayland;
		case GLFW_PLATFORM_X11:
			return Platform::X11;
		case GLFW_PLATFORM_WIN32:
			return Platform::Win32;
		default:
			return Platform::None;
	}
}

auto initGlfw(const Platform iRequested) -> bool {
	OWL_PROFILE_FUNCTION()

	glfwSetErrorCallback(glfwErrorCallback);
	const int hint = toGlfwPlatform(iRequested);
	if (hint != GLFW_ANY_PLATFORM && glfwPlatformSupported(hint) == GLFW_FALSE) {
		OWL_CORE_WARN("GLFW: Platform {} not compiled in, falling back to auto.", platformName(iRequested))
		glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
	} else {
		glfwInitHint(GLFW_PLATFORM, hint);
	}
	bool success = glfwInit() == GLFW_TRUE;
	if (!success && hint != GLFW_ANY_PLATFORM) {
		OWL_CORE_WARN("GLFW: Could not initialise the {} platform, falling back to auto.", platformName(iRequested))
		glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
		success = glfwInit() == GLFW_TRUE;
	}
	g_GlfwPlatform = success ? fromGlfwPlatform(glfwGetPlatform()) : Platform::None;
	return success;
}

[[nodiscard]] auto refreshPeriod(GLFWwindow* iWindow) -> std::chrono::nanoseconds {
	GLFWmonitor* monitor = glfwGetWindowMonitor(iWindow);
	if (monitor == nullptr)
		monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
	const int rate = (mode != nullptr && mode->refreshRate > 0) ? mode->refreshRate : 60;
	return std::chrono::nanoseconds{1'000'000'000LL / rate};
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

		OWL_CORE_INFO("Creating window {} ({}, {}).", iProps.title, iProps.width, iProps.height)

		if (g_GlfwWindowCount == 0) {
			if (!initGlfw(iProps.platform)) {
				OWL_CORE_CRITICAL("GLFW: Could not initialise any platform (no display?).")
				return;
			}
			OWL_CORE_INFO("GLFW: Using the {} platform (requested {}).", platformName(getPlatform()),
						  platformName(iProps.platform))
		}
		m_appId = iProps.appId.empty() ? makeAppId(iProps.title) : iProps.appId;
	}
	// window creation.
	{

		OWL_PROFILE_SCOPE("glfwCreateWindow")
		const auto api = renderer::gpu::RenderCommand::getApi();
		if (api == renderer::gpu::RenderAPI::Type::Vulkan) {
			if (glfwVulkanSupported() == GLFW_FALSE) {
				OWL_CORE_CRITICAL("No Vulkan support for glfw.")
				return;
			}
		}
#if defined(OWL_DEBUG)
		if (api == renderer::gpu::RenderAPI::Type::OpenGL)

			glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
		if (api == renderer::gpu::RenderAPI::Type::Vulkan)

			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHintString(GLFW_WAYLAND_APP_ID, m_appId.c_str());
		glfwWindowHintString(GLFW_X11_CLASS_NAME, m_appId.c_str());
		glfwWindowHintString(GLFW_X11_INSTANCE_NAME, m_appId.c_str());
		// The engine sizes its swapchain and viewports from the window size, so keep framebuffer == window size.
		glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_FALSE);
		mp_glfwWindow = glfwCreateWindow(static_cast<int>(iProps.width), static_cast<int>(iProps.height),
										 m_windowData.title.c_str(), nullptr, nullptr);
		if (mp_glfwWindow == nullptr) {
			OWL_CORE_CRITICAL("GLFW: Window creation failed.")
			return;
		}
		++g_GlfwWindowCount;
		const auto scale = getContentScale();
		OWL_CORE_INFO("GLFW: Window created, app id {}, content scale {}x{}.", m_appId, scale.x(), scale.y())
	}
	initIcon(iProps);
	// Graph context
	{
		m_context = renderer::gpu::GraphContext::create(mp_glfwWindow);
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

		glfwSetKeyCallback(
				mp_glfwWindow,
				[](GLFWwindow* iWindow, const int iKey, [[maybe_unused]] int iScancode, const int iAction,
				   [[maybe_unused]] int iMods) -> void {
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

		glfwSetMouseButtonCallback(
				mp_glfwWindow,
				[](GLFWwindow* iWindow, const int iButton, const int iAction,
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

		glfwSetScrollCallback(
				mp_glfwWindow, [](GLFWwindow* iWindow, const double iXOffset, const double iYOffset) -> void {
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
			for (int i = 0; i < iCount; ++i) paths.emplace_back(iPaths[i]);
			event::FileDropEvent event(std::move(paths));
			static_cast<WindowData*>(glfwGetWindowUserPointer(iWindow))->eventCallback(event);
		});
	}
}

void Window::initIcon(const Properties& iProps) {
	if (iProps.iconPath.empty())
		return;
	if (getPlatform() != Platform::Wayland) {
		setIcon(iProps.iconPath);
		return;
	}
	if (!iProps.installDesktopEntry)
		return;
	std::error_code errorCode;
	const auto executable = std::filesystem::read_symlink("/proc/self/exe", errorCode);
	if (errorCode) {
		OWL_CORE_WARN("GLFW: Cannot resolve the executable path, no desktop entry for {}.", m_appId)
		return;
	}
	platform::installDesktopEntry({.appId = m_appId,
								   .name = iProps.title,
								   .executable = executable,
								   .icon = std::filesystem::absolute(iProps.iconPath)});
}

auto Window::getPlatform() const -> Platform { return g_GlfwPlatform; }

auto Window::getContentScale() const -> math::vec2 {
	if (mp_glfwWindow == nullptr)
		return {1.f, 1.f};
	math::vec2 scale{1.f, 1.f};
	glfwGetWindowContentScale(mp_glfwWindow, &scale.x(), &scale.y());
	return scale;
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
		glfwSetWindowMonitor(mp_glfwWindow, nullptr, 100, 100, static_cast<int>(m_windowData.windowedSize.x()),
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
	if (getPlatform() == Platform::Wayland)
		return;// Wayland compositor picks the icon from a .desktop file, not the app.
	if (!exists(iIconPath)) {
		OWL_CORE_WARN("Window icon not found: {}.", iIconPath.string())
		return;
	}
	GLFWimage icon;
	int channels = 0;
	icon.pixels = stbi_load(iIconPath.string().c_str(), &icon.width, &icon.height, &channels, 4);
	if (icon.pixels != nullptr) {
		glfwSetWindowIcon(mp_glfwWindow, 1, &icon);
		stbi_image_free(icon.pixels);
	} else {
		OWL_CORE_WARN("Failed to load window icon: {}.", iIconPath.string())
	}
}

void Window::setCursorMode(const window::CursorMode iMode) {
	OWL_PROFILE_FUNCTION()

	m_cursorMode = iMode;
	if (mp_glfwWindow == nullptr)
		return;
	const bool disabled = iMode == window::CursorMode::Disabled;
	glfwSetInputMode(mp_glfwWindow, GLFW_CURSOR, disabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	if (disabled && glfwRawMouseMotionSupported() == GLFW_TRUE)
		glfwSetInputMode(mp_glfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
}

void Window::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (mp_glfwWindow == nullptr)
		return;
	m_context->waitIdle();
	glfwDestroyWindow(mp_glfwWindow);
	--g_GlfwWindowCount;
	mp_glfwWindow = nullptr;
	OWL_CORE_INFO("GLFW: Window closed after {} presented frames.", m_presentedFrames)
	if (g_GlfwWindowCount == 0) {
		glfwTerminate();
		g_GlfwPlatform = Platform::None;
	}
}

void Window::onUpdate() {
	OWL_PROFILE_FUNCTION()

	glfwPollEvents();
	m_context->swapBuffers();
	++m_presentedFrames;
	OWL_CORE_FRAME_TRACE("GLFW: {} frames presented.", m_presentedFrames)
	if (m_paceFrames)
		paceFrame();
}

void Window::paceFrame() {
	OWL_PROFILE_FUNCTION()

	const auto now = std::chrono::steady_clock::now();
	m_nextFrame += m_framePeriod;
	if (m_nextFrame < now) {
		m_nextFrame = now;
		return;
	}
	std::this_thread::sleep_until(m_nextFrame);
}

void Window::setVSync(const bool iEnabled) {
	OWL_PROFILE_FUNCTION()

	if (const auto api = renderer::gpu::RenderCommand::getApi(); api == renderer::gpu::RenderAPI::Type::OpenGL) {
		m_paceFrames = iEnabled && getPlatform() == Platform::Wayland;
		glfwSwapInterval(iEnabled && !m_paceFrames ? 1 : 0);
		if (m_paceFrames) {
			m_framePeriod = refreshPeriod(mp_glfwWindow);
			m_nextFrame = std::chrono::steady_clock::now();
			OWL_CORE_INFO("GLFW: Wayland OpenGL vsync paced at {:.1f} Hz instead of a blocking swap interval.",
						  1e9 / static_cast<double>(m_framePeriod.count()))
		}
	}
	renderer::gpu::RenderCommand::setVSync(iEnabled);
	m_windowData.vSync = iEnabled;
}

auto Window::isVSync() const -> bool { return m_windowData.vSync; }

}// namespace owl::window::glfw
