/**
 * @file Scene.h
 * @author Silmaen
 * @date 22/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "DocumentKind.h"
#include "GameState.h"
#include "core/Timestep.h"
#include "core/UUID.h"
#include "input/ActionFrame.h"
#include "input/InputFrame.h"
#include "math/Transform.h"
#include "renderer/Camera.h"
#include "renderer/RenderStack.h"
#include "scene/PhysicsSettings.h"
#include "scene/SimulationClock.h"
#include "scene/SystemSchedule.h"
#include "scene/ui/HudLayout.h"

#include <entt/entt.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @brief
 *  Namespace for the scene elements.
 */
namespace owl::app {
class EngineContext;
}// namespace owl::app

namespace owl::physics {
class PhysicCommand;
/// Box2D world of one scene (defined by the physics module).
class PhysicsWorld;
}// namespace owl::physics

namespace owl::scene {

class Entity;
class ScriptableEntity;
namespace nav {
class NavWorld;
}// namespace nav
/// Shared sink for asynchronously generated voxel chunks (defined in Scene.cpp).
struct VoxelStreamState;

namespace component {
struct AnimatedSpriteRenderer;
struct BackgroundTexture;
struct Camera;
struct Canvas;
struct CircleRenderer;
struct EntityLink;
struct FlyCamera;
struct Hierarchy;
struct ID;
struct LuaScript;
struct NativeScript;
struct PhysicBody;
struct Player;
struct PrefabLink;
struct RaycastDoor;
struct RaycastPushWall;
struct RendererTag;
struct SoundListener;
struct SoundSource;
struct SpriteRenderer;
struct Tag;
struct Text;
struct IsometricObject;
struct Tilemap;
struct Transform;
struct Trigger;
struct UiButton;
struct UiFocus;
struct UiImage;
struct UiLayout;
struct UiList;
struct UiPanel;
struct UiProgressBar;
struct UiRect;
struct UiScrollView;
struct UiSlider;
struct UiText;
struct UiToolCursor;
struct UiTooltip;
struct UiWindow;
struct Visibility;
struct VoxelPlayer;
struct VoxelWorld;
}// namespace component

/**
 * @brief
 *  CPU time of the phases of one `Scene::onUpdateRuntime`, in milliseconds.
 */
struct OWL_API RuntimeTimings {
	/// `SystemPhase::Scripts`: native and Lua script updates.
	double scriptsMs{0.0};
	/// The fixed ticks (physics steps, collision callbacks, `on_tick`) and the `SystemPhase::Physics` update.
	double physicsMs{0.0};
	/// Render pass: voxel streaming, render stack recording and submission, screen transition.
	double renderMs{0.0};
	/// Whole update; the other phases (gameplay, links, triggers, transforms, sound) are the remainder.
	double totalMs{0.0};
};

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
	 *  Destructor, also releases the physics world when it is bound to this scene.
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
	 *  Destroy an entity, handing its children over to its parent.
	 *
	 * The children take the entity's place among its parent's children (sibling order kept) and
	 * their local transform is recomputed so their world position does not move.
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
	 *  Hot reload: take a file changed on disk into account.
	 *
	 *  - `.owltileset` / `.owltilemap`: the components using it drop their copy, re-resolved from disk at the next
	 *    `resolveAllTilemapAssets` (colliders built at `onStartRuntime` are not rebuilt);
	 *  - `.lua`: each running instance of the script is replaced by a fresh one, given the current values of the
	 *    component's properties, then `on_create` runs; a script that fails to load keeps the previous instance.
	 * @param[in] iFile The changed file (absolute, or relative to the working directory).
	 * @return True when something of the scene uses the file.
	 */
	auto onAssetFileChanged(const std::filesystem::path& iFile) -> bool;

	/**
	 * @brief
	 *  Update actions for the runtime.
	 *
	 * Advances the simulation clock by the frame duration, then runs the systems of `getSystems()` phase by phase
	 * (`SystemPhase` order; only `Ended` once the game is won or lost): the `update` functions of `Scripts` and
	 * `PrePhysics`, the fixed ticks the clock holds, the `update` functions of `Physics` and `PostPhysics`. Prepares
	 * the world transforms before `Late`, then flushes the deferred destructions. When `iRender` is true
	 * (the default) also draws the scene with the primary camera. Set it to
	 * false to run simulation in the background without touching the renderer
	 * (used by the editor for non-active document tabs in Play mode).
	 * @param[in] iTimeStep The time step.
	 * @param[in] iRender When true, issue the Renderer2D draw pass at the end.
	 */
	void onUpdateRuntime(const core::Timestep& iTimeStep, bool iRender = true);

	/**
	 * @brief
	 *  Start or stop timing the phases of `onUpdateRuntime` (off by default).
	 * @param[in] iEnabled True to time the next updates.
	 */
	void setRuntimeTimingsEnabled(const bool iEnabled) { m_runtimeTimingsEnabled = iEnabled; }

	/**
	 * @brief
	 *  Get the phase timings of the last `onUpdateRuntime`.
	 * @return The timings, zero while timing is off.
	 */
	[[nodiscard]] auto getLastRuntimeTimings() const -> const RuntimeTimings& { return m_lastRuntimeTimings; }

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
	 *  Draw only the HUD (the canvases of the scene, then those of its HUD asset) in pixels, at a given size.
	 *
	 * The HUD editor draws a document at a resolution of its own, whatever the viewport size of the scene; the layout
	 * (`getHudLayout`) is left computed for that size.
	 * @param[in] iViewport Size in pixels of the surface drawn into.
	 * @param[in] iEditorMode Use the editor visibility instead of the game one.
	 */
	void renderHud(const math::vec2ui& iViewport, bool iEditorMode);

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
	 *  Bind every `EntityLink` to its target before the first runtime frame.
	 *
	 * Called at `onStartRuntime` so the per-frame link-update loop starts with a warm cache. A link binds by
	 * `linkedEntityId` first, then by `linkedEntityName` (one tag index for all the name lookups); a missing target
	 * is reported once and the link ignored.
	 */
	void resolveAllEntityLinks();

	/**
	 * @brief
	 *  Bind one `EntityLink` to its target: by UUID, else by name (then its UUID is set).
	 *
	 * A target found by UUID also refreshes `linkedEntityName`, so a renamed target keeps its link.
	 * @param[in,out] ioLink The link to bind.
	 * @return True when the target exists.
	 */
	auto resolveEntityLink(component::EntityLink& ioLink) const -> bool;

	/**
	 * @brief
	 *  Give every `EntityLink` that has only a name the UUID of the first entity of that name (silent).
	 *
	 * Run before saving, so an authored link survives a later rename of its target.
	 */
	void assignEntityLinkIds();

	/**
	 * @brief
	 *  Point the `EntityLink`s and the `UiFocus` neighbours of some entities at new UUIDs (duplication, prefab
	 *  instantiation).
	 * @param[in] iEntities The entities whose links and neighbours are remapped.
	 * @param[in] iRemap Old target UUID to new target UUID; targets absent from it are kept.
	 */
	static void remapEntityLinks(const std::vector<Entity>& iEntities,
								 const std::unordered_map<core::UUID, core::UUID>& iRemap);


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
	 *  Get the list of all entities, in creation order (the order of a loaded file).
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
	 *  Compute the world-space transform for an entity.
	 *
	 * Walks the whole parent chain, whatever its depth. A chain that loops (corrupted data) is detected and the
	 * entity's local transform is returned instead.
	 * @param[in] iEntity The entity.
	 * @return The world-space transform.
	 */
	[[nodiscard]] auto getWorldTransform(const Entity& iEntity) const -> math::Transform;

	/**
	 * @brief
	 *  Compute the world matrix of every entity for the current frame and hand them to the 2D renderer.
	 *
	 * Walks every entity carrying both `Transform` and `Hierarchy` in pre-order (parents before children, no depth
	 * limit), composing `world = parentWorld * local` once per entity. The matrices are stored in the array returned
	 * by `getWorldMatrices()` (slot = `getWorldIndex(entity)`), kept in the persistent per-entity world cache for
	 * the CPU consumers (an unchanged entity is not recomposed), and passed to `renderer::Renderer2D::setSceneWorlds()`, which uploads them as its `sceneWorlds[]`
	 * storage buffer. The GPU never recomposes the hierarchy.
	 */
	void prepareWorldTransforms() const;

	/**
	 * @brief
	 *  Look up the slot index of an entity in the world-matrix array populated
	 *  by the most recent `prepareWorldTransforms()` call.
	 *
	 * Returns `UINT32_MAX` when the entity is missing from the current frame's
	 * flattening — typically because it lacks a `Transform`/`Hierarchy`
	 * component, was created after the prepare call, or because no prepare has
	 * run yet on this tick. Callers that hit the sentinel must fall back to
	 * the transient world path on the renderer (a per-frame scratch SSBO).
	 * @param[in] iEntity The entity to look up.
	 * @return The slot index, or `UINT32_MAX` if the entity is not in the
	 * current frame's world-matrix array.
	 */
	[[nodiscard]] auto getWorldIndex(const Entity& iEntity) const -> uint32_t;

	/**
	 * @brief
	 *  World matrices computed by the last `prepareWorldTransforms()` call.
	 *
	 * Indexed by the slot returned from `getWorldIndex()`; this is exactly what the 2D renderer uploads to its
	 * `sceneWorlds[]` storage buffer. Empty until the first prepare call.
	 * @return A view over the world matrices, valid until the next `prepareWorldTransforms()` call.
	 */
	[[nodiscard]] auto getWorldMatrices() const -> std::span<const math::mat4>;

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
	 *  Whether the current draw pass should include this entity.
	 *
	 * Decision is based on the entity's optional `RendererTag` and the layer being processed; a render layer's
	 * `onRender` uses it to pick the entities routed to it (every entity when no layer is active).
	 * @param[in] iEntity The entity being evaluated.
	 * @return True if the entity should be drawn in the current layer.
	 */
	[[nodiscard]] auto layerAccepts(const Entity& iEntity) const -> bool;

	/**
	 * @brief
	 *  First layer of the active render stack that draws its own entities and has some in this scene.
	 *
	 * Such a layer (the isometric one) makes the editor render the stack layer by layer, as in Play, instead of its
	 * single 2D composite, and puts the viewport in the isometric mode.
	 * @return The layer, or null when the stack holds none with content.
	 */
	[[nodiscard]] auto getSelfDrawnLayer() const -> shared<renderer::RenderLayer>;

	/**
	 * @brief
	 *  Whether this scene wants the cursor captured for mouse-look (a `VoxelPlayer` with `captureCursor` is present).
	 * @return True if at least one voxel player opts into cursor capture; false otherwise (the default).
	 */
	[[nodiscard]] auto wantsCursorCapture() const -> bool;

	/**
	 * @brief
	 *  Set the targeted-block highlight of the voxel HUD for this frame.
	 *
	 * In Play the `owl.voxel_players` system sets it from the player's target; in the editor there is no
	 * player, so the voxel brush pushes its current target here before render.
	 * @param[in] iShow Whether to draw a highlight this frame.
	 * @param[in] iBlock World-grid coordinates of the targeted block (used when `iShow`).
	 * @param[in] iNormal Outward normal of the impacted face (used when `iShow`).
	 */
	void setEditorVoxelHighlight(bool iShow, const math::vec3i& iBlock = {}, const math::vec3i& iNormal = {});

	/**
	 * @brief
	 *  Show or hide the aiming crosshair of the voxel HUD for this frame.
	 * @param[in] iShow True to draw the crosshair.
	 */
	void setShowCrosshair(bool iShow);

	/**
	 * @brief
	 *  Show a transient on-screen message (e.g. "Fly mode ON") for a few seconds.
	 * @param[in] iMessage The message text.
	 * @param[in] iSeconds How long to keep it on screen.
	 */
	void showToast(const std::string& iMessage, float iSeconds = 3.f);

	/**
	 * @brief
	 *  Access the systems this scene runs every runtime frame (a copy of `SystemSchedule::getDefault()`).
	 * @return The schedule.
	 */
	[[nodiscard]] auto getSystems() -> SystemSchedule& { return m_systems; }

	/**
	 * @brief
	 *  Access the systems this scene runs every runtime frame.
	 * @return The schedule.
	 */
	[[nodiscard]] auto getSystems() const -> const SystemSchedule& { return m_systems; }

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
	 *  Queue an entity and its subtree for destruction at the next safe point.
	 *
	 * Used by every runtime-originated destruction (Lua `scene.destroy_entity`, trigger callbacks): the
	 * entity stays alive until `flushPendingDestructions()`, so a script may destroy its own entity
	 * without freeing the Lua state that is running it. Queuing the same entity twice is a no-op.
	 * @param[in] iEntity The entity to destroy.
	 */
	void destroyEntityDeferred(const Entity& iEntity);

	/**
	 * @brief
	 *  Check whether an entity is queued for deferred destruction.
	 * @param[in] iEntity The entity to check.
	 * @return True if the entity is waiting in the destruction queue.
	 */
	[[nodiscard]] auto isPendingDestruction(const Entity& iEntity) const -> bool;

	/**
	 * @brief
	 *  Check whether an entity will be destroyed by the next flush, queued itself or through an ancestor.
	 * @param[in] iEntity The entity to check.
	 * @return True if the entity or one of its ancestors is waiting in the destruction queue.
	 */
	[[nodiscard]] auto isPendingDestructionInTree(const Entity& iEntity) const -> bool;

	/**
	 * @brief
	 *  Destroy every entity queued by `destroyEntityDeferred()`, with its children.
	 *
	 * Each destroyed entity first gets its runtime teardown: Lua `on_destroy` (called once), native
	 * script `onDestroy`, Box2D body removal and sound stop. Destructions queued by `on_destroy` are
	 * processed in the same call. Run at the end of `onUpdateRuntime()` (after scripts, physics and
	 * triggers) and at the start of `onEndRuntime()`.
	 */
	void flushPendingDestructions();

	/**
	 * @brief
	 *  Call Lua `on_collision(other_id)` on both entities of every collision begun this step.
	 *
	 * Called from `onUpdateRuntime` right after the physics step. Entities that are gone, hidden, or
	 * queued for destruction (themselves or through an ancestor) are skipped; the check is redone
	 * before each call, so a callback that destroys the other entity cancels its notification.
	 */
	void dispatchCollisionEvents();

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
	 * Repairs corrupted data on the way, with a warning each time: an entity whose parent
	 * does not exist moves to the root, and every hierarchy cycle is cut (one entity of the
	 * cycle becomes a root), so subtree walks always terminate.
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

	/**
	 * @brief
	 *  Access the scene's physics settings (mutable). Read by `PhysicCommand::init()`.
	 * @return The physics settings.
	 */
	[[nodiscard]] auto getPhysicsSettings() -> PhysicsSettings& { return m_physicsSettings; }

	/**
	 * @brief
	 *  Access the scene's physics settings (const).
	 * @return The physics settings.
	 */
	[[nodiscard]] auto getPhysicsSettings() const -> const PhysicsSettings& { return m_physicsSettings; }

	/**
	 * @brief
	 *  Fixed simulation clock of the scene (mutable): the ticks of `onUpdateRuntime`, the physics steps.
	 *
	 * `onStartRuntime()` resets it and gives it the simulation settings of the engine context (the project's tick
	 * rate) when the scene has one; a scene without context keeps the settings set here.
	 * @return The simulation clock.
	 */
	[[nodiscard]] auto getSimulationClock() -> SimulationClock& { return m_simulationClock; }

	/**
	 * @brief
	 *  Fixed simulation clock of the scene (const).
	 * @return The simulation clock.
	 */
	[[nodiscard]] auto getSimulationClock() const -> const SimulationClock& { return m_simulationClock; }

	/**
	 * @brief
	 *  Input of the running step: the tick frame inside a simulation tick, the frame one otherwise.
	 *
	 * `onUpdateRuntime()` samples `input::Input::readState()` once per frame into the frame input, and builds one
	 * tick input per simulation tick (`input::InputSampler`): the `tick` functions and `on_tick` see each edge once
	 * whatever the number of ticks in a frame, the `update` functions and `on_update` see it once per frame.
	 * `onStartRuntime()` starts both from the held state, without edge.
	 * @return The input frame.
	 */
	[[nodiscard]] auto getInputFrame() const -> const input::InputFrame& {
		return m_inTick ? m_tickInput : m_frameInput;
	}

	/**
	 * @brief
	 *  Actions of the running step: the tick frame inside a simulation tick, the frame one otherwise.
	 *
	 * The actions of the game's map (`app::EngineContext::getActionMap()`, the built-in ones without a context)
	 * evaluated on `getInputFrame()` each time the input advances: gameplay and scripts read these, not the keys.
	 * @return The action frame.
	 */
	[[nodiscard]] auto getActions() const -> const input::ActionFrame& {
		return m_inTick ? m_tickActions : m_frameActions;
	}

	/**
	 * @brief
	 *  Physical world of this scene, created by `physics::PhysicCommand::init()`.
	 * @return The world, or nullptr while the scene is not simulated.
	 */
	[[nodiscard]] auto getPhysicsWorld() const -> physics::PhysicsWorld* { return m_physicsWorld.get(); }

	/**
	 * @brief
	 *  Pathfinding state of this scene (graphs of its tilemaps and path requests), created on first use.
	 * @return The navigation world.
	 */
	[[nodiscard]] auto getNavigation() const -> nav::NavWorld&;

	/**
	 * @brief
	 *  Engine state this scene uses (screen transition, settings, voxel meshes): the application's one by default.
	 * @return The engine context, or nullptr when the scene was created without application and none was given.
	 */
	[[nodiscard]] auto getEngineContext() const -> app::EngineContext* { return mp_engineContext; }

	/**
	 * @brief
	 *  Give the scene the engine context it uses (tests create their own); kept by `copy()`.
	 * @param[in] iContext The context, or nullptr to run without transition overlay nor voxel meshes.
	 */
	void setEngineContext(app::EngineContext* iContext) { mp_engineContext = iContext; }

	/**
	 * @brief
	 *  Mouse state of the scene's runtime UI, kept by `UiInputSystem` from one frame to the next.
	 */
	struct UiInputState {
		/// Whether a UI element is currently hovered.
		bool consuming = false;
		/// Whether the mouse was pressed last frame (for click detection).
		bool wasPressed = false;
		/// Whether `mousePosition` was set by an update (a HUD tool cursor needs a mouse).
		bool hasMouse = false;
		/// Mouse position of the last update, in Y-down viewport pixels.
		math::vec2 mousePosition{0.f, 0.f};
		/// `UiWindow` whose title bar is being dragged (`entt::null` for none).
		entt::entity dragWindow = entt::null;
		/// Distance from the top-left corner of the dragged window to the mouse, in pixels.
		math::vec2 dragGrab{0.f, 0.f};
		/// Whether `dragWindow` belongs to the HUD asset rather than to the scene.
		bool dragInHud = false;
		/// `UiScrollView` or `UiList` whose bar thumb is being dragged (`entt::null` for none).
		entt::entity dragScroll = entt::null;
		/// Whether `dragScroll` belongs to the HUD asset rather than to the scene.
		bool dragScrollInHud = false;
		/// Whether the dragged bar is the vertical one.
		bool dragScrollVertical = true;
		/// Distance from the start of the dragged thumb to the mouse, in pixels.
		float dragScrollGrab = 0.f;
		/// Widget whose `UiTooltip` is waiting to show or shown (`entt::null` for none).
		entt::entity tooltipWidget = entt::null;
		/// Whether `tooltipWidget` belongs to the HUD asset.
		bool tooltipInHud = false;
		/// Seconds the mouse has stayed over `tooltipWidget`.
		float hoverTime = 0.f;
		/// Widget holding the keyboard and gamepad focus (`entt::null` for none).
		entt::entity focus = entt::null;
		/// Whether `focus` belongs to the HUD asset.
		bool focusInHud = false;
		/// Whether the focus is drawn: set by the keyboard, the gamepad and scripts, cleared when the mouse takes over.
		bool focusVisible = false;
		/// Whether the widget flagged `UiFocus::initial` has been given the focus (done once, on the first update).
		bool focusStarted = false;
		/// Widget the scroll containers were last scrolled to show (`entt::null` for none).
		entt::entity revealedFocus = entt::null;
		/// Direction held for the auto-repeat (0 none, else 1 up, 2 down, 3 left, 4 right).
		uint8_t repeatDirection = 0;
		/// Seconds `repeatDirection` has been held.
		float repeatTime = 0.f;
	};

	/**
	 * @brief
	 *  Access the runtime UI mouse state of this scene.
	 * @return The UI input state.
	 */
	[[nodiscard]] auto getUiInputState() -> UiInputState& { return m_uiInputState; }

	/**
	 * @brief
	 *  Access the runtime UI mouse state of this scene.
	 * @return The UI input state.
	 */
	[[nodiscard]] auto getUiInputState() const -> const UiInputState& { return m_uiInputState; }

	/**
	 * @brief
	 *  Document the scene was loaded from, which `SceneSerializer` writes it back to.
	 * @return `DocumentKind::Hud` for the scene of an `.owlhud`, `DocumentKind::Scene` otherwise.
	 */
	[[nodiscard]] auto getDocumentKind() const -> DocumentKind { return m_documentKind; }

	/**
	 * @brief
	 *  Set the document the scene is saved to (set by `SceneSerializer` when it loads one).
	 * @param[in] iKind The document.
	 */
	void setDocumentKind(const DocumentKind iKind) { m_documentKind = iKind; }

	/**
	 * @brief
	 *  HUD asset the scene asks for: its `Hud:` key.
	 * @return The asset path (relative to the asset folders); an empty path means no HUD, nothing means the project's
	 *  default HUD.
	 */
	[[nodiscard]] auto getHudPath() const -> const std::optional<std::string>& { return m_hudPath; }

	/**
	 * @brief
	 *  Choose the HUD asset of the scene.
	 * @param[in] iPath The asset path, an empty path for no HUD, nothing for the project's default HUD.
	 */
	void setHudPath(std::optional<std::string> iPath) { m_hudPath = std::move(iPath); }

	/**
	 * @brief
	 *  Path of the HUD asset the scene draws: its own `Hud:` key, else the project's `DefaultHud`.
	 * @return The asset path, empty when the scene has no HUD.
	 */
	[[nodiscard]] auto getEffectiveHudPath() const -> const std::string&;

	/**
	 * @brief
	 *  The HUD scene drawn over this scene and receiving its UI input, loaded on first use by the engine context's
	 *  `HudLibrary` and shared with every other scene asking for the same file.
	 * @return The HUD scene, or nullptr when the scene has no HUD, no engine context, or the file cannot be loaded.
	 */
	[[nodiscard]] auto getHud() const -> shared<Scene>;

	/**
	 * @brief
	 *  Access the HUD layout of this scene, computed by the HUD pass and reused by `UiInputSystem`.
	 * @return The HUD layout.
	 */
	[[nodiscard]] auto getHudLayout() -> ui::HudLayout& { return m_hudLayout; }

	/**
	 * @brief
	 *  Access the HUD layout of this scene, computed by the HUD pass and reused by `UiInputSystem`.
	 * @return The HUD layout.
	 */
	[[nodiscard]] auto getHudLayout() const -> const ui::HudLayout& { return m_hudLayout; }

private:
	/**
	 * @brief
	 *  Cut every parent cycle by moving one entity of each cycle to the root.
	 * @param[in] iByUuid Index of the scene entities by UUID (parents already known to exist).
	 */
	void breakHierarchyCycles(const std::unordered_map<core::UUID, entt::entity>& iByUuid);

	/**
	 * @brief
	 *  Action map to evaluate: the one of the engine context, the built-in one without a context.
	 * @return The action map.
	 */
	[[nodiscard]] auto getActionMap() const -> shared<const input::ActionMap>;

	/// Game state key-value store (progression data).
	GameState m_gameState;
	/// Scene-level enable/override of the project renderer stack (empty → all active with defaults).
	renderer::EnabledRenderersConfig m_enabledRenderers;
	/// Interpolation and solver settings of the scene's physics world.
	PhysicsSettings m_physicsSettings;
	/// Fixed-tick accumulator of the runtime (systems' `tick`, physics steps).
	SimulationClock m_simulationClock;
	/// Context given to the systems' `tick` functions, reused from frame to frame.
	SystemContext m_tickContext;
	/// Input sampled once per runtime frame (read outside the ticks).
	input::InputFrame m_frameInput;
	/// Input built once per simulation tick (read inside the ticks).
	input::InputFrame m_tickInput;
	/// Latch turning the frame samples into tick samples.
	input::InputSampler m_inputSampler;
	/// Actions evaluated on the frame input.
	input::ActionFrame m_frameActions;
	/// Actions evaluated on the tick input.
	input::ActionFrame m_tickActions;
	/// Whether the simulation ticks are running (selects the tick input).
	bool m_inTick = false;
	/// Box2D world of the scene while it is simulated (owned here, managed by `physics::PhysicCommand`).
	shared<physics::PhysicsWorld> m_physicsWorld;
	/// Pathfinding graphs and requests, created by `getNavigation`.
	mutable uniq<nav::NavWorld> m_navigation;
	/// Runtime UI mouse state (hover, previous press).
	UiInputState m_uiInputState;
	/// Layout of the HUD widgets drawn by the last HUD pass.
	ui::HudLayout m_hudLayout;
	/// Document the scene is saved to.
	DocumentKind m_documentKind = DocumentKind::Scene;
	/// HUD asset asked by the scene (nothing: the project's default HUD, empty: none).
	std::optional<std::string> m_hudPath;
	/// Systems run every runtime frame, phase by phase.
	SystemSchedule m_systems;
	/// Engine state used by the scene (not owned).
	app::EngineContext* mp_engineContext = nullptr;
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
	/// UUIDs queued by `destroyEntityDeferred()`, drained by `flushPendingDestructions()`.
	std::vector<core::UUID> m_pendingDestructions;
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
	 *  True during the read-only tail of `onUpdateRuntime` (armed once scripts,
	 *  physics, entity links and trigger callbacks have run) and during
	 *  `onUpdateEditor`, right before `prepareWorldTransforms()` — gates the effective visibility computed by that
	 *  call and `m_layerContentCache*`
	 *  so they only serve callers that can guarantee Visibility / RendererTag
	 *  flags don't mutate mid-pass.
	 */
	mutable bool m_inUpdatePass = false;
	/// True while the phases of `onUpdateRuntime` are timed.
	bool m_runtimeTimingsEnabled = false;
	/// Phase timings of the last `onUpdateRuntime`.
	RuntimeTimings m_lastRuntimeTimings;
	/**
	 * @brief
	 *  Per-pass cache for `layerHasContent(name, iIsFirst=true)`. Populated
	 *  lazily by the render-stack walk, dropped at the start of every update
	 *  tick. Avoids the 7-view scan being
	 *  repeated for every render frame of a stable scene.
	 */
	mutable std::unordered_map<std::string, bool> m_layerContentCacheFirst;
	/// Per-pass cache for `layerHasContent(name, iIsFirst=false)`.
	mutable std::unordered_map<std::string, bool> m_layerContentCacheNotFirst;
	/**
	 * @brief
	 *  Cached world matrix of one entity, indexed by entity index in `m_worldNodes`.
	 *
	 * The cache needs no write hook: an entry stays valid while the local transform it was composed from, its parent
	 * and the parent's `stamp` are unchanged, which a walk up the chain checks with a few compares per level. A
	 * recomposed entry takes a new `stamp`, so its descendants see the change and recompose in turn.
	 */
	struct WorldNode {
		/// Entity owning the entry (its version tells a recycled index apart).
		entt::entity handle = entt::null;
		/// Parent entity the world matrix was composed with (`entt::null` for a root).
		entt::entity parent = entt::null;
		/// UUID of `parent`, checked against the `Hierarchy` before trusting `parent`.
		core::UUID parentId{0};
		/// Unique number of the current `world` value (0 = never composed).
		uint64_t stamp = 0;
		/// Parent's `stamp` when `world` was composed.
		uint64_t parentStamp = 0;
		/// `stamp` that `worldTransform` was decomposed from (0 = none).
		uint64_t decomposedStamp = 0;
		/// World pass in which the entry was last checked (trusted without re-check during that pass).
		uint32_t checkedPass = 0;
		/// World pass of the `prepareWorldTransforms()` call that gave the entry `slot`.
		uint32_t slotPass = 0;
		/// Slot in `m_worldMatrices` given by the last `prepareWorldTransforms()` call.
		uint32_t slot = 0;
		/// World pass of the `prepareWorldTransforms()` call that computed `visible`.
		uint32_t visibilityPass = 0;
		/// Effective visibility (entity and all its ancestors) in game (0) and editor (1) mode.
		std::array<bool, 2> visible{true, true};
		/// Local transform `world` was composed from.
		math::Transform local;
		/// World matrix.
		math::mat4 world;
		/// Decomposition of `world`, built on demand for the CPU consumers of `getWorldTransform`.
		math::Transform worldTransform;
	};
	/// World matrix cache, indexed by `entt::to_entity(handle)`; survives frames.
	mutable std::vector<WorldNode> m_worldNodes;
	/// Last stamp given to a composed `WorldNode`.
	mutable uint64_t m_worldStamp = 0;
	/// Current world pass; bumped by every `prepareWorldTransforms()` call.
	mutable uint32_t m_worldPass = 1;
	/// True while `prepareWorldTransforms()` runs: entries checked in the current pass are trusted and get a slot.
	mutable bool m_preparingWorlds = false;
	/**
	 * @brief
	 *  True only during the read-only tail of a tick (sound + render): the world nodes checked by the last
	 *  `prepareWorldTransforms()` are trusted without walking their chain again. Scripts and physics can mutate
	 *  transforms freely while this is false, every read re-checks the chain.
	 */
	mutable bool m_worldTransformCacheActive = false;
	/// World matrices of the last `prepareWorldTransforms()` call, indexed by world slot (parents before children).
	mutable std::vector<math::mat4> m_worldMatrices;
	/**
	 * @brief
	 *  Hook run when a component is added to an entity.
	 *
	 * The engine components have their own specialisation (declared after the class); a component registered by
	 * a game (`ComponentRegistry`) needs none and gets this empty one.
	 * @tparam T The component type.
	 * @param[in] iEntity The entity.
	 * @param[in,out] ioComponent The new component.
	 */
	template<typename T>
	void onComponentAdded([[maybe_unused]] const Entity& iEntity, [[maybe_unused]] T& ioComponent) {}

	/**
	 * @brief
	 *  Draw the elements.
	 */
	void render();

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
	 * Runs between the raycast layer's `onRender` (whose tilemap walls fill the per-column zBuffer with
	 * the static tilemap depths) and `renderRaycastSprites` (which uses that
	 * zBuffer for billboard occlusion), so a closed door correctly hides what
	 * lies behind it from both walls and sprites.
	 * @param[in] iEditorMode True if the scene is being drawn for the editor view.
	 */
	void renderRaycastDynamicWalls(bool iEditorMode);

	/**
	 * @brief
	 *  Build/cache the GPU resources for voxel worlds (meshes, textures, pipeline).
	 *
	 *  Called by `renderWithStack` before the first layer records a draw, so the editor and the runner share one
	 *  path: the voxel pipeline is created against the bound render target and no GPU resource is created mid-pass.
	 */
	void prepareVoxelRenderData();

	/**
	 * @brief
	 *  Upload the changed tilemap chunks of every visible tilemap entity (`RendererTilemap::prepareTilemap`).
	 *
	 *  Called by `renderWithStack` before the first layer records a draw, so no chunk upload splits a render pass.
	 */
	void prepareTilemapRenderData();

	/**
	 * @brief
	 *  Draw every visible `VoxelWorld` entity accepted by the current voxel layer
	 *  through `RendererVoxel`.
	 * @param[in] iEditorMode True if the scene is being drawn for the editor view.
	 */
	void renderVoxelWorlds(bool iEditorMode);

	/**
	 * @brief
	 *  Stream procedural voxel chunks in and out around a camera position.
	 *
	 * For each `VoxelWorld` with `proceduralTerrain`, generates missing chunks
	 * within the streaming radius (budgeted per frame) from its `TerrainGenerator`
	 * and removes chunks that fell outside it. No-op for authored voxel worlds.
	 * @param[in] iCameraWorldPos The camera position in world space.
	 */
	void updateVoxelStreaming(const math::vec3& iCameraWorldPos);

	/**
	 * @brief
	 *  Draw the world-space overlays (voxel targeted-block highlight + aiming crosshair) through the active camera.
	 *
	 * Both the block wireframe and the crosshair are emitted as world-space lines
	 * in a single `Renderer2D` scene that uses `iCamera` directly, so they project
	 * exactly like the voxel world (no manual screen projection) and never mix a
	 * second 2D camera into the frame.
	 * @param[in] iCamera The active perspective camera.
	 */
	void renderOverlays(const renderer::Camera& iCamera);

	/**
	 * @brief
	 *  Draw the active transient toast message as a screen-space overlay.
	 */
	void renderToast();

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
	 *  Give the scene its own copy of every tilemap asset another scene still holds (a `copy()` shares them), so
	 *  runtime edits never reach the edit scene.
	 */
	void detachSharedTilemapAssets();

	/**
	 * @brief
	 *  Public invalidator: mark the tilemap / tileset asset cache as needing a re-resolve on
	 *  the next `resolveAllTilemapAssets()` call. Use this from editor code when a tilemap
	 *  or tileset path is mutated outside the component-add path (e.g. inspector text-edit).
	 */
	void invalidateTilemapAssets() { m_tilemapAssetsDirty = true; }

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
	 *  Render one runtime frame through the primary camera: voxel streaming, renderer stack, screen transition.
	 * @param[in] iTimeStep The frame duration (advances the screen transition).
	 */
	void renderRuntimeFrame(const core::Timestep& iTimeStep);

	/**
	 * @brief
	 *  Run the fixed ticks of a frame: for each one, the `tick` functions of `Scripts` to `PostPhysics`.
	 * @param[in] iFrame The frame context (`tickCount` ticks to run).
	 */
	void runSimulationTicks(const SystemContext& iFrame);

	/**
	 * @brief
	 *  Bring the cached world matrix of an entity and of its ancestors up to date.
	 *
	 * Walks up to the root (or to an entry already checked in the current pass), then back down, recomposing only
	 * the entries whose local transform, parent or parent's world changed. No depth limit, no recursion.
	 * @param[in] iEntity The entity (must carry `Transform` and `Hierarchy`).
	 * @param[in] iForbiddenAncestor UUID that must not appear among the ancestors (`0` for none).
	 * @return The entity's node, or nullptr when the chain loops or contains `iForbiddenAncestor`.
	 */
	[[nodiscard]] auto resolveWorldNode(entt::entity iEntity, core::UUID iForbiddenAncestor) const -> WorldNode*;

	/**
	 * @brief
	 *  Parent entity of an entity, through the world cache when it still matches the hierarchy.
	 * @param[in] iEntity The entity.
	 * @param[in] iParentId The parent UUID read from the entity's `Hierarchy` (non-zero).
	 * @return The parent entity carrying `Transform` and `Hierarchy`, or `entt::null` when it does not exist.
	 */
	[[nodiscard]] auto parentOf(entt::entity iEntity, core::UUID iParentId) const -> entt::entity;

	/**
	 * @brief
	 *  Compose an entity's world matrix by walking its whole parent chain, with no depth limit.
	 *
	 * A chain longer than the number of entities can only be a loop: it is reported and rejected.
	 * @param[in] iEntity The entity.
	 * @param[in] iForbiddenAncestor UUID that must not appear in the chain (`0` for none); `setParent` uses it to
	 *  reject a reparent that would create a cycle in the same walk that composes the new parent's world.
	 * @return The world matrix, or `std::nullopt` when the chain loops or contains `iForbiddenAncestor`.
	 */
	[[nodiscard]] auto composeWorldMatrix(const Entity& iEntity, core::UUID iForbiddenAncestor) const
			-> std::optional<math::mat4>;

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
	 * Empty during the legacy single-pass path. Set by `renderWithStack` before each `render()` call so per-layer
	 * entity filtering can run.
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

	/// Shared sink collecting worker-generated voxel chunks for `updateVoxelStreaming` to install on the main thread.
	shared<VoxelStreamState> m_voxelStream;

	/// Transient on-screen message text (drawn while `m_toastTimer` is positive).
	std::string m_toastMessage;
	/// Remaining display time for `m_toastMessage` in seconds.
	float m_toastTimer = 0.f;
	/// True while a `VoxelPlayer` targets a block this frame (drives the wireframe highlight).
	bool m_hasVoxelHighlight = false;
	/// World-grid coordinates of the highlighted block (valid when `m_hasVoxelHighlight`).
	math::vec3i m_voxelHighlightBlock{0, 0, 0};
	/// Outward normal of the ray-impacted face of the highlighted block (the only face outlined).
	math::vec3i m_voxelHighlightNormal{0, 0, 0};
	/// True while a captured `VoxelPlayer` is active (draws the centre-screen aiming crosshair).
	bool m_showCrosshair = false;

	friend class Entity;
	friend class ScriptableEntity;
	friend class physics::PhysicCommand;
};

/// @cond
template<>
OWL_API void Scene::onComponentAdded<component::ID>(const Entity& iEntity, component::ID& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Tag>(const Entity& iEntity, component::Tag& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Transform>(const Entity& iEntity, component::Transform& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Camera>(const Entity& iEntity, component::Camera& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::SpriteRenderer>(const Entity& iEntity,
																component::SpriteRenderer& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::AnimatedSpriteRenderer>(const Entity& iEntity,
																		component::AnimatedSpriteRenderer& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::CircleRenderer>(const Entity& iEntity,
																component::CircleRenderer& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::NativeScript>(const Entity& iEntity,
															  component::NativeScript& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Text>(const Entity& iEntity, component::Text& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::PhysicBody>(const Entity& iEntity, component::PhysicBody& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Player>(const Entity& iEntity, component::Player& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Trigger>(const Entity& iEntity, component::Trigger& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::EntityLink>(const Entity& iEntity, component::EntityLink& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::BackgroundTexture>(const Entity& iEntity,
																   component::BackgroundTexture& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Visibility>(const Entity& iEntity, component::Visibility& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Hierarchy>(const Entity& iEntity, component::Hierarchy& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::SoundSource>(const Entity& iEntity,
															 component::SoundSource& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::SoundListener>(const Entity& iEntity,
															   component::SoundListener& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::LuaScript>(const Entity& iEntity, component::LuaScript& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Canvas>(const Entity& iEntity, component::Canvas& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiRect>(const Entity& iEntity, component::UiRect& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiText>(const Entity& iEntity, component::UiText& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiImage>(const Entity& iEntity, component::UiImage& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiPanel>(const Entity& iEntity, component::UiPanel& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiButton>(const Entity& iEntity, component::UiButton& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiSlider>(const Entity& iEntity, component::UiSlider& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiProgressBar>(const Entity& iEntity,
															   component::UiProgressBar& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiLayout>(const Entity& iEntity, component::UiLayout& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiWindow>(const Entity& iEntity, component::UiWindow& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiTooltip>(const Entity& iEntity, component::UiTooltip& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiToolCursor>(const Entity& iEntity,
															  component::UiToolCursor& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiFocus>(const Entity& iEntity, component::UiFocus& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiScrollView>(const Entity& iEntity,
															  component::UiScrollView& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::UiList>(const Entity& iEntity, component::UiList& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::PrefabLink>(const Entity& iEntity, component::PrefabLink& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::RendererTag>(const Entity& iEntity,
															 component::RendererTag& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Tilemap>(const Entity& iEntity, component::Tilemap& ioComponent);

/**
 * @brief
 *  Specialisation of `onComponentAdded` for `IsometricObject`: schedules the sheet resolution.
 * @param[in] iEntity The entity the component was added to.
 * @param[in,out] ioComponent The component.
 */
template<>
OWL_API void Scene::onComponentAdded<component::IsometricObject>(const Entity& iEntity,
																 component::IsometricObject& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::RaycastDoor>(const Entity& iEntity,
															 component::RaycastDoor& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::RaycastPushWall>(const Entity& iEntity,
																 component::RaycastPushWall& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::VoxelWorld>(const Entity& iEntity, component::VoxelWorld& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::FlyCamera>(const Entity& iEntity, component::FlyCamera& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::VoxelPlayer>(const Entity& iEntity,
															 component::VoxelPlayer& ioComponent);
/// @endcond

}// namespace owl::scene
