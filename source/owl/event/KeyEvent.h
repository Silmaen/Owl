/**
 * @file KeyEvent.h
 * @author argawaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "Event.h"
#include "core/KeyCodes.h"
#include <fmt/format.h>

namespace owl::event {

/**
 * @brief Semi-abstracted class for all Keyboard events
 */
class OWL_API KeyEvent : public Event {
public:
  /**
   * @brief Get the Key code
   * @return
   */
  [[nodiscard]] core::KeyCode getKeyCode() const { return keyCode; }

  [[nodiscard]] uint8_t getCategoryFlags() const override {
    return category::Input | category::Keyboard;
  }
protected:
  explicit KeyEvent(const core::KeyCode keycode) : keyCode(keycode) {}
  /// Key code
  core::KeyCode keyCode;
};

class OWL_API KeyPressedEvent : public KeyEvent {
public:
  explicit KeyPressedEvent(const core::KeyCode keycode, bool isRepeat = false)
      : KeyEvent(keycode), isRepeat(isRepeat) {}

  /**
   * @brief Check if the key is repeated
   * @return True if repeated
   */
  [[nodiscard]] bool isRepeated() const { return isRepeat; }

  [[nodiscard]] std::string toString() const override {
    return fmt::format("KeyPressedEvent: {} (repeat = {})", keyCode,
                       isRepeat);
  }
  [[nodiscard]] std::string getName() const override {
    return fmt::format("KeyPressedEvent");
  }

  [[nodiscard]] static type getStaticType() { return type::KeyPressed; }
  [[nodiscard]] type getType() const override { return getStaticType(); }
private:
  bool isRepeat;
};

class OWL_API  KeyReleasedEvent : public KeyEvent {
public:
  explicit KeyReleasedEvent(const core::KeyCode keycode) : KeyEvent(keycode) {}

  [[nodiscard]] std::string toString() const override {
    return fmt::format("KeyReleasedEvent: {}", keyCode);
  }
  [[nodiscard]] std::string getName() const override {
    return fmt::format("KeyReleasedEvent");
  }

  [[nodiscard]] static type getStaticType() { return type::KeyReleased; }
  [[nodiscard]] type getType() const override { return getStaticType(); }
};

class KeyTypedEvent : public KeyEvent {
public:
  explicit KeyTypedEvent(const core::KeyCode keycode) : KeyEvent(keycode) {}

  [[nodiscard]] std::string toString() const override {
    return fmt::format("KeyTypedEvent: {}", keyCode);
  }
  [[nodiscard]] std::string getName() const override {
    return fmt::format("KeyTypedEvent");
  }
  [[nodiscard]] static type getStaticType() { return type::KeyTyped; }
  [[nodiscard]] type getType() const override { return getStaticType(); }
};

} // namespace owl::event
