/**
 * @file droneLayer.h
 * @author Silmaen
 * @date 16/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "controller/UAVState.h"
#include "event/KeyEvent.h"
#include "panels/BasePanel.h"
#include <owl.h>


namespace drone {

class droneLayer : public owl::core::layer::Layer {
public:
	droneLayer(const droneLayer &) = delete;
	droneLayer(droneLayer &&) = delete;
	droneLayer &operator=(const droneLayer &) = delete;
	droneLayer &operator=(droneLayer &&) = delete;
	/**
	 * @brief Default constructor.
	 */
	droneLayer();
	/**
	 * @brief Destructor.
	 */
	~droneLayer() override = default;

	void onAttach() override;
	void onDetach() override;
	void onUpdate(const owl::core::Timestep &ts) override;
	void onEvent(owl::event::Event &event) override;
	void onImGuiRender(const owl::core::Timestep &ts) override;

private:
	void renderStats(const owl::core::Timestep &ts);
	void renderFakeDrone(const owl::core::Timestep &ts);
	void renderMenu();
	void renderToolbar();

	bool onKeyPressed(owl::event::KeyPressedEvent &e);
	bool onMouseButtonPressed(owl::event::MouseButtonPressedEvent &e);

	bool isConnected() const;
	void toggleConnect();

	bool connected = false;
	bool showStats = true;
	bool showFakeDrone = true;

	enum struct DisplayMode {
		Settings,
		Gauges
	};
	DisplayMode mode = DisplayMode::Gauges;

	// remote controller
	owl::shared<controller::UAVState> rc;

	// Panels
	owl::shared<panels::BasePanel> gauges;
	owl::shared<panels::BasePanel> information;
	owl::shared<panels::BasePanel> settings;
	owl::shared<panels::BasePanel> viewport;
};

}// namespace drone
