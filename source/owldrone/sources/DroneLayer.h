/**
 * @file DroneLayer.h
 * @author Silmaen
 * @date 16/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "controller/RemoteController.h"
#include "panels/BasePanel.h"

#include <owl.h>

namespace drone {
class DroneLayer final : public owl::core::layer::Layer {
public:
	DroneLayer(const DroneLayer&) = delete;
	DroneLayer(DroneLayer&&) = delete;
	auto operator=(const DroneLayer&) -> DroneLayer& = delete;
	auto operator=(DroneLayer&&) -> DroneLayer& = delete;
	/**
	 * @brief Default constructor.
	 */
	DroneLayer();
	/**
	 * @brief Destructor.
	 */
	~DroneLayer() override = default;

	void onAttach() override;
	void onDetach() override;
	void onUpdate(const owl::core::Timestep& iTimeStep) override;
	void onEvent(owl::event::Event& ioEvent) override;
	void onImGuiRender(const owl::core::Timestep& iTimeStep) override;

private:
	void renderStats(const owl::core::Timestep& iTimeStep);
	void renderFakeDrone(const owl::core::Timestep& iTimeStep);
	void renderMenu();
	void renderToolbar();

	auto onKeyPressed(owl::event::KeyPressedEvent& ioEvent) -> bool;
	auto onMouseButtonPressed(owl::event::MouseButtonPressedEvent& ioEvent) -> bool;

	[[nodiscard]] auto isConnected() const -> bool;
	void toggleConnect();

	bool m_connected = false;
	bool m_showStats = true;
	bool m_showFakeDrone = true;

	enum struct DisplayMode : uint8_t { Settings, Gauges };

	DisplayMode m_mode = DisplayMode::Gauges;

	// remote controller
	owl::shared<controller::RemoteController> m_rc;

	// Panels
	owl::shared<panels::BasePanel> m_gauges;
	owl::shared<panels::BasePanel> m_information;
	owl::shared<panels::BasePanel> m_settings;
	owl::shared<panels::BasePanel> m_viewport;
};
}// namespace drone
