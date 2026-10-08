/**
 * @file RenderStackYaml.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/external/yaml.h"
#include "renderer/RenderStack.h"

#include <string>

/**
 * @brief
 *  YAML node conversions of the render-stack configuration, for the engine's own serializers.
 *
 * The public structures hold YAML text so that yaml-cpp stays out of the public headers; these helpers
 * convert between that text and nodes inside the engine.
 */
namespace owl::renderer {

/**
 * @brief
 *  Parse YAML text into a node.
 * @param[in] iYaml The YAML text.
 * @return The node, or a null node when the text is empty or does not parse.
 */
[[nodiscard]] auto parseYamlText(const std::string& iYaml) -> YAML::Node;

/**
 * @brief
 *  Dump a node to YAML text.
 * @param[in] iNode The node.
 * @return The text, empty for an undefined, null or empty node.
 */
[[nodiscard]] auto dumpYamlText(const YAML::Node& iNode) -> std::string;

/**
 * @brief
 *  Serialize a project stack config to a YAML node.
 * @param[in] iConfig The config.
 * @return A sequence of maps.
 */
[[nodiscard]] auto stackToYaml(const RendererStackConfig& iConfig) -> YAML::Node;

/**
 * @brief
 *  Parse a project stack config from a YAML node (see `RendererStackConfig::fromYaml`).
 * @param[in] iNode The sequence node.
 * @return The parsed config.
 */
[[nodiscard]] auto stackFromYaml(const YAML::Node& iNode) -> RendererStackConfig;

/**
 * @brief
 *  Serialize a scene enable/override config to a YAML node.
 * @param[in] iConfig The config.
 * @return A sequence of maps.
 */
[[nodiscard]] auto enabledToYaml(const EnabledRenderersConfig& iConfig) -> YAML::Node;

/**
 * @brief
 *  Parse a scene enable/override config from a YAML node.
 * @param[in] iNode The sequence node.
 * @return The parsed config.
 */
[[nodiscard]] auto enabledFromYaml(const YAML::Node& iNode) -> EnabledRenderersConfig;

}// namespace owl::renderer
