/**
 * @file LayerStack.cpp
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/layer/LayerStack.h"

namespace owl::core::layer {

Layer::~Layer() = default;

LayerStack::~LayerStack() {
	for (auto& layer: m_layers) {
		layer->onDetach();
		layer.reset();
	}
}

void LayerStack::clear() {
	for (auto& layer: m_layers) {
		auto name = layer->getName();
		layer->onDetach();
		layer.reset();
		OWL_CORE_TRACE("Layer {} deleted.", name)
	}
	m_layers.clear();
	m_layerInsertIndex = 0;
	OWL_CORE_TRACE("LayerStack is empty.")
}

void LayerStack::pushLayer(shared<Layer>&& iLayer) {
	iLayer->onAttach();
	m_layers.emplace(m_layers.begin() + m_layerInsertIndex, std::move(iLayer));
	m_layerInsertIndex++;
}

void LayerStack::pushOverlay(shared<Layer>&& iOverlay) {
	m_layers.emplace_back(std::move(iOverlay));
	m_layers.back()->onAttach();
}

void LayerStack::popLayer(const shared<Layer>& iLayer) {
	if (const auto it = std::find(m_layers.begin(), m_layers.begin() + m_layerInsertIndex, iLayer);
		it != m_layers.begin() + m_layerInsertIndex) {
		iLayer->onDetach();
		m_layers.erase(it);
		m_layerInsertIndex--;
	}
}

void LayerStack::popOverlay(const shared<Layer>& iOverlay) {
	if (const auto it = std::find(m_layers.begin() + m_layerInsertIndex, m_layers.end(), iOverlay);
		it != m_layers.end()) {
		iOverlay->onDetach();
		m_layers.erase(it);
	}
}

}// namespace owl::core::layer
