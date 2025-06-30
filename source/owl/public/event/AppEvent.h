/**
 * @file AppEvent.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "event/Event.h"
#include "math/vectors.h"

namespace owl::event {
/**
	 * @brief Window Resize Event.
	 */
class OWL_API WindowResizeEvent final : public Event {
public:
	/**
	 * @brief Constructor.
	 * @param[in] iSize Window's size.
	 */
	explicit WindowResizeEvent(const math::vec2ui& iSize) : m_size{iSize} {}

	/**
	 * @brief Get window's width.
	 * @return The window's width.
	 */
	[[nodiscard]] auto getWidth() const -> uint32_t { return m_size.x(); }

	/**
	 * @brief Get window's height.
	 * @return The window's height.
	 */
	[[nodiscard]] auto getHeight() const -> uint32_t { return m_size.y(); }

	/**
	 * @brief Get window's size.
	 * @return The window's size.
	 */
	[[nodiscard]] auto getSize() const -> const math::vec2ui& { return m_size; }

	/**
	 * @brief Get the event as string.
	 * @return String of the event.
	 */
	[[nodiscard]] auto toString() const -> std::string override {
		return std::format("WindowResizeEvent: {}, {}", m_size.x(), m_size.y());
	}

	/**
	 * @brief Get the event's name.
	 * @return Event's name.
	 */
	[[nodiscard]] auto getName() const -> std::string override { return std::format("WindowResizeEvent"); }

	/**
	 * @brief Get the event's static type.
	 * @return Event's static type.
	 */
	[[nodiscard]] static auto getStaticType() -> Type { return Type::WindowResize; }

	/**
	 * @brief Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }

	/**
	 * @brief Get the event's category flags.
	 * @return Event's category flags.
	 */
	[[nodiscard]] auto getCategoryFlags() const -> uint8_t override;

private:
	/// New size.
	math::vec2ui m_size;
};

/**
 * @brief Window close Event
 */
class OWL_API WindowCloseEvent final : public Event {
public:
	/**
	 * @brief Constructor.
	 */
	WindowCloseEvent() = default;

	/**
	 * @brief Get the event as string.
	 * @return String of the event.
	 */
	[[nodiscard]] auto toString() const -> std::string override { return std::format("WindowCloseEvent"); }

	/**
	 * @brief Get the event's name.
	 * @return Event's name.
	 */
	[[nodiscard]] auto getName() const -> std::string override { return std::format("WindowCloseEvent"); }

	/**
	 * @brief Get the event's static type.
	 * @return Event's static type.
	 */
	[[nodiscard]] static auto getStaticType() -> Type { return Type::WindowClose; }

	/**
	 * @brief Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }

	/**
		 * @brief Get the event's category flags.
		 * @return Event's category flags.
		 */
	[[nodiscard]] auto getCategoryFlags() const -> uint8_t override;
};

/**
 * @brief Application Tick Event
 */
class OWL_API AppTickEvent final : public Event {
public:
	/**
	 * @brief Constructor.
	 */
	AppTickEvent() = default;

	/**
	 * @brief Get the event as string.
	 * @return String of the event.
	 */
	[[nodiscard]] auto toString() const -> std::string override { return std::format("AppTickEvent"); }

	/**
	 * @brief Get the event's name.
	 * @return Event's name.
	 */
	[[nodiscard]] auto getName() const -> std::string override { return std::format("AppTickEvent"); }

	/**
	 * @brief Get the event's static type.
	 * @return Event's static type.
	 */
	[[nodiscard]] static auto getStaticType() -> Type { return Type::AppTick; }

	/**
	 * @brief Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }

	/**
	 * @brief Get the event's category flags.
	 * @return Event's category flags.
	 */
	[[nodiscard]] auto getCategoryFlags() const -> uint8_t override;
};

/**
 * @brief Application Update Event
 */
class OWL_API AppUpdateEvent final : public Event {
public:
	/**
	 * @brief Constructor.
	 */
	AppUpdateEvent() = default;

	/**
	 * @brief Get the event as string.
	 * @return String of the event.
	 */
	[[nodiscard]] auto toString() const -> std::string override { return std::format("AppUpdateEvent"); }

	/**
	 * @brief Get the event's name.
	 * @return Event's name.
	 */
	[[nodiscard]] auto getName() const -> std::string override { return std::format("AppUpdateEvent"); }

	/**
	 * @brief Get the event's static type.
	 * @return Event's static type.
	 */
	[[nodiscard]] static auto getStaticType() -> Type { return Type::AppUpdate; }

	/**
	 * @brief Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }

	/**
	 * @brief Get the event's category flags.
	 * @return Event's category flags.
	 */
	[[nodiscard]] auto getCategoryFlags() const -> uint8_t override;
};

/**
 * @brief Application Rendering Event
 */
class OWL_API AppRenderEvent final : public Event {
public:
	/**
	 * @brief Constructor.
	 */
	AppRenderEvent() = default;

	/**
	 * @brief Get the event as string.
	 * @return String of the event.
	 */
	[[nodiscard]] auto toString() const -> std::string override { return std::format("AppRenderEvent"); }

	/**
	 * @brief Get the event's name.
	 * @return Event's name.
	 */
	[[nodiscard]] auto getName() const -> std::string override { return std::format("AppRenderEvent"); }

	/**
	 * @brief Get the event's static type.
	 * @return Event's static type.
	 */
	[[nodiscard]] static auto getStaticType() -> Type { return Type::AppRender; }

	/**
	 * @brief Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }

	/**
	 * @brief Get the event's category flags.
	 * @return Event's category flags.
	 */
	[[nodiscard]] auto getCategoryFlags() const -> uint8_t override;
};
}// namespace owl::event
