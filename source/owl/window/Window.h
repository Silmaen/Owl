/**
 * @file Window.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Core.h"
#include "event/Event.h"
#include <functional>

namespace owl::window {

/**
 * @brief Types of Windows Manager
 */
enum struct Type {
  Null, /// Nothing
  GLFW  /// Windows managed by Glfw library
};

/**
 * @brief Structure holding base windows properties
 */
struct Properties {
  /// Type of Window manager
  Type winType = Type::GLFW;
  /// Window title
  std::string Title = "Owl Engine";
  /// Width
  uint32_t width = 1280;
  /// Height
  uint32_t height = 720;
};

/**
 * @brief Abstract Window class
 */
class OWL_API Window {
public:
  Window(const Window &) = delete;
  Window(Window &&) = delete;
  Window &operator=(const Window &) = delete;
  Window &operator=(Window &&) = delete;
  /// Call back function's type
  using EventCallback = std::function<void(event::Event &)>;
  /// Constructor
  Window() = default;
  /// Destructor
  virtual ~Window();
  /**
   * @brief Function called at Update Time
   */
  virtual void onUpdate() = 0;
  /**
   * @brief Get Size attribute of width
   * @return The window's width
   */
  [[nodiscard]] virtual uint32_t getWidth() const = 0;
  /**
   * @brief Get Size attribute of height
   * @return The window's height
   */
  [[nodiscard]] virtual uint32_t getHeight() const = 0;

  /**
   * @brief Define the Event Callback function
   * @param callback The new callback function
   */
  virtual void setEventCallback(const EventCallback &callback) = 0;
  /**
   * @brief St the VSync
   * @param enabled Should VSync enabled
   */
  virtual void setVSync(bool enabled) = 0;
  /**
   * @brief Check for VSync
   * @return True if VSync enabled
   */
  [[nodiscard]] virtual bool isVSync() const = 0;

  /**
   * @brief Access to the Native Window
   * @return Native window's raw pointer
   */
  [[nodiscard]] virtual void *getNativeWindow() const = 0;

  /**
   * @brief Helper for Window creation
   * @param props The window properties
   * @return Pointer to the window
   */
  static uniq<Window> Create(const Properties &props = Properties());
};

} // namespace owl::window
