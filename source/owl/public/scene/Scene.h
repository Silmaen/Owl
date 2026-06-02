/**
 * @file Scene.h
 * @author Silmaen
 * @date 22/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "GameState.h"
#include "core/Timestep.h"
#include "core/UUID.h"
#include "math/Transform.h"
#include "renderer/Camera.h"
#include "renderer/RenderStack.h"

#include <entt/entt.hpp>

namespace owl::renderer::gpu {
class StorageBuffer;
}// namespace owl::renderer::gpu

namespace owl::renderer::utils {
class WorldTransformPass;
}// namespace owl::renderer::utils

/**
 * @brief
 *  Namespace for the scene elements.
 */
namespace owl::scene {

class Entity;
class ScriptableEntity;

/**
 * @brief
 *  Class describing a scene.
 */
class OWL_API Scene final {
public:
	Scene(const Scene&) = delete;

	Scene(Scene&&) = delete;

	auto operator=(const Scene&) -> Scene& = delete;

	auto operator=(Scene&&) -> Scene& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	Scene();

	/**
	 * @brief
	 *  Destructor.
	 */
	~Scene();

	/**
	 * @brief
	 *  Create a copy of the scene.
	 * @param[in] iOther The scene to copy.
	 * @return Pointer to the new scene.
	 */
	static auto copy(const shared<Scene>& iOther) -> shared<Scene>;

	/**
	 * @brief
	 *  Create entity and add it to registry.
	 * @param[in] iName Entity's name.
	 * @return The Entity.
	 */
	auto createEntity(const std::string& iName = std::string()) -> Entity;

	/**
	 * @brief
	 *  Create entity with UUID and add it to registry.
	 * @param[in] iName Entity's name.
	 * @param[in] iUuid The Entity's UUID.
	 * @return The Entity.
	 */
	auto createEntityWithUUID(core::UUID iUuid, const std::string& iName = std::string()) -> Entity;

	/**
	 * @brief
	 *  Destroy n entity.
	 * @param[in,out] ioEntity Entity to destroy.
	 */
	void destroyEntity(Entity& ioEntity);

	/**
	 * @brief
	 *  Beginning the scene as runtime (enabling physics).
	 */
	void onStartRuntime();

	/**
	 * @brief
	 *  End scene runtime mode.
	 */
	void onEndRuntime();

	/**
	 * @brief
	 *  Update actions for the runtime.
	 *
	 * Advances physics, scripts, triggers, and animations. When `iRender` is true
	 * (the default) also draws the scene with the primary camera. Set it to
	 * false to run simulation in the background without touching the renderer
	 * (used by the editor for non-active document tabs in Play mode).
	 * @param[in] iTimeStep The time step.
	 * @param[in] iRender When true, issue the Renderer2D draw pass at the end.
	 */
	void onUpdateRuntime(const core::Timestep& iTimeStep, bool iRender = true);

	/**
	 * @brief
	 *  Advance `RaycastDoor` / `RaycastPushWall` state machines for one tick.
	 *
	 * Handles the engine-built-in activation path (player proximity + key edge),
	 * advances the open/close (or one-shot slide) animation, updates each
	 * entity's local transform along `slideDirection`, and mirrors that to the
	 * kinematic Box2D body when the entity carries a `PhysicBody`. Lua scripts
	 * that bypass the built-in path (by setting `interactionKey` to `0`) drive
	 * the same state machine through `door.activate` / `pushwall.activate`.
	 *
	 * Called automatically by `onUpdateRuntime`; exposed publicly so tests and
	 * tool code can drive the state machine deterministically.
	 * @param[in] iTimeStep The elapsed time in seconds for this tick.
	 */
	void updateRaycastDynamicWalls(float iTimeStep);

	/**
	 * @brief
	 *  Render the runtime scene without simulation (for pause mode).
	 */
	void onRenderRuntime();

	/**
	 * @brief
	 *  Update actions in the editor, external camera, no gameplay.
	 * @param[in] iTimeStep The time step.
	 * @param[in] iCamera The editor camera.
	 */
	void onUpdateEditor(const core::Timestep& iTimeStep, const renderer::Camera& iCamera);

	/**
	 * @brief
	 *  Action when viewport resized.
	 * @param[in] iSize New viewport's size.
	 */
	void onViewportResize(const math::vec2ui& iSize);

	/**
	 * @brief
	 *  Read the current viewport size in pixels.
	 * @return The viewport size last set on this scene (defaults to `{0, 0}`).
	 */
	[[nodiscard]] auto getViewportSize() const -> const math::vec2ui& { return m_viewportSize; }

	/**
	 * @brief
	 *  Duplicate an entity.
	 * @param[in] iEntity Entity to duplicate.
	 * @return The created entity.
	 */
	auto duplicateEntity(const Entity& iEntity) -> Entity;

	/**
	 * @brief
	 *  Access to the primary Camera.
	 * @return The primary camera.
	 */
	[[nodiscard]] auto getPrimaryCamera() const -> Entity;

	/**
	 * @brief
	 *  Access to the primary player.
	 *
	 * Result is cached in `m_primaryPlayerCache` and reused across calls — the
	 * full registry scan only runs after the cache has been invalidated
	 * (entity destroyed, `Player` component added/removed, scene cleared).
	 * Avoids the per-frame O(N) scan from the trigger / input / sound /
	 * dynamic-wall loops that all hit this accessor multiple times per tick.
	 * @return The primary player.
	 */
	[[nodiscard]] auto getPrimaryPlayer() const -> Entity;

	/**
	 * @brief
	 *  Pre-populate every `EntityLink.linkedEntity` from its `linkedEntityName`.
	 *
	 * Called at `onStartRuntime` so the per-frame link-update loop starts with
	 * a warm cache and never falls into the O(N²) "scan all Tag components for
	 * the matching name" path on the first tick. Tag renames still trigger a
	 * one-tick rescan from the runtime loop's mismatch check.
	 */
	void resolveAllEntityLinks();

	/**
	 * @brief
	 *  Drop the cached primary-player handle so the next `getPrimaryPlayer()`
	 *  call re-scans the registry.
	 *
	 * Called automatically by entity destruction and by `Player`-component
	 * mutations. External callers that mutate `Player.primary` without going
	 * through the standard component API should call this manually.
	 */
	void invalidatePrimaryPlayerCache() { m_primaryPlayerCache = entt::null; }

	/**
	 * @brief
	 *  Get the list of all entities.
	 * @return List of all entities.
	 */
	[[nodiscard]] auto getAllEntities() const -> std::vector<Entity>;

	/**
	 * @brief
	 *  Get root entities only (those with no parent).
	 * @return List of root entities.
	 */
	[[nodiscard]] auto getRootEntities() const -> std::vector<Entity>;

	/**
	 * @brief
	 *  Get the children of an entity.
	 * @param[in] iEntity The parent entity.
	 * @return List of child entities.
	 */
	[[nodiscard]] auto getChildren(const Entity& iEntity) const -> std::vector<Entity>;

	/**
	 * @brief
	 *  Find an entity by its UUID.
	 * @param[in] iUuid The UUID to search for.
	 * @return The entity, or an invalid entity if not found.
	 */
	[[nodiscard]] auto findEntityByUUID(core::UUID iUuid) const -> Entity;

	/**
	 * @brief
	 *  Compute the world-space transform for an entity (walks parent chain).
	 * @param[in] iEntity The entity.
	 * @return The world-space transform.
	 */
	[[nodiscard]] auto getWorldTransform(const Entity& iEntity) const -> math::Transform;

	/**
	 * @brief
	 *  Rebuild the GPU world-transform SSBO and the entity → slot index map
	 *  for the current frame.
	 *
	 * Walks every entity carrying both `Transform` and `Hierarchy` in pre-order
	 * (root → leaf) so the resulting topo-sorted entry array satisfies the
	 * `parentIdx < own index` contract that
	 * `renderer::utils::WorldTransformPass` requires. The same pass populates
	 * `m_worldTransformCache` and an internal CPU mirror of the world matrices,
	 * which lets `getWorldTransform()` and other CPU consumers serve cached
	 * data without a GPU readback.
	 *
	 * Idempotent within a frame — the prepared state is invalidated when
	 * `m_worldTransformCacheActive` is cleared at the end of a tick. Renderers
	 * call `getWorldsBuffer()` / `getWorldIndex(entity)` to consume the result.
	 *
	 * Lazy-inits the underlying `WorldTransformPass` on the first call. The
	 * renderer must be initialised (so the compute shader can be compiled)
	 * before invoking this function.
	 */
	void prepareWorldTransforms() const;

	/**
	 * @brief
	 *  Look up the slot index of an entity in the GPU `worlds[]` SSBO populated
	 *  by the most recent `prepareWorldTransforms()` call.
	 *
	 * Returns `UINT32_MAX` when the entity is missing from the current frame's
	 * flattening — typically because it lacks a `Transform`/`Hierarchy`
	 * component, was created after the prepare call, or because no prepare has
	 * run yet on this tick. Callers that hit the sentinel must fall back to
	 * the transient world path on the renderer (a per-frame scratch SSBO).
	 * @param[in] iEntity The entity to look up.
	 * @return The slot index, or `UINT32_MAX` if the entity is not in the
	 * current frame's GPU buffer.
	 */
	[[nodiscard]] auto getWorldIndex(const Entity& iEntity) const -> uint32_t;

	/**
	 * @brief
	 *  GPU world-matrix SSBO populated by `prepareWorldTransforms()`.
	 *
	 * Indexed by the slot returned from `getWorldIndex()`. Renderers bind this
	 * as the `sceneWorlds[]` storage buffer in their instanced draw shaders.
	 * Returns `nullptr` until the first `prepareWorldTransforms()` call has
	 * succeeded.
	 * @return The world-matrix SSBO, or `nullptr` if no frame has been prepared.
	 */
	[[nodiscard]] auto getWorldsBuffer() const -> shared<renderer::gpu::StorageBuffer>;

	/**
	 * @brief
	 *  Check if an entity is effectively visible (walks parent chain).
	 * @param[in] iEntity The entity.
	 * @param[in] iEditorMode True if checking editor visibility, false for game visibility.
	 * @return True if the entity and all its ancestors are visible.
	 */
	[[nodiscard]] auto isEffectivelyVisible(const Entity& iEntity, bool iEditorMode) const -> bool;

	/**
	 * @brief
	 *  Set the parent of an entity. Handles reparenting and local transform recomputation.
	 * @param[in] iChild The entity to reparent.
	 * @param[in] iNewParent The new parent entity.
	 */
	void setParent(const Entity& iChild, const Entity& iNewParent) const;

	/**
	 * @brief
	 *  Remove an entity from its parent (make it a root entity).
	 * @param[in] iChild The entity to unparent.
	 */
	void unparent(const Entity& iChild) const;

	/**
	 * @brief
	 *  Destroy an entity and all its children recursively.
	 * @param[in,out] ioEntity Entity to destroy with children.
	 */
	void destroyEntityWithChildren(Entity& ioEntity);

	/**
	 * @brief
	 *  Duplicate an entity and all its descendants recursively.
	 * @param[in] iEntity Root entity of the subtree to duplicate.
	 * @return The duplicated root entity.
	 */
	auto duplicateSubtree(const Entity& iEntity) -> Entity;

	/**
	 * @brief
	 *  Rebuild children lists from parent references.
	 *
	 * Called after deserialization to reconstruct childrenIds from parentId relationships.
	 */
	void rebuildHierarchyChildren();

	/// Entities registry.
	entt::registry registry;
	/**
	 * @brief
	 *  List the statuses.
	 */
	enum struct Status : uint8_t {
		Editing,///< Editing.
		Playing,///< Playing.
		Victory,///< player won.
		Death,///< Player loose.
	};
	/// The scene status.
	Status status = Status::Editing;
	/**
	 * @brief
	 *  Describes a pending teleport request (set by Teleport triggers).
	 */
	struct TeleportRequest {
		/// Whether a teleport is pending.
		bool pending = false;
		/// Level to load (empty = same level).
		std::string levelName;
		/// Name of the target entity to teleport to.
		std::string targetName;
		/// Player velocity to apply after teleport (rotated by delta).
		math::vec2f initialVelocity = {0.f, 0.f};
		/// Rotation delta between trigger and target.
		float rotationDelta = 0.f;
	};
	/// Pending teleport request (used for cross-level teleport).
	TeleportRequest teleportRequest;
	/**
	 * @brief
	 *  Describes a pending save/load request (set by Lua scripts).
	 */
	struct SaveLoadRequest {
		/// Whether a save or load is pending.
		bool pending = false;
		/// True = load, false = save.
		bool isLoad = false;
		/// Save slot number.
		uint32_t slot = 0;
	};
	/// Pending save/load request.
	SaveLoadRequest saveLoadRequest;
	/// Set to true when the game requests to quit (Lua scene.quit()).
	bool quitRequested = false;

	/**
	 * @brief
	 *  Count the entities in the scene.
	 * @return The count of Entity in the scene.
	 */
	[[nodiscard]] auto getEntityCount() const -> uint32_t;

	/**
	 * @brief
	 *  Access the game state.
	 * @return The game state.
	 */
	[[nodiscard]] auto getGameState() -> GameState& { return m_gameState; }

	/**
	 * @brief
	 *  Access the game state (const).
	 * @return The game state.
	 */
	[[nodiscard]] auto getGameState() const -> const GameState& { return m_gameState; }

	/**
	 * @brief
	 *  Access the scene's enabled-renderers config (mutable).
	 * @return The enabled-renderers configuration.
	 */
	[[nodiscard]] auto getEnabledRenderers() -> renderer::EnabledRenderersConfig& { return m_enabledRenderers; }

	/**
	 * @brief
	 *  Access the scene's enabled-renderers config (const).
	 * @return The enabled-renderers configuration.
	 */
	[[nodiscard]] auto getEnabledRenderers() const -> const renderer::EnabledRenderersConfig& {
		return m_enabledRenderers;
	}

private:
	/// Game state key-value store (progression data).
	GameState m_gameState;
	/// Scene-level enable/override of the project renderer stack (empty → all active with defaults).
	renderer::EnabledRenderersConfig m_enabledRenderers;
	/// Cached primary-player entity handle. `entt::null` means "not resolved yet".
	mutable entt::entity m_primaryPlayerCache = entt::null;
	/**
	 * @brief
	 *  UUID → entt::entity index, kept warm across the scene's lifetime.
	 *  Populated by `createEntityWithUUID`, cleared by `destroyEntity` /
	 *  `destroyEntityWithChildren`. `findEntityByUUID` consults this map
	 *  first (O(1)) and falls back to a registry scan + repair on miss,
	 *  so even paths that bypass the canonical create remain correct.
	 */
	mutable std::unordered_map<core::UUID, entt::entity> m_uuidIndex;
	/**
	 * @brief
	 *  Tilemap / RaycastDoor / RaycastPushWall asset cache dirty bit. True on
	 *  fresh scene + after any component-add or explicit invalidation; cleared
	 *  at the end of `resolveAllTilemapAssets()` so the function returns
	 *  instantly on subsequent frames once everything is loaded.
	 */
	bool m_tilemapAssetsDirty = true;
	/**
	 * @brief
	 *  Per-update-pass cache for `isEffectivelyVisible`. Key packs the entity
	 *  id with a "is editor mode" bit; the bool value is the effective
	 *  visibility (current entity + all ancestors). Only consulted when
	 *  `m_inUpdatePass` is true — outside an update tick (tests, inspector
	 *  inspection helpers, …) the cache is bypassed so callers always see
	 *  fresh `Visibility` state. Cleared at the start of every update tick.
	 */
	mutable std::unordered_map<uint64_t, bool> m_visibilityCache;
	/**
	 * @brief
	 *  True while `onUpdateRuntime` / `onUpdateEditor` (and the render passes
	 *  they spawn) are running — gates `m_visibilityCache` and
	 *  `m_layerContentCache*` so they only serve callers that can guarantee
	 *  Visibility / RendererTag flags don't mutate mid-pass.
	 */
	mutable bool m_inUpdatePass = false;
	/**
	 * @brief
	 *  Per-pass cache for `layerHasContent(name, iIsFirst=true)`. Populated
	 *  lazily by the render-stack walk, dropped at the start of every update
	 *  tick along with `m_visibilityCache`. Avoids the 7-view scan being
	 *  repeated for every render frame of a stable scene.
	 */
	mutable std::unordered_map<std::string, bool> m_layerContentCacheFirst;
	/// Per-pass cache for `layerHasContent(name, iIsFirst=false)`.
	mutable std::unordered_map<std::string, bool> m_layerContentCacheNotFirst;
	/**
	 * @brief
	 *  Per-pass cache for `getWorldTransform`. The same entity transform is
	 *  recomputed up to ~30× per frame across sprites + circles + text +
	 *  tilemaps + raycast sprites + dynamic walls + doors + physics sync +
	 *  sound listener / source paths; caching kills the duplicates. Gated
	 *  by `m_worldTransformCacheActive` (a narrower window than
	 *  `m_inUpdatePass`) — only valid after the mutating phases (scripts /
	 *  physics / entity links) have finished, where transforms are stable.
	 */
	mutable std::unordered_map<entt::entity, math::Transform> m_worldTransformCache;
	/**
	 * @brief
	 *  True only during the read-only tail of a tick (sound + render), arming
	 *  `m_worldTransformCache`. Scripts and physics can mutate transforms
	 *  freely while this is false — the cache would otherwise hand back stale
	 *  values to the post-mutation reads.
	 */
	mutable bool m_worldTransformCacheActive = false;
	/**
	 * @brief
	 *  GPU compute pass that walks the parent chain and writes world matrices
	 *  into a `worlds[]` SSBO. Held by `uniq` (the engine `std::unique_ptr`
	 *  alias) so the full type only needs to be known in `Scene.cpp`.
	 *  Lazily constructed on the first `prepareWorldTransforms()` call,
	 *  destroyed with the scene.
	 */
	mutable uniq<renderer::utils::WorldTransformPass> mp_worldTransformPass;
	/**
	 * @brief
	 *  Per-frame entity → slot index in the GPU `worlds[]` SSBO populated by
	 *  `prepareWorldTransforms()`. Refilled on every prepare call alongside
	 *  `m_worldTransformCache`; lookup via `getWorldIndex()`.
	 */
	mutable std::unordered_map<entt::entity, uint32_t> m_entityToWorldIndex;
	/**
	 * @brief
	 *  Action when component is added to an entity.
	 * @tparam T Type of the added component.
	 * @param[in] iEntity Entity receiving new component.
	 * @param[in,out] ioComponent The new component.
	 */
	template<typename T>
	void onComponentAdded(const Entity& iEntity, T& ioComponent);

	/**
	 * @brief
	 *  Draw the elements.
	 */
	void render();

	/**
	 * @brief
	 *  Draw every visible tilemap accepted by the current layer.
	 * @param[in] iEditorMode True if the scene is being drawn for the editor view.
	 * @param[in] iRaycastLayer True if the current layer is a raycast renderer (routes walls
	 * through `RendererRaycast::drawTilemapWalls` instead of emitting per-cell 2D quads).
	 */
	void renderTilemaps(bool iEditorMode, bool iRaycastLayer);

	/**
	 * @brief
	 *  Collect Sprite/AnimatedSprite entities and submit them to the raycast renderer.
	 * @param[in] iEditorMode True if the scene is being drawn for the editor view.
	 */
	void renderRaycastSprites(bool iEditorMode);

	/**
	 * @brief
	 *  Collect `RaycastDoor` and `RaycastPushWall` entities accepted by the current
	 *  raycast layer and submit them to `RendererRaycast::drawDynamicWalls`.
	 *
	 * Runs between `renderTilemaps` (which fills the per-column wall zBuffer with
	 * the static tilemap depths) and `renderRaycastSprites` (which uses that
	 * zBuffer for billboard occlusion), so a closed door correctly hides what
	 * lies behind it from both walls and sprites.
	 * @param[in] iEditorMode True if the scene is being drawn for the editor view.
	 */
	void renderRaycastDynamicWalls(bool iEditorMode);

	/**
	 * @brief
	 *  Resolve every tilemap component's `.owltilemap` asset and its associated `.owltileset`.
	 *
	 * Two-phase pass over every `Tilemap` component in the scene:
	 *   1. If the component has a non-empty `tilemapPath` and a null `asset`, load the
	 *      `.owltilemap` from disk into a fresh `TilemapAsset`.
	 *   2. If the resolved asset has a non-empty `tilesetPath` and a null `tileset`, load the
	 *      `.owltileset` from disk.
	 *
	 * Idempotent. Called eagerly from `onStartRuntime` so that physics initialisation can read
	 * the tileset's collidable flags, and lazily by the renderer for the editor preview path.
	 *
	 * Gated by `m_tilemapAssetsDirty`: the function early-returns when the flag is clean (the
	 * common case once a scene has been resolved once), avoiding three empty view iterations
	 * per render frame. The flag is set by `onComponentAdded<Tilemap | RaycastDoor |
	 * RaycastPushWall>` and by `invalidateTilemapAssets()` (for explicit inspector mutations
	 * like "edit tilemap path").
	 */
	void resolveAllTilemapAssets();

	/**
	 * @brief
	 *  Public invalidator: mark the tilemap / tileset asset cache as needing a re-resolve on
	 *  the next `resolveAllTilemapAssets()` call. Use this from editor code when a tilemap
	 *  or tileset path is mutated outside the component-add path (e.g. inspector text-edit).
	 */
	void invalidateTilemapAssets() { m_tilemapAssetsDirty = true; }

	/**
	 * @brief
	 *  Refresh every visible `EntityLink` host's transform from its target.
	 *
	 * Called from `onUpdateRuntime` after physics. Hidden hosts are skipped
	 * (matching the dormant pattern used by triggers and scripts), and a
	 * stale `linkedEntity` triggers a one-shot tag rescan to recover from
	 * tag renames; the warm cache from `resolveAllEntityLinks` keeps the
	 * common case at O(1).
	 */
	void updateEntityLinks();

	/**
	 * @brief
	 *  Draw screen-space UI overlays (Canvas entities) within the current render batch.
	 * @param[in] iEffectiveViewProjection The view-projection matrix the active layer has bound
	 * to `Renderer2D` (world camera VP for legacy 2D layers, pixel-space ortho VP for raycast /
	 * screen-overlay layers). Used to map pixel anchors into the same coordinate frame the layer
	 * is currently drawing in, so HUDs follow the layer's space rather than the raw world camera
	 * (which would rotate the HUD with the player in the raycast scene).
	 */
	void renderUI(const math::mat4& iEffectiveViewProjection);

	/**
	 * @brief
	 *  Orchestrate per-layer rendering using the engine's active `RenderStack`.
	 *
	 * Falls back to a single legacy pass when the stack is empty. Centralises the
	 * `Renderer2D::resetStats / beginScene / endScene` book-keeping.
	 * @param[in] iCamera The camera used for the rendering passes.
	 */
	void renderWithStack(const renderer::Camera& iCamera);

	/**
	 * @brief
	 *  Whether the current draw pass should include this entity.
	 *
	 * Decision is based on the entity's optional `RendererTag` and the layer being processed.
	 * @param[in] iEntity The entity being evaluated.
	 * @return True if the entity should be drawn in the current layer.
	 */
	[[nodiscard]] auto layerAccepts(const Entity& iEntity) const -> bool;

	/**
	 * @brief
	 *  Whether the named layer has at least one visible renderable entity routed to it.
	 *
	 * Used by `renderWithStack` to skip layers with nothing to draw — avoids paying for an empty
	 * `beginScene/endScene` pair (which on Vulkan translates into an empty render pass and causes
	 * visible flicker on the neighbouring layers).
	 * @param[in] iLayerName Name of the layer to test.
	 * @param[in] iIsFirst Whether the layer is the first one in the stack — untagged renderable
	 * entities default to it.
	 * @return True if at least one visible entity is routed to this layer.
	 */
	[[nodiscard]] auto layerHasContent(const std::string& iLayerName, bool iIsFirst) const -> bool;

	/// The viewport's size.
	math::vec2ui m_viewportSize = {0, 0};
	/// Inverse of camera view rotation matrix (for skybox rendering).
	math::mat4 m_inverseViewRotation = math::identity<float, 4>();
	/**
	 * @brief
	 *  Name of the `RenderLayer` currently being filled.
	 *
	 * Empty during the legacy single-pass path. Set by `renderWithStack` before each `render()` /
	 * `renderUI()` call so per-layer entity filtering can run.
	 */
	std::string m_currentLayerName;
	/// Whether the layer being processed is the first one in the stack (untagged entities default to it).
	bool m_currentLayerIsFirst = true;
	/**
	 * @brief
	 *  Active layer driving the current `render()` pass.
	 *
	 * May be null on the legacy single-pass path. Used by `render()` to dispatch type-specific
	 * entity rendering (e.g. send tilemap walls to `RendererRaycast` instead of the per-cell
	 * `Renderer2D` quad loop).
	 */
	const renderer::RenderLayer* mp_currentLayer = nullptr;

	friend class Entity;
	friend class ScriptableEntity;
};

}// namespace owl::scene
