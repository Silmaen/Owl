/**
 * @file Window.h
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "window/Window.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

struct android_app;
struct AInputEvent;

namespace owl::window::android {
/**
 * @brief
 *  Full-screen window of the Android native activity, presented by Vulkan.
 *
 * The activity's looper delivers the window commands (window ready, resized, focus, destroyed) and the input events;
 * onUpdate() drains it once per frame. When the system takes the window away (the game goes to the background), the
 * application is asked to close.
 */
class OWL_API Window final : public window::Window {
public:
	Window(const Window&) = delete;

	Window(Window&&) = delete;

	auto operator=(const Window&) -> Window& = delete;

	auto operator=(Window&&) -> Window& = delete;

	/**
	 * @brief
	 *  Wait for the native window of the activity, then create the graph context on it.
	 * @param[in] iProps The window properties (the size is the screen's).
	 */
	explicit Window(const Properties& iProps);

	/**
	 * @brief
	 *  Destructor.
	 */
	~Window() override;

	/**
	 * @brief
	 *  Present the frame, then handle the pending activity commands and input events.
	 */
	void onUpdate() override;

	/**
	 * @brief
	 *  Get the window width.
	 * @return The window width in pixels.
	 */
	[[nodiscard]] auto getWidth() const -> uint32_t override { return m_size.x(); }

	/**
	 * @brief
	 *  Get the window height.
	 * @return The window height in pixels.
	 */
	[[nodiscard]] auto getHeight() const -> uint32_t override { return m_size.y(); }

	/**
	 * @brief
	 *  Get the window size.
	 * @return The window size in pixels.
	 */
	[[nodiscard]] auto getSize() const -> const math::vec2ui& override { return m_size; }

	/**
	 * @brief
	 *  Get the type of window manager.
	 * @return Android.
	 */
	[[nodiscard]] auto getType() const -> Type override { return Type::Android; }

	/**
	 * @brief
	 *  Native platform of the window.
	 * @return Android.
	 */
	[[nodiscard]] auto getPlatform() const -> Platform override { return Platform::Android; }

	/**
	 * @brief
	 *  Content scale of the screen: its density over the 160 dpi baseline.
	 * @return The scale on both axes.
	 */
	[[nodiscard]] auto getContentScale() const -> math::vec2 override { return {m_contentScale, m_contentScale}; }

	/**
	 * @brief
	 *  Number of frames handed to the presentation engine.
	 * @return The number of presented frames.
	 */
	[[nodiscard]] auto getPresentedFrames() const -> uint64_t override { return m_presentedFrames; }

	/**
	 * @brief
	 *  Define the Event Callback function.
	 * @param[in] iCallback The new callback function.
	 */
	void setEventCallback(const EventCallback& iCallback) override { m_eventCallback = iCallback; }

	/**
	 * @brief
	 *  Set the VSync.
	 * @param[in] iEnabled Should VSync enabled.
	 */
	void setVSync(bool iEnabled) override;

	/**
	 * @brief
	 *  Check for VSync.
	 * @return True if VSync enabled.
	 */
	[[nodiscard]] auto isVSync() const -> bool override { return m_vSync; }

	/**
	 * @brief
	 *  Set the low-latency present mode.
	 * @param[in] iEnabled Whether to prefer a non-blocking present.
	 */
	void setLowLatencyPresent(bool iEnabled) override;

	/**
	 * @brief
	 *  Access to the Native Window.
	 * @return The `ANativeWindow` of the activity.
	 */
	[[nodiscard]] auto getNativeWindow() const -> void* override;

	/**
	 * @brief
	 *  Set the window title: an Android activity shows none.
	 * @param[in] iTitle The new title.
	 */
	void setTitle(const std::string& iTitle) override { m_title = iTitle; }

	/**
	 * @brief
	 *  Set the fullscreen state: always full screen.
	 */
	void setFullscreen(bool) override {}

	/**
	 * @brief
	 *  Check whether fullscreen.
	 * @return True.
	 */
	[[nodiscard]] auto isFullscreen() const -> bool override { return true; }

	/**
	 * @brief
	 *  Check if the activity has the input focus.
	 * @return True when focused.
	 */
	[[nodiscard]] auto isFocused() const -> bool override { return m_focused; }

	/**
	 * @brief
	 *  Wait for an activity command or an input event.
	 * @param[in] iTimeout Longest wait.
	 */
	void waitEvents(std::chrono::milliseconds iTimeout) override;

	/**
	 * @brief
	 *  Wake a waitEvents() call up.
	 */
	void wakeUp() override;

	/**
	 * @brief
	 *  Set the resizable state: the screen decides.
	 */
	void setResizable(bool) override {}

	/**
	 * @brief
	 *  Set the size: the screen decides.
	 */
	void setSize(uint32_t, uint32_t) override {}

	/**
	 * @brief
	 *  Set the icon: the APK carries it.
	 */
	void setIcon(const std::filesystem::path&) override {}

	/**
	 * @brief
	 *  Set the cursor mode: a touch screen has no cursor.
	 * @param[in] iMode The requested mode.
	 */
	void setCursorMode(const CursorMode iMode) override { m_cursorMode = iMode; }

	/**
	 * @brief
	 *  Get the cursor mode.
	 * @return The last requested mode.
	 */
	[[nodiscard]] auto getCursorMode() const -> CursorMode override { return m_cursorMode; }

	/**
	 * @brief
	 *  Release the graph context.
	 */
	void shutdown() override;

private:
	/**
	 * @brief
	 *  Handle one command of the activity.
	 * @param[in] iCommand The `APP_CMD_*` value.
	 */
	void handleCommand(int32_t iCommand);

	/**
	 * @brief
	 *  Handle one input event of the activity.
	 * @param[in] iEvent The event.
	 * @return True when the event was consumed.
	 */
	auto handleInput(const AInputEvent* iEvent) -> bool;

	/**
	 * @brief
	 *  Run the pending activity commands and input events.
	 * @param[in] iTimeoutMs Wait for the first one, in milliseconds (0: return at once).
	 */
	void pollEvents(int iTimeoutMs);

	/**
	 * @brief
	 *  Read the size of the native window, send a resize event when it changed.
	 */
	void updateSize();

	/**
	 * @brief
	 *  Send the close event, once.
	 */
	void requestClose();

	/// The native activity.
	android_app* mp_app = nullptr;
	/// Window size in pixels.
	math::vec2ui m_size;
	/// Screen density over 160 dpi.
	float m_contentScale = 1.f;
	/// Title asked by the application.
	std::string m_title;
	/// Event callback.
	EventCallback m_eventCallback;
	/// VSync state.
	bool m_vSync = true;
	/// Low-latency present request.
	bool m_lowLatencyPresent = false;
	/// Whether the activity has the input focus.
	bool m_focused = true;
	/// Whether the close event was sent.
	bool m_closeSent = false;
	/// Last requested cursor mode.
	CursorMode m_cursorMode = CursorMode::Normal;
	/// Presented frames.
	uint64_t m_presentedFrames = 0;
};
}// namespace owl::window::android
