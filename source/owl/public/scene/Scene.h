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
#include "data/geometry/LooseOctree.h"
#include "input/ActionFrame.h"
#include "input/InputFrame.h"
#include "math/Transform.h"
#include "math/matrices.h"
#include "math/matrixCreation.h"
#include "math/vectors.h"
#include "renderer/Camera.h"
#include "renderer/ColorVision.h"
#include "renderer/LensFlare.h"
#include "renderer/Lighting2D.h"
#include "renderer/LocalShadows.h"
#include "renderer/RenderGraph.h"
#include "renderer/RenderStack.h"
#include "renderer/Renderer3D.h"
#include "renderer/ShadowCascades.h"
#include "scene/Accessibility.h"
#include "scene/FloatingOriginSettings.h"
#include "scene/PhysicsSettings.h"
#include "scene/SimulationClock.h"
#include "scene/Subtitles.h"
#include "scene/SystemSchedule.h"
#include "scene/WorldPartitionSettings.h"
#include "scene/component/LuaScript.h"
#include "scene/component/Visibility.h"
#include "scene/ui/HudLayout.h"
#include "sound/Mixer.h"

#include <entt/entt.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

namespace owl::core::task {
class Scheduler;
}// namespace owl::core::task

namespace owl::physics {
class PhysicCommand;
/// Box2D world of one scene (defined by the physics module).
class PhysicsWorld;
}// namespace owl::physics

namespace owl::renderer {
class MaterialLibrary;
class MeshResource;
}// namespace owl::renderer

namespace owl::renderer::gpu {
class Framebuffer;
}// namespace owl::renderer::gpu

namespace owl::scene {
class WorldStreamer;
}// namespace owl::scene

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
struct MeshRenderer;
struct Animator;
struct MeshSockets;
struct SocketAttachment;
struct Light;
struct Light2D;
struct LightFlicker;
struct ParticleEmitter;
struct Trail;
struct Planet;
struct StreamingSource;
struct Oscillator;
struct Patrol;
struct Spline;
struct SplineFollower;
struct SequencePlayer;
struct Drift;
struct WorldPosition;
struct FollowTarget;
struct RenderTarget;
struct ProceduralSky;
struct DayNightCycle;
struct ReflectionProbe;
struct FirstPersonController;
struct Environment;
struct NativeScript;
struct PhysicBody;
struct Player;
struct PrefabLink;
struct RaycastDoor;
struct RaycastPushWall;
struct RendererTag;
struct SoundListener;
struct SoundSource;
struct ReverbZone;
struct AdaptiveMusic;
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
struct HudMarker;
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
 *  How a 2D layer takes its 2D lighting (`Scene::applyLighting2D`).
 */
struct OWL_API Layer2DLighting {
	/// The lights and the sprites meet on the ground plane (x, z) of a Y-up world (billboard layers).
	bool groundPlane = false;
	/// The layer draws into a 3D scene (billboards or depth tested): it follows the light and exposure of the scene.
	bool inScene3D = false;
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
	 *  Counters of the spatial culling of the last frame.
	 */
	struct CullingStatistics {
		/// Mesh entities filed in the spatial index.
		uint32_t meshesIndexed = 0;
		/// Mesh entities the last mesh pass found in its frustum.
		uint32_t meshesInFrustum = 0;
		/// Sprite entities filed in the spatial index.
		uint32_t spritesIndexed = 0;
		/// Sprite entities the last 2D pass found in its frustum.
		uint32_t spritesInFrustum = 0;
	};

	/**
	 * @brief
	 *  Get the counters of the spatial culling of the last frame.
	 * @return The counters.
	 */
	[[nodiscard]] auto getCullingStatistics() const -> const CullingStatistics& { return m_cullingStats; }

	/**
	 * @brief
	 *  Get the spatial index of the mesh entities (world boxes, ids are entity indices), current after a render.
	 * @return The index.
	 */
	[[nodiscard]] auto getMeshSpatialIndex() const -> const data::geometry::LooseOctree& { return m_meshIndex; }

	/**
	 * @brief
	 *  Get the entity an id of the mesh spatial index was filed from.
	 * @param[in] iId An id returned by a query of `getMeshSpatialIndex`.
	 * @return The entity, `entt::null` for an id no entity holds.
	 */
	[[nodiscard]] auto getMeshSpatialEntity(const uint32_t iId) const -> entt::entity {
		return iId < m_meshEntries.size() ? m_meshEntries[iId].handle : entt::null;
	}

	/**
	 * @brief
	 *  Get the spatial index of the sprite entities (world boxes, ids are entity indices), current after a render.
	 * @return The index.
	 */
	[[nodiscard]] auto getSpriteSpatialIndex() const -> const data::geometry::LooseOctree& { return m_spriteIndex; }

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
	 *  Set the light added to every lit surface while the scene is edited (never in Play), to see dark places.
	 * @param[in] iRadiance Linear radiance; black for none.
	 */
	void setEditorAmbient(const math::vec3& iRadiance) { m_editorAmbient = iRadiance; }

	/**
	 * @brief
	 *  Capture every reflection probe again at its next frame, after the geometry or the lighting changed.
	 */
	void requestReflectionProbeCapture() { ++m_probeGeneration; }

	/**
	 * @brief
	 *  Get the number of reflection probe captures the scene recorded since it was created.
	 * @return The capture count.
	 */
	[[nodiscard]] auto getReflectionProbeCaptureCount() const -> uint64_t { return m_probeCaptureCount; }

	/**
	 * @brief
	 *  Get the reflection probes the last frame gave `Renderer3D`, the most important first.
	 * @return The probes.
	 */
	[[nodiscard]] auto getReflectionProbes() const -> std::span<const renderer::ReflectionProbeData> {
		return m_probeData;
	}

	/**
	 * @brief
	 *  Get the light added to every lit surface while the scene is edited.
	 * @return The linear radiance.
	 */
	[[nodiscard]] auto getEditorAmbient() const -> const math::vec3& { return m_editorAmbient; }

	/**
	 * @brief
	 *  Show or hide the HUD while the scene is edited; Play always draws it.
	 * @param[in] iVisible True to draw the HUD in Edit mode.
	 */
	void setEditorHudVisible(const bool iVisible) { m_editorHudVisible = iVisible; }

	/**
	 * @brief
	 *  Whether the HUD is drawn while the scene is edited.
	 * @return True when the HUD is drawn in Edit mode.
	 */
	[[nodiscard]] auto isEditorHudVisible() const -> bool { return m_editorHudVisible; }

	/**
	 * @brief
	 *  The accessibility settings of the player, from the settings of the scene's application.
	 * @return The settings, the defaults without an engine context.
	 */
	[[nodiscard]] auto getAccessibility() const -> Accessibility;

	/**
	 * @brief
	 *  The subtitles of the scene, drawn at the bottom of the screen in Play.
	 * @return The subtitles.
	 */
	[[nodiscard]] auto getSubtitles() -> Subtitles& { return m_subtitles; }

	/**
	 * @brief
	 *  The subtitles of the scene, drawn at the bottom of the screen in Play.
	 * @return The subtitles.
	 */
	[[nodiscard]] auto getSubtitles() const -> const Subtitles& { return m_subtitles; }

	/**
	 * @brief
	 *  Set the colour vision filter Owl Nest previews the frame through (a simulation for the designer), in Edit and
	 *  in Play; an inactive filter gives the frame back to the player setting.
	 * @param[in] iFilter The filter.
	 */
	void setColorVisionPreview(const renderer::ColorVisionFilter& iFilter) { m_colorVisionPreview = iFilter; }

	/**
	 * @brief
	 *  The colour vision filter Owl Nest previews the frame through.
	 * @return The filter, inactive by default.
	 */
	[[nodiscard]] auto getColorVisionPreview() const -> const renderer::ColorVisionFilter& {
		return m_colorVisionPreview;
	}

	/**
	 * @brief
	 *  The colour vision filter of the final frame: the preview when active, else the correction of the
	 *  `color_blind_mode` setting outside Edit mode.
	 * @return The filter.
	 */
	[[nodiscard]] auto getColorVisionFilter() const -> renderer::ColorVisionFilter;

	/**
	 * @brief
	 *  Draw the edited scene through a camera, as `onUpdateEditor` does, without advancing anything: no toast timer, no
	 *  voxel streaming (it follows the camera of `onUpdateEditor`). A second view of the same frame (the camera preview
	 *  of the editor) draws with it.
	 * @param[in] iCamera The camera to draw through.
	 */
	void renderEditorView(const renderer::Camera& iCamera);

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
	 *  Draw the view of every enabled `RenderTarget` into its image, the overlays and the HUD left out.
	 *
	 * Called by `onRenderRuntime` and `onUpdateEditor` before the main view; the framebuffer bound before the call is
	 * bound again after it.
	 */
	void renderTargets();

	/**
	 * @brief
	 *  Get the image of a render target of the scene.
	 * @param[in] iName The `targetName` of the `RenderTarget`.
	 * @return Its framebuffer, or null when no render target of that name has drawn yet.
	 */
	[[nodiscard]] auto getRenderTarget(const std::string& iName) const -> shared<renderer::gpu::Framebuffer>;

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
	 *  Where a world point of an entity lands in the viewport of the frame being drawn.
	 *
	 * Projects through the layer the entity is routed to (`RendererTag`, the first layer when untagged), so a point
	 * of a raycast, isometric or 3D entity lands where that layer draws it. Only valid while the scene renders (the
	 * HUD pass): outside a frame there is no camera.
	 * @param[in] iEntity The entity whose layer projects the point.
	 * @param[in] iWorld The world point.
	 * @param[in] iViewport The viewport size in pixels.
	 * @return Its place in the viewport, or nothing outside a frame.
	 */
	[[nodiscard]] auto projectToViewport(const Entity& iEntity, const math::vec3& iWorld,
										 const math::vec2ui& iViewport) const -> std::optional<renderer::ViewportPoint>;

	/**
	 * @brief
	 *  Point the HUD markers measure their distance from: the primary player, else the camera of the frame.
	 * @return The world position, or nothing without a player outside a frame.
	 */
	[[nodiscard]] auto getViewerPosition() const -> std::optional<math::vec3>;

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
	 *  Set the outlined box of the interaction a first-person walker aims at, for this frame.
	 * @param[in] iBox World box to outline, or none to outline nothing.
	 */
	void setInteractionHighlight(const std::optional<math::box3f>& iBox) { m_interactionHighlight = iBox; }

	/**
	 * @brief
	 *  An entity outlined over the 3D scene: the silhouette of the meshes of its subtree.
	 */
	struct Outline {
		/// Root of the outlined subtree.
		core::UUID root{0};
		/// Colour of the line.
		math::vec4 color{1.f, 0.8f, 0.35f, 1.f};
	};

	/**
	 * @brief
	 *  Set the entity outlined from the next frame on (the selection of Owl Nest, what a first-person walker aims at).
	 * @param[in] iOutline The outline, or none to outline nothing.
	 */
	void setOutline(const std::optional<Outline>& iOutline) { m_outline = iOutline; }

	/**
	 * @brief
	 *  Get the entity outlined over the 3D scene.
	 * @return The outline, or none.
	 */
	[[nodiscard]] auto getOutline() const -> const std::optional<Outline>& { return m_outline; }

	/**
	 * @brief
	 *  Whether an entity is the given root or below it in the hierarchy.
	 * @param[in] iHandle The entity.
	 * @param[in] iRoot The root.
	 * @return True when it is in the subtree.
	 */
	[[nodiscard]] auto isInSubtree(entt::entity iHandle, entt::entity iRoot) const -> bool;

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
	 *  Snap an entity on a socket of another one: the owner becomes its parent and a `SocketAttachment` keeps its
	 *  local transform on the socket from then on (the hierarchy is rebuilt in place, nothing is reloaded).
	 * @param[in] iEntity The entity to attach.
	 * @param[in] iOwner The entity owning the socket (`MeshSockets` or a mesh socket of its `MeshRenderer`).
	 * @param[in] iSocket Name of the socket.
	 * @return False when the owner has no such socket or would become a child of the entity.
	 */
	auto attachToSocket(const Entity& iEntity, const Entity& iOwner, const std::string& iSocket) -> bool;

	/**
	 * @brief
	 *  Release an entity from its socket: the `SocketAttachment` is removed, the entity keeps its parent and its
	 *  current transform.
	 * @param[in] iEntity The entity.
	 * @return False when the entity is attached to no socket.
	 */
	auto detachFromSocket(const Entity& iEntity) -> bool;

	/**
	 * @brief
	 *  Get the transform of a socket relative to its owner, following its bone in the current pose.
	 * @param[in] iOwner The entity owning the socket.
	 * @param[in] iSocket Name of the socket.
	 * @return The local matrix of the socket, nothing when the owner has no such socket.
	 */
	[[nodiscard]] auto getSocketTransform(const Entity& iOwner, std::string_view iSocket) const
			-> std::optional<math::mat4>;

	/**
	 * @brief
	 *  List the sockets of an entity: those of its `MeshSockets`, then those of its mesh file.
	 * @param[in] iOwner The entity.
	 * @return The socket names, without duplicates.
	 */
	[[nodiscard]] auto getSocketNames(const Entity& iOwner) const -> std::vector<std::string>;

	/**
	 * @brief
	 *  Advance the animators of the skinned meshes and compute their poses (the `owl.animation` system).
	 * @param[in] iSeconds Elapsed time.
	 * @param[in] iPreviewOnly True in Edit mode: only the animators previewed in Owl Nest advance.
	 */
	void updateAnimators(float iSeconds, bool iPreviewOnly);

	/**
	 * @brief
	 *  Snap every entity holding a `SocketAttachment` on the socket of its parent (the `owl.sockets` system).
	 */
	void updateSocketAttachments();

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
	 *  Access the scene's mix (mutable): volume, mute and solo of each bus, Doppler factor, given to the mixer when
	 *  the scene starts.
	 * @return The mix.
	 */
	[[nodiscard]] auto getMixSettings() -> sound::MixSettings& { return m_mixSettings; }

	/**
	 * @brief
	 *  Access the scene's mix (const).
	 * @return The mix.
	 */
	[[nodiscard]] auto getMixSettings() const -> const sound::MixSettings& { return m_mixSettings; }

	/**
	 * @brief
	 *  Switch every running adaptive music of the scene to a state.
	 * @param[in] iState The name of the state.
	 * @return True when at least one music knows the state.
	 */
	auto setMusicState(std::string_view iState) -> bool;

	/**
	 * @brief
	 *  State of the first running adaptive music of the scene.
	 * @return The state, empty when no music runs.
	 */
	[[nodiscard]] auto getMusicState() const -> std::string;

	/**
	 * @brief
	 *  Access the scene's floating origin settings (mutable), read every frame by the `owl.floating_origin` system.
	 * @return The floating origin settings.
	 */
	[[nodiscard]] auto getFloatingOriginSettings() -> FloatingOriginSettings& { return m_floatingOriginSettings; }

	/**
	 * @brief
	 *  Access the scene's floating origin settings (const).
	 * @return The floating origin settings.
	 */
	[[nodiscard]] auto getFloatingOriginSettings() const -> const FloatingOriginSettings& {
		return m_floatingOriginSettings;
	}

	/**
	 * @brief
	 *  Access the scene's world partition settings (mutable), read every frame by the `owl.world_streaming` system.
	 * @return The world partition settings.
	 */
	[[nodiscard]] auto getWorldPartitionSettings() -> WorldPartitionSettings& { return m_worldPartitionSettings; }

	/**
	 * @brief
	 *  Access the scene's world partition settings (const).
	 * @return The world partition settings.
	 */
	[[nodiscard]] auto getWorldPartitionSettings() const -> const WorldPartitionSettings& {
		return m_worldPartitionSettings;
	}

	/**
	 * @brief
	 *  The streaming runtime of the world partition (which cells are loaded), created on the first call; a copy of
	 *  the scene starts with none.
	 * @return The streamer.
	 */
	[[nodiscard]] auto getWorldStreamer() -> WorldStreamer&;

	/**
	 * @brief
	 *  Whether the scene has a streaming runtime yet.
	 * @return True once `getWorldStreamer` was called.
	 */
	[[nodiscard]] auto hasWorldStreamer() const -> bool { return mp_worldStreamer != nullptr; }

	/**
	 * @brief
	 *  World position of an entity in double: its `WorldPosition` for a root that has one, the world origin plus its
	 *  world transform otherwise.
	 * @param[in] iEntity The entity.
	 * @return The world position.
	 */
	[[nodiscard]] auto getEntityWorldPosition(const Entity& iEntity) const -> math::vec3d;

	/**
	 * @brief
	 *  World position of the scene origin: the point the float `Transform` of a root entity is relative to.
	 * @return The world origin, in double precision.
	 */
	[[nodiscard]] auto getWorldOrigin() const -> const math::vec3d& { return m_worldOrigin; }

	/**
	 * @brief
	 *  Move the world origin, keeping every root entity where it is in the world.
	 *
	 * A root entity with a `WorldPosition` is placed again from it; any other root entity is translated by the shift.
	 * Children follow their parent.
	 * @param[in] iOrigin The new world origin.
	 */
	void shiftWorldOrigin(const math::vec3d& iOrigin);

	/**
	 * @brief
	 *  Bring the `WorldPosition` and the `Transform` of every root entity in step: a moved `Transform` updates the world
	 *  position, an edited world position moves the `Transform`.
	 */
	void syncWorldPositions();

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

	/// Highest accepted time scale.
	static constexpr float maxTimeScale = 16.f;

	/**
	 * @brief
	 *  Set the speed of the scene clock: the frame duration the runtime update gives to the scripts, the fixed ticks,
	 *  the physics, the animations and the behaviours is the real one times this scale.
	 *
	 * The UI input, the screen transitions and the toasts keep the real time. `onStartRuntime()` sets it back to 1.
	 * @param[in] iScale The scale, clamped to [0, `maxTimeScale`] (a non-finite one is ignored).
	 */
	void setTimeScale(float iScale);

	/**
	 * @brief
	 *  Speed of the scene clock.
	 * @return The scale set by `setTimeScale`, 1 by default.
	 */
	[[nodiscard]] auto getTimeScale() const -> float { return m_timeScale; }

	/**
	 * @brief
	 *  Pause or resume the scene clock: paused, the runtime update runs every frame with a zero duration (no tick, no
	 *  physics step, no animation), the scripts still get `on_update` to drive a pause menu.
	 *
	 * `onStartRuntime()` resumes it.
	 * @param[in] iPaused True to pause.
	 */
	void setPaused(const bool iPaused) { m_paused = iPaused; }

	/**
	 * @brief
	 *  Whether the scene clock is paused.
	 * @return True when paused.
	 */
	[[nodiscard]] auto isPaused() const -> bool { return m_paused; }

	/**
	 * @brief
	 *  Scale the runtime update applies to the frame duration.
	 * @return 0 when paused, else the time scale.
	 */
	[[nodiscard]] auto getEffectiveTimeScale() const -> float { return m_paused ? 0.f : m_timeScale; }

	/**
	 * @brief
	 *  Real duration of the last runtime frame, before the time scale and the pause.
	 * @return The duration in seconds.
	 */
	[[nodiscard]] auto getRealFrameSeconds() const -> float { return m_realFrameSeconds; }

	/**
	 * @brief
	 *  Queue the entities of a subtree spawned at runtime (a prefab instance) to start: their scripts are created
	 *  and get `on_create`, their animations play and their timer triggers start at the next safe point of the runtime
	 *  update (before or after the scripts of the frame), never inside the script call that spawned them.
	 * @param[in] iRoot The root of the subtree.
	 */
	void startSpawnedSubtree(const Entity& iRoot);

	/**
	 * @brief
	 *  Start the subtrees queued by `startSpawnedSubtree`; the ones destroyed meanwhile are skipped.
	 */
	void flushPendingSpawns();

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
	 *  Task scheduler the scene prepares its renders on: the application's one by default.
	 * @return The scheduler, or nullptr when the scene was created without application and none was given.
	 */
	[[nodiscard]] auto getTaskScheduler() const -> core::task::Scheduler* { return mp_taskScheduler; }

	/**
	 * @brief
	 *  Give the scene the scheduler it prepares its renders on (tests create their own); kept by `copy()`.
	 * @param[in] iScheduler The scheduler, or nullptr to prepare on the calling thread only.
	 */
	void setTaskScheduler(core::task::Scheduler* iScheduler) { mp_taskScheduler = iScheduler; }

	/**
	 * @brief
	 *  Smallest entity count for which `prepareWorldTransforms()` and the spatial index update run on the scheduler.
	 * @return The entity count.
	 */
	[[nodiscard]] auto getParallelMinEntities() const -> size_t { return m_parallelMinEntities; }

	/**
	 * @brief
	 *  Set the smallest entity count for which the render preparation runs on the scheduler; the result does not
	 *  depend on it (tests compare both paths).
	 * @param[in] iMinEntities The entity count: 0 always runs in parallel, `SIZE_MAX` never.
	 */
	void setParallelMinEntities(const size_t iMinEntities) { m_parallelMinEntities = iMinEntities; }

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
		/// Whether a `UiWindow` was shown when the UI input of the frame started (a click closing it stays in the UI).
		bool windowShown = false;
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
	 *  Name of the prefab the scene holds (`DocumentKind::Prefab`), written as the root key of its file.
	 * @return The name.
	 */
	[[nodiscard]] auto getPrefabName() const -> const std::string& { return m_prefabName; }

	/**
	 * @brief
	 *  Version of the prefab the scene holds (`DocumentKind::Prefab`).
	 * @return The version.
	 */
	[[nodiscard]] auto getPrefabVersion() const -> uint32_t { return m_prefabVersion; }

	/**
	 * @brief
	 *  Set the name and version of the prefab the scene holds (read from its file, written back with it).
	 * @param[in] iName The name.
	 * @param[in] iVersion The version.
	 */
	void setPrefabInfo(std::string iName, const uint32_t iVersion) {
		m_prefabName = std::move(iName);
		m_prefabVersion = iVersion;
	}

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
	 *  Seconds of simulated time since Play started: the sum of the frame durations the runtime update has run.
	 * @return The time, 0 outside Play.
	 */
	[[nodiscard]] auto getRuntimeSeconds() const -> double { return m_runtimeSeconds; }

	/**
	 * @brief
	 *  The script the scene owns (its `SceneScript:` key): it runs before the entity scripts, with the hooks of an
	 *  entity script and no entity (no `entity_id`).
	 * @return The script; an empty `scriptPath` means the scene has none.
	 */
	[[nodiscard]] auto getSceneScript() -> component::LuaScript& { return m_sceneScript; }

	/**
	 * @brief
	 *  The script the scene owns, read only.
	 * @return The script; an empty `scriptPath` means the scene has none.
	 */
	[[nodiscard]] auto getSceneScript() const -> const component::LuaScript& { return m_sceneScript; }

	/**
	 * @brief
	 *  The script of the HUD the scene draws, run in Play as a second scene script: the scene script of the
	 *  `.owlhud`, where the callbacks of its widgets are defined. Runtime only.
	 * @return The script; empty when the HUD has none or the scene is not playing.
	 */
	[[nodiscard]] auto getHudScript() const -> const component::LuaScript& { return m_hudScript; }

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

	/**
	 * @brief
	 *  Give the `Light2D` entities and the shadow-casting sprites of the layer being drawn to `Renderer2D`, which the
	 *  2D, tilemap and isometric renderers read; without an enabled light the 2D lighting is turned off.
	 * @param[in] iEditorMode True if the scene is being drawn for the editor view.
	 * @param[in] iLayer How the layer being drawn takes its lighting: a billboard layer places the lights on the
	 *  ground plane (x, z), without shadow; a layer of a 3D scene (billboards or depth tested) takes the light of the
	 *  3D scene as its ambient and undoes the exposure compensation of the camera.
	 */
	void applyLighting2D(bool iEditorMode, const Layer2DLighting& iLayer = {});

	/**
	 * @brief
	 *  The 2D lighting gathered by the last `applyLighting2D`.
	 * @return The lights, occluders and ambient given to `Renderer2D`.
	 */
	[[nodiscard]] auto getLighting2D() const -> const renderer::Lighting2D& { return m_lighting2D; }

	/**
	 * @brief
	 *  Whether the render stack has a world `Renderer2D` layer that is depth tested or turns its sprites to the camera:
	 *  the editor then draws the layers in their stack order, like Play, instead of one 2D composite under the 3D ones.
	 * @return True when such a layer exists.
	 */
	[[nodiscard]] static auto hasDepthTested2DLayer() -> bool;

private:
	/**
	 * @brief
	 *  Draw the 2D entities routed to the 2D layer being drawn: sprites, animated sprites, circles, texts and raycast
	 *  plates, turned to the camera and sorted by their view distance in a layer of a 3D scene.
	 * @param[in] iEditorMode True if the scene is being drawn for the editor view.
	 */
	void render2D(bool iEditorMode);

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
	/// Floating origin settings of the scene.
	FloatingOriginSettings m_floatingOriginSettings;
	/// Mix of the scene, given to the mixer when it starts.
	sound::MixSettings m_mixSettings;
	/// World partition settings (`WorldPartition:` in the scene file).
	WorldPartitionSettings m_worldPartitionSettings;
	/// Streaming runtime of the world partition.
	shared<WorldStreamer> mp_worldStreamer;
	/// World position of the scene origin.
	math::vec3d m_worldOrigin{0.0, 0.0, 0.0};
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
	/// Name of the prefab the scene holds.
	std::string m_prefabName = "untitled";
	/// Version of the prefab the scene holds.
	uint32_t m_prefabVersion = 1;
	/// HUD asset asked by the scene (nothing: the project's default HUD, empty: none).
	std::optional<std::string> m_hudPath;
	/// Seconds of simulated time since Play started.
	double m_runtimeSeconds = 0.0;
	/// Speed of the scene clock (`setTimeScale`).
	float m_timeScale = 1.f;
	/// Whether the scene clock is paused.
	bool m_paused = false;
	/// Real duration of the last runtime frame, in seconds.
	float m_realFrameSeconds = 0.f;
	/// Frame time step scaled by the time scale, reused every frame.
	core::Timestep m_scaledTimeStep;
	/// Roots of the subtrees spawned at runtime, started by `flushPendingSpawns()`.
	std::vector<core::UUID> m_pendingSpawns;
	/// Script the scene owns (runs before the entity scripts).
	component::LuaScript m_sceneScript;
	/// The script of the HUD, copied from the `.owlhud` and run in Play.
	component::LuaScript m_hudScript;
	/// Systems run every runtime frame, phase by phase.
	SystemSchedule m_systems;
	/// Engine state used by the scene (not owned).
	app::EngineContext* mp_engineContext = nullptr;
	/// Scheduler the render preparation runs on (not owned).
	core::task::Scheduler* mp_taskScheduler = nullptr;
	/// Entity count from which the render preparation runs on the scheduler.
	size_t m_parallelMinEntities = 4096;
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
		/// Unique number of the current `world` value (0 = never composed): entity index in the high half, count in the low half.
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
	/// Walk stacks of the parallel world preparation, one per chunk of the hierarchy pool; survive frames.
	mutable std::vector<std::vector<entt::entity>> m_worldWalkStacks;
	/// True when a `WorldNode` was composed since the spatial indexes last looked.
	mutable bool m_worldMoved = true;
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
	/// True while a parallel `prepareWorldTransforms()` resolves the nodes: slots are given afterwards, in order.
	mutable bool m_deferWorldSlots = false;
	/// Node index of the entity owning each slot of the last parallel `prepareWorldTransforms()` call.
	mutable std::vector<uint32_t> m_worldSlotOwners;
	/// Roots counted by the last `prepareWorldTransforms()` call (0 before the first).
	mutable size_t m_worldRootCount = 0;
	/// True when `m_worldSlotOwners` holds the slots the serial body would give to the current entities.
	mutable bool m_worldSlotsValid = false;
	/// Sizes and ends of the `Transform` and `Hierarchy` pools when the slots were given.
	mutable std::array<size_t, 8> m_worldPoolSignature{};
	/// True when an entity was added or removed, or a node got another entity or parent, since the slots were given.
	mutable bool m_worldLayoutDirty = true;
	/**
	 * @brief
	 *  What a spatial index entry was filed from, indexed by `entt::to_entity(handle)`.
	 */
	struct SpatialEntry {
		/// Entity filed (its version tells a recycled index apart), `entt::null` when free.
		entt::entity handle = entt::null;
		/// `WorldNode::stamp` of the world matrix the box was computed from.
		uint64_t stamp = 0;
		/// Mesh resource the box was computed from (null for a sprite).
		const void* source = nullptr;
		/// Update pass that last saw the entity.
		uint32_t pass = 0;
	};
	/// Spatial index of the `MeshRenderer` entities.
	data::geometry::LooseOctree m_meshIndex;
	/// Spatial index of the sprite entities (`SpriteRenderer`, `AnimatedSpriteRenderer`).
	data::geometry::LooseOctree m_spriteIndex;
	/// Sources of `m_meshIndex`.
	std::vector<SpatialEntry> m_meshEntries;
	/// Sources of `m_spriteIndex`.
	std::vector<SpatialEntry> m_spriteEntries;
	/// Current pass of `updateSpatialIndex()`.
	uint32_t m_spatialPass = 0;
	/**
	 * @brief
	 *  Box of an entity to refile in a spatial index, computed after the entities are walked.
	 */
	struct SpatialJob {
		/// Index to file the box in.
		data::geometry::LooseOctree* tree = nullptr;
		/// Entity index (`entt::to_entity`), also the index of its world node.
		uint32_t index = 0;
		/// Box of the entity in its own space.
		math::box3f localBox;
	};
	/// Boxes to refile in the current `updateSpatialIndex()` (scratch, kept for its capacity).
	std::vector<SpatialJob> m_spatialJobs;
	/// World boxes of `m_spatialJobs`, same order (scratch, kept for its capacity).
	std::vector<math::box3f> m_spatialBoxes;
	/**
	 * @brief
	 *  Entities of a spatial index found in a frustum, kept while neither the index nor the view-projection changes.
	 */
	struct VisibleSet {
		/// Entities found.
		std::vector<uint32_t> ids;
		/// One byte per entity index, set for the entities in `ids`.
		std::vector<uint8_t> marks;
		/// True when the whole index is in view: no mark is set, every entity is visible.
		bool all = false;
		/// True once a query filled the set.
		bool valid = false;
		/// Generation of the index the set was found in.
		uint64_t generation = 0;
		/// View-projection the set was found with.
		math::mat4 viewProjection = math::identity<float, 4>();
	};
	/// Visible entities of `m_meshIndex`.
	VisibleSet m_meshVisible;
	/// Visible entities of `m_spriteIndex`.
	VisibleSet m_spriteVisible;
	/// True when a sprite or mesh component was added, removed, replaced or got another mesh since the last update.
	bool m_spatialDirty = true;
	/// Counters of the spatial culling of the last frame.
	CullingStatistics m_cullingStats;
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
	 *  Resolve the mesh of every `MeshRenderer` through the engine context's mesh cache and upload its GPU mesh.
	 *
	 *  Called by `renderWithStack` before the first layer records a draw, so no mesh upload splits a render pass; a
	 *  mesh whose file changed (hot reload) is fetched again.
	 */
	void prepareMeshRenderData();

	/**
	 * @brief
	 *  Bring the spatial indexes up to date: the world box of every mesh and sprite entity, refiled only when its world
	 *  matrix or its mesh changed; the entities gone are removed.
	 */
	void updateSpatialIndex();

	/**
	 * @brief
	 *  Mark the entities of an index whose box touches the frustum of a view-projection.
	 * @param[in] iIndex The spatial index.
	 * @param[in] iViewProjection The view-projection of the pass.
	 * @param[in,out] ioSet The visible set of that index, reused when neither the index nor the view-projection changed.
	 * @return The number of entities marked; `isMarkedVisible` reads them.
	 */
	static auto markVisible(const data::geometry::LooseOctree& iIndex, const math::mat4& iViewProjection,
							VisibleSet& ioSet) -> uint32_t;

	/**
	 * @brief
	 *  Check whether a visible set holds an entity.
	 * @param[in] iSet The set.
	 * @param[in] iEntity The entity.
	 * @return True when marked.
	 */
	[[nodiscard]] static auto isMarkedVisible(const VisibleSet& iSet, entt::entity iEntity) -> bool;

	/**
	 * @brief
	 *  Registry listener: a component the spatial indexes follow was added, removed or replaced.
	 * @param[in,out] ioRegistry The registry of the scene.
	 * @param[in] iEntity The entity.
	 */
	void onSpatialComponentChanged(entt::registry& ioRegistry, entt::entity iEntity);

	/**
	 * @brief
	 *  Registry listener: an entity gained or lost its `Transform` or `Hierarchy`, so the world slots must be given again.
	 * @param[in,out] ioRegistry The registry of the scene.
	 * @param[in] iEntity The entity.
	 */
	void onWorldLayoutChanged(entt::registry& ioRegistry, entt::entity iEntity);

	/**
	 * @brief
	 *  Draw every visible `MeshRenderer` entity accepted by the current mesh layer through `RendererMesh`.
	 * @param[in] iEditorMode True if the scene is being drawn for the editor view.
	 */
	void renderMeshes(bool iEditorMode);

	/**
	 * @brief
	 *  Get the fraction of the screen height a mesh covers, seen from the camera of the last main pass.
	 * @param[in] iMesh The mesh.
	 * @param[in] iModel Its model matrix.
	 * @return The screen size, for the level-of-detail pick.
	 */
	[[nodiscard]] auto getScreenSize(const renderer::MeshResource& iMesh, const math::mat4& iModel) const -> float;

	/**
	 * @brief
	 *  Draw the particle emitters and the trails accepted by the current particle layer through `RendererParticles`.
	 * @param[in] iEditorMode True if the scene is being drawn for the editor view.
	 */
	void renderParticles(bool iEditorMode);

	/**
	 * @brief
	 *  Draw the planets accepted by the current planet layer through `RendererPlanet`.
	 * @param[in] iEditorMode True if the scene is being drawn for the editor view.
	 */
	void renderPlanets(bool iEditorMode);

	/**
	 * @brief
	 *  Update the terrain of every planet for the render camera (selection, background builds, uploads).
	 *
	 *  Called by `renderWithStack` before the first layer records a draw: the uploads would split a render pass.
	 */
	void preparePlanetRenderData();

	/**
	 * @brief
	 *  Run the pending GPU simulation steps of the particle emitters and sort them for the render camera.
	 *
	 *  Called by `renderWithStack` before the first layer records a draw: the compute dispatches would split a render
	 *  pass.
	 */
	void prepareParticleRenderData();

	/**
	 * @brief
	 *  Gather the lights that flare for the render camera, the brightest first, into `m_flareSources`.
	 * @return The number of sources.
	 */
	auto collectFlareSources() -> size_t;


	/**
	 * @brief
	 *  Resolve the material of one slot of a mesh renderer: its `.owlmat` (or the file's own material), its overrides,
	 *  then the component's texture and transparency.
	 * @param[in,out] ioMeshRenderer The component, its mesh loaded.
	 * @param[in] iSlot Index of the slot.
	 * @param[in,out] ioLibrary The material library.
	 */
	void resolveMaterialSlot(component::MeshRenderer& ioMeshRenderer, size_t iSlot,
							 renderer::MaterialLibrary& ioLibrary);

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
	 * Falls back to a single pass when the stack is empty. Centralises the
	 * `Renderer2D::resetStats / beginScene / endScene` book-keeping.
	 * @param[in] iCamera The camera used for the rendering passes.
	 */
	void renderWithStack(const renderer::Camera& iCamera);

	/**
	 * @brief
	 *  Add one frame-graph pass per renderer-stack layer with content, each drawing into the scene colour.
	 * @param[in,out] ioGraph The frame graph.
	 * @param[in] iSceneColor The linear scene colour target.
	 */
	void recordWorldPasses(renderer::RenderGraph& ioGraph, renderer::RenderGraph::Resource iSceneColor);

	/**
	 * @brief
	 *  Hand the visible `Light` entities to `Renderer3D` and, when a directional light casts shadows, add one
	 *  frame-graph pass per cascade drawing the opaque meshes and voxel worlds into its shadow map; the point and
	 *  spot lights casting shadows get theirs from `recordLocalShadowPasses`.
	 * @param[in,out] ioGraph The frame graph.
	 */
	void recordLightPasses(renderer::RenderGraph& ioGraph);

	/**
	 * @brief
	 *  Add the shadow passes of the point and spot lights nearest to the camera, within the budget of the backend:
	 *  one map per spot light, six faces copied into a cube per point light.
	 * @param[in,out] ioGraph The frame graph.
	 */
	void recordLocalShadowPasses(renderer::RenderGraph& ioGraph);

	/**
	 * @brief
	 *  Give `Renderer3D` the reflection probes nearest to the camera, within the budget of the backend, and add the
	 *  passes capturing the ones that need it: six faces of the scene into level 0 of the cube, then each prefiltered
	 *  level from the one above it. A probe is captured when it first shows, moves, changes size, is asked to
	 *  (`captureRequest`, `requestReflectionProbeCapture`, the environment finishing to load) and, for a realtime one,
	 *  in turn with the others, one per frame.
	 * @param[in,out] ioGraph The frame graph.
	 */
	void recordReflectionProbePasses(renderer::RenderGraph& ioGraph);

	/**
	 * @brief
	 *  Draw the sky and the 3D layers seen by one face camera of a probe, without reflection probes.
	 * @param[in] iCamera The face camera.
	 * @param[in] iFaceSize Size of the face, in texels.
	 */
	void renderProbeFace(const renderer::Camera& iCamera, uint32_t iFaceSize);

	/**
	 * @brief
	 *  Declare the reads of the shadow maps of the frame on a pass that samples them.
	 * @param[in,out] ioBuilder The builder of the pass.
	 */
	void readShadowTargets(renderer::RenderGraph::PassBuilder& ioBuilder) const;

	/**
	 * @brief
	 *  Add the depth of field and the motion blur of the render camera after the world passes, with the depth of the
	 *  3D layers they read.
	 * @param[in,out] ioGraph The frame graph.
	 * @param[in] iSceneColor The linear scene colour.
	 * @param[in] iFrameSize Size of the frame.
	 * @return The colour the bloom and the encoding read: the scene colour without any of these effects.
	 */
	auto recordDepthEffects(renderer::RenderGraph& ioGraph, renderer::RenderGraph::Resource iSceneColor,
							const math::vec2ui& iFrameSize) -> renderer::RenderGraph::Resource;

	/**
	 * @brief
	 *  Record the occlusion passes when a mesh layer with content asks for them: the depth of the mesh and voxel
	 *  occluders seen by the render camera, then the pyramid the mesh layers test their opaque meshes against.
	 * @param[in,out] ioGraph Graph of the frame.
	 */
	void recordOcclusionPasses(renderer::RenderGraph& ioGraph);

	/**
	 * @brief
	 *  Hand the first visible `Environment` to `Renderer3D` (loading and prefiltering its image on a worker the first
	 *  time) and, when it shows the sky, add the sky pass drawing into the scene colour.
	 * @param[in,out] ioGraph The frame graph.
	 * @param[in] iSceneColor The linear scene colour target.
	 */
	void recordEnvironmentPass(renderer::RenderGraph& ioGraph, renderer::RenderGraph::Resource iSceneColor);

	/**
	 * @brief
	 *  Add the outline of `m_outline` to a frame graph: its silhouette, the depth of the scene, then the line over the
	 *  display target. Nothing without an outline, or when its subtree holds no mesh.
	 * @param[in,out] ioGraph The graph of the frame.
	 * @param[in] iTarget The display target.
	 */
	void recordOutlinePasses(renderer::RenderGraph& ioGraph, renderer::RenderGraph::Resource iTarget);

	/**
	 * @brief
	 *  Start drawing and prefiltering the procedural sky on a worker when its inputs changed and no drawing runs; the
	 *  previous sky stays lit until the new one is ready.
	 * @param[in] iSky The sky of the environment.
	 */
	void requestProceduralSky(const component::ProceduralSky& iSky);

	/**
	 * @brief
	 *  Draw the opaque geometry of the 3D layers (meshes, voxel worlds) with the current camera; used by the shadow
	 *  passes.
	 * @param[in] iCamera The camera drawing the geometry.
	 * @param[in] iMeshesOnly Whether only the mesh layers draw (an outline mask), the voxel worlds left out.
	 */
	void renderShadowCasters(const renderer::Camera& iCamera, bool iMeshesOnly = false);

	/**
	 * @brief
	 *  Give `Renderer3D` the shadow maps the cascade passes drew, before a 3D layer runs.
	 * @param[in] iContext Context of the running pass.
	 */
	void useShadowMaps(const renderer::RenderGraph::PassContext& iContext);

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
	 *  Set the frame time step of the scene clock (`m_scaledTimeStep`): the real one times the effective time scale.
	 * @param[in] iTimeStep The real frame time step.
	 */
	void updateScaledTimeStep(const core::Timestep& iTimeStep);

	/**
	 * @brief
	 *  Start one entity of a spawned subtree: its script, its animation, its timer trigger.
	 * @param[in] iEntity The entity.
	 */
	void startSpawnedEntity(const Entity& iEntity);

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
	 *  Check one world node against its local transform and its parent, recompose it when either changed, and mark it
	 *  checked in the current pass (with its inherited visibility while preparing).
	 * @param[in,out] ioNode The node of `iHandle`.
	 * @param[in] iHandle The entity.
	 * @param[in] iParent The parent entity, `entt::null` for a root or a missing parent.
	 * @param[in] iParentNode The parent's node, already checked in this pass (nullptr without parent).
	 * @param[in] iLocal The local transform of the entity.
	 * @param[in] iParentId The `Hierarchy` parent UUID of the entity.
	 * @param[in] iVisibilities The `Visibility` storage (nullptr when not preparing).
	 */
	void refreshWorldNode(WorldNode& ioNode, entt::entity iHandle, entt::entity iParent, const WorldNode* iParentNode,
						  const math::Transform& iLocal, core::UUID iParentId,
						  const entt::storage_for_t<const component::Visibility>* iVisibilities) const;

	/**
	 * @brief
	 *  Serial body of `prepareWorldTransforms()`: every entity is checked after its ancestors and gets its slot.
	 */
	void prepareWorldsSerial() const;

	/**
	 * @brief
	 *  Parallel body of `prepareWorldTransforms()`: the subtrees of the roots are checked on the scheduler, the
	 *  entities its walk missed are resolved serially, then the slots are given in the order the serial body gives them.
	 * @param[in,out] ioScheduler The scheduler running the subtrees.
	 */
	void prepareWorldsParallel(core::task::Scheduler& ioScheduler) const;

	/// The component storages the parallel preparation reads, fetched once per chunk.
	struct WorldStorages;

	/**
	 * @brief
	 *  Check a root and every descendant reachable through `childrenIds` whose `parentId` agrees.
	 * @param[in] iStorages The component storages.
	 * @param[in] iRoot The root entity.
	 * @param[in,out] ioStack Scratch stack of the calling thread.
	 * @return The number of nodes checked, the root included.
	 */
	auto refreshWorldSubtree(const WorldStorages& iStorages, entt::entity iRoot,
							 std::vector<entt::entity>& ioStack) const -> size_t;

	/**
	 * @brief
	 *  Give the entities checked in the current pass their slot, ancestors first, in view order.
	 */
	void assignWorldSlots() const;

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
	 * Empty during the single pass of an empty stack. Set by `renderWithStack` before each `render()` call so per-layer
	 * entity filtering can run.
	 */
	std::string m_currentLayerName;
	/// Whether the layer being processed is the first one in the stack (untagged entities default to it).
	bool m_currentLayerIsFirst = true;
	/**
	 * @brief
	 *  Active layer driving the current `render()` pass.
	 *
	 * May be null during the single pass of an empty stack. Used by `render()` to dispatch type-specific
	 * entity rendering (e.g. send tilemap walls to `RendererRaycast` instead of the per-cell
	 * `Renderer2D` quad loop).
	 */
	const renderer::RenderLayer* mp_currentLayer = nullptr;
	/// Whether `renderWithStack` draws a render target: no overlay, no HUD.
	bool m_drawingRenderTarget = false;
	/// The render target being drawn, whose hidden layers are skipped; null for the main view.
	const component::RenderTarget* mp_drawnTarget = nullptr;
	/// Camera of the frame-graph run in progress (`renderWithStack`).
	const renderer::Camera* mp_renderCamera = nullptr;
	/// Lights of the frame, as `Renderer3D` reads them.
	std::vector<renderer::GpuLight> m_gpuLights;
	/// 2D lights and occluders of the layer being drawn, reused every frame.
	renderer::Lighting2D m_lighting2D;
	/// Light added to every lit surface in Edit mode.
	math::vec3 m_editorAmbient{0.f, 0.f, 0.f};
	/// Whether the HUD is drawn in Edit mode.
	bool m_editorHudVisible = true;
	/// Colour vision filter previewed by Owl Nest, inactive by default.
	renderer::ColorVisionFilter m_colorVisionPreview;
	/// Accessibility settings of the frame being recorded.
	Accessibility m_frameAccessibility;
	/// Subtitles on screen.
	Subtitles m_subtitles;
	/// Targets the colour encode of the frame reads.
	struct PostTargets {
		/// The linear scene colour.
		renderer::RenderGraph::Resource sceneColor;
		/// The bloom chain result, invalid without bloom.
		renderer::RenderGraph::Resource bloom;
		/// The adapted EV100, invalid with a manual exposure.
		renderer::RenderGraph::Resource exposure;
	};
	/// Post-process targets of the frame being recorded.
	PostTargets m_postTargets;
	/// Depth of the 3D layers the depth of field and the motion blur read this frame, invalid without them.
	renderer::RenderGraph::Resource m_postDepth{};
	/// Lights flaring in the frame being recorded, the brightest first.
	std::array<renderer::LensFlareSource, renderer::LensFlare::g_maxSources> m_flareSources{};
	/// Projection and view of the render camera in the previous frame of Play, for the motion blur.
	struct PreviousView {
		/// Projection of the camera.
		math::mat4 projection;
		/// View of the camera.
		math::mat4 view;
	};
	/// The previous frame of Play, empty before the first one.
	std::optional<PreviousView> m_previousView;
	/// Shadow cascades of the frame.
	renderer::ShadowCascades m_shadowCascades;
	/// Shadow of the frame, its maps filled when the 3D layers run.
	renderer::ShadowSettings m_shadowSettings;
	/// A point or spot light of the frame that casts a shadow, by its index in `m_gpuLights`.
	struct LocalCaster {
		/// World position of the light.
		math::vec3 position{0.f, 0.f, 0.f};
		/// Direction the light shines (spot).
		math::vec3 direction{0.f, 0.f, -1.f};
		/// Half angle of the outer cone (spot), in radians.
		float outerAngle = 0.f;
		/// Range of the light.
		float range = 10.f;
		/// Size of the shadow map, or of a cube face, in texels.
		uint32_t resolution = 1024;
		/// Normal-offset bias, in texels.
		float bias = 1.5f;
		/// Radius of the emitting surface of an area light (0 for a point or spot light).
		float lightSize = 0.f;
		/// Whether the light casts through a cube (a point or area light; a spot light otherwise).
		bool point = false;
	};
	/// Local casters of the frame, by light index (meaningful for the shadow candidates only).
	std::vector<LocalCaster> m_localCasters;
	/// Point and spot lights that may get a shadow this frame.
	std::vector<renderer::ShadowCandidate> m_shadowCandidates;
	/// Shadows of the local lights of the frame, their spot maps filled when the 3D layers run.
	renderer::LocalShadowSettings m_localShadows;
	/// Shadow-map targets of the spot shadows of the frame.
	std::vector<renderer::RenderGraph::Resource> m_spotShadowTargets;
	/// Face targets of the point shadows of the frame, one per face size.
	std::vector<renderer::RenderGraph::Resource> m_pointFaceTargets;
	/// Light cameras of the spot shadows of the frame.
	std::vector<renderer::Camera> m_spotShadowCameras;
	/// Face cameras of the point shadows of the frame.
	std::vector<renderer::PointShadowFaces> m_pointShadowFaces;
	/// Cubes of the point shadows, kept from frame to frame.
	std::vector<shared<renderer::gpu::TextureCube>> m_pointShadowCubes;
	/// What the scene keeps of a reflection probe from frame to frame.
	struct ProbeState {
		/// The prefiltered cube.
		shared<renderer::gpu::TextureCube> cube;
		/// The raw capture and its mip chain, the source of the filter.
		shared<renderer::gpu::TextureCube> capture;
		/// World position of the last capture.
		math::vec3 position{0.f, 0.f, 0.f};
		/// Exposure of the last capture.
		float captureExposure = 1.f;
		/// `m_probeGeneration` at the last capture.
		uint32_t generation = 0;
		/// `captureRequest` of the component at the last capture.
		uint32_t request = 0;
		/// Frame of the last capture.
		uint64_t frame = 0;
		/// Whether the cube holds a capture.
		bool captured = false;
	};
	/// Reflection probes by entity.
	std::unordered_map<entt::entity, ProbeState> m_probeStates;
	/// A probe captured this frame: its cubes, its face cameras and the targets of its levels.
	struct ProbeCapture {
		/// The prefiltered cube being filled.
		shared<renderer::gpu::TextureCube> cube;
		/// The raw capture being filled with its mip chain.
		shared<renderer::gpu::TextureCube> capture;
		/// Cameras of the six faces.
		renderer::PointShadowFaces faces;
		/// Target of each level of the mip chain, level 0 the capture.
		std::vector<renderer::RenderGraph::Resource> chainTargets;
		/// Target of each filtered level, from level 1.
		std::vector<renderer::RenderGraph::Resource> filterTargets;
	};
	/// Probes captured this frame.
	std::vector<ProbeCapture> m_probeCaptures;
	/// Probes of the frame, as `Renderer3D` reads them.
	std::vector<renderer::ReflectionProbeData> m_probeData;
	/// Raised to capture every probe again.
	uint32_t m_probeGeneration = 0;
	/// Probe captures recorded since the scene was created.
	uint64_t m_probeCaptureCount = 0;
	/// Frames that recorded the probes, to take the realtime ones in turn.
	uint64_t m_probeFrame = 0;
	/// Whether the environment pass of the frame draws the sky.
	bool m_drawsSky = false;
	/// The environment image of the scene: its loading on a worker, then its GPU textures.
	struct EnvironmentState {
		/// Asset path of the loaded (or loading) image; empty for a procedural sky.
		std::filesystem::path hdri;
		/// Whether the environment is a procedural sky.
		bool procedural = false;
		/// Quantised inputs of the procedural sky drawn last, to draw it again only when they change.
		std::array<float, 16> skyKey{};
		/// Result of the worker, read once its task finished.
		shared<std::optional<renderer::EnvironmentMap>> result;
		/// Task loading and prefiltering the image.
		size_t task = 0;
		/// Whether a task runs.
		bool loading = false;
		/// The uploaded environment.
		renderer::EnvironmentLighting lighting;

		/**
		 * @brief
		 *  Whether an environment was uploaded.
		 * @return True once the first image is on the GPU.
		 */
		[[nodiscard]] auto levelsReady() const -> bool { return lighting.levelCount > 0; }
	};
	/// Environment of the scene.
	EnvironmentState m_environment;
	/// Shadow-map targets of the frame, one per cascade.
	std::array<renderer::RenderGraph::Resource, renderer::g_maxShadowCascades> m_shadowTargets{};
	/// Depth of the occluders from the camera this frame (the outline reuses it); invalid without occlusion.
	renderer::RenderGraph::Resource m_occluderDepth{};
	/// Token target of the occlusion pyramid pass, read by the 3D passes; invalid without occlusion.
	renderer::RenderGraph::Resource m_occlusionToken{};

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
	/// Entity outlined over the 3D scene.
	std::optional<Outline> m_outline;
	/// View-projection of the last main (not shadow) mesh pass, the camera of the level-of-detail pick.
	math::mat4 m_lodViewProjection = math::identity<float, 4>();
	/// Root of the subtree `renderMeshes` draws alone (the outline mask), `entt::null` for every mesh.
	entt::entity m_meshFilterRoot = entt::null;
	/// World box of the interaction aimed at this frame, outlined over the scene.
	std::optional<math::box3f> m_interactionHighlight;
	/// True while a captured `VoxelPlayer` is active (draws the centre-screen aiming crosshair).
	bool m_showCrosshair = false;

	friend class Entity;
	friend class ScriptableEntity;
	friend class physics::PhysicCommand;
	friend class SaveManager;
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
OWL_API void Scene::onComponentAdded<component::MeshRenderer>(const Entity& iEntity,
															  component::MeshRenderer& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Light>(const Entity& iEntity, component::Light& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::ReflectionProbe>(const Entity& iEntity,
																 component::ReflectionProbe& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Light2D>(const Entity& iEntity, component::Light2D& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Animator>(const Entity& iEntity, component::Animator& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::MeshSockets>(const Entity& iEntity,
															 component::MeshSockets& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::SocketAttachment>(const Entity& iEntity,
																  component::SocketAttachment& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::LightFlicker>(const Entity& iEntity,
															  component::LightFlicker& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::ParticleEmitter>(const Entity& iEntity,
																 component::ParticleEmitter& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Trail>(const Entity& iEntity, component::Trail& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Planet>(const Entity& iEntity, component::Planet& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::StreamingSource>(const Entity& iEntity,
																 component::StreamingSource& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Oscillator>(const Entity& iEntity, component::Oscillator& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Patrol>(const Entity& iEntity, component::Patrol& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Spline>(const Entity& iEntity, component::Spline& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::SplineFollower>(const Entity& iEntity,
																component::SplineFollower& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::SequencePlayer>(const Entity& iEntity,
																component::SequencePlayer& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Drift>(const Entity& iEntity, component::Drift& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::WorldPosition>(const Entity& iEntity,
															   component::WorldPosition& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::FollowTarget>(const Entity& iEntity,
															  component::FollowTarget& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::RenderTarget>(const Entity& iEntity,
															  component::RenderTarget& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::ProceduralSky>(const Entity& iEntity,
															   component::ProceduralSky& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::DayNightCycle>(const Entity& iEntity,
															   component::DayNightCycle& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::FirstPersonController>(const Entity& iEntity,
																	   component::FirstPersonController& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::Environment>(const Entity& iEntity,
															 component::Environment& ioComponent);

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
OWL_API void Scene::onComponentAdded<component::ReverbZone>(const Entity& iEntity, component::ReverbZone& ioComponent);

template<>
OWL_API void Scene::onComponentAdded<component::AdaptiveMusic>(const Entity& iEntity,
															   component::AdaptiveMusic& ioComponent);

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
OWL_API void Scene::onComponentAdded<component::HudMarker>(const Entity& iEntity, component::HudMarker& ioComponent);

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
