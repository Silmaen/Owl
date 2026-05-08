/**
 * @file NodeCanvasSerializer.h
 * @author Silmaen
 * @date 24/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "NodeCanvas.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace owl::gui::widgets {
/**
 * @brief
 *  YAML serialization helpers for `NodeCanvas`.
 *
 * The format is domain-agnostic — it stores nodes (with their `customData`
 * blob treated as opaque text) and links. The Scene Flow view, animation
 * graphs, behaviour trees and dialogue trees all share the same format, each
 * putting its own schema into `customData`.
 *
 * Top-level YAML structure:
 * ```yaml
 * NodeGraph: <display name>
 * Version: 1
 * Nodes:
 *   - id: 12345
 *     title: "Level 1"
 *     position: [100, 50]
 *     titleColor: [1.0, 1.0, 1.0, 1.0]
 *     inputs:
 *       - {id: 1, label: "entry", typeTag: "scene_entry"}
 *     outputs:
 *       - {id: 2, label: "→ Level 2", typeTag: "scene_exit"}
 *     customData: |
 *       scenePath: scenes/level1.owl
 * Links:
 *   - {id: 10, from: 2, to: 11}
 * ```.
 */
class OWL_API NodeCanvasSerializer final {
public:
	NodeCanvasSerializer() = delete;

	/**
	 * @brief
	 *  Serialize the canvas to a YAML string.
	 * @param[in] iCanvas The canvas to snapshot.
	 * @param[in] iName Optional display name written under the `NodeGraph:` key (may be empty).
	 * @return The YAML document as a string.
	 */
	[[nodiscard]] static auto serializeToString(const NodeCanvas& iCanvas, std::string_view iName = "") -> std::string;

	/**
	 * @brief
	 *  Populate a canvas from a YAML string.
	 * @param[in,out] ioCanvas Target canvas — its existing contents are cleared on success.
	 * @param[in] iYaml The YAML document as a string.
	 * @return True on success, false on malformed input (canvas left untouched).
	 */
	static auto deserializeFromString(NodeCanvas& ioCanvas, std::string_view iYaml) -> bool;

	/**
	 * @brief
	 *  Write the canvas to a file on disk.
	 * @param[in] iCanvas The canvas to snapshot.
	 * @param[in] iPath Destination file.
	 * @param[in] iName Optional display name.
	 * @return True on success.
	 */
	static auto serializeToFile(const NodeCanvas& iCanvas, const std::filesystem::path& iPath,
								std::string_view iName = "") -> bool;

	/**
	 * @brief
	 *  Load a canvas from a file on disk.
	 * @param[in,out] ioCanvas Target canvas.
	 * @param[in] iPath Source file.
	 * @return True on success.
	 */
	static auto deserializeFromFile(NodeCanvas& ioCanvas, const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Serialize a subset of the canvas (clipboard use case).
	 * @param[in] iCanvas The canvas to snapshot.
	 * @param[in] iNodeIds Node UUIDs to include (links are included when both endpoints are in the set).
	 * @return YAML document as a string, or empty if `iNodeIds` is empty.
	 */
	[[nodiscard]] static auto serializeSubset(const NodeCanvas& iCanvas, std::span<const core::UUID> iNodeIds)
			-> std::string;

	/**
	 * @brief
	 *  Paste a serialized subset into a canvas, generating fresh UUIDs for every node/pin/link.
	 * @param[in,out] ioCanvas Target canvas — receives the pasted content on top of what it already holds.
	 * @param[in] iYaml Document produced by `serializeSubset`.
	 * @return UUIDs of the freshly created nodes (empty vector on failure).
	 */
	static auto pasteSubset(NodeCanvas& ioCanvas, std::string_view iYaml) -> std::vector<core::UUID>;
};

}// namespace owl::gui::widgets
