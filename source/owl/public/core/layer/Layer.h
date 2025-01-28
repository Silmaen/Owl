/**
 * @file Layer.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include <utility>

#include "event/Event.h"
#include "core/Core.h"
#include "core/Timestep.h"

/**
 * @brief Namespace for the layer definition
 */
namespace owl::core::layer {

/**
 * @brief Base class for game layer definition.
 */
class OWL_API Layer {
public:
	Layer(const Layer&) = delete;
	Layer(Layer&&) = delete;
	auto operator=(const Layer&) -> Layer& = delete;
	auto operator=(Layer&&) -> Layer& = delete;

	/**
	 * @brief Default constructor.
	 * @param[in] iDebugName Name of the layer for debugging purpose.
	 */
	explicit Layer(std::string iDebugName = "Layer") : m_debugName(std::move(iDebugName)) {}

	/**
	 * @brief Destructor.
	 */
	virtual ~Layer();

	/**
	 * @brief Action on Attach.
	 */
	virtual void onAttach() {}

	/**
	 * @brief Action on detach.
	 */
	virtual void onDetach() {}

	/**
	 * @brief Action on update.
	 * @param[in] iTimeStep The time step since last frame.
	 */
	virtual void onUpdate([[maybe_unused]] const Timestep& iTimeStep) {}

	/**
	 * @brief Action for that layer when gui is rendered.
	 * @param[in] iTimeStep The time step since last frame.
	 */
	virtual void onImGuiRender([[maybe_unused]] const Timestep& iTimeStep) {}

	/**
	 * @brief Action on event.
	 * @param[in,out] ioEvent The Event to react.
	 */
	virtual void onEvent([[maybe_unused]] event::Event& ioEvent) {}

	/**
	 * @brief Get the debug name of the layer.
	 * @return Debug Name of the layer.
	 */
	[[nodiscard]] auto getName() const -> const std::string& { return m_debugName; }

protected:
	/// Debug name for this layer.
	std::string m_debugName;
};

}// namespace owl::core::layer
