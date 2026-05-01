/**
 * @file RenderLayerFactory.h
 * @author Silmaen
 * @date 30/04/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "RenderLayer.h"

#include <functional>
#include <string>
#include <vector>

namespace owl::renderer {

/**
 * @brief Global registry of `RenderLayer` factory functions keyed by type string.
 *
 * The engine registers built-in layer types (`Renderer2D`, future `Raycasting`,
 * `Voxel`, ...) at renderer init. Mods or tests may register additional types.
 *
 * Used by `RenderStack::buildFromConfig` to instantiate concrete layers from a
 * project's `RendererStack` YAML.
 */
class OWL_API RenderLayerFactory {
public:
	/// Factory function type — receives the runtime instance name, returns the layer.
	using CreateFn = std::function<shared<RenderLayer>(const std::string& iName)>;

	/**
	 * @brief Register a layer type with a factory function.
	 *
	 * If `iTypeKey` is already registered, the existing entry is replaced and a
	 * warning is logged.
	 * @param[in] iTypeKey The type key (e.g. `"Renderer2D"`).
	 * @param[in] iFactory The factory function.
	 */
	static void registerType(const std::string& iTypeKey, CreateFn iFactory);

	/**
	 * @brief Unregister a layer type.
	 * @param[in] iTypeKey The type key to remove.
	 * @return True if a type was removed, false if the key was not registered.
	 */
	static auto unregisterType(const std::string& iTypeKey) -> bool;

	/**
	 * @brief Check whether a type is registered.
	 * @param[in] iTypeKey The type key to look up.
	 * @return True if registered.
	 */
	[[nodiscard]] static auto hasType(const std::string& iTypeKey) -> bool;

	/**
	 * @brief Create a layer instance.
	 * @param[in] iTypeKey The type key (must be registered).
	 * @param[in] iName The runtime instance name.
	 * @return The new layer, or nullptr if `iTypeKey` is not registered.
	 */
	[[nodiscard]] static auto create(const std::string& iTypeKey, const std::string& iName) -> shared<RenderLayer>;

	/**
	 * @brief List all registered type keys.
	 * @return Sorted list of registered type keys.
	 */
	[[nodiscard]] static auto registeredTypes() -> std::vector<std::string>;

	/**
	 * @brief Remove every registered type (testing helper).
	 */
	static void clear();
};

}// namespace owl::renderer
