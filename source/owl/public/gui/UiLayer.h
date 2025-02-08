/**
 * @file UiLayer.h
 * @author Silmaen
 * @date 05/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "Theme.h"
#include "core/layer/Layer.h"

/**
 * @brief Namespace for gui
 */
namespace owl::gui {
/**
 * @brief Class UiLayer
 */
class OWL_API UiLayer final : public core::layer::Layer {
public:
	UiLayer(const UiLayer&) = delete;
	UiLayer(UiLayer&&) = delete;
	auto operator=(const UiLayer&) -> UiLayer& = delete;
	auto operator=(UiLayer&&) -> UiLayer& = delete;

	/**
	 * @brief Default constructor.
	 */
	UiLayer();

	/**
	 * @brief Destructor.
	 */
	~UiLayer() override;

	/**
	 * @brief Action on Attach.
	 */
	void onAttach() override;

	/**
	 * @brief Action on detach.
	 */
	void onDetach() override;

	/**
	 * @brief Action on event.
	 * @param[in,out] ioEvent The Event to react.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief Begin layer definition.
	 */
	void begin() const;

	/**
	 * @brief End layer definition.
	 */
	void end() const;

	/**
	 * @brief Defines layer as blocking events.
	 * @param[in] iBlock If layer block event or let them pass to next layer.
	 */
	void blockEvents(const bool iBlock) { m_blockEvent = iBlock; }

	/**
	 * @brief Defines the theme for the UI.
	 * @param[in] iTheme The theme to apply.
	 */
	static void setTheme(const Theme& iTheme = Theme());

	/**
	 * @brief Enable docking of windows.
	 */
	void enableDocking() { m_dockingEnable = true; }

	/**
	 * @brief Disable docking of windows.
	 */
	void disableDocking() { m_dockingEnable = false; }

	/**
	 * @brief Assume that there is no application.
	 */
	void disableApp() { m_withApp = false; }

	/**
	 * @brief Get the active widget id.
	 * @return The active widget id.
	 */
	static auto getActiveWidgetId() -> uint32_t;

private:
	/// If event should be bocked.
	bool m_blockEvent = true;
	/// If the docking space exists.
	bool m_dockingEnable = false;
	/// If is attached.
	bool m_withApp = true;

	/// Function that initialize the docking port.
	static void initializeDocking();
};
}// namespace owl::gui
