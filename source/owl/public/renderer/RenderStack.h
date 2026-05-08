/**
 * @file RenderStack.h
 * @author Silmaen
 * @date 30/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "RenderLayer.h"

#include <vector>

namespace owl::renderer {

/**
 * @brief
 *  Project-level definition of a single layer slot.
 *
 * Stored in `owl_project.yml` under `RendererStack:`. Combined with the scene's
 * `EnabledRenderers` overrides at scene activation time.
 */
// NOLINTBEGIN(bugprone-exception-escape)
// `YAML::Node::operator=` may throw `InvalidNode`; the implicit copy/move-assignment
// of this struct inherits that, but in practice the entries we copy/move are always
// well-formed and the throw never happens. Suppress here rather than at every call
// site (vector reallocation, `std::swap`, …).
struct OWL_API RendererStackEntry {
	/// Factory type key (e.g. `"Renderer2D"`).
	std::string typeKey;
	/// Runtime instance name (unique within the stack, used by scenes and entities).
	std::string name;
	/// Default config (project-level). May be empty.
	YAML::Node defaultConfig;
};
// NOLINTEND(bugprone-exception-escape)

/**
 * @brief
 *  Project-level renderer stack definition (ordered list of entries).
 *
 * The order in `entries` is the back-to-front render order. Empty stack means
 * "use the implicit `[Renderer2D(default)]` fallback".
 */
struct OWL_API RendererStackConfig {
	/// Ordered layer slots.
	std::vector<RendererStackEntry> entries;

	/**
	 * @brief
	 *  Whether the config is empty (caller should fall back to default).
	 * @return True if no entry is defined.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return entries.empty(); }

	/**
	 * @brief
	 *  Find an entry by its instance name.
	 * @param[in] iName The instance name to find.
	 * @return Pointer to the entry, or nullptr if not found.
	 */
	[[nodiscard]] auto find(const std::string& iName) const -> const RendererStackEntry*;

	/**
	 * @brief
	 *  Build a default config containing a single `Renderer2D` named `"default"`.
	 * @return The fallback config.
	 */
	[[nodiscard]] static auto makeDefault() -> RendererStackConfig;

	/**
	 * @brief
	 *  Serialize the config to a YAML node.
	 * @return The YAML node (a sequence of maps).
	 */
	[[nodiscard]] auto toYaml() const -> YAML::Node;

	/**
	 * @brief
	 *  Parse the config from a YAML node.
	 *
	 * Accepts a sequence node where each item has `Type` (required), `Name`
	 * (required, unique), and optional `DefaultConfig`. Invalid items are
	 * skipped with a warning.
	 * @param[in] iNode The YAML sequence node.
	 * @return The parsed config.
	 */
	[[nodiscard]] static auto fromYaml(const YAML::Node& iNode) -> RendererStackConfig;
};

/**
 * @brief
 *  Scene-level enable/override list for the renderer stack.
 *
 * Stored in `.owl` files under `EnabledRenderers:`. An empty list means "all
 * renderers from the project stack are active with their default config, in
 * project order". When non-empty, the listing also acts as an *ordering
 * override*: layers appear in the order written by the user, and any project
 * layer the scene does not mention is appended afterwards in project order
 * (with its project-default config).
 */
struct OWL_API EnabledRenderersConfig {
	// NOLINTBEGIN(bugprone-exception-escape)
	// `YAML::Node::operator=` may throw `InvalidNode`; the implicit copy/move-assignment
	// of this struct inherits that, but in practice the entries we copy/move are always
	// well-formed and the throw never happens. Suppress here rather than at every call
	// site (vector reallocation, `std::swap` in the editor's settings panel, …).
	/// Per-renderer-instance enable + override.
	struct Entry {
		/// Instance name from the project stack.
		std::string name;
		/// Whether this renderer is active for the scene.
		bool enabled = true;
		/// Override config for this scene (merged on top of project DefaultConfig).
		YAML::Node overrides;
	};
	// NOLINTEND(bugprone-exception-escape)

	/// Ordered entries (preserved in the order written by the user).
	std::vector<Entry> entries;

	/**
	 * @brief
	 *  Whether the config is empty (caller should activate every project renderer).
	 * @return True if no entry is defined.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return entries.empty(); }

	/**
	 * @brief
	 *  Find an entry by its instance name.
	 * @param[in] iName The instance name.
	 * @return Pointer to the entry, or nullptr if absent.
	 */
	[[nodiscard]] auto find(const std::string& iName) const -> const Entry*;

	/**
	 * @brief
	 *  Serialize the config to a YAML node.
	 * @return The YAML node (a sequence of maps).
	 */
	[[nodiscard]] auto toYaml() const -> YAML::Node;

	/**
	 * @brief
	 *  Parse the config from a YAML node.
	 * @param[in] iNode The YAML sequence node.
	 * @return The parsed config.
	 */
	[[nodiscard]] static auto fromYaml(const YAML::Node& iNode) -> EnabledRenderersConfig;
};

/**
 * @brief
 *  Ordered, runtime-instantiated stack of `RenderLayer` instances.
 *
 * Built from a `RendererStackConfig` (project) filtered/configured by an
 * `EnabledRenderersConfig` (scene). Holds the active layers in render order.
 */
class OWL_API RenderStack {
public:
	RenderStack(const RenderStack&) = delete;

	RenderStack(RenderStack&&) = default;

	auto operator=(const RenderStack&) -> RenderStack& = delete;

	auto operator=(RenderStack&&) -> RenderStack& = default;

	/**
	 * @brief
	 *  Default constructor (empty stack).
	 */
	RenderStack() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~RenderStack() = default;

	/**
	 * @brief
	 *  Build a stack from the project + scene configuration.
	 *
	 * Two-pass emission. Pass 1 walks the scene listing in scene order, looks
	 * each entry up in the project, and emits the matching layer (honouring
	 * the scene's `enabled` flag and `Overrides` block). Pass 2 then walks the
	 * project entries and emits any layer the scene did not mention, using
	 * the project default config. The net effect: the scene fully controls
	 * the order of layers it lists; layers it omits append at the end in
	 * project order. An empty scene listing falls back to project order
	 * verbatim.
	 *
	 * Unknown type keys log an error and skip the entry; scene entries that
	 * reference an unknown layer name log a warning and are dropped.
	 * @param[in] iProject The project's stack config (must not be empty).
	 * @param[in] iScene The scene's enable/overrides/order config (may be empty).
	 * @return The built stack.
	 */
	[[nodiscard]] static auto buildFromConfig(const RendererStackConfig& iProject, const EnabledRenderersConfig& iScene)
			-> RenderStack;

	/**
	 * @brief
	 *  Get the ordered list of layers.
	 * @return The active layers (in render order).
	 */
	[[nodiscard]] auto getLayers() const -> const std::vector<shared<RenderLayer>>& { return m_layers; }

	/**
	 * @brief
	 *  Find a layer by its instance name.
	 * @param[in] iName The instance name.
	 * @return The layer, or nullptr if absent.
	 */
	[[nodiscard]] auto findByName(const std::string& iName) const -> shared<RenderLayer>;

	/**
	 * @brief
	 *  Get the first layer in the stack (used as fallback for untagged entities).
	 * @return The first layer, or nullptr if the stack is empty.
	 */
	[[nodiscard]] auto getDefaultLayer() const -> shared<RenderLayer>;

	/**
	 * @brief
	 *  Whether the stack has no layers.
	 * @return True if empty.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return m_layers.empty(); }

	/**
	 * @brief
	 *  Open a frame on every active layer (in order).
	 * @param[in] iCamera The active camera.
	 */
	void beginFrame(const Camera& iCamera) const;

	/**
	 * @brief
	 *  Render the scene through every active layer (in order).
	 * @param[in,out] ioScene The scene to render.
	 */
	void renderScene(scene::Scene& ioScene) const;

	/**
	 * @brief
	 *  Close the frame on every active layer (in reverse order).
	 */
	void endFrame();

private:
	/// Active layers, in render (back-to-front) order.
	std::vector<shared<RenderLayer>> m_layers;
};

}// namespace owl::renderer
