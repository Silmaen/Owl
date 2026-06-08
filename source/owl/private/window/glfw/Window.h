/**
 * @file Window.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/glfw3.h"
#include "window/Window.h"

/**
 * @brief
 *  Namespace for the glfw  elements.
 */
namespace owl::window::glfw {
/**
 * @brief
 *  Specialization class for glfw window.
 */
class OWL_API Window final : public window::Window {
public:
	Window(const Window&) = delete;

	Window(Window&&) = delete;

	auto operator=(const Window&) -> Window& = delete;

	auto operator=(Window&&) -> Window& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iProps The window properties.
	 */
	explicit Window(const Properties& iProps);

	/**
	 * @brief
	 *  Destructor.
	 */
	~Window() override;

	/**
	 * @brief
	 *  Function called at Update Time.
	 */
	void onUpdate() override;

	/**
	 * @brief
	 *  Get Size attribute of width.
	 * @return The window's width.
	 */
	[[nodiscard]] auto getWidth() const -> uint32_t override { return m_windowData.size.x(); }

	/**
	 * @brief
	 *  Get Size attribute of height.
	 * @return The window's height.
	 */
	[[nodiscard]] auto getHeight() const -> uint32_t override { return m_windowData.size.y(); }

	/**
	 * @brief
	 *  Access to texture's size.
	 * @return Texture's size.
	 */
	[[nodiscard]] auto getSize() const -> const math::vec2ui& override { return m_windowData.size; }

	/**
	 * @brief
	 *  Get the type of window manager.
	 * @return The window manager's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return Type::Glfw; }

	/**
	 * @brief
	 *  Define the Event Callback function.
	 * @param[in] iCallback The new callback function.
	 */
	void setEventCallback(const EventCallback& iCallback) override { m_windowData.eventCallback = iCallback; }

	/**
	 * @brief
	 *  St the VSync.
	 * @param[in] iEnabled Should VSync enabled.
	 */
	void setVSync(bool iEnabled) override;

	/**
	 * @brief
	 *  Check for VSync.
	 * @return True if VSync enabled.
	 */
	[[nodiscard]] auto isVSync() const -> bool override;

	/**
	 * @brief
	 *  Access to the Native Window.
	 * @return Native window's raw pointer.
	 */
	[[nodiscard]] auto getNativeWindow() const -> void* override { return mp_glfwWindow; }

	/**
	 * @brief
	 *  Set the window title.
	 * @param[in] iTitle The new title.
	 */
	void setTitle(const std::string& iTitle) override;

	/**
	 * @brief
	 *  Set the fullscreen.
	 * @param[in] iFullscreen True to switch to fullscreen mode, false for windowed.
	 */
	void setFullscreen(bool iFullscreen) override;

	/**
	 * @brief
	 *  Check whether fullscreen.
	 * @return True when fullscreen.
	 */
	[[nodiscard]] auto isFullscreen() const -> bool override;

	/**
	 * @brief
	 *  Set the resizable.
	 * @param[in] iResizable True to allow user resizing.
	 */
	void setResizable(bool iResizable) override;

	/**
	 * @brief
	 *  Set the size.
	 * @param[in] iWidth Width in pixels/units.
	 * @param[in] iHeight Height in pixels/units.
	 */
	void setSize(uint32_t iWidth, uint32_t iHeight) override;

	/**
	 * @brief
	 *  Set the icon.
	 * @param[in] iIconPath Path to the icon image (or empty to clear it).
	 */
	void setIcon(const std::filesystem::path& iIconPath) override;

	/**
	 * @brief
	 *  Set the cursor mode (visible/free or hidden/locked for mouse-look).
	 * @param[in] iMode The cursor mode.
	 */
	void setCursorMode(window::CursorMode iMode) override;

	/**
	 * @brief
	 *  Get the current cursor mode.
	 * @return The cursor mode.
	 */
	[[nodiscard]] auto getCursorMode() const -> window::CursorMode override { return m_cursorMode; }

	/**
	 * @brief
	 *  Terminate the window.
	 */
	void shutdown() override;

private:
	/**
	 * @brief
	 *  Initialize the window.
	 * @param[in] iProps Properties of the window.
	 */
	void init(const Properties& iProps);

	/// Pointer to the GLFW window.
	GLFWwindow* mp_glfwWindow{nullptr};
	/// Current cursor mode (visible/free or hidden/locked).
	window::CursorMode m_cursorMode{window::CursorMode::Normal};

	/**
	 * @brief
	 *  Window's data.
	 */
	struct WindowData {
		/// Window's title.
		std::string title;
		/// Window's size.
		math::vec2ui size;
		/// Windowed-mode size (for fullscreen restore).
		math::vec2ui windowedSize;
		/// Window's VSync property.
		bool vSync{false};
		/// Whether the window is fullscreen.
		bool fullscreen{false};
		/// Event Call back.
		EventCallback eventCallback;
	};

	/// The Window's data.
	WindowData m_windowData{};
};
}// namespace owl::window::glfw
