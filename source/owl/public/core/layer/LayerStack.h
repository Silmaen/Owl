/**
 * @file LayerStack.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/layer/Layer.h"
#include <vector>

namespace owl::core::layer {

/**
 * @brief Class for managing a stack of game layers.
 */
class OWL_API LayerStack final {
public:
	/// Type for the list of Layer.
	using LayerList = std::vector<shared<Layer>>;
	/// Iterator in the layer list.
	using iterator = LayerList::iterator;
	/// Const iterator in the layer list.
	using const_iterator = LayerList::const_iterator;
	/// Reverse iterator in the layer list.
	using reverse_iterator = LayerList::reverse_iterator;
	/// Const reverse iterator in the layer list.
	using const_reverse_iterator = LayerList::const_reverse_iterator;
	LayerStack(const LayerStack&) = delete;
	LayerStack(LayerStack&&) = delete;
	auto operator=(const LayerStack&) -> LayerStack& = delete;
	auto operator=(LayerStack&&) -> LayerStack& = delete;

	/**
	 * @brief Default constructor.
	 */
	LayerStack() = default;

	/**
	 * @brief Destructor.
	 */
	~LayerStack();

	/**
	 * @brief Adding a layer on top of the layers.
	 * @param[in] iLayer The new layer to add.
	 */
	void pushLayer(shared<Layer>&& iLayer);

	/**
	 * @brief Adding an overlay on top of everything.
	 * @param[in] iOverlay The new overlay.
	 */
	void pushOverlay(shared<Layer>&& iOverlay);

	/**
	 * @brief Remove the given layer.
	 * @param[in] iLayer the layer to remove.
	 */
	void popLayer(const shared<Layer>& iLayer);

	/**
	 * @brief Remove the given overlay.
	 * @param[in] iOverlay The overlay to remove.
	 */
	void popOverlay(const shared<Layer>& iOverlay);

	/**
	 * @brief Get the starting of the layer list.
	 * @return Iterator at the beginning of the layer list.
	 */
	[[nodiscard]] auto begin() -> iterator { return m_layers.begin(); }

	/**
	 * @brief Get the ending of the layer list.
	 * @return Iterator at the end of the layer list.
	 */
	[[nodiscard]] auto end() -> iterator { return m_layers.end(); }

	/**
	 * @brief Get the starting of the reversed layer list.
	 * @return Reverse iterator at the beginning of the layer list.
	 */
	[[nodiscard]] auto rbegin() -> reverse_iterator { return m_layers.rbegin(); }

	/**
	 * @brief Get the ending of the reversed layer list.
	 * @return Reverse iterator at the end of the layer list.
	 */
	[[nodiscard]] auto rend() -> reverse_iterator { return m_layers.rend(); }

	/**
	 * @brief Get the starting of the layer list.
	 * @return Const iterator at the beginning of the layer list.
	 */
	[[nodiscard]] auto begin() const -> const_iterator { return m_layers.begin(); }

	/**
	 * @brief Get the ending of the layer list.
	 * @return Const iterator at the end of the layer list.
	 */
	[[nodiscard]] auto end() const -> const_iterator { return m_layers.end(); }

	/**
	 * @brief Get the starting of the reversed layer list.
	 * @return Const reverse iterator at the beginning of the layer list.
	 */
	[[nodiscard]] auto rbegin() const -> const_reverse_iterator { return m_layers.rbegin(); }

	/**
	 * @brief Get the ending of the reversed layer list.
	 * @return Const reverse iterator at the end of the layer list.
	 */
	[[nodiscard]] auto rend() const -> const_reverse_iterator { return m_layers.rend(); }

	/**
	 * @brief Destroy all layers and overlays.
	 */
	void clear();

private:
	/// The layers.
	LayerList m_layers;
	/// The inserting index.
	unsigned int m_layerInsertIndex = 0;
};

}// namespace owl::core::layer
