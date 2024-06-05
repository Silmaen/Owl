/**
 * @file Window.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../Window.h"
#include "core/external/glfw3.h"

/**
 * @brief Namespace for the glfw  elements.
 */
namespace owl::window::glfw {
/**
     * @brief Class GLFW Window.
     */
class OWL_API Window final : public ::owl::window::Window {
public:
    Window(const Window &) = delete;

    Window(Window &&) = delete;

    Window &operator=(const Window &) = delete;

    Window &operator=(Window &&) = delete;

    /**
         * @brief Default constructor.
         * @param[in] iProps The window properties.
         */
    explicit Window(const Properties &iProps);

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
    [[nodiscard]] uint32_t getWidth() const override { return m_windowData.width; }

    /**
         * @brief Get Size attribute of height.
         * @return The window's height.
         */
    [[nodiscard]] uint32_t getHeight() const override { return m_windowData.height; }

    /**
         * @brief Access to texture's size.
         * @return Texture's size.
         */
    [[nodiscard]] math::FrameSize getSize() const override { return {m_windowData.width, m_windowData.height}; }
    /**
         * @brief Get the type of window manager.
         * @return The window manager's type.
         */
    [[nodiscard]] Type getType() const override { return Type::GLFW; }

    /**
         * @brief Define the Event Callback function.
         * @param[in] iCallback The new callback function.
         */
    void setEventCallback(const EventCallback &iCallback) override { m_windowData.eventCallback = iCallback; }
    /**
         * @brief St the VSync.
         * @param[in] iEnabled Should VSync enabled.
         */
    void setVSync(bool iEnabled) override;

    /**
         * @brief Check for VSync.
         * @return True if VSync enabled.
         */
    [[nodiscard]] bool isVSync() const override;

    /**
         * @brief Access to the Native Window.
         * @return Native window's raw pointer.
         */
    [[nodiscard]] void *getNativeWindow() const override { return mp_glfwWindow; }

    /**
         * @brief Terminate the window.
         */
    void shutdown() override;

private:
    /**
         * @brief Initialize the window.
         * @param[in] iProps Properties of the window.
         */
    void init(const Properties &iProps);

    /// Pointer to the GLFW window.
    GLFWwindow *mp_glfwWindow{nullptr};

    /**
         * @brief Window's data.
         */
    struct WindowData {
        /// Window's title.
        std::string title;
        /// Window's width.
        uint32_t width{0};
        /// Window's height.
        uint32_t height{0};
        /// Window's VSync property.
        bool vSync{false};
        /// Event Call back.
        EventCallback eventCallback;
    };

    /// The Window's data.
    WindowData m_windowData{};
};
}// namespace owl::window::glfw
