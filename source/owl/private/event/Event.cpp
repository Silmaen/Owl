/**
 * @file Event.cpp
 * @author Silmaen
 * @date 16/02/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "event/Event.h"

#include "event/AppEvent.h"
#include "event/KeyEvent.h"
#include "event/MouseEvent.h"

namespace owl::event {

Event::~Event() = default;

auto WindowResizeEvent::getCategoryFlags() const -> uint8_t { return Application; }
auto WindowResizeEvent::toString() const -> std::string {
	return fmt::format("WindowResizeEvent: {}, {}", m_size.x(), m_size.y());
}
auto WindowResizeEvent::getName() const -> std::string { return fmt::format("WindowResizeEvent"); }
auto WindowCloseEvent::getCategoryFlags() const -> uint8_t { return Application; }
auto WindowCloseEvent::toString() const -> std::string { return fmt::format("WindowCloseEvent"); }
auto WindowCloseEvent::getName() const -> std::string { return fmt::format("WindowCloseEvent"); }
auto AppTickEvent::getCategoryFlags() const -> uint8_t { return Application; }
auto AppTickEvent::toString() const -> std::string { return fmt::format("AppTickEvent"); }
auto AppTickEvent::getName() const -> std::string { return fmt::format("AppTickEvent"); }
auto AppUpdateEvent::getCategoryFlags() const -> uint8_t { return Application; }
auto AppUpdateEvent::toString() const -> std::string { return fmt::format("AppUpdateEvent"); }
auto AppUpdateEvent::getName() const -> std::string { return fmt::format("AppUpdateEvent"); }
auto AppRenderEvent::getCategoryFlags() const -> uint8_t { return Application; }
auto AppRenderEvent::toString() const -> std::string { return fmt::format("AppRenderEvent"); }
auto AppRenderEvent::getName() const -> std::string { return fmt::format("AppRenderEvent"); }

auto KeyEvent::getCategoryFlags() const -> uint8_t { return Input | Keyboard; }
auto KeyPressedEvent::getName() const -> std::string { return fmt::format("KeyPressedEvent"); }
auto KeyPressedEvent::toString() const -> std::string {
	return fmt::format("KeyPressedEvent: {} (repeat = {})", m_keyCode, m_repeatCount);
}
auto KeyReleasedEvent::getName() const -> std::string { return fmt::format("KeyReleasedEvent"); }
auto KeyReleasedEvent::toString() const -> std::string { return fmt::format("KeyReleasedEvent: {}", m_keyCode); }
auto KeyTypedEvent::getName() const -> std::string { return fmt::format("KeyTypedEvent"); }
auto KeyTypedEvent::toString() const -> std::string { return fmt::format("KeyTypedEvent: {}", m_keyCode); }

auto MouseMovedEvent::getCategoryFlags() const -> uint8_t { return Input | Mouse; }
auto MouseMovedEvent::toString() const -> std::string {
	return fmt::format("MouseMovedEvent: {:.1f}, {:.1f}", m_mouseX, m_mouseY);
}
auto MouseMovedEvent::getName() const -> std::string { return fmt::format("MouseMovedEvent"); }
auto MouseScrolledEvent::getCategoryFlags() const -> uint8_t { return Input | Mouse; }
auto MouseScrolledEvent::toString() const -> std::string {
	return fmt::format("MouseScrolledEvent: {}, {}", getXOff(), getYOff());
}
auto MouseScrolledEvent::getName() const -> std::string { return fmt::format("MouseScrolledEvent"); }
auto MouseButtonEvent::getCategoryFlags() const -> uint8_t {
	return static_cast<uint8_t>(Input | Mouse) | static_cast<uint8_t>(MouseButton);
}
auto MouseButtonPressedEvent::getName() const -> std::string { return fmt::format("MouseButtonPressedEvent"); }
auto MouseButtonPressedEvent::toString() const -> std::string {
	return fmt::format("MouseButtonPressedEvent: {}", m_mouseButton);
}
auto MouseButtonReleasedEvent::getName() const -> std::string { return fmt::format("MouseButtonReleasedEvent"); }
auto MouseButtonReleasedEvent::toString() const -> std::string {
	return fmt::format("MouseButtonReleasedEvent: {}", m_mouseButton);
}

}// namespace owl::event
