/**
 * @file Window.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/glfw3.h"
#include "input/Window.h"

/**
 * @brief Namespace for the glfw  elements.
 */
namespace owl::input::glfw {
/**
 * @brief Specialization class for glfw window.
 */
class OWL_API Window final : public input::Window {
public:
	Window(const Window&) = delete;
	Window(Window&&) = delete;
	auto operator=(const Window&) -> Window& = delete;
	auto operator=(Window&&) -> Window& = delete;

	/**
	 * @brief Default constructor.
	 * @param[in] iProps The window properties.
	 */
	explicit Window(const Properties& iProps);

	/**
	 * @brief Destructor.
	 */
	~Window() override;

	/**
	 * @brief Function called at Update Time.
	 */
	void onUpdate() override;

	/**
	 * @brief Get Size attribute of width.
	 * @return The window's width.
	 */
	[[nodiscard]] auto getWidth() const -> uint32_t override { return m_windowData.size.x(); }

	/**
	 * @brief Get Size attribute of height.
	 * @return The window's height.
	 */
	[[nodiscard]] auto getHeight() const -> uint32_t override { return m_windowData.size.y(); }

	/**
	 * @brief Access to texture's size.
	 * @return Texture's size.
	 */
	[[nodiscard]] auto getSize() const -> const math::vec2ui& override { return m_windowData.size; }
	/**
	 * @brief Get the type of window manager.
	 * @return The window manager's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return Type::Glfw; }

	/**
	 * @brief Define the Event Callback function.
	 * @param[in] iCallback The new callback function.
	 */
	void setEventCallback(const EventCallback& iCallback) override { m_windowData.eventCallback = iCallback; }
	/**
	 * @brief St the VSync.
	 * @param[in] iEnabled Should VSync enabled.
	 */
	void setVSync(bool iEnabled) override;

	/**
	 * @brief Check for VSync.
	 * @return True if VSync enabled.
	 */
	[[nodiscard]] auto isVSync() const -> bool override;

	/**
	 * @brief Access to the Native Window.
	 * @return Native window's raw pointer.
	 */
	[[nodiscard]] auto getNativeWindow() const -> void* override { return mp_glfwWindow; }

	/**
	 * @brief Terminate the window.
	 */
	void shutdown() override;

private:
	/**
	 * @brief Initialize the window.
	 * @param[in] iProps Properties of the window.
	 */
	void init(const Properties& iProps);

	/// Pointer to the GLFW window.
	GLFWwindow* mp_glfwWindow{nullptr};

	/**
	 * @brief Window's data.
	 */
	struct WindowData {
		/// Window's title.
		std::string title;
		/// Window's size.
		math::vec2ui size;
		/// Window's VSync property.
		bool vSync{false};
		/// Event Call back.
		EventCallback eventCallback;
	};

	/// The Window's data.
	WindowData m_windowData{};
};
}// namespace owl::input::glfw
