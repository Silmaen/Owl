/**
 * @file Scene.cpp
 * @author Silmaen
 * @date 22/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/Scene.h"

#include "data/voxel/TerrainGenerator.h"
#include "math/Frustum.h"
#include "platform/FileUtils.h"
#include "renderer/BackgroundRenderer.h"
#include "renderer/CameraOrtho.h"
#include "renderer/ClipSpace.h"
#include "renderer/ColorEncoder.h"
#include "renderer/IsometricObjects.h"
#include "renderer/MaterialLibrary.h"
#include "renderer/MeshCache.h"
#include "renderer/OutlinePass.h"
#include "renderer/PostProcess.h"
#include "renderer/ProceduralSky.h"
#include "renderer/Renderer.h"
#include "renderer/Renderer2D.h"
#include "renderer/Renderer2DLayer.h"
#include "renderer/RendererIsometric.h"
#include "renderer/RendererIsometricLayer.h"
#include "renderer/RendererMesh.h"
#include "renderer/RendererMeshLayer.h"
#include "renderer/RendererRaycast.h"
#include "renderer/RendererRaycastLayer.h"
#include "renderer/RendererTilemap.h"
#include "renderer/RendererVoxel.h"
#include "renderer/gpu/Framebuffer.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/StorageBuffer.h"
#include "renderer/utils/Billboards.h"
#include "scene/ComponentRegistry.h"
#include "scene/Entity.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"
#include "scene/nav/NavDebugDraw.h"
#include "scene/nav/NavWorld.h"
#include "systems/EngineSystems.h"

#include "ScriptLoader.h"
#include "app/Application.h"
#include "app/EngineContext.h"
#include "core/task/Scheduler.h"
#include "core/task/Task.h"
#include "data/voxel/Chunk.h"
#include "input/Input.h"
#include "physics/PhysicCommand.h"
#include "scene/HudLibrary.h"
#include "scene/ScreenTransition.h"
#include "scene/component/components.h"
#include "scene/ui/HudPass.h"
#include "script/ScriptEngine.h"
#include "script/ScriptInstance.h"
#include "sound/SoundCommand.h"
#include "sound/SoundSystem.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <mutex>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace owl::scene {

// One worker-generated chunk (entity id, coord, parameters it was generated with, filled chunk) awaiting installation.
struct CompletedVoxelChunk {
	int entityId;
	math::vec3i coord;
	data::voxel::TerrainParams params;
	shared<data::voxel::Chunk> chunk;
};

// Async voxel generation sink: workers push completed chunks under the mutex, the main thread drains them.
struct VoxelStreamState {
	std::mutex mutex;
	std::vector<CompletedVoxelChunk> completed;
};

namespace {

// Entities of one chunk handed to a worker: below it the dispatch costs more than the work it spreads.
constexpr size_t g_ParallelChunkEntities = 1024;

// Roots below which the subtrees are too few to share between workers.
constexpr size_t g_ParallelMinRoots = 16;

auto unpackChunkKey(const uint64_t iKey) -> math::vec3i {
	const auto dec = [](const uint64_t iBits) -> int32_t {
		return static_cast<int32_t>(static_cast<int64_t>(iBits & 0x1FFFFF) - (1 << 20));
	};
	return math::vec3i{dec(iKey), dec(iKey >> 21), dec(iKey >> 42)};
}

void copyCopiableComponents(Entity& oDst, const Entity& iSrc) {
	for (const auto& desc: ComponentRegistry::getAll())
		if (desc.copiable)
			desc.copy(oDst, iSrc);
}

// A world-space string takes its em height from the Y scale: the X scale does not squeeze the glyphs.
auto uniformTextTransform(math::Transform iTransform) -> math::Transform {
	iTransform.scale().x() = std::copysign(iTransform.scale().y(), iTransform.scale().x());
	return iTransform;
}

auto collectSubtree(const Scene& iScene, const Entity& iRoot) -> std::vector<Entity> {
	std::vector<Entity> subtree{iRoot};
	for (size_t i = 0; i < subtree.size(); ++i) {
		for (const auto childId: subtree[i].getComponent<component::Hierarchy>().childrenIds) {
			if (const Entity child = iScene.findEntityByUUID(childId); child)
				subtree.push_back(child);
		}
	}
	return subtree;
}

void releaseLuaScript(entt::registry& ioRegistry, const entt::entity iEntity) {
	// Taken out first: `on_destroy` may grow the storages and move the component.
	if (const auto instance = std::move(ioRegistry.get<component::LuaScript>(iEntity).instance);
		instance && instance->isValid())
		instance->onDestroy();
}

void releaseNativeScript(entt::registry& ioRegistry, const entt::entity iEntity) {
	if (auto& nsc = ioRegistry.get<component::NativeScript>(iEntity); nsc.instance != nullptr) {
		nsc.instance->onDestroy();
		if (nsc.destroyScript != nullptr)
			nsc.destroyScript(&nsc);
		nsc.instance = nullptr;
	}
}

void releaseSoundSource(entt::registry& ioRegistry, const entt::entity iEntity) {
	auto& soundComp = ioRegistry.get<component::SoundSource>(iEntity).sound;
	if (soundComp.runtimeHandle != sound::invalidSoundHandle)
		sound::SoundCommand::stop(soundComp.runtimeHandle);
	soundComp.runtimeHandle = sound::invalidSoundHandle;
}

void releaseRuntimeResources(const Entity& iEntity) {
	auto& registry = iEntity.getScene()->registry;
	const auto handle = static_cast<entt::entity>(iEntity);
	if (registry.all_of<component::LuaScript>(handle))
		releaseLuaScript(registry, handle);
	if (registry.all_of<component::NativeScript>(handle))
		releaseNativeScript(registry, handle);
	physics::PhysicCommand::destroyBody(iEntity);
	if (registry.all_of<component::SoundSource>(handle))
		releaseSoundSource(registry, handle);
}

void connectRuntimeHooks(entt::registry& ioRegistry) {
	ioRegistry.on_destroy<component::LuaScript>().connect<&releaseLuaScript>();
	ioRegistry.on_destroy<component::NativeScript>().connect<&releaseNativeScript>();
	ioRegistry.on_destroy<component::SoundSource>().connect<&releaseSoundSource>();
}

void disconnectRuntimeHooks(entt::registry& ioRegistry) {
	ioRegistry.on_destroy<component::LuaScript>().disconnect<&releaseLuaScript>();
	ioRegistry.on_destroy<component::NativeScript>().disconnect<&releaseNativeScript>();
	ioRegistry.on_destroy<component::SoundSource>().disconnect<&releaseSoundSource>();
}

}// namespace

Scene::Scene()
	: m_systems{SystemSchedule::getDefault()},
	  mp_engineContext{app::Application::instanced() ? &app::Application::get().getEngineContext() : nullptr},
	  mp_taskScheduler{app::Application::instanced() ? &app::Application::get().getTaskScheduler() : nullptr} {
	registry.on_construct<component::Transform>().connect<&Scene::onWorldLayoutChanged>(*this);
	registry.on_destroy<component::Transform>().connect<&Scene::onWorldLayoutChanged>(*this);
	registry.on_construct<component::Hierarchy>().connect<&Scene::onWorldLayoutChanged>(*this);
	registry.on_destroy<component::Hierarchy>().connect<&Scene::onWorldLayoutChanged>(*this);
	registry.on_construct<component::SpriteRenderer>().connect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_update<component::SpriteRenderer>().connect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_destroy<component::SpriteRenderer>().connect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_construct<component::AnimatedSpriteRenderer>().connect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_update<component::AnimatedSpriteRenderer>().connect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_destroy<component::AnimatedSpriteRenderer>().connect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_construct<component::MeshRenderer>().connect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_update<component::MeshRenderer>().connect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_destroy<component::MeshRenderer>().connect<&Scene::onSpatialComponentChanged>(*this);
}

Scene::~Scene() {
	registry.on_construct<component::Transform>().disconnect<&Scene::onWorldLayoutChanged>(*this);
	registry.on_destroy<component::Transform>().disconnect<&Scene::onWorldLayoutChanged>(*this);
	registry.on_construct<component::Hierarchy>().disconnect<&Scene::onWorldLayoutChanged>(*this);
	registry.on_destroy<component::Hierarchy>().disconnect<&Scene::onWorldLayoutChanged>(*this);
	registry.on_construct<component::SpriteRenderer>().disconnect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_update<component::SpriteRenderer>().disconnect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_destroy<component::SpriteRenderer>().disconnect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_construct<component::AnimatedSpriteRenderer>().disconnect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_update<component::AnimatedSpriteRenderer>().disconnect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_destroy<component::AnimatedSpriteRenderer>().disconnect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_construct<component::MeshRenderer>().disconnect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_update<component::MeshRenderer>().disconnect<&Scene::onSpatialComponentChanged>(*this);
	registry.on_destroy<component::MeshRenderer>().disconnect<&Scene::onSpatialComponentChanged>(*this);
	disconnectRuntimeHooks(registry);
	physics::PhysicCommand::destroy(*this);
}

auto Scene::getNavigation() const -> nav::NavWorld& {
	if (m_navigation == nullptr)
		m_navigation = mkUniq<nav::NavWorld>();
	return *m_navigation;
}

auto Scene::copy(const shared<Scene>& iOther) -> shared<Scene> {
	shared<Scene> newScene = mkShared<Scene>();

	newScene->m_viewportSize = iOther->m_viewportSize;
	newScene->m_gameState = iOther->m_gameState;
	newScene->m_enabledRenderers = iOther->m_enabledRenderers;
	newScene->m_physicsSettings = iOther->m_physicsSettings;
	newScene->m_hudPath = iOther->m_hudPath;
	newScene->m_sceneScript = iOther->m_sceneScript;
	newScene->m_documentKind = iOther->m_documentKind;
	newScene->m_simulationClock.setSettings(iOther->m_simulationClock.getSettings());
	newScene->m_systems = iOther->m_systems;
	newScene->mp_engineContext = iOther->mp_engineContext;
	newScene->mp_taskScheduler = iOther->mp_taskScheduler;
	newScene->m_parallelMinEntities = iOther->m_parallelMinEntities;

	auto& srcSceneRegistry = iOther->registry;
	auto& dstSceneRegistry = newScene->registry;
	std::unordered_map<core::UUID, entt::entity> enttMap;

	// Create entities in new scene
	for (const auto idView = srcSceneRegistry.view<component::ID>(); const auto e: idView) {
		const core::UUID uuid = srcSceneRegistry.get<component::ID>(e).id;
		const auto& name = srcSceneRegistry.get<component::Tag>(e).tag;
		const Entity newEntity = newScene->createEntityWithUUID(uuid, name);
		enttMap[uuid] = static_cast<entt::entity>(newEntity);
	}

	// Copy components (except IDComponent and TagComponent)
	for (const auto& desc: ComponentRegistry::getAll())
		if (desc.copiable)
			desc.copyAll(dstSceneRegistry, srcSceneRegistry, enttMap);
	// In-flight streaming jobs belong to the source scene: the copy must queue its own.
	for (const auto view = dstSceneRegistry.view<component::VoxelWorld>(); const auto entity: view)
		view.get<component::VoxelWorld>(entity).pendingChunks.clear();

	return newScene;
}

auto Scene::getEffectiveHudPath() const -> const std::string& {
	static const std::string noHud;
	if (m_hudPath)
		return *m_hudPath;
	return mp_engineContext != nullptr ? mp_engineContext->getDefaultHud() : noHud;
}

auto Scene::getHud() const -> shared<Scene> {
	if (mp_engineContext == nullptr)
		return nullptr;
	const auto& path = getEffectiveHudPath();
	return path.empty() ? nullptr : mp_engineContext->getHudLibrary().get(path);
}

auto Scene::createEntity(const std::string& iName) -> Entity { return createEntityWithUUID(core::UUID(), iName); }

auto Scene::createEntityWithUUID(const core::UUID iUuid, const std::string& iName) -> Entity {
	Entity entity = {registry.create(), this};
	entity.addComponent<component::Transform>();
	auto& [id] = entity.addComponent<component::ID>();
	id = iUuid;
	auto& [tag] = entity.addComponent<component::Tag>();
	tag = iName.empty() ? "Entity" : iName;
	entity.addComponent<component::Visibility>();
	entity.addComponent<component::Hierarchy>();
	m_uuidIndex.insert_or_assign(iUuid, entity.m_entityHandle);
	return entity;
}

void Scene::destroyEntity(Entity& ioEntity) {
	releaseRuntimeResources(ioEntity);
	const core::UUID uuid = ioEntity.getUUID();
	const auto [grandParentId, childrenIds] = ioEntity.getComponent<component::Hierarchy>();
	const Entity grandParent = grandParentId != core::UUID{0} ? findEntityByUUID(grandParentId) : Entity{};
	const math::mat4 toGrandParent =
			grandParent ? math::inverse(getWorldTransform(grandParent)()) : math::identity<float, 4>();
	std::vector<core::UUID> adopted;
	for (const auto childId: childrenIds) {
		const Entity child = findEntityByUUID(childId);
		if (!child)
			continue;
		const math::Transform childWorld = getWorldTransform(child);
		child.getComponent<component::Hierarchy>().parentId = grandParent ? grandParentId : core::UUID{0};
		child.getComponent<component::Transform>().transform = math::Transform{toGrandParent * childWorld()};
		adopted.push_back(childId);
	}
	// The children take the deleted entity's slot among the grandparent's children, keeping sibling order.
	if (grandParent) {
		auto& siblings = grandParent.getComponent<component::Hierarchy>().childrenIds;
		auto slot = std::ranges::find(siblings, uuid);
		if (slot != siblings.end())
			slot = siblings.erase(slot);
		siblings.insert(slot, adopted.begin(), adopted.end());
	}
	if (m_primaryPlayerCache == ioEntity.m_entityHandle)
		m_primaryPlayerCache = entt::null;
	m_uuidIndex.erase(ioEntity.getUUID());
	registry.destroy(ioEntity.m_entityHandle);
	ioEntity.m_entityHandle = entt::null;
}

auto Scene::getActionMap() const -> shared<const input::ActionMap> {
	return mp_engineContext != nullptr ? mp_engineContext->getActionMap() : input::ActionMap::getDefault();
}

void Scene::onStartRuntime() {
	OWL_PROFILE_FUNCTION()

	status = Status::Playing;
	if (mp_engineContext != nullptr)
		m_simulationClock.setSettings(mp_engineContext->getSimulationSettings());
	m_simulationClock.reset();
	m_runtimeSeconds = 0.0;
	m_timeScale = 1.f;
	m_paused = false;
	m_pendingSpawns.clear();
	const auto heldInput = input::Input::readState();
	m_frameInput.reset(heldInput);
	m_tickInput.reset(heldInput);
	m_inputSampler.reset(heldInput);
	m_frameActions.update(getActionMap(), m_frameInput);
	m_tickActions.update(getActionMap(), m_tickInput);

	using clk = std::chrono::steady_clock;
	const auto runtimeStart = clk::now();
	const auto ms = [](const clk::duration iDur) -> double {
		return std::chrono::duration<double, std::milli>{iDur}.count();
	};
	OWL_CORE_INFO("Scene::onStartRuntime: begin.")

	const auto tilesetStart = clk::now();
	resolveAllTilemapAssets();
	detachSharedTilemapAssets();
	OWL_CORE_INFO("Scene::onStartRuntime: tileset resolve {:.1f} ms.", ms(clk::now() - tilesetStart))

	const auto linksStart = clk::now();
	resolveAllEntityLinks();
	OWL_CORE_INFO("Scene::onStartRuntime: entity-link resolve {:.1f} ms.", ms(clk::now() - linksStart))

	const auto physicsStart = clk::now();
	physics::PhysicCommand::init(*this);
	OWL_CORE_INFO("Scene::onStartRuntime: physics init {:.1f} ms.", ms(clk::now() - physicsStart))

	const auto soundStart = clk::now();
	// Initialize sound listener from primary SoundListener entity
	for (const auto view = registry.view<component::Transform, component::SoundListener>(); const auto entity: view) {
		if (const auto& [transform, listener] = view.get<component::Transform, component::SoundListener>(entity);
			listener.primary) {
			const Entity ent{entity, this};
			const auto wt = getWorldTransform(ent);

			sound::SoundCommand::setListenerPosition(
					{wt.translation().x(), wt.translation().y(), wt.translation().z()});
			// Derive forward/up from rotation (Z-axis rotation for 2D)
			const float rotZ = wt.rotation().z();

			sound::SoundCommand::setListenerOrientation({std::sin(rotZ), std::cos(rotZ), 0.0f}, {0.0f, 0.0f, 1.0f});
			break;
		}
	}
	// Start sounds with playOnStart
	if (sound::SoundSystem::getState() == sound::SoundSystem::State::Running ||

		sound::SoundSystem::getState() == sound::SoundSystem::State::Error) {
		auto& soundLibrary = sound::SoundSystem::getSoundLibrary();
		for (const auto view = registry.view<component::Transform, component::SoundSource>(); const auto entity: view) {
			auto& [soundComp] = view.get<component::SoundSource>(entity);
			if (!soundComp.playOnStart || soundComp.soundAsset.empty())
				continue;
			// A source starts its sound even when no script played it before: load it on first use.
			const auto soundData = soundLibrary.exists(soundComp.soundAsset) ? soundLibrary.get(soundComp.soundAsset)
																			 : soundLibrary.load(soundComp.soundAsset);
			if (!soundData)
				continue;
			const Entity ent{entity, this};
			const auto wt = getWorldTransform(ent);
			const sound::PlayParams params{
					.volume = soundComp.volume,
					.pitch = soundComp.pitch,
					.loop = soundComp.loop,
					.spatial = soundComp.spatial,
					.position = {wt.translation().x(), wt.translation().y(), wt.translation().z()},
					.maxDistance = soundComp.maxDistance,
					.rolloff = soundComp.rolloff};
			soundComp.runtimeHandle = sound::SoundCommand::play(soundData, params);
		}
	}

	OWL_CORE_INFO("Scene::onStartRuntime: sound init {:.1f} ms.", ms(clk::now() - soundStart))

	const auto hudStart = clk::now();
	// Loaded before the scripts: its fonts would otherwise count against the time quota of the first one reaching it.
	if (const auto hud = getHud(); hud)
		OWL_CORE_INFO("Scene::onStartRuntime: hud load {:.1f} ms.", ms(clk::now() - hudStart))

	const auto luaStart = clk::now();
	size_t luaCount = 0;
	if (!m_sceneScript.scriptPath.empty()) {
		m_sceneScript.instance = loadScriptInstance(*this, m_sceneScript, 0, "scene");
		if (m_sceneScript.instance)
			m_sceneScript.instance->onCreate();
		++luaCount;
	}
	// Initialize Lua scripting
	for (const auto view = registry.view<component::LuaScript>(); const auto entity: view) {
		auto& luaScript = view.get<component::LuaScript>(entity);
		if (luaScript.scriptPath.empty())
			continue;
		const auto uuid = static_cast<uint64_t>(registry.get<component::ID>(entity).id);
		const auto* tag = registry.try_get<component::Tag>(entity);
		luaScript.instance = loadScriptInstance(*this, luaScript, uuid, tag != nullptr ? tag->tag : std::string{});
		if (luaScript.instance)
			luaScript.instance->onCreate();
		++luaCount;
	}
	OWL_CORE_INFO("Scene::onStartRuntime: lua compile + onCreate ({} scripts) {:.1f} ms.", luaCount,
				  ms(clk::now() - luaStart))

	// Reset animated sprites
	for (const auto view = registry.view<component::AnimatedSpriteRenderer>(); const auto entity: view) {
		auto& anim = view.get<component::AnimatedSpriteRenderer>(entity);
		anim.m_currentFrame = anim.firstFrame;
		anim.m_elapsedTime = 0.0f;
		anim.m_playing = true;
	}
	// Start timer triggers and reset overlap state.
	for (const auto view = registry.view<component::Trigger>(); const auto ent: view) {
		auto& trigger = view.get<component::Trigger>(ent).trigger;
		trigger.setOverlapping(false);
		if (trigger.type == SceneTrigger::TriggerType::Timer)
			trigger.startTimer();
	}
	// Drop cached voxel meshes from a previous run; renderWithStack() rebuilds them on the next rendered frame.
	if (mp_engineContext != nullptr)
		mp_engineContext->getVoxelMeshCache().clear();
	// Generation requested by the scene this one was copied from completes into that scene, not this one.
	for (const auto view = registry.view<component::VoxelWorld>(); const auto entity: view)
		view.get<component::VoxelWorld>(entity).pendingChunks.clear();
	connectRuntimeHooks(registry);
	OWL_CORE_INFO("Scene::onStartRuntime: total {:.1f} ms.", ms(clk::now() - runtimeStart))
}

void Scene::onEndRuntime() {
	OWL_PROFILE_FUNCTION()

	flushPendingDestructions();
	disconnectRuntimeHooks(registry);
	// Stop all active sounds (both component-based and Lua-created).
	sound::SoundCommand::stopAll();

	// Destroy Lua scripts
	if (m_sceneScript.instance && m_sceneScript.instance->isValid())
		m_sceneScript.instance->onDestroy();
	m_sceneScript.instance.reset();
	for (const auto view = registry.view<component::LuaScript>(); const auto entity: view) {
		auto& luaScript = view.get<component::LuaScript>(entity);
		if (luaScript.instance && luaScript.instance->isValid())
			luaScript.instance->onDestroy();
		luaScript.instance.reset();
	}

	physics::PhysicCommand::destroy(*this);
	status = Status::Editing;
}

void Scene::onUpdateRuntime(const core::Timestep& iTimeStep, const bool iRender) {
	OWL_PROFILE_FUNCTION()

	using clock = std::chrono::steady_clock;
	const bool timed = m_runtimeTimingsEnabled;
	const auto now = [timed]() -> clock::time_point { return timed ? clock::now() : clock::time_point{}; };
	const auto elapsedMs = [](const clock::time_point& iFrom, const clock::time_point& iTo) -> double {
		return std::chrono::duration<double, std::milli>(iTo - iFrom).count();
	};
	const auto tStart = now();
	m_lastRuntimeTimings = {};
	m_toastTimer = std::max(0.f, m_toastTimer - iTimeStep.getSeconds());
	m_realFrameSeconds = iTimeStep.getSeconds();
	updateScaledTimeStep(iTimeStep);
	const core::Timestep& sceneStep = m_scaledTimeStep;
	m_runtimeSeconds += static_cast<double>(sceneStep.getSeconds());
	m_inUpdatePass = false;
	m_worldTransformCacheActive = false;
	const auto frameInput = input::Input::readState();
	m_frameInput.advance(frameInput);
	m_inputSampler.observe(frameInput);
	m_frameActions.update(getActionMap(), m_frameInput);
	if (status == Status::Victory || status == Status::Death) {
		m_systems.run(SystemPhase::Ended, *this, {.timeStep = iTimeStep, .render = iRender});
		return;
	}
	const uint32_t ticks = m_simulationClock.advance(sceneStep);
	const SystemContext context{.timeStep = sceneStep,
								.render = iRender,
								.tickCount = ticks,
								.alpha = m_simulationClock.getAlpha()};
	const auto tScripts = now();
	flushPendingSpawns();
	m_systems.run(SystemPhase::Scripts, *this, context);
	flushPendingSpawns();
	const auto tScriptsEnd = now();
	m_systems.run(SystemPhase::PrePhysics, *this, context);
	const auto tPhysics = now();
	runSimulationTicks(context);
	m_systems.run(SystemPhase::Physics, *this, context);
	const auto tPhysicsEnd = now();
	m_systems.run(SystemPhase::PostPhysics, *this, context);
	// Every mutating phase (scripts, physics, links, trigger callbacks) is done: arm the per-pass caches.
	m_layerContentCacheFirst.clear();
	m_layerContentCacheNotFirst.clear();
	m_inUpdatePass = true;
	m_worldTransformCacheActive = true;
	prepareWorldTransforms();
	m_systems.run(SystemPhase::Late, *this, context);

	const auto tRender = now();
	if (iRender)
		renderRuntimeFrame(iTimeStep);
	// Disarm per-pass caches; the next tick repopulates from scratch.
	m_inUpdatePass = false;
	m_worldTransformCacheActive = false;
	if (timed) {
		const auto tEnd = clock::now();
		m_lastRuntimeTimings = {.scriptsMs = elapsedMs(tScripts, tScriptsEnd),
								.physicsMs = elapsedMs(tPhysics, tPhysicsEnd),
								.renderMs = elapsedMs(tRender, tEnd),
								.totalMs = elapsedMs(tStart, tEnd)};
	}
	flushPendingDestructions();
}

void Scene::updateScaledTimeStep(const core::Timestep& iTimeStep) {
	// Assigned into a member: the time step's buffers are reused instead of allocated every frame.
	m_scaledTimeStep = iTimeStep;
	if (const float scale = getEffectiveTimeScale(); scale < 1.f || scale > 1.f)
		m_scaledTimeStep.forceUpdate(std::chrono::duration_cast<core::Timestep::duration>(std::chrono::duration<double>(
				static_cast<double>(iTimeStep.getSeconds()) * static_cast<double>(scale))));
}

void Scene::setTimeScale(const float iScale) {
	if (!std::isfinite(iScale)) {
		OWL_CORE_WARN("Scene: Time scale {} ignored, not a finite number.", iScale)
		return;
	}
	m_timeScale = std::clamp(iScale, 0.f, maxTimeScale);
}

void Scene::startSpawnedSubtree(const Entity& iRoot) {
	if (iRoot)
		m_pendingSpawns.push_back(iRoot.getUUID());
}

void Scene::flushPendingSpawns() {
	if (m_pendingSpawns.empty() || status != Status::Playing)
		return;
	for (const auto uuid: std::exchange(m_pendingSpawns, {})) {
		const Entity root = findEntityByUUID(uuid);
		if (!root || isPendingDestructionInTree(root))
			continue;
		for (const auto& member: collectSubtree(*this, root)) startSpawnedEntity(member);
	}
}

void Scene::startSpawnedEntity(const Entity& iEntity) {
	if (iEntity.hasComponent<component::AnimatedSpriteRenderer>()) {
		auto& anim = iEntity.getComponent<component::AnimatedSpriteRenderer>();
		anim.m_currentFrame = anim.firstFrame;
		anim.m_elapsedTime = 0.0f;
		anim.m_playing = true;
	}
	if (iEntity.hasComponent<component::Trigger>()) {
		auto& trigger = iEntity.getComponent<component::Trigger>().trigger;
		trigger.setOverlapping(false);
		if (trigger.type == SceneTrigger::TriggerType::Timer)
			trigger.startTimer();
	}
	if (!iEntity.hasComponent<component::LuaScript>())
		return;
	auto& luaScript = iEntity.getComponent<component::LuaScript>();
	if (luaScript.scriptPath.empty() || luaScript.instance)
		return;
	luaScript.instance =
			loadScriptInstance(*this, luaScript, static_cast<uint64_t>(iEntity.getUUID()), iEntity.getName());
	if (luaScript.instance)
		luaScript.instance->onCreate();
}

void Scene::runSimulationTicks(const SystemContext& iFrame) {
	if (iFrame.tickCount == 0)
		return;
	// Assigned into a member: the time step's buffers are reused instead of allocated every frame.
	m_tickContext.timeStep = m_simulationClock.getStepTimestep();
	m_tickContext.render = iFrame.render;
	m_tickContext.tickCount = iFrame.tickCount;
	m_tickContext.alpha = iFrame.alpha;
	m_inTick = true;
	for (uint32_t index = 0; index < iFrame.tickCount; ++index) {
		m_tickContext.tickIndex = index;
		m_tickInput.advance(m_inputSampler.take());
		m_tickActions.update(getActionMap(), m_tickInput);
		for (const auto phase:
			 {SystemPhase::Scripts, SystemPhase::PrePhysics, SystemPhase::Physics, SystemPhase::PostPhysics})
			m_systems.runTick(phase, *this, m_tickContext);
	}
	m_inTick = false;
}

void Scene::renderRuntimeFrame(const core::Timestep& iTimeStep) {
	renderer::Camera* mainCamera = nullptr;
	math::Transform camTransform;
	for (const auto view = registry.view<component::Transform, component::Camera>(); const auto entity: view) {
		if (auto& camera = view.get<component::Camera>(entity); camera.primary) {
			mainCamera = &camera.camera;
			camTransform = getWorldTransform(Entity{entity, this});
			break;
		}
	}
	if (mainCamera == nullptr)
		return;
	updateVoxelStreaming(camTransform.translation());
	mainCamera->setTransform(camTransform());
	// Compute inverse(projection * viewRotation) for skybox (includes FOV/aspect ratio)
	math::mat4 viewRotation = mainCamera->getView();

	viewRotation(0, 3) = 0.0f;

	viewRotation(1, 3) = 0.0f;

	viewRotation(2, 3) = 0.0f;
	m_inverseViewRotation = inverse(mainCamera->getProjection() * viewRotation);

	renderTargets();
	renderWithStack(*mainCamera);

	if (mp_engineContext == nullptr)
		return;
	auto& transition = mp_engineContext->getScreenTransition();
	transition.update(iTimeStep.getSeconds());
	transition.render(static_cast<float>(m_viewportSize.x()), static_cast<float>(m_viewportSize.y()));
}

void Scene::onRenderRuntime() {
	OWL_PROFILE_FUNCTION()

	m_worldTransformCacheActive = true;
	prepareWorldTransforms();

	// find camera
	renderer::Camera* mainCamera = nullptr;
	math::mat4 cameraTransform;
	math::Transform camTransform;
	for (const auto view = registry.view<component::Transform, component::Camera>(); const auto entity: view) {
		auto [transform, camera] = view.get<component::Transform, component::Camera>(entity);
		if (camera.primary) {
			mainCamera = &camera.camera;
			const Entity camEntity{entity, this};
			camTransform = getWorldTransform(camEntity);
			cameraTransform = camTransform();
			break;
		}
	}

	if (status == Status::Victory) {
		if (mainCamera != nullptr) {
			const auto font = app::Application::get().getFontLibrary().getDefaultFont();
			math::Transform textTransform = camTransform;
			textTransform.translation().z() = 0;
			textTransform.scale().x() = 3.f;
			mainCamera->setTransform(cameraTransform);
			renderer::Renderer2D::resetStats();
			renderer::Renderer2D::beginScene(*mainCamera);
			renderer::Renderer2D::drawString({.transform = textTransform,
											  .text = "Victory!",
											  .font = font,
											  .color = {1, 1, 1, 1},
											  .entityId = 0});
			renderer::Renderer2D::endScene();
		}
		return;
	}
	if (status == Status::Death) {
		if (mainCamera != nullptr) {
			const auto font = app::Application::get().getFontLibrary().getDefaultFont();
			math::Transform textTransform = camTransform;
			textTransform.translation().z() = 0;
			textTransform.scale().x() = 3.f;
			mainCamera->setTransform(cameraTransform);
			renderer::Renderer2D::resetStats();
			renderer::Renderer2D::beginScene(*mainCamera);
			renderer::Renderer2D::drawString({.transform = textTransform,
											  .text = "You loose!",
											  .font = font,
											  .color = {1, 1, 1, 1},
											  .entityId = 0});
			renderer::Renderer2D::endScene();
		}
		return;
	}

	// Render 2D
	if (mainCamera != nullptr) {
		mainCamera->setTransform(cameraTransform);
		// Compute inverse(projection * viewRotation) for skybox (includes FOV/aspect ratio)
		math::mat4 viewRotation = mainCamera->getView();
		viewRotation(0, 3) = 0.0f;
		viewRotation(1, 3) = 0.0f;
		viewRotation(2, 3) = 0.0f;
		m_inverseViewRotation = inverse(mainCamera->getProjection() * viewRotation);
		renderTargets();
		renderWithStack(*mainCamera);
		if (mp_engineContext != nullptr)
			mp_engineContext->getScreenTransition().render(static_cast<float>(m_viewportSize.x()),
														   static_cast<float>(m_viewportSize.y()));
	}
	m_worldTransformCacheActive = false;
}

void Scene::onUpdateEditor(const core::Timestep& iTimeStep, const renderer::Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	m_toastTimer = std::max(0.f, m_toastTimer - iTimeStep.getSeconds());
	// Stream procedural voxel terrain around the editor camera (its world position is the inverse-view translation).
	const math::mat4 camToWorld = inverse(iCamera.getView());
	updateVoxelStreaming(math::vec3{camToWorld(0, 3), camToWorld(1, 3), camToWorld(2, 3)});
	systems::applyDayNightCycles(*this);
	renderTargets();
	renderEditorView(iCamera);
}

void Scene::renderEditorView(const renderer::Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	m_layerContentCacheFirst.clear();
	m_layerContentCacheNotFirst.clear();
	m_inUpdatePass = true;
	m_worldTransformCacheActive = true;
	prepareWorldTransforms();

	// Compute inverse(projection * viewRotation) for skybox (includes FOV/aspect ratio)
	math::mat4 viewRotation = iCamera.getView();
	viewRotation(0, 3) = 0.0f;
	viewRotation(1, 3) = 0.0f;
	viewRotation(2, 3) = 0.0f;
	m_inverseViewRotation = inverse(iCamera.getProjection() * viewRotation);
	renderWithStack(iCamera);
	// Disarm per-pass caches; the next tick repopulates from scratch.
	m_inUpdatePass = false;
	m_worldTransformCacheActive = false;
}

auto Scene::hasDepthTested2DLayer() -> bool {
	return std::ranges::any_of(renderer::Renderer::getRenderStack().getLayers(), [](const auto& iLayer) -> bool {
		const auto* const layer2D = dynamic_cast<const renderer::Renderer2DLayer*>(iLayer.get());
		return layer2D != nullptr && layer2D->getSpace() == renderer::Renderer2DLayer::Space::World &&
			   (layer2D->isDepthTested() || layer2D->getBillboard() != renderer::Renderer2DLayer::Billboard::None);
	});
}

auto Scene::layerAccepts(const Entity& iEntity) const -> bool {
	if (m_currentLayerName.empty())
		return true;// single pass without a stack — every entity is drawn.
	if (!iEntity.hasComponent<component::RendererTag>())
		return m_currentLayerIsFirst;// untagged entities default to the first layer.
	return iEntity.getComponent<component::RendererTag>().rendererName == m_currentLayerName;
}

auto Scene::getSelfDrawnLayer() const -> shared<renderer::RenderLayer> {
	const auto& layers = renderer::Renderer::getRenderStack().getLayers();
	for (size_t index = 0; index < layers.size(); ++index) {
		if (layers[index]->drawsOwnEntities() && layerHasContent(layers[index]->getName(), index == 0))
			return layers[index];
	}
	return nullptr;
}

auto Scene::layerHasContent(const std::string& iLayerName, const bool iIsFirst) const -> bool {
	OWL_PROFILE_FUNCTION()

	auto& cache = iIsFirst ? m_layerContentCacheFirst : m_layerContentCacheNotFirst;
	if (m_inUpdatePass) {
		if (const auto it = cache.find(iLayerName); it != cache.end())
			return it->second;
	}

	auto* self = const_cast<Scene*>(this);
	const auto matches = [&](const entt::entity e) -> bool {
		const Entity ent{e, self};
		if (!isEffectivelyVisible(ent, /*iEditorMode=*/false))
			return false;
		if (!ent.hasComponent<component::RendererTag>())
			return iIsFirst;
		return ent.getComponent<component::RendererTag>().rendererName == iLayerName;
	};
	const auto scan = [&]() -> bool {
		for (const auto e: registry.view<component::Tilemap>())
			if (matches(e))
				return true;
		for (const auto e: registry.view<component::SpriteRenderer>())
			if (matches(e))
				return true;
		for (const auto e: registry.view<component::AnimatedSpriteRenderer>())
			if (matches(e))
				return true;
		for (const auto e: registry.view<component::CircleRenderer>())
			if (matches(e))
				return true;
		for (const auto e: registry.view<component::Text>())
			if (matches(e))
				return true;
		for (const auto e: registry.view<component::VoxelWorld>())
			if (matches(e))
				return true;
		for (const auto e: registry.view<component::MeshRenderer>())
			if (matches(e))
				return true;
		// Backgrounds always sit behind every layer — they only contribute to the first one.
		if (iIsFirst) {
			for (const auto e: registry.view<component::BackgroundTexture>())
				if (matches(e))
					return true;
		}
		return false;
	};
	const bool result = scan();
	if (m_inUpdatePass)
		cache.emplace(iLayerName, result);
	return result;
}

void Scene::renderWithStack(const renderer::Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	prepareVoxelRenderData();
	prepareTilemapRenderData();
	prepareMeshRenderData();
	updateSpatialIndex();
	renderer::RendererMesh::clearOcclusion();
	mp_renderCamera = &iCamera;
	using Graph = renderer::RenderGraph;
	auto& graph = renderer::Renderer::getRenderGraph();
	graph.reset();
	const Graph::Resource target = graph.importTarget("target");
	const Graph::Resource sceneColor = graph.createTarget("scene colour", Graph::TargetFormat::Linear);
	// The first write of the scene colour clears it, even when no layer has anything to draw.
	graph.addPass(
			"scene clear", [sceneColor](Graph::PassBuilder& ioBuilder) -> void { ioBuilder.write(sceneColor); },
			[](const Graph::PassContext&) -> void {});
	recordLightPasses(graph);
	recordOcclusionPasses(graph);
	recordEnvironmentPass(graph, sceneColor);
	recordWorldPasses(graph, sceneColor);
	const auto* const bound = renderer::gpu::Framebuffer::getBound();
	const math::vec2ui frameSize = bound != nullptr ? bound->getSpecification().size : m_viewportSize;
	const auto& post = iCamera.getPostProcess();
	m_postTargets.sceneColor = sceneColor;
	m_postTargets.exposure = renderer::PostProcess::recordExposure(graph, sceneColor, post, frameSize);
	m_postTargets.bloom =
			renderer::PostProcess::recordBloom(graph, sceneColor, post, frameSize, m_postTargets.exposure);
	const auto encodeSetup = [sceneColor, target](Graph::PassBuilder& ioBuilder) -> void {
		ioBuilder.read(sceneColor);
		ioBuilder.write(target);
	};
	graph.addPass(
			"colour encode",
			[targets = m_postTargets, target](Graph::PassBuilder& ioBuilder) -> void {
				ioBuilder.read(targets.sceneColor);
				if (targets.bloom.isValid())
					ioBuilder.read(targets.bloom);
				if (targets.exposure.isValid())
					ioBuilder.read(targets.exposure);
				ioBuilder.write(target);
			},
			[this](const Graph::PassContext& iContext) -> void {
				const auto& targets = m_postTargets;
				const auto& settings = mp_renderCamera->getPostProcess();
				const auto lut = renderer::PostProcess::getLut(settings.lut);
				renderer::ColorEncoder::draw(
						iContext.getTexture(targets.sceneColor), mp_renderCamera->getToneMapping(),
						{.settings = &settings,
						 .bloom = targets.bloom.isValid() ? iContext.getTexture(targets.bloom) : nullptr,
						 .lut = lut.texture,
						 .lutSize = lut.size,
						 .frameSize = iContext.getSize(),
						 .exposure = targets.exposure.isValid() ? iContext.getTexture(targets.exposure) : nullptr});
			});
	if (!m_drawingRenderTarget)
		recordOutlinePasses(graph, target);
	// The display target takes the entity ids and the depth of the world, for picking and the depth-tested overlays.
	graph.addPass("entity ids and depth", encodeSetup,
				  [sceneColor, target](const Graph::PassContext& iContext) -> void {
					  auto* const source = iContext.getFramebuffer(sceneColor);
					  auto* const destination = iContext.getFramebuffer(target);
					  if (source == nullptr || destination == nullptr)
						  return;
					  source->copyAttachment(1, *destination);
					  source->copyAttachment(2, *destination);
				  });
	graph.addPass(
			"overlays and HUD", [target](Graph::PassBuilder& ioBuilder) -> void { ioBuilder.write(target); },
			[this](const Graph::PassContext&) -> void {
				if (m_drawingRenderTarget)
					return;
				renderOverlays(*mp_renderCamera);
				ui::HudPass::render(*this, m_viewportSize, status == Status::Editing);
				renderToast();
			});
	if (graph.compile()) {
		graph.execute(frameSize);
	}
	// Renderer3D is shared: a mesh preview drawn later in the frame must not see this scene's lights.
	renderer::Renderer3D::setSceneLights({});
	renderer::Renderer3D::setLayerSun(true);
	renderer::Renderer3D::setEditorAmbient({});
	renderer::Renderer3D::setExposure(renderer::Renderer3D::exposureOf(renderer::Renderer3D::g_defaultEv100));
	renderer::Renderer3D::setShadow({});
	renderer::Renderer3D::setEnvironment({});
	mp_renderCamera = nullptr;
}

void Scene::renderTargets() {
	OWL_PROFILE_FUNCTION()

	auto* const previous = renderer::gpu::Framebuffer::getBound();
	const math::mat4 mainInverseViewRotation = m_inverseViewRotation;
	bool drawn = false;
	for (const auto view = registry.view<component::RenderTarget, component::Camera>(); const auto handle: view) {
		auto& target = view.get<component::RenderTarget>(handle);
		if (!target.enabled || target.targetName.empty())
			continue;
		if (!target.m_framebuffer) {
			using Attachment = renderer::gpu::AttachmentSpecification;
			target.m_framebuffer = renderer::gpu::Framebuffer::create(
					{.size = target.size,
					 .attachments = {{.format = Attachment::Format::Surface, .tiling = Attachment::Tiling::Optimal},
									 {.format = Attachment::Format::RedInteger, .tiling = Attachment::Tiling::Optimal},
									 {.format = Attachment::Format::Depth32F, .tiling = Attachment::Tiling::Optimal}},
					 .samples = 1,
					 .swapChainTarget = false,
					 .debugName = "render_target"});
		} else if (target.m_framebuffer->getSpecification().size != target.size) {
			target.m_framebuffer->resize(target.size);
		}
		auto& camera = view.get<component::Camera>(handle).camera;
		camera.setViewportSize(target.size);
		camera.setTransform(getWorldTransform(Entity{handle, this})());
		target.m_framebuffer->bind();
		renderer::gpu::RenderCommand::setClearColor(target.clearColor);
		renderer::gpu::RenderCommand::clear();
		target.m_framebuffer->clearAttachment(1, -1);
		math::mat4 viewRotation = camera.getView();
		viewRotation(0, 3) = 0.0f;
		viewRotation(1, 3) = 0.0f;
		viewRotation(2, 3) = 0.0f;
		m_inverseViewRotation = inverse(camera.getProjection() * viewRotation);
		m_drawingRenderTarget = true;
		renderWithStack(camera);
		m_drawingRenderTarget = false;
		target.m_framebuffer->unbind();
		drawn = true;
	}
	m_inverseViewRotation = mainInverseViewRotation;
	if (drawn && previous != nullptr)
		previous->bind();
}

auto Scene::getRenderTarget(const std::string& iName) const -> shared<renderer::gpu::Framebuffer> {
	for (const auto view = registry.view<const component::RenderTarget>(); const auto handle: view) {
		if (const auto& target = view.get<const component::RenderTarget>(handle); target.targetName == iName)
			return target.m_framebuffer;
	}
	return nullptr;
}

void Scene::recordLightPasses(renderer::RenderGraph& ioGraph) {
	using Graph = renderer::RenderGraph;
	const bool editorMode = status == Status::Editing;
	m_gpuLights.clear();
	m_shadowSettings = {};
	renderer::Renderer3D::setExposure(renderer::Renderer3D::exposureOf(mp_renderCamera->getPostProcess().ev100));
	m_shadowCascades.count = 0;
	const component::Light* shadowLight = nullptr;
	math::vec3 shadowDirection{0.f, -1.f, 0.f};
	for (const auto [handle, light]: registry.view<component::Light>().each()) {
		if (m_gpuLights.size() >= renderer::Renderer3D::g_maxLights)
			break;
		const Entity entity{handle, this};
		if (!light.enabled || !isEffectivelyVisible(entity, editorMode))
			continue;
		const math::Transform world = getWorldTransform(entity);
		const math::vec4 forward = world() * math::vec4{0.f, 0.f, -1.f, 0.f};
		const math::vec3 direction = math::vec3{forward.x(), forward.y(), forward.z()}.normalized();
		const math::vec3& position = world.translation();
		// Lux for a directional light; the luminous intensity (cd) of the lumens of a point or spot light.
		float strength = std::max(light.intensity, 0.f);
		if (light.type == component::Light::Type::Point)
			strength /= 4.f * std::numbers::pi_v<float>;
		else if (light.type == component::Light::Type::Spot)
			strength /= std::numbers::pi_v<float>;
		const math::vec3 color = math::vec3{renderer::ColorEncoder::srgbToLinear(light.color.x()),
											renderer::ColorEncoder::srgbToLinear(light.color.y()),
											renderer::ColorEncoder::srgbToLinear(light.color.z())} *
								 strength;
		m_gpuLights.push_back(
				{.positionRange = {position.x(), position.y(), position.z(), std::max(light.range, 0.01f)},
				 .directionType = {direction.x(), direction.y(), direction.z(), static_cast<float>(light.type)},
				 .color = {color.x(), color.y(), color.z(), 0.f},
				 .spotCosines = {std::cos(light.innerAngle), std::cos(std::max(light.outerAngle, light.innerAngle)),
								 0.f, 0.f}});
		if (shadowLight == nullptr && light.type == component::Light::Type::Directional && light.castShadows) {
			shadowLight = &light;
			shadowDirection = direction;
			m_shadowSettings.lightIndex = static_cast<int32_t>(m_gpuLights.size() - 1);
		}
	}
	renderer::Renderer3D::setSceneLights(m_gpuLights);
	// The layer sun stands in for a scene that has no light at all, never for lights switched off.
	renderer::Renderer3D::setLayerSun(registry.view<component::Light>().empty());
	renderer::Renderer3D::setEditorAmbient(editorMode ? m_editorAmbient : math::vec3{0.f, 0.f, 0.f});
	if (shadowLight == nullptr)
		return;
	m_shadowCascades = renderer::computeShadowCascades(*mp_renderCamera, {.lightDirection = shadowDirection,
																		  .shadowDistance = shadowLight->shadowDistance,
																		  .cascadeCount = shadowLight->cascadeCount,
																		  .resolution = shadowLight->shadowResolution});
	m_shadowSettings.cascadeCount = m_shadowCascades.count;
	m_shadowSettings.bias = shadowLight->shadowBias;
	m_shadowSettings.resolution = shadowLight->shadowResolution;
	m_shadowSettings.viewDepthRow = mp_renderCamera->getView().row(2);
	const math::vec2ui mapSize{shadowLight->shadowResolution, shadowLight->shadowResolution};
	for (uint32_t index = 0; index < m_shadowCascades.count; ++index) {
		const auto& cascade = m_shadowCascades.cascades[index];
		const auto& lightView = cascade.camera.getView();
		const math::vec4 lightEye = inverse(lightView) * math::vec4{0.f, 0.f, 0.f, 1.f};
		m_shadowSettings.relativeViewProjections[index] =
				cascade.camera.getProjection() * renderer::cameraRelativeView(lightView);
		m_shadowSettings.origins[index] = math::vec3{lightEye.x(), lightEye.y(), lightEye.z()};
		m_shadowSettings.splitDepths[index] = cascade.splitDepth;
		m_shadowSettings.texelWorldSizes[index] = cascade.texelWorldSize;
		m_shadowTargets[index] = ioGraph.createTarget("shadow map", Graph::TargetFormat::Depth, mapSize);
		ioGraph.addPass(
				"shadow cascade",
				[target = m_shadowTargets[index]](Graph::PassBuilder& ioBuilder) -> void { ioBuilder.write(target); },
				[this, index](const Graph::PassContext&) -> void {
					renderShadowCasters(m_shadowCascades.cascades[index].camera);
				});
	}
}

void Scene::recordOcclusionPasses(renderer::RenderGraph& ioGraph) {
	using Graph = renderer::RenderGraph;
	m_occlusionToken = {};
	m_occluderDepth = {};
	const auto& layers = renderer::Renderer::getRenderStack().getLayers();
	float scale = 0.f;
	for (size_t index = 0; index < layers.size(); ++index) {
		const auto* meshLayer = dynamic_cast<const renderer::RendererMeshLayer*>(layers[index].get());
		if (meshLayer != nullptr && meshLayer->getConfig().occlusionCulling &&
			layerHasContent(meshLayer->getName(), index == 0))
			scale = std::max(scale, meshLayer->getConfig().occlusionScale);
	}
	if (scale <= 0.f)
		return;
	const auto* const bound = renderer::gpu::Framebuffer::getBound();
	const math::vec2ui frameSize = bound != nullptr ? bound->getSpecification().size : m_viewportSize;
	const math::vec2ui size{std::max(1u, static_cast<uint32_t>(static_cast<float>(frameSize.x()) * scale)),
							std::max(1u, static_cast<uint32_t>(static_cast<float>(frameSize.y()) * scale))};
	const Graph::Resource depth = ioGraph.createTarget("occluder depth", Graph::TargetFormat::Depth, size);
	m_occluderDepth = depth;
	ioGraph.addPass(
			"occluder depth", [depth](Graph::PassBuilder& ioBuilder) -> void { ioBuilder.write(depth); },
			[this](const Graph::PassContext&) -> void { renderShadowCasters(*mp_renderCamera); });
	// The pyramid lives outside the graph: the 3D passes read this token target so the graph keeps and orders it.
	m_occlusionToken = ioGraph.createTarget("occlusion pyramid", Graph::TargetFormat::Depth, {1, 1});
	ioGraph.addPass(
			"occlusion pyramid",
			[depth, token = m_occlusionToken](Graph::PassBuilder& ioBuilder) -> void {
				ioBuilder.read(depth);
				ioBuilder.write(token);
			},
			[this, depth, size](const Graph::PassContext& iContext) -> void {
				renderer::RendererMesh::buildOcclusion(iContext.getTexture(depth), size,
													   mp_renderCamera->getViewProjection());
			});
}

void Scene::recordEnvironmentPass(renderer::RenderGraph& ioGraph, const renderer::RenderGraph::Resource iSceneColor) {
	using Graph = renderer::RenderGraph;
	const bool editorMode = status == Status::Editing;
	const component::Environment* environment = nullptr;
	const component::ProceduralSky* sky = nullptr;
	for (const auto [handle, candidate]: registry.view<component::Environment>().each()) {
		if (isEffectivelyVisible(Entity{handle, this}, editorMode)) {
			environment = &candidate;
			if (const auto* procedural = registry.try_get<component::ProceduralSky>(handle);
				procedural != nullptr && procedural->enabled)
				sky = procedural;
			break;
		}
	}
	if (environment == nullptr) {
		renderer::Renderer3D::setEnvironment({});
		return;
	}
	const auto& color = environment->ambient;
	renderer::EnvironmentLighting constantSky;
	constantSky.intensity = std::max(environment->intensity, 0.f);
	constantSky.ambient =
			math::vec3{renderer::ColorEncoder::srgbToLinear(color.x()), renderer::ColorEncoder::srgbToLinear(color.y()),
					   renderer::ColorEncoder::srgbToLinear(color.z())};
	auto& state = m_environment;
	if (sky != nullptr) {
		requestProceduralSky(*sky);
	} else if (environment->hdri.empty()) {
		m_environment = {};
		renderer::Renderer3D::setEnvironment(constantSky);
		return;
	} else if (state.procedural || state.hdri != environment->hdri) {
		state = {};
		state.hdri = environment->hdri;
		state.result = mkShared<std::optional<renderer::EnvironmentMap>>();
		const auto build = [result = state.result, path = state.hdri]() -> void {
			if (const auto image = renderer::loadHdrAsset(path); image)
				*result = renderer::EnvironmentMap::build(*image);
		};
		if (app::Application::instanced()) {
			state.task = app::Application::get().getTaskScheduler().pushTask(core::task::Task{build});
			state.loading = true;
		} else {
			build();
		}
	}
	if (state.loading && app::Application::instanced() &&
		app::Application::get().getTaskScheduler().isTaskFinished(state.task))
		state.loading = false;
	if (!state.loading && state.result && state.result->has_value()) {
		const auto& map = **state.result;
		state.lighting.levelCount = static_cast<uint32_t>(std::min(map.levels.size(), state.lighting.levels.size()));
		for (uint32_t level = 0; level < state.lighting.levelCount; ++level) {
			const auto& image = map.levels[level];
			std::vector<float> pixels;
			pixels.reserve(image.pixels.size() * 4);
			for (const auto& pixel: image.pixels) pixels.insert(pixels.end(), {pixel.x(), pixel.y(), pixel.z(), 1.f});
			auto texture = renderer::gpu::Texture2D::create(
					renderer::gpu::Texture2D::Specification{.size = image.size,
															.format = renderer::gpu::ImageFormat::Rgba32F});
			texture->setData(pixels.data(), static_cast<uint32_t>(pixels.size() * sizeof(float)));
			state.lighting.levels[level] = std::move(texture);
		}
		state.lighting.irradiance = map.irradiance;
		state.result.reset();
	}
	if (state.lighting.levelCount == 0) {
		renderer::Renderer3D::setEnvironment(constantSky);
		return;
	}
	state.lighting.intensity = std::max(environment->intensity, 0.f);
	state.lighting.rotation = environment->rotation;
	state.lighting.bodies = sky != nullptr ? renderer::skyBodiesOf({.sunDirection = sky->sunDirection,
																	.nightBrightness = sky->nightBrightness,
																	.stars = sky->stars,
																	.discSize = sky->discSize})
										   : renderer::SkyBodies{};
	renderer::Renderer3D::setEnvironment(state.lighting);
	if (!environment->showSky)
		return;
	ioGraph.addPass(
			"environment sky", [iSceneColor](Graph::PassBuilder& ioBuilder) -> void { ioBuilder.write(iSceneColor); },
			[this](const Graph::PassContext&) -> void {
				renderer::Renderer3D::beginScene(*mp_renderCamera);
				renderer::Renderer3D::drawSky();
				renderer::Renderer3D::endScene();
			});
}

void Scene::requestProceduralSky(const component::ProceduralSky& iSky) {
	auto& state = m_environment;
	if (!state.procedural) {
		state = {};
		state.procedural = true;
	}
	if (state.loading)
		return;
	const auto quantise = [](const float iValue, const float iStep) -> float { return std::round(iValue / iStep); };
	constexpr float directionStep = 0.008f;
	constexpr float valueStep = 0.002f;
	const std::array<float, 16> key{quantise(iSky.sunDirection.x(), directionStep),
									quantise(iSky.sunDirection.y(), directionStep),
									quantise(iSky.sunDirection.z(), directionStep),
									quantise(iSky.turbidity, valueStep),
									quantise(iSky.groundColor.x(), valueStep),
									quantise(iSky.groundColor.y(), valueStep),
									quantise(iSky.groundColor.z(), valueStep),
									quantise(iSky.nightColor.x(), valueStep),
									quantise(iSky.nightColor.y(), valueStep),
									quantise(iSky.nightColor.z(), valueStep),
									quantise(iSky.nightBrightness, valueStep * 0.01f),
									quantise(iSky.stars, valueStep),
									quantise(iSky.discSize, valueStep),
									static_cast<float>(iSky.resolution),
									1.f,
									0.f};
	if (state.levelsReady() && key == state.skyKey)
		return;
	state.skyKey = key;
	const auto linear = [](const math::vec3& iColor) -> math::vec3 {
		return {renderer::ColorEncoder::srgbToLinear(iColor.x()), renderer::ColorEncoder::srgbToLinear(iColor.y()),
				renderer::ColorEncoder::srgbToLinear(iColor.z())};
	};
	const renderer::SkyParameters parameters{.sunDirection = iSky.sunDirection,
											 .turbidity = iSky.turbidity,
											 .groundColor = linear(iSky.groundColor),
											 .nightColor = linear(iSky.nightColor),
											 .nightBrightness = iSky.nightBrightness,
											 .stars = iSky.stars,
											 .discSize = iSky.discSize};
	const math::vec2ui size{iSky.resolution, std::max(iSky.resolution / 2, 1u)};
	state.result = mkShared<std::optional<renderer::EnvironmentMap>>();
	const auto build = [result = state.result, parameters, size]() -> void {
		*result = renderer::EnvironmentMap::build(renderer::renderProceduralSky(parameters, size));
	};
	if (app::Application::instanced()) {
		state.task = app::Application::get().getTaskScheduler().pushTask(core::task::Task{build});
		state.loading = true;
	} else {
		build();
	}
}

void Scene::useShadowMaps(const renderer::RenderGraph::PassContext& iContext) {
	if (m_shadowSettings.cascadeCount == 0)
		return;
	for (uint32_t index = 0; index < m_shadowSettings.cascadeCount; ++index)
		m_shadowSettings.maps[index] = iContext.getTexture(m_shadowTargets[index]);
	renderer::Renderer3D::setShadow(m_shadowSettings);
}

auto Scene::isInSubtree(entt::entity iHandle, const entt::entity iRoot) const -> bool {
	for (int depth = 0; depth < 64 && iHandle != entt::null; ++depth) {
		if (iHandle == iRoot)
			return true;
		const auto* hierarchy = registry.try_get<component::Hierarchy>(iHandle);
		if (hierarchy == nullptr || hierarchy->parentId == core::UUID{0})
			return false;
		iHandle = static_cast<entt::entity>(findEntityByUUID(hierarchy->parentId));
	}
	return false;
}

void Scene::recordOutlinePasses(renderer::RenderGraph& ioGraph, const renderer::RenderGraph::Resource iTarget) {
	using Graph = renderer::RenderGraph;
	if (!m_outline)
		return;
	const Entity root = findEntityByUUID(m_outline->root);
	if (!root)
		return;
	const auto rootHandle = static_cast<entt::entity>(root);
	const auto meshes = registry.view<component::MeshRenderer>();
	if (std::ranges::none_of(meshes, [&](const auto iHandle) -> bool { return isInSubtree(iHandle, rootHandle); }))
		return;
	const Graph::Resource mask = ioGraph.createTarget("outline mask", Graph::TargetFormat::Depth);
	// The occluder depth of the frame, when the occlusion culling made one, spares a depth pass of the whole scene.
	const bool reuse = m_occluderDepth.isValid();
	const Graph::Resource depth =
			reuse ? m_occluderDepth : ioGraph.createTarget("outline scene depth", Graph::TargetFormat::Depth);
	ioGraph.addPass(
			"outline mask", [mask](Graph::PassBuilder& ioBuilder) -> void { ioBuilder.write(mask); },
			[this, rootHandle](const Graph::PassContext&) -> void {
				m_meshFilterRoot = rootHandle;
				renderShadowCasters(*mp_renderCamera, true);
				m_meshFilterRoot = entt::null;
			});
	if (!reuse)
		ioGraph.addPass(
				"outline scene depth", [depth](Graph::PassBuilder& ioBuilder) -> void { ioBuilder.write(depth); },
				[this](const Graph::PassContext&) -> void { renderShadowCasters(*mp_renderCamera); });
	ioGraph.addPass(
			"outline",
			[mask, depth, iTarget](Graph::PassBuilder& ioBuilder) -> void {
				ioBuilder.read(mask);
				ioBuilder.read(depth);
				ioBuilder.write(iTarget);
			},
			[this, mask, depth](const Graph::PassContext& iContext) -> void {
				renderer::OutlinePass::draw({.mask = iContext.getTexture(mask),
											 .sceneDepth = iContext.getTexture(depth),
											 .color = m_outline ? m_outline->color : math::vec4{1.f, 0.8f, 0.35f, 1.f},
											 .frameSize = iContext.getSize()});
			});
}

void Scene::renderShadowCasters(const renderer::Camera& iCamera, const bool iMeshesOnly) {
	const bool editorMode = status == Status::Editing;
	renderer::Renderer3D::setShadowPass(true);
	const auto& layers = renderer::Renderer::getRenderStack().getLayers();
	for (size_t index = 0; index < layers.size(); ++index) {
		const auto& layer = layers[index];
		const std::string_view type{layer->getTypeKey()};
		if ((type != "RendererVoxel" && type != "RendererMesh") || (iMeshesOnly && type != "RendererMesh") ||
			!layerHasContent(layer->getName(), index == 0))
			continue;
		layer->onBeginFrame(iCamera);
		m_currentLayerName = layer->getName();
		m_currentLayerIsFirst = index == 0;
		mp_currentLayer = layer.get();
		if (type == "RendererVoxel")
			renderVoxelWorlds(editorMode);
		else
			renderMeshes(editorMode);
		layer->onEndFrame();
	}
	m_currentLayerName.clear();
	m_currentLayerIsFirst = true;
	mp_currentLayer = nullptr;
	renderer::Renderer3D::setShadowPass(false);
}

void Scene::recordWorldPasses(renderer::RenderGraph& ioGraph, const renderer::RenderGraph::Resource iSceneColor) {
	using Graph = renderer::RenderGraph;
	const auto writeScene = [iSceneColor](Graph::PassBuilder& ioBuilder) -> void { ioBuilder.write(iSceneColor); };
	const auto writeSceneReadShadows = [this, iSceneColor](Graph::PassBuilder& ioBuilder) -> void {
		ioBuilder.write(iSceneColor);
		for (uint32_t index = 0; index < m_shadowSettings.cascadeCount; ++index) ioBuilder.read(m_shadowTargets[index]);
		if (m_occlusionToken.isValid())
			ioBuilder.read(m_occlusionToken);
	};
	const auto& layers = renderer::Renderer::getRenderStack().getLayers();
	const bool editorMode = status == Status::Editing;
	if (layers.empty() || (editorMode && getSelfDrawnLayer() == nullptr && !hasDepthTested2DLayer())) {
		// The editor composite is one world-space 2D pass drawing every entity.
		ioGraph.addPass("editor composite", writeScene, [this](const Graph::PassContext&) -> void {
			renderer::Renderer2DLayer composite{"editor"};
			m_currentLayerName.clear();
			m_currentLayerIsFirst = true;
			mp_currentLayer = nullptr;
			composite.onBeginFrame(*mp_renderCamera);
			composite.onRender(*this);
			render();
			composite.onEndFrame();
		});
		if (!editorMode)
			return;
		// The editor composite above is 2D-only: the 3D voxel and mesh layers draw as passes of their own.
		for (size_t index = 0; index < layers.size(); ++index) {
			const auto& layer = layers[index];
			const std::string_view type{layer->getTypeKey()};
			if ((type != "RendererVoxel" && type != "RendererMesh") || !layerHasContent(layer->getName(), index == 0))
				continue;
			ioGraph.addPass(layer->getName(), writeSceneReadShadows,
							[this, index](const Graph::PassContext& iContext) -> void {
								useShadowMaps(iContext);
								const auto& stackLayer = renderer::Renderer::getRenderStack().getLayers()[index];
								stackLayer->setViewport(m_viewportSize);
								stackLayer->onBeginFrame(*mp_renderCamera);
								m_currentLayerName = stackLayer->getName();
								m_currentLayerIsFirst = index == 0;
								mp_currentLayer = stackLayer.get();
								if (std::string_view{stackLayer->getTypeKey()} == "RendererVoxel")
									renderVoxelWorlds(true);
								else
									renderMeshes(true);
								stackLayer->onEndFrame();
								m_currentLayerName.clear();
								m_currentLayerIsFirst = true;
								mp_currentLayer = nullptr;
							});
		}
		return;
	}
	renderer::Renderer2D::resetStats();
	for (size_t index = 0; index < layers.size(); ++index) {
		const auto& layer = layers[index];
		// The raycaster casts from the scene camera: the editor never draws it.
		if (editorMode && std::string_view{layer->getTypeKey()} == "RendererRaycast")
			continue;
		if (!layerHasContent(layer->getName(), index == 0))
			continue;
		const std::string_view type{layer->getTypeKey()};
		const bool threeD = type == "RendererVoxel" || type == "RendererMesh";
		// Two pointer-sized captures at most: the body stays in the small buffer of `std::function`, no allocation.
		const auto body = [this, index](const Graph::PassContext& iContext) -> void {
			const auto& stackLayer = renderer::Renderer::getRenderStack().getLayers()[index];
			if (const std::string_view layerType{stackLayer->getTypeKey()};
				layerType == "RendererVoxel" || layerType == "RendererMesh")
				useShadowMaps(iContext);
			stackLayer->setViewport(m_viewportSize);
			stackLayer->onBeginFrame(*mp_renderCamera);
			m_currentLayerName = stackLayer->getName();
			m_currentLayerIsFirst = index == 0;
			mp_currentLayer = stackLayer.get();
			stackLayer->onRender(*this);
			if (!stackLayer->drawsOwnEntities())
				render();
			stackLayer->onEndFrame();
			m_currentLayerName.clear();
			m_currentLayerIsFirst = true;
			mp_currentLayer = nullptr;
		};
		if (threeD)
			ioGraph.addPass(layer->getName(), writeSceneReadShadows, body);
		else
			ioGraph.addPass(layer->getName(), writeScene, body);
	}
}

void Scene::renderHud(const math::vec2ui& iViewport, const bool iEditorMode) {
	ui::HudPass::render(*this, iViewport, iEditorMode);
}

void Scene::applyLighting2D(const bool iEditorMode, const Layer2DLighting& iLayer) {
	OWL_PROFILE_FUNCTION()

	auto& lighting = m_lighting2D;
	lighting.lights.clear();
	lighting.occluders.clear();
	lighting.ambient = math::vec3{0.f, 0.f, 0.f};
	lighting.groundPlane = iLayer.groundPlane;
	lighting.inScene3D = iLayer.inScene3D;
	lighting.exposureScale = iLayer.inScene3D && mp_renderCamera != nullptr
									 ? std::exp2(-mp_renderCamera->getPostProcess().exposure)
									 : 1.f;
	// On the ground plane the second axis of the lights is the world z.
	const size_t planeRow = iLayer.groundPlane ? 2 : 1;
	bool anyLight = false;
	bool anyAmbient = false;
	const auto toLinear = [](const math::vec3& iColor, const float iIntensity) -> math::vec3 {
		return math::vec3{renderer::ColorEncoder::srgbToLinear(iColor.x()),
						  renderer::ColorEncoder::srgbToLinear(iColor.y()),
						  renderer::ColorEncoder::srgbToLinear(iColor.z())} *
			   iIntensity;
	};
	bool anyShadow = false;
	for (const auto view = registry.view<component::Light2D>(); const auto entity: view) {
		const auto& light = view.get<component::Light2D>(entity);
		const Entity ent{entity, this};
		if (!light.enabled || !isEffectivelyVisible(ent, iEditorMode))
			continue;
		anyLight = true;
		if (light.type == component::Light2D::Type::Ambient) {
			anyAmbient = true;
			lighting.ambient += toLinear(light.color, light.intensity);
			continue;
		}
		if (lighting.lights.size() >= renderer::g_maxLights2D)
			continue;
		const math::mat4 world = getWorldTransform(ent)();
		math::vec2 direction{world(0, 0), world(planeRow, 0)};
		direction = direction.normSq() > 1e-12f ? direction.normalized() : math::vec2{1.f, 0.f};
		const bool spot = light.type == component::Light2D::Type::Spot;
		lighting.lights.push_back({.position = {world(0, 3), world(planeRow, 3)},
								   .height = light.height,
								   .radius = light.radius,
								   .color = toLinear(light.color, light.intensity),
								   .falloff = light.falloff,
								   .direction = direction,
								   .cosInner = spot ? std::cos(light.innerAngle) : 1.f,
								   .cosOuter = spot ? std::cos(std::max(light.outerAngle, light.innerAngle)) : -2.f,
								   .castShadows = light.castShadows,
								   .shadowSoftness = light.shadowSoftness});
		anyShadow = anyShadow || light.castShadows;
	}
	if (!anyLight && !iLayer.inScene3D) {
		renderer::Renderer2D::clearLighting();
		return;
	}
	if (!anyAmbient)
		lighting.ambient = math::vec3{1.f, 1.f, 1.f};
	if (iLayer.inScene3D) {
		const math::vec3 scene = renderer::Renderer3D::getSurfaceLight(math::vec3{0.f, 1.f, 0.f});
		lighting.ambient = math::vec3{lighting.ambient.x() * scene.x(), lighting.ambient.y() * scene.y(),
									  lighting.ambient.z() * scene.z()};
	}
	if (anyShadow && !iLayer.groundPlane) {
		for (const auto view = registry.view<component::SpriteRenderer>(); const auto entity: view) {
			const Entity ent{entity, this};
			if (!view.get<component::SpriteRenderer>(entity).castShadows || !isEffectivelyVisible(ent, iEditorMode) ||
				!layerAccepts(ent))
				continue;
			const math::mat4 world = getWorldTransform(ent)();
			std::array<math::vec2, 4> corners{};
			constexpr std::array<math::vec2, 4> g_unitCorners{math::vec2{-0.5f, -0.5f}, math::vec2{0.5f, -0.5f},
															  math::vec2{0.5f, 0.5f}, math::vec2{-0.5f, 0.5f}};
			for (size_t corner = 0; corner < corners.size(); ++corner) {
				const auto& unit = g_unitCorners[corner];
				corners[corner] = {world(0, 0) * unit.x() + world(0, 1) * unit.y() + world(0, 3),
								   world(1, 0) * unit.x() + world(1, 1) * unit.y() + world(1, 3)};
			}
			for (size_t corner = 0; corner < corners.size(); ++corner)
				lighting.occluders.push_back({.start = corners[corner], .end = corners[(corner + 1) % corners.size()]});
		}
	}
	renderer::Renderer2D::setLighting(lighting);
}

void Scene::render() {
	OWL_PROFILE_FUNCTION()

	const bool editorMode = status == Status::Editing;
	const bool raycastLayer =
			mp_currentLayer != nullptr && std::string_view{mp_currentLayer->getTypeKey()} == "RendererRaycast";
	const bool voxelLayer =
			mp_currentLayer != nullptr && std::string_view{mp_currentLayer->getTypeKey()} == "RendererVoxel";
	const bool meshLayer =
			mp_currentLayer != nullptr && std::string_view{mp_currentLayer->getTypeKey()} == "RendererMesh";

	if (m_currentLayerIsFirst && !raycastLayer && !voxelLayer) {
		if (const auto bgView = registry.view<component::BackgroundTexture>(); !bgView.empty()) {
			if (const auto bgEntity = bgView.front(); isEffectivelyVisible(Entity{bgEntity, this}, editorMode)) {
				const auto& [mode, type, color, topColor, texture] = bgView.get<component::BackgroundTexture>(bgEntity);
				const int i_mode = mode == component::BackgroundTexture::Mode::Skybox ? 3 : static_cast<int>(type);

				renderer::BackgroundRenderer::drawBackground({.mode = i_mode,
															  .color = color,
															  .topColor = topColor,
															  .inverseViewRotation = m_inverseViewRotation,
															  .texture = texture});
			}
		}
	}

	if (voxelLayer) {
		renderVoxelWorlds(editorMode);
		return;
	}

	if (meshLayer) {
		renderMeshes(editorMode);
		return;
	}

	if (raycastLayer) {
		renderRaycastDynamicWalls(editorMode);
		renderRaycastSprites(editorMode);
		return;
	}

	render2D(editorMode);
}

void Scene::render2D(const bool iEditorMode) {
	OWL_PROFILE_FUNCTION()

	const bool editorMode = iEditorMode;
	const auto* const layer2D = dynamic_cast<const renderer::Renderer2DLayer*>(mp_currentLayer);
	const auto billboard = layer2D != nullptr && layer2D->getSpace() == renderer::Renderer2DLayer::Space::World
								   ? layer2D->getBillboard()
								   : renderer::Renderer2DLayer::Billboard::None;
	const bool turned = billboard != renderer::Renderer2DLayer::Billboard::None;
	// A depth-tested or turned layer is a 3D one: its primitives sort by their distance to the camera.
	const bool viewSorted = turned || (layer2D != nullptr && layer2D->isDepthTested());
	const renderer::utils::BillboardFrame facing =
			layer2D != nullptr ? renderer::utils::makeBillboardFrame(billboard, layer2D->getCameraWorld())
							   : renderer::utils::BillboardFrame{};
	applyLighting2D(editorMode, {.groundPlane = turned, .inScene3D = viewSorted});
	// Sort key of a 2D primitive: its world z (larger is nearer); equal keys keep the submission order.
	const auto sortDepthOf = [this, viewSorted, &facing](const Entity& iEntity, const int32_t iWorldIndex) -> float {
		const math::vec3 position = iWorldIndex >= 0
											? math::vec3{m_worldMatrices[static_cast<size_t>(iWorldIndex)](0, 3),
														 m_worldMatrices[static_cast<size_t>(iWorldIndex)](1, 3),
														 m_worldMatrices[static_cast<size_t>(iWorldIndex)](2, 3)}
											: getWorldTransform(iEntity).translation();
		return viewSorted ? renderer::utils::viewSortKey(facing, position) : position.z();
	};
	// The world transform of a primitive of a turned layer faces the camera, keeping its size and its own z rotation.
	const auto turnedTransform = [this, &facing](const Entity& iEntity) -> math::Transform {
		const math::Transform world = getWorldTransform(iEntity);
		return math::Transform{renderer::utils::billboardMatrix(
				facing, world.translation(), world.scale(),
				iEntity.getComponent<component::Transform>().transform.rotation().z())};
	};
	// A primitive drawn from the GPU world matrices takes an empty transform; the others give theirs.
	const auto drawTransform = [&](const Entity& iEntity, const int32_t iWorldIndex) -> math::Transform {
		if (turned)
			return turnedTransform(iEntity);
		return iWorldIndex < 0 ? getWorldTransform(iEntity) : math::Transform{};
	};
	m_cullingStats.spritesInFrustum =
			markVisible(m_spriteIndex, renderer::Renderer2D::getViewProjection(), m_spriteVisible);
	// Draw sprites; the first owning group sorts the `Transform` pool, which the world slots follow.
	m_worldLayoutDirty = m_worldLayoutDirty || !registry.owned<component::Transform>();
	for (const auto group = registry.group<component::Transform>(entt::get<component::SpriteRenderer>);
		 auto entity: group) {
		if (!isMarkedVisible(m_spriteVisible, entity))
			continue;
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		auto [transform, sprite] = group.get<component::Transform, component::SpriteRenderer>(entity);
		const auto worldIndex = turned ? -1 : static_cast<int32_t>(getWorldIndex(ent));

		renderer::Renderer2D::drawQuad({.transform = drawTransform(ent, worldIndex),
										.worldIndex = worldIndex,
										.color = sprite.color,
										.texture = sprite.texture,
										.tilingFactor = sprite.tilingFactor,
										.entityId = static_cast<int>(entity),
										.depth = sortDepthOf(ent, worldIndex),
										.normalMap = sprite.normalMap,
										.lit = sprite.lit,
										.shadowCaster = sprite.castShadows});
	}
	// Draw animated sprites
	for (const auto view = registry.view<component::Transform, component::AnimatedSpriteRenderer>();
		 auto entity: view) {
		if (!isMarkedVisible(m_spriteVisible, entity))
			continue;
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		auto [transform, anim] = view.get<component::Transform, component::AnimatedSpriteRenderer>(entity);
		const auto worldIndex = turned ? -1 : static_cast<int32_t>(getWorldIndex(ent));
		const uint32_t safeCols = std::max(anim.columns, 1u);
		const uint32_t safeRows = std::max(anim.rows, 1u);
		const uint32_t frame = std::clamp(anim.m_currentFrame, anim.firstFrame, anim.lastFrame);
		const uint32_t col = frame % safeCols;
		const uint32_t row = frame / safeCols;
		const float uMin = static_cast<float>(col) / static_cast<float>(safeCols);
		const float uMax = static_cast<float>(col + 1) / static_cast<float>(safeCols);
		const float vMax = 1.0f - static_cast<float>(row) / static_cast<float>(safeRows);
		const float vMin = 1.0f - static_cast<float>(row + 1) / static_cast<float>(safeRows);

		renderer::Renderer2D::drawQuad({.transform = drawTransform(ent, worldIndex),
										.worldIndex = worldIndex,
										.color = anim.color,
										.texture = anim.texture,
										.textureCoords = {math::vec2{uMin, vMin}, math::vec2{uMax, vMin},
														  math::vec2{uMax, vMax}, math::vec2{uMin, vMax}},
										.entityId = static_cast<int>(entity),
										.depth = sortDepthOf(ent, worldIndex),
										.lit = anim.lit});
	}
	// Draw circles
	for (const auto view = registry.view<component::Transform, component::CircleRenderer>(); auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		auto [transform, circle] = view.get<component::Transform, component::CircleRenderer>(entity);
		const auto worldIndex = turned ? -1 : static_cast<int32_t>(getWorldIndex(ent));

		renderer::Renderer2D::drawCircle({.transform = drawTransform(ent, worldIndex),
										  .worldIndex = worldIndex,
										  .color = circle.color,
										  .thickness = circle.thickness,
										  .fade = circle.fade,
										  .entityId = static_cast<int>(entity),
										  .depth = sortDepthOf(ent, worldIndex)});
	}
	// drawString always uses the transient buffer (per-glyph local matrices), so worldIndex is left default.
	for (const auto view = registry.view<component::Transform, component::Text>(); auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		auto [transform, text] = view.get<component::Transform, component::Text>(entity);
		const math::Transform worldTransform = turned ? turnedTransform(ent) : getWorldTransform(ent);

		renderer::Renderer2D::drawString(
				{.transform = uniformTextTransform(worldTransform),
				 .text = text.text,
				 .font = text.font,
				 .color = text.color,
				 .layout = {.kerning = text.kerning, .lineSpacing = text.lineSpacing},
				 .entityId = static_cast<int>(entity),
				 .depth = viewSorted ? renderer::utils::viewSortKey(facing, worldTransform.translation())
									 : worldTransform.translation().z(),
				 .outlineColor = text.outlineColor,
				 .outlineWidth = text.outlineWidth,
				 .backgroundColor = text.backgroundColor,
				 .backgroundPadding = text.backgroundPadding});
	}
	// Pushwall plates override the entity's scale to a 1-cell square, so the cached world matrix doesn't match.
	for (const auto view = registry.view<component::Transform, component::RaycastPushWall>(); auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		const auto& push = view.get<component::RaycastPushWall>(entity);
		if (!push.tileset || !push.tileset->texture)
			continue;
		math::Transform worldTransform = getWorldTransform(ent);
		worldTransform.scale().x() = 1.f;
		worldTransform.scale().y() = 1.f;
		const auto corners = push.tileset->getTileUv(push.tileIndex);
		renderer::Renderer2D::drawQuad({.transform = worldTransform,
										.color = math::vec4{1.f, 1.f, 1.f, 1.f},
										.texture = push.tileset->texture,
										.textureCoords = corners,
										.entityId = static_cast<int>(entity)});
	}
	// Door plates use a non-uniform scale baked in CPU-side, so they keep the transient path.
	for (const auto view = registry.view<component::Transform, component::RaycastDoor>(); auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		const auto& door = view.get<component::RaycastDoor>(entity);
		if (!door.tileset || !door.tileset->texture)
			continue;
		const auto worldTransform = getWorldTransform(ent);
		const float cx = worldTransform.translation().x();
		const float cy = worldTransform.translation().y();
		const float cz = worldTransform.translation().z();
		using OD = component::RaycastDoor::OpeningDirection;
		const bool slideAlongY = (door.openingDirection == OD::North || door.openingDirection == OD::South);
		constexpr float kPlateSpriteThickness = 0.18f;
		math::Transform plateTr;
		plateTr.translation() = math::vec3{cx, cy, cz};
		plateTr.scale() =
				math::vec3{slideAlongY ? kPlateSpriteThickness : 1.f, slideAlongY ? 1.f : kPlateSpriteThickness, 1.f};
		const auto corners = door.tileset->getTileUv(door.faceTileIndex);
		renderer::Renderer2D::drawQuad({.transform = plateTr,
										.color = math::vec4{1.f, 1.f, 1.f, 1.f},
										.texture = door.tileset->texture,
										.textureCoords = corners,
										.entityId = static_cast<int>(entity)});
	}
}

void Scene::prepareTilemapRenderData() {
	OWL_PROFILE_FUNCTION()

	resolveAllTilemapAssets();
	renderer::RendererTilemap::beginPrepare();
	renderer::RendererIsometric::beginPrepare();
	const bool editorMode = status == Status::Editing;
	for (const auto view = registry.view<component::Tilemap>(); const auto entity: view) {
		const auto& asset = view.get<component::Tilemap>(entity).asset;
		if (!asset || !asset->tileset || !asset->tileset->texture)
			continue;
		if (!isEffectivelyVisible(Entity{entity, this}, editorMode))
			continue;
		// An isometric tileset is drawn by the isometric layer from its own chunk buffers.
		if (asset->tileset->kind == Tileset::Kind::Isometric)
			renderer::RendererIsometric::prepareTilemap(*asset);
		else
			renderer::RendererTilemap::prepareTilemap(*asset);
	}
}

void Scene::prepareVoxelRenderData() {
	OWL_PROFILE_FUNCTION()

	// Ensure tileset atlases are resolved before meshing (gated/cheap).
	resolveAllTilemapAssets();
	if (mp_engineContext == nullptr)
		return;
	auto& cache = mp_engineContext->getVoxelMeshCache();
	renderer::RendererVoxel::beginPrepare(cache);
	for (const auto view = registry.view<component::VoxelWorld>(); const auto entity: view)
		renderer::RendererVoxel::prepareWorld(cache, view.get<component::VoxelWorld>(entity), static_cast<int>(entity));
}

void Scene::prepareMeshRenderData() {
	OWL_PROFILE_FUNCTION()

	if (mp_engineContext == nullptr)
		return;
	auto& cache = mp_engineContext->getMeshCache();
	auto& library = mp_engineContext->getMaterialLibrary();
	for (const auto view = registry.view<component::MeshRenderer>(); const auto entity: view) {
		auto& meshRenderer = view.get<component::MeshRenderer>(entity);
		if (meshRenderer.mesh && meshRenderer.mesh->isStale()) {
			meshRenderer.mesh.reset();
			m_spatialDirty = true;
			for (auto& slot: meshRenderer.materials) slot.invalidate();
		}
		if (!meshRenderer.mesh) {
			meshRenderer.mesh = cache.get(meshRenderer.meshPath);
			m_spatialDirty = m_spatialDirty || meshRenderer.mesh != nullptr;
		}
		if (!meshRenderer.mesh)
			continue;
		const auto& submeshes = meshRenderer.mesh->getSubmeshes();
		if (meshRenderer.materials.size() < submeshes.size())
			meshRenderer.materials.resize(submeshes.size());
		for (size_t index = 0; index < submeshes.size(); ++index) {
			auto& slot = meshRenderer.materials[index];
			if (!slot.resolved || slot.generation != library.getGeneration())
				resolveMaterialSlot(meshRenderer, index, library);
			std::ignore = meshRenderer.mesh->getSubmeshMesh(index, slot.resolved->isBlended());
		}
	}
}

namespace {
// Local box of a sprite quad: the unit square centred on its origin, flat.
const math::box3f g_spriteLocalBox{math::vec3{-0.5f, -0.5f, 0.f}, math::vec3{0.5f, 0.5f, 0.f}};

}// namespace

void Scene::onSpatialComponentChanged([[maybe_unused]] entt::registry& ioRegistry,
									  [[maybe_unused]] const entt::entity iEntity) {
	m_spatialDirty = true;
	// The sprite pass owns the `Transform` pool through a group: it reorders the pool when a sprite comes or goes.
	m_worldLayoutDirty = true;
}

void Scene::onWorldLayoutChanged([[maybe_unused]] entt::registry& ioRegistry,
								 [[maybe_unused]] const entt::entity iEntity) {
	m_worldLayoutDirty = true;
}

void Scene::updateSpatialIndex() {
	OWL_PROFILE_FUNCTION()

	// Nothing added, removed, replaced nor moved since the last update: the indexes are current.
	if (!m_spatialDirty && !m_worldMoved)
		return;
	m_spatialDirty = false;
	m_worldMoved = false;
	++m_spatialPass;
	m_spatialJobs.clear();
	const auto refile = [this](std::vector<SpatialEntry>& ioEntries, data::geometry::LooseOctree& ioIndex,
							   const entt::entity iEntity, const void* iSource, const math::box3f& iLocalBox) -> void {
		const auto index = static_cast<uint32_t>(entt::to_entity(iEntity));
		// The nodes `prepareWorldTransforms` checked this pass are current: no walk up the hierarchy again.
		const WorldNode* node = index < m_worldNodes.size() && m_worldNodes[index].handle == iEntity &&
												m_worldNodes[index].checkedPass == m_worldPass
										? &m_worldNodes[index]
										: resolveWorldNode(iEntity, core::UUID{0});
		if (node == nullptr)
			return;
		if (index >= ioEntries.size())
			ioEntries.resize(std::max<size_t>(index + 1, ioEntries.size() * 2));
		auto& entry = ioEntries[index];
		if (entry.handle != iEntity || entry.stamp != node->stamp || entry.source != iSource) {
			m_spatialJobs.push_back({.tree = &ioIndex, .index = index, .localBox = iLocalBox});
			entry.handle = iEntity;
			entry.stamp = node->stamp;
			entry.source = iSource;
		}
		entry.pass = m_spatialPass;
	};
	for (const auto view = registry.view<component::Transform, component::MeshRenderer>(); const auto entity: view) {
		const auto& meshRenderer = view.get<component::MeshRenderer>(entity);
		if (!meshRenderer.mesh)
			continue;
		const auto& geometry = meshRenderer.mesh->getGeometry();
		refile(m_meshEntries, m_meshIndex, entity, meshRenderer.mesh.get(),
			   math::box3f{geometry.boundsMin, geometry.boundsMax});
	}
	for (const auto entity: registry.view<component::Transform, component::SpriteRenderer>())
		refile(m_spriteEntries, m_spriteIndex, entity, nullptr, g_spriteLocalBox);
	for (const auto entity: registry.view<component::Transform, component::AnimatedSpriteRenderer>())
		refile(m_spriteEntries, m_spriteIndex, entity, nullptr, g_spriteLocalBox);
	// The boxes are independent of one another; the octree is filed serially, in the walk order.
	m_spatialBoxes.resize(m_spatialJobs.size());
	const auto computeBoxes = [this](const size_t iBegin, const size_t iEnd) -> void {
		for (size_t i = iBegin; i < iEnd; ++i)
			m_spatialBoxes[i] =
					math::transformBox(m_worldNodes[m_spatialJobs[i].index].world, m_spatialJobs[i].localBox);
	};
	if (mp_taskScheduler != nullptr && m_spatialJobs.size() >= m_parallelMinEntities)
		mp_taskScheduler->parallelFor(m_spatialJobs.size(), g_ParallelChunkEntities, computeBoxes);
	else
		computeBoxes(0, m_spatialJobs.size());
	for (size_t i = 0; i < m_spatialJobs.size(); ++i)
		m_spatialJobs[i].tree->insert(m_spatialJobs[i].index, m_spatialBoxes[i]);
	const auto sweep = [this](std::vector<SpatialEntry>& ioEntries, data::geometry::LooseOctree& ioIndex) -> void {
		for (size_t index = 0; index < ioEntries.size(); ++index) {
			if (auto& entry = ioEntries[index]; entry.handle != entt::null && entry.pass != m_spatialPass) {
				ioIndex.remove(static_cast<uint32_t>(index));
				entry = {};
			}
		}
	};
	sweep(m_meshEntries, m_meshIndex);
	sweep(m_spriteEntries, m_spriteIndex);
	m_cullingStats.meshesIndexed = static_cast<uint32_t>(m_meshIndex.size());
	m_cullingStats.spritesIndexed = static_cast<uint32_t>(m_spriteIndex.size());
}

auto Scene::markVisible(const data::geometry::LooseOctree& iIndex, const math::mat4& iViewProjection, VisibleSet& ioSet)
		-> uint32_t {
	OWL_PROFILE_FUNCTION()

	if (ioSet.valid && ioSet.generation == iIndex.getGeneration() && ioSet.viewProjection == iViewProjection)
		return ioSet.all ? static_cast<uint32_t>(iIndex.size()) : static_cast<uint32_t>(ioSet.ids.size());
	if (!ioSet.all) {
		for (const uint32_t id: ioSet.ids) ioSet.marks[id] = 0;
	}
	ioSet.ids.clear();
	ioSet.valid = true;
	ioSet.generation = iIndex.getGeneration();
	ioSet.viewProjection = iViewProjection;
	const auto frustum = math::Frustum::fromViewProjection(iViewProjection);
	// Everything filed is in view (a 2D scene on one screen): no box to test, no mark to set.
	ioSet.all = iIndex.size() == 0 || frustum.classify(iIndex.getContentBounds()) == math::Containment::Inside;
	if (ioSet.all)
		return static_cast<uint32_t>(iIndex.size());
	iIndex.query(frustum, ioSet.ids);
	ioSet.all = ioSet.ids.size() == iIndex.size();
	if (ioSet.all)
		return static_cast<uint32_t>(ioSet.ids.size());
	for (const uint32_t id: ioSet.ids) {
		if (id >= ioSet.marks.size())
			ioSet.marks.resize(std::max<size_t>(id + 1, ioSet.marks.size() * 2), 0);
		ioSet.marks[id] = 1;
	}
	return static_cast<uint32_t>(ioSet.ids.size());
}

auto Scene::isMarkedVisible(const VisibleSet& iSet, const entt::entity iEntity) -> bool {
	if (iSet.all)
		return true;
	const auto index = static_cast<size_t>(entt::to_entity(iEntity));
	return index < iSet.marks.size() && iSet.marks[index] != 0;
}

void Scene::resolveMaterialSlot(component::MeshRenderer& ioMeshRenderer, const size_t iSlot,
								renderer::MaterialLibrary& ioLibrary) {
	auto& slot = ioMeshRenderer.materials[iSlot];
	shared<const renderer::Material> base = slot.material.empty() ? nullptr : ioLibrary.get(slot.material);
	if (!base)
		base = ioMeshRenderer.mesh->getFileMaterial(iSlot);
	renderer::MaterialOverrides overrides = slot.overrides;
	if (ioMeshRenderer.transparent && !overrides.has(renderer::MaterialField::AlphaMode)) {
		overrides.values.alphaMode = renderer::AlphaMode::Blend;
		overrides.set(renderer::MaterialField::AlphaMode);
	}
	const bool ownTexture = ioMeshRenderer.texture && !slot.overrides.has(renderer::MaterialField::BaseColorMap);
	slot.generation = ioLibrary.getGeneration();
	// A slot that changes nothing shares its material: the mesh pass instances the entities drawing the same one.
	if (overrides.isEmpty() && !ownTexture) {
		slot.resolved = base;
		return;
	}
	auto material = overrides.isEmpty() ? *base : base->withOverrides(overrides);
	if (ownTexture)
		material = material.withTexture(renderer::MaterialMap::BaseColor, ioMeshRenderer.texture);
	slot.resolved = mkShared<const renderer::Material>(std::move(material));
}

void Scene::renderMeshes(const bool iEditorMode) {
	OWL_PROFILE_FUNCTION()

	m_cullingStats.meshesInFrustum = markVisible(m_meshIndex, renderer::Renderer3D::getViewProjection(), m_meshVisible);
	for (const auto view = registry.view<component::Transform, component::MeshRenderer>(); const auto entity: view) {
		if (!isMarkedVisible(m_meshVisible, entity))
			continue;
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, iEditorMode) || !layerAccepts(ent))
			continue;
		if (m_meshFilterRoot != entt::null && !isInSubtree(entity, m_meshFilterRoot))
			continue;
		auto& meshRenderer = view.get<component::MeshRenderer>(entity);
		if (!meshRenderer.mesh)
			continue;
		const math::mat4 model = getWorldTransform(ent)();
		const auto submeshCount = std::min(meshRenderer.mesh->getSubmeshes().size(), meshRenderer.materials.size());
		for (size_t index = 0; index < submeshCount; ++index) {
			const auto& slot = meshRenderer.materials[index];
			if (!slot.resolved)
				continue;
			renderer::RendererMesh::drawSubmesh(*meshRenderer.mesh, index, model,
												{.tint = meshRenderer.color,
												 .entityId = static_cast<int>(entity),
												 .material = slot.resolved.get()});
		}
	}
}

void Scene::renderVoxelWorlds(const bool iEditorMode) {
	OWL_PROFILE_FUNCTION()

	if (mp_engineContext == nullptr)
		return;
	for (const auto view = registry.view<component::Transform, component::VoxelWorld>(); const auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, iEditorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		auto& voxelWorld = view.get<component::VoxelWorld>(entity);
		const math::Transform worldTransform = getWorldTransform(ent);
		renderer::RendererVoxel::drawVoxelWorld(mp_engineContext->getVoxelMeshCache(), voxelWorld, worldTransform,
												static_cast<int>(entity));
	}
}

void Scene::updateVoxelStreaming(const math::vec3& iCameraWorldPos) {
	OWL_PROFILE_FUNCTION()

	bool anyProcedural = false;
	for (const auto view = registry.view<component::VoxelWorld>(); const auto entity: view) {
		if (view.get<component::VoxelWorld>(entity).proceduralTerrain) {
			anyProcedural = true;
			break;
		}
	}
	if (!anyProcedural)
		return;
	if (!m_voxelStream)
		m_voxelStream = mkShared<VoxelStreamState>();
	auto& stream = *m_voxelStream;
	const auto key = [](const math::vec3i& iCoord) -> uint64_t {
		const auto enc = [](const int32_t iValue) -> uint64_t {
			return static_cast<uint64_t>(static_cast<int64_t>(iValue) + (1 << 20)) & 0x1FFFFF;
		};
		return enc(iCoord.x()) | (enc(iCoord.y()) << 21) | (enc(iCoord.z()) << 42);
	};

	// Install chunks finished on worker threads (main thread; the worlds are only mutated here).
	std::vector<CompletedVoxelChunk> done;
	{
		const std::lock_guard<std::mutex> lock{stream.mutex};
		done.swap(stream.completed);
	}
	for (auto& finished: done) {
		const auto entity = static_cast<entt::entity>(finished.entityId);
		if (!registry.valid(entity) || !registry.any_of<component::VoxelWorld>(entity))
			continue;
		auto& vw = registry.get<component::VoxelWorld>(entity);
		// Not pending any more (streamed out or regenerated) or generated with older parameters: stale, drop it.
		if (vw.pendingChunks.erase(key(finished.coord)) == 0 || !vw.proceduralTerrain || !finished.chunk ||
			finished.params != vw.terrain)
			continue;
		vw.world.insertChunk(finished.coord, std::move(*finished.chunk));
	}

	constexpr int32_t kMaxPushPerFrame = 16;
	int32_t budget = kMaxPushPerFrame;
	const math::vec3i camBlock{static_cast<int32_t>(std::floor(iCameraWorldPos.x())),
							   static_cast<int32_t>(std::floor(iCameraWorldPos.y())),
							   static_cast<int32_t>(std::floor(iCameraWorldPos.z()))};
	auto& scheduler = app::Application::get().getTaskScheduler();
	for (const auto view = registry.view<component::VoxelWorld>(); const auto entity: view) {
		auto& vw = view.get<component::VoxelWorld>(entity);
		if (!vw.proceduralTerrain)
			continue;
		const int entityId = static_cast<int>(entity);
		const bool editorMode = status == Status::Editing;
		const int32_t r = std::max(0, editorMode ? vw.editorStreamRadius : vw.streamRadius);
		const int32_t h = std::max(0, editorMode ? vw.editorStreamHeight : vw.streamHeight);
		const math::vec3i camChunk = data::voxel::worldToChunk(camBlock);
		// Queue missing chunks for async generation (budgeted; pendingChunks avoids re-queuing in-flight ones).
		for (int32_t dy = -h; dy <= h && budget > 0; ++dy) {
			for (int32_t dz = -r; dz <= r && budget > 0; ++dz) {
				for (int32_t dx = -r; dx <= r && budget > 0; ++dx) {
					const math::vec3i coord{camChunk.x() + dx, camChunk.y() + dy, camChunk.z() + dz};
					const uint64_t k = key(coord);
					if (vw.world.getChunk(coord) || vw.pendingChunks.contains(k))
						continue;
					vw.pendingChunks.insert(k);
					--budget;
					const data::voxel::TerrainParams params = vw.terrain;
					auto sink = m_voxelStream;
					scheduler.pushTask(core::task::Task{[params, coord, sink, entityId]() -> void {
						auto chunk = mkShared<data::voxel::Chunk>(coord);
						data::voxel::TerrainGenerator{params}.generateChunk(*chunk, coord);
						const std::lock_guard<std::mutex> lock{sink->mutex};
						sink->completed.push_back(CompletedVoxelChunk{.entityId = entityId,
																	  .coord = coord,
																	  .params = params,
																	  .chunk = chunk});
					}});
				}
			}
		}
		// Unload chunks (and forget pending ones) that drifted outside the radius (+1 chunk of hysteresis).
		const auto outside = [&](const math::vec3i& iCoord) -> bool {
			return std::abs(iCoord.x() - camChunk.x()) > r + 1 || std::abs(iCoord.z() - camChunk.z()) > r + 1 ||
				   std::abs(iCoord.y() - camChunk.y()) > h + 1;
		};
		for (const auto& coord: vw.world.chunkCoordinates()) {
			if (outside(coord))
				vw.world.removeChunk(coord);
		}
		std::erase_if(vw.pendingChunks, [&](const uint64_t iKey) -> bool { return outside(unpackChunkKey(iKey)); });
	}
}

void Scene::setShowCrosshair(const bool iShow) { m_showCrosshair = iShow; }

void Scene::showToast(const std::string& iMessage, const float iSeconds) {
	m_toastMessage = iMessage;
	m_toastTimer = iSeconds;
}

void Scene::renderOverlays(const renderer::Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	const bool navigationDebug = m_navigation != nullptr && m_navigation->isDebugVisible();
	if (navigationDebug) {
		// The graph lies on the ground of the isometric view: the lines go through the camera the isometric layer draws with.
		std::optional<renderer::IsometricCamera> isometricCamera;
		for (const auto& layer: renderer::Renderer::getRenderStack().getLayers()) {
			if (const auto* isometric = dynamic_cast<const renderer::RendererIsometricLayer*>(layer.get());
				isometric != nullptr) {
				isometricCamera = isometric->makeCamera(iCamera);
				break;
			}
		}
		renderer::Renderer2D::beginScene(isometricCamera.has_value() ? *isometricCamera : iCamera);
		nav::drawNavigationGraph(*this);
		nav::drawNavigationRoutes(*this);
		renderer::Renderer2D::endScene();
	}
	if (!m_hasVoxelHighlight && !m_showCrosshair && !m_interactionHighlight)
		return;
	const math::vec4 cameraPos4 = inverse(iCamera.getView()) * math::vec4{0.f, 0.f, 0.f, 1.f};
	const math::vec3 cameraPos{cameraPos4.x(), cameraPos4.y(), cameraPos4.z()};
	renderer::Renderer2D::beginScene(iCamera);

	if (m_hasVoxelHighlight) {
		constexpr float pad = 0.004f;
		const float lx = static_cast<float>(m_voxelHighlightBlock.x()) - pad;
		const float ly = static_cast<float>(m_voxelHighlightBlock.y()) - pad;
		const float lz = static_cast<float>(m_voxelHighlightBlock.z()) - pad;
		const float hx = lx + 1.f + 2.f * pad;
		const float hy = ly + 1.f + 2.f * pad;
		const float hz = lz + 1.f + 2.f * pad;
		const std::array<math::vec3, 8> c{math::vec3{lx, ly, lz}, math::vec3{hx, ly, lz}, math::vec3{hx, hy, lz},
										  math::vec3{lx, hy, lz}, math::vec3{lx, ly, hz}, math::vec3{hx, ly, hz},
										  math::vec3{hx, hy, hz}, math::vec3{lx, hy, hz}};
		struct Face {
			math::vec3i normal;
			std::array<int, 4> corners;
		};
		constexpr std::array<Face, 6> faces{Face{{0, 0, -1}, {0, 1, 2, 3}}, Face{{0, 0, 1}, {4, 5, 6, 7}},
											Face{{0, -1, 0}, {0, 1, 5, 4}}, Face{{0, 1, 0}, {3, 2, 6, 7}},
											Face{{-1, 0, 0}, {0, 3, 7, 4}}, Face{{1, 0, 0}, {1, 2, 6, 5}}};
		const math::vec4 color{0.96f, 0.96f, 1.f, 0.9f};
		// Outline only the face the ray hit (its outward normal matches the recorded hit normal).
		for (const auto& face: faces) {
			if (face.normal != m_voxelHighlightNormal)
				continue;
			for (size_t e = 0; e < 4; ++e)
				renderer::Renderer2D::drawLine({.point1 = c[static_cast<size_t>(face.corners[e])],
												.point2 = c[static_cast<size_t>(face.corners[(e + 1) % 4])],
												.color = color});
		}
	}

	if (m_interactionHighlight) {
		constexpr float pad = 0.01f;
		const math::vec3 low = m_interactionHighlight->min() - math::vec3{pad, pad, pad};
		const math::vec3 high = m_interactionHighlight->max() + math::vec3{pad, pad, pad};
		const std::array<math::vec3, 8> corner{
				math::vec3{low.x(), low.y(), low.z()},    math::vec3{high.x(), low.y(), low.z()},
				math::vec3{high.x(), high.y(), low.z()},  math::vec3{low.x(), high.y(), low.z()},
				math::vec3{low.x(), low.y(), high.z()},   math::vec3{high.x(), low.y(), high.z()},
				math::vec3{high.x(), high.y(), high.z()}, math::vec3{low.x(), high.y(), high.z()}};
		constexpr std::array<std::pair<size_t, size_t>, 12> edges{
				{{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}}};
		const math::vec4 color{1.f, 0.82f, 0.4f, 0.95f};
		for (const auto& [from, to]: edges)
			renderer::Renderer2D::drawLine({.point1 = corner[from], .point2 = corner[to], .color = color});
	}

	if (m_showCrosshair) {
		const math::mat4 invView = inverse(iCamera.getView());
		const auto toWorldDir = [&](const float iX, const float iY, const float iZ) -> math::vec3 {
			const math::vec4 v = invView * math::vec4{iX, iY, iZ, 0.f};
			return math::vec3{v.x(), v.y(), v.z()}.normalized();
		};
		const math::vec3 right = toWorldDir(1.f, 0.f, 0.f);
		const math::vec3 up = toWorldDir(0.f, 1.f, 0.f);
		const math::vec3 forward = toWorldDir(0.f, 0.f, -1.f);
		// Size the cross from the vertical FOV so it keeps a constant on-screen size at the fixed placement distance.
		const float proj11 = iCamera.getProjection()(1, 1);
		constexpr float distance = 1.f;
		const float halfHeight = proj11 > 0.001f ? distance / proj11 : distance * 0.4f;
		const float arm = halfHeight * 0.03f;
		const float gap = arm * 0.35f;
		const math::vec3 center = cameraPos + forward * distance;
		const math::vec4 color{1.f, 1.f, 1.f, 0.9f};
		renderer::Renderer2D::drawLine(
				{.point1 = center - right * arm, .point2 = center - right * gap, .color = color});
		renderer::Renderer2D::drawLine(
				{.point1 = center + right * gap, .point2 = center + right * arm, .color = color});
		renderer::Renderer2D::drawLine({.point1 = center - up * arm, .point2 = center - up * gap, .color = color});
		renderer::Renderer2D::drawLine({.point1 = center + up * gap, .point2 = center + up * arm, .color = color});
	}

	renderer::Renderer2D::endScene();
}

void Scene::renderToast() {
	OWL_PROFILE_FUNCTION()

	if (m_toastTimer <= 0.f || m_toastMessage.empty() || m_viewportSize.x() == 0 || m_viewportSize.y() == 0)
		return;
	const auto vpW = static_cast<float>(m_viewportSize.x());
	const auto vpH = static_cast<float>(m_viewportSize.y());
	const renderer::CameraOrtho overlay{0.f, vpW, 0.f, vpH};
	constexpr float kTextHeight = 28.f;
	math::Transform textTransform;
	textTransform.translation() = math::vec3{vpW * 0.5f, vpH - kTextHeight * 1.5f, 0.f};
	textTransform.scale() = math::vec3{kTextHeight, kTextHeight, 1.f};
	const auto font = app::Application::get().getFontLibrary().getDefaultFont();
	renderer::Renderer2D::beginScene(overlay);
	renderer::Renderer2D::drawString({.transform = textTransform,
									  .text = m_toastMessage,
									  .font = font,
									  .color = math::vec4{1.f, 1.f, 0.6f, 1.f},
									  .entityId = 0});
	renderer::Renderer2D::endScene();
}

void Scene::renderRaycastSprites(const bool iEditorMode) {
	OWL_PROFILE_FUNCTION()

	const auto resolveSize = [](const math::vec2& iOverride, const math::vec3& iScale) -> math::vec2 {
		if (iOverride.x() > 0.f && iOverride.y() > 0.f)
			return iOverride;
		return {iScale.x(), iScale.y()};
	};
	thread_local std::vector<renderer::RaycastSpriteData> raycastSprites;
	raycastSprites.clear();
	for (const auto view = registry.view<component::Transform, component::SpriteRenderer>(); const auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, iEditorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		const auto& sprite = view.get<component::SpriteRenderer>(entity);
		if (!sprite.texture)
			continue;
		const math::Transform worldTransform = getWorldTransform(ent);
		raycastSprites.push_back({.worldPosition = {worldTransform.translation().x(), worldTransform.translation().y()},
								  .worldZOffset = worldTransform.translation().z() + sprite.raycastZOffset,
								  .worldSize = resolveSize(sprite.raycastSize, worldTransform.scale()),
								  .tint = sprite.color,
								  .texture = sprite.texture,
								  .entityId = static_cast<int>(entity)});
	}
	for (const auto view = registry.view<component::Transform, component::AnimatedSpriteRenderer>();
		 const auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, iEditorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		const auto& anim = view.get<component::AnimatedSpriteRenderer>(entity);
		if (!anim.texture)
			continue;
		const math::Transform worldTransform = getWorldTransform(ent);
		const uint32_t safeCols = std::max(anim.columns, 1u);
		const uint32_t safeRows = std::max(anim.rows, 1u);
		const uint32_t frame = std::clamp(anim.m_currentFrame, anim.firstFrame, anim.lastFrame);
		const uint32_t cellCol = frame % safeCols;
		const uint32_t cellRow = frame / safeCols;
		const float uMin = static_cast<float>(cellCol) / static_cast<float>(safeCols);
		const float uMax = static_cast<float>(cellCol + 1) / static_cast<float>(safeCols);
		const float vMax = 1.0f - static_cast<float>(cellRow) / static_cast<float>(safeRows);
		const float vMin = 1.0f - static_cast<float>(cellRow + 1) / static_cast<float>(safeRows);
		raycastSprites.push_back({.worldPosition = {worldTransform.translation().x(), worldTransform.translation().y()},
								  .worldZOffset = worldTransform.translation().z() + anim.raycastZOffset,
								  .worldSize = resolveSize(anim.raycastSize, worldTransform.scale()),
								  .tint = anim.color,
								  .texture = anim.texture,
								  .textureCoords = {math::vec2{uMin, vMin}, math::vec2{uMax, vMin},
													math::vec2{uMax, vMax}, math::vec2{uMin, vMax}},
								  .entityId = static_cast<int>(entity)});
	}
	renderer::RendererRaycast::drawSprites(raycastSprites);
	// The thread-local scratch must not keep textures alive past the frame (they would outlive the device).
	raycastSprites.clear();
}

namespace {
auto tileUvRectFromTileset(const Tileset& iTileset, const uint32_t iTileIndex) -> math::vec4 {
	const auto corners = iTileset.getTileUv(iTileIndex);
	return math::vec4{corners[0].x(), corners[0].y(), corners[2].x(), corners[2].y()};
}
}// namespace

void Scene::renderRaycastDynamicWalls(const bool iEditorMode) {
	OWL_PROFILE_FUNCTION()

	thread_local std::vector<renderer::RaycastDynamicWallData> walls;
	walls.clear();
	for (const auto view = registry.view<component::Transform, component::RaycastPushWall>(); const auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, iEditorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		const auto& push = view.get<component::RaycastPushWall>(entity);
		if (!push.tileset || !push.tileset->texture)
			continue;
		const math::Transform worldTransform = getWorldTransform(ent);
		walls.push_back({.worldCenter = {worldTransform.translation().x(), worldTransform.translation().y()},
						 .halfExtent = {std::max(1e-3f, worldTransform.scale().x() * 0.5f),
										std::max(1e-3f, worldTransform.scale().y() * 0.5f)},
						 .texture = push.tileset->texture,
						 .uvRect = tileUvRectFromTileset(*push.tileset, push.tileIndex),
						 .tint = math::vec4{1.f, 1.f, 1.f, 1.f},
						 .wallHeight = 1.f,
						 .entityId = static_cast<int>(entity)});
	}
	renderer::RendererRaycast::drawDynamicWalls(walls);
	walls.clear();

	// Doors — 2 static laterals + 1 moving plate per door cell, route to drawDoors.
	thread_local std::vector<renderer::RaycastDoorData> doors;
	doors.clear();
	for (const auto view = registry.view<component::Transform, component::RaycastDoor>(); const auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, iEditorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		const auto& door = view.get<component::RaycastDoor>(entity);
		if (!door.tileset || !door.tileset->texture)
			continue;
		const math::Transform worldTransform = getWorldTransform(ent);
		doors.push_back({.openingDirection = static_cast<uint8_t>(door.openingDirection),
						 .cellCenter = {worldTransform.translation().x(), worldTransform.translation().y()},
						 .plateOffset = door.currentOffset,
						 .faceTexture = door.tileset->texture,
						 .faceUvRect = tileUvRectFromTileset(*door.tileset, door.faceTileIndex),
						 .lateralTexture = door.tileset->texture,
						 .lateralUvRect = tileUvRectFromTileset(*door.tileset, door.lateralTileIndex),
						 .tint = math::vec4{1.f, 1.f, 1.f, 1.f},
						 .wallHeight = 1.f,
						 .entityId = static_cast<int>(entity)});
	}
	renderer::RendererRaycast::drawDoors(doors);
	doors.clear();
}

void Scene::detachSharedTilemapAssets() {
	// A Play copy shares the edit scene's tilemaps: the runtime edits its own copy so Stop finds the map untouched.
	std::unordered_map<const TilemapAsset*, shared<TilemapAsset>> detached;
	for (const auto view = registry.view<component::Tilemap>(); const auto entity: view) {
		auto& tilemap = view.get<component::Tilemap>(entity);
		if (!tilemap.asset)
			continue;
		const auto* original = tilemap.asset.get();
		if (const auto known = detached.find(original); known != detached.end()) {
			tilemap.asset = known->second;
		} else if (tilemap.asset.use_count() > 1) {
			auto copy = mkShared<TilemapAsset>(*tilemap.asset);
			detached.emplace(original, copy);
			tilemap.asset = std::move(copy);
		}
	}
}

void Scene::resolveAllTilemapAssets() {
	const auto hasPendingObject = [this]() -> bool {
		return std::ranges::any_of(registry.view<component::IsometricObject>(), [this](const auto iEntity) -> bool {
			const auto& object = registry.get<component::IsometricObject>(iEntity);
			return !object.atlas && !object.atlasPath.empty();
		});
	};
	if (!m_tilemapAssetsDirty && !hasPendingObject())
		return;
	if (!app::Application::instanced())
		return;
	const auto& app = app::Application::get();
	const auto& assetDirs = app.getAssetDirectories();

	// Try the open pack first (packaged games have no loose files on disk), then the asset directories.
	const auto loadAsset = [&](const std::filesystem::path& iRelative, const auto& iFromBytes,
							   const auto& iFromFile) -> bool {
		if (const std::string rel = iRelative.generic_string(); app.hasOpenPack() && app.packContains(rel)) {
			if (const auto data = app.loadFromPack(rel)) {
				if (const std::string text(data->begin(), data->end()); iFromBytes(text))
					return true;
			}
		}
		for (const auto& [title, assetsPath]: assetDirs) {
			if (const auto fullPath = assetsPath / iRelative; exists(fullPath)) {
				if (iFromFile(fullPath))
					return true;
			}
		}
		return exists(iRelative) && iFromFile(iRelative);
	};

	for (const auto view = registry.view<component::Tilemap>(); const auto entity: view) {
		auto& tilemap = view.get<component::Tilemap>(entity);
		const auto& tilemapPath = tilemap.tilemapPath;
		if (tilemap.asset || tilemapPath.empty())
			continue;
		auto resolved = mkShared<TilemapAsset>();
		const bool loaded = loadAsset(
				tilemapPath,
				[&](const std::string_view iYaml) -> bool { return resolved->deserializeFromString(iYaml); },
				[&](const std::filesystem::path& iPath) -> bool { return resolved->loadFromFile(iPath); });
		if (!loaded) {
			OWL_CORE_WARN("Scene: Tilemap {} not found.", tilemapPath.generic_string())
			continue;
		}
		tilemap.asset = std::move(resolved);
	}

	std::unordered_map<std::string, shared<Tileset>> tilesetCache;
	const auto resolveTileset = [&](const std::filesystem::path& iPath) -> shared<Tileset> {
		if (iPath.empty())
			return nullptr;
		const std::string key = iPath.generic_string();
		if (const auto cached = tilesetCache.find(key); cached != tilesetCache.end())
			return cached->second;
		auto fresh = mkShared<Tileset>();
		const bool loaded = loadAsset(
				iPath, [&](const std::string_view iYaml) -> bool { return fresh->deserializeFromString(iYaml); },
				[&](const std::filesystem::path& iFullPath) -> bool { return fresh->loadFromFile(iFullPath); });
		if (!loaded) {
			OWL_CORE_WARN("Scene: Tileset {} not found.", key)
			tilesetCache.emplace(key, nullptr);
			return nullptr;
		}
		tilesetCache.emplace(key, fresh);
		return fresh;
	};
	for (const auto view = registry.view<component::Tilemap>(); auto entity: view) {
		auto& tilemap = view.get<component::Tilemap>(entity);
		if (!tilemap.asset || tilemap.asset->tileset || tilemap.asset->tilesetPath.empty())
			continue;
		if (auto resolved = resolveTileset(tilemap.asset->tilesetPath); resolved)
			tilemap.asset->tileset = std::move(resolved);
	}
	std::unordered_map<std::string, shared<renderer::IsometricObjectSet>> objectSetCache;
	const auto resolveObjectSet = [&](const std::filesystem::path& iPath) -> shared<renderer::IsometricObjectSet> {
		const std::string key = iPath.generic_string();
		if (const auto cached = objectSetCache.find(key); cached != objectSetCache.end())
			return cached->second;
		auto fresh = mkShared<renderer::IsometricObjectSet>();
		const auto parse = [&fresh](const std::string_view iText) -> bool {
			auto parsed = renderer::IsometricObjectSet::parse(iText);
			if (!parsed)
				return false;
			*fresh = std::move(*parsed);
			return true;
		};
		const bool loaded = loadAsset(iPath, parse, [&parse](const std::filesystem::path& iFullPath) -> bool {
			return parse(platform::fileToString(iFullPath));
		});
		if (!loaded)
			OWL_CORE_WARN("Scene: Isometric object sheet {} not found or invalid.", key)
		else {
			const std::filesystem::path texturePath = std::filesystem::path{iPath}.replace_extension(".png");
			fresh->setTexture(renderer::gpu::Texture2D::createFromSerializedForDeserialize(
					(texturePath.is_absolute() ? "pat:" : "nam:") + texturePath.generic_string()));
		}
		objectSetCache.emplace(key, fresh);
		return fresh;
	};
	for (const auto view = registry.view<component::IsometricObject>(); const auto entity: view) {
		auto& object = view.get<component::IsometricObject>(entity);
		if (!object.atlas && !object.atlasPath.empty())
			object.atlas = resolveObjectSet(object.atlasPath);
	}
	for (const auto view = registry.view<component::RaycastDoor>(); auto entity: view) {
		auto& door = view.get<component::RaycastDoor>(entity);
		if (door.tileset || door.tilesetPath.empty())
			continue;
		if (auto resolved = resolveTileset(door.tilesetPath); resolved)
			door.tileset = std::move(resolved);
	}
	for (const auto view = registry.view<component::RaycastPushWall>(); auto entity: view) {
		auto& push = view.get<component::RaycastPushWall>(entity);
		if (push.tileset || push.tilesetPath.empty())
			continue;
		if (auto resolved = resolveTileset(push.tilesetPath); resolved)
			push.tileset = std::move(resolved);
	}
	for (const auto view = registry.view<component::VoxelWorld>(); auto entity: view) {
		auto& voxel = view.get<component::VoxelWorld>(entity);
		if (voxel.tileset || voxel.tilesetPath.empty())
			continue;
		if (auto resolved = resolveTileset(voxel.tilesetPath); resolved)
			voxel.tileset = std::move(resolved);
	}

	m_tilemapAssetsDirty = false;
}

void Scene::resolveAllEntityLinks() {
	OWL_PROFILE_FUNCTION()

	std::unordered_map<std::string, entt::entity> tagIndex;
	const auto byName = [this, &tagIndex](const std::string& iName) -> Entity {
		if (tagIndex.empty())
			for (const auto view = registry.view<component::Tag>(); const auto entity: view)
				tagIndex.emplace(view.get<component::Tag>(entity).tag, entity);
		const auto it = tagIndex.find(iName);
		return it != tagIndex.end() ? Entity{it->second, this} : Entity{};
	};
	for (const auto view = registry.view<component::EntityLink>(); const auto entity: view) {
		auto& link = view.get<component::EntityLink>(entity);
		link.linkedEntity = {};
		if (link.linkedEntityId != core::UUID{0}) {
			if (const Entity target = findEntityByUUID(link.linkedEntityId); target) {
				link.linkedEntity = target;
				link.linkedEntityName = target.getName();
				link.wasUnresolvedReported = false;
				continue;
			}
		}
		if (link.linkedEntityName.empty())
			continue;
		if (const Entity target = byName(link.linkedEntityName); target) {
			link.linkedEntity = target;
			link.linkedEntityId = target.getUUID();
			link.wasUnresolvedReported = false;
			continue;
		}
		OWL_CORE_WARN("Scene: Entity link of '{}' targets missing entity '{}', link ignored.",
					  Entity(entity, this).getName(), link.linkedEntityName)
		link.wasUnresolvedReported = true;
	}
}

auto Scene::resolveEntityLink(component::EntityLink& ioLink) const -> bool {
	auto* self = const_cast<Scene*>(this);
	ioLink.linkedEntity = {};
	if (ioLink.linkedEntityId != core::UUID{0}) {
		if (const Entity target = findEntityByUUID(ioLink.linkedEntityId); target) {
			ioLink.linkedEntity = target;
			ioLink.linkedEntityName = target.getName();
			return true;
		}
	}
	if (ioLink.linkedEntityName.empty())
		return false;
	for (const auto view = registry.view<component::Tag>(); const auto entity: view) {
		if (view.get<component::Tag>(entity).tag == ioLink.linkedEntityName) {
			ioLink.linkedEntity = Entity{entity, self};
			ioLink.linkedEntityId = ioLink.linkedEntity.getUUID();
			return true;
		}
	}
	return false;
}

void Scene::assignEntityLinkIds() {
	const auto links = registry.view<component::EntityLink>();
	if (std::ranges::none_of(links, [&links](const entt::entity iEntity) -> bool {
			const auto& link = links.get<component::EntityLink>(iEntity);
			return link.linkedEntityId == core::UUID{0} && !link.linkedEntityName.empty();
		}))
		return;
	std::unordered_map<std::string, core::UUID> tagIndex;
	for (const auto view = registry.view<component::Tag, component::ID>(); const auto entity: view)
		tagIndex.emplace(view.get<component::Tag>(entity).tag, view.get<component::ID>(entity).id);
	for (const auto entity: links) {
		auto& link = links.get<component::EntityLink>(entity);
		if (link.linkedEntityId != core::UUID{0} || link.linkedEntityName.empty())
			continue;
		if (const auto it = tagIndex.find(link.linkedEntityName); it != tagIndex.end())
			link.linkedEntityId = it->second;
	}
}

void Scene::remapEntityLinks(const std::vector<Entity>& iEntities,
							 const std::unordered_map<core::UUID, core::UUID>& iRemap) {
	const auto remapped = [&iRemap](core::UUID& ioId) -> void {
		if (const auto it = iRemap.find(ioId); it != iRemap.end())
			ioId = it->second;
	};
	for (const auto& entity: iEntities) {
		if (!entity)
			continue;
		if (entity.hasComponent<component::EntityLink>()) {
			auto& link = entity.getComponent<component::EntityLink>();
			if (const auto it = iRemap.find(link.linkedEntityId); it != iRemap.end()) {
				link.linkedEntityId = it->second;
				link.linkedEntity = {};
			}
		}
		if (entity.hasComponent<component::UiFocus>()) {
			auto& focus = entity.getComponent<component::UiFocus>();
			for (auto* neighbour: {&focus.up, &focus.down, &focus.left, &focus.right}) remapped(*neighbour);
		}
	}
}

void Scene::onViewportResize(const math::vec2ui& iSize) {

	if (iSize.surface() == 0)
		return;
	m_viewportSize = iSize;
	// Resize our non-FixedAspectRatio cameras
	for (const auto view = registry.view<component::Camera>(); const auto entity: view) {
		if (auto& cameraComponent = view.get<component::Camera>(entity); !cameraComponent.fixedAspectRatio)
			cameraComponent.camera.setViewportSize(iSize);
	}
}

auto Scene::getAllEntities() const -> std::vector<Entity> {
	std::vector<Entity> entities;
	for (auto&& [e]: registry.storage<entt::entity>()->each()) { entities.emplace_back(e, const_cast<Scene*>(this)); }
	// EnTT walks its storage from the newest entity: creation order keeps a saved file in the order it was loaded.
	std::ranges::reverse(entities);
	return entities;
}

auto Scene::duplicateEntity(const Entity& iEntity) -> Entity {
	const std::string name = iEntity.getName();
	Entity newEntity = createEntity(name);
	copyCopiableComponents(newEntity, iEntity);
	// Reset hierarchy: duplicate is always a root entity with no children.
	auto& [parentId, childrenIds] = newEntity.getComponent<component::Hierarchy>();
	parentId = core::UUID{0};
	childrenIds.clear();
	return newEntity;
}

auto Scene::getPrimaryCamera() const -> Entity {
	auto* self = const_cast<Scene*>(this);
	for (const auto view = registry.view<component::Camera>(); const auto entity: view) {
		if (view.get<component::Camera>(entity).primary)
			return Entity{entity, self};
	}
	return {};
}

auto Scene::getPrimaryPlayer() const -> Entity {
	auto* self = const_cast<Scene*>(this);

	if (m_primaryPlayerCache != entt::null && registry.valid(m_primaryPlayerCache)) {
		if (const auto* player = registry.try_get<component::Player>(m_primaryPlayerCache);
			player != nullptr && player->primary)
			return Entity{m_primaryPlayerCache, self};
		// Cached entity is no longer a primary player — fall through to rescan.
		m_primaryPlayerCache = entt::null;
	}
	for (const auto view = registry.view<component::Player>(); const auto entity: view) {
		if (view.get<component::Player>(entity).primary) {
			m_primaryPlayerCache = entity;
			return Entity{entity, self};
		}
	}
	return {};
}

auto Scene::getEntityCount() const -> uint32_t {
	const auto* st = registry.storage<component::ID>();
	if (st == nullptr)
		return 0;
	return static_cast<uint32_t>(st->size());
}

auto Scene::findEntityByUUID(const core::UUID iUuid) const -> Entity {
	auto* self = const_cast<Scene*>(this);
	if (const auto it = m_uuidIndex.find(iUuid); it != m_uuidIndex.end()) {
		if (registry.valid(it->second) && registry.try_get<component::ID>(it->second) != nullptr &&
			registry.get<component::ID>(it->second).id == iUuid)
			return Entity{it->second, self};
		m_uuidIndex.erase(it);
	}
	for (const auto view = registry.view<component::ID>(); const auto entity: view) {
		if (view.get<component::ID>(entity).id == iUuid) {
			m_uuidIndex.insert_or_assign(iUuid, entity);
			return Entity{entity, self};
		}
	}
	return {};
}

auto Scene::getRootEntities() const -> std::vector<Entity> {
	std::vector<Entity> entities;
	for (auto&& [e]: registry.storage<entt::entity>()->each()) {
		if (const Entity entity{e, const_cast<Scene*>(this)};
			entity.getComponent<component::Hierarchy>().parentId == core::UUID{0})
			entities.push_back(entity);
	}
	return entities;
}

auto Scene::getChildren(const Entity& iEntity) const -> std::vector<Entity> {
	std::vector<Entity> children;
	const auto& [parentId, childrenIds] = iEntity.getComponent<component::Hierarchy>();
	children.reserve(childrenIds.size());
	for (const auto childId: childrenIds) {
		if (const Entity child = findEntityByUUID(childId); child)
			children.push_back(child);
	}
	return children;
}

namespace {
// Exact comparison of two transforms (a NaN or a signed zero only costs a recomposition).
auto sameTransform(const math::Transform& iLeft, const math::Transform& iRight) -> bool {
	return iLeft.translation() == iRight.translation() && iLeft.rotation() == iRight.rotation() &&
		   iLeft.scale() == iRight.scale();
}

// Index of an entity in the dense per-entity caches.
auto denseIndex(const entt::entity iHandle) -> size_t { return static_cast<size_t>(entt::to_entity(iHandle)); }

// Own visibility of an entity in game (0) and editor (1) mode; no `Visibility` component means visible.
auto selfVisibility(const entt::storage_for_t<const component::Visibility>* iStorage, const entt::entity iHandle)
		-> std::array<bool, 2> {
	if (iStorage == nullptr || !iStorage->contains(iHandle))
		return {true, true};
	const auto& vis = iStorage->get(iHandle);
	return {vis.gameVisible, vis.editorVisible};
}
}// namespace

auto Scene::parentOf(const entt::entity iEntity, const core::UUID iParentId) const -> entt::entity {
	if (const size_t index = denseIndex(iEntity); index < m_worldNodes.size()) {
		// A live handle keeps its UUID (never rewritten) and its mandatory Transform / Hierarchy.
		if (const auto& node = m_worldNodes[index];
			node.handle == iEntity && node.parentId == iParentId && registry.valid(node.parent))
			return node.parent;
	}
	const Entity parent = findEntityByUUID(iParentId);
	if (!parent || !registry.all_of<component::Transform, component::Hierarchy>(static_cast<entt::entity>(parent)))
		return entt::null;
	return static_cast<entt::entity>(parent);
}

auto Scene::resolveWorldNode(const entt::entity iEntity, const core::UUID iForbiddenAncestor) const -> WorldNode* {
	struct Link {
		entt::entity handle;
		entt::entity parent;
		core::UUID parentId;
	};
	// An entry checked in the current pass is trusted only while nothing can move: inside prepare and the armed tail.
	const bool trustPass = (m_preparingWorlds || m_worldTransformCacheActive) && iForbiddenAncestor == core::UUID{0};
	// Storages fetched once: each registry.get<T>() pays a pool lookup by type.
	const auto* hierarchies = registry.storage<component::Hierarchy>();
	const auto* transforms = registry.storage<component::Transform>();
	if (hierarchies == nullptr || transforms == nullptr)
		return nullptr;
	const auto* visibilities = m_preparingWorlds ? registry.storage<component::Visibility>() : nullptr;
	thread_local std::vector<Link> chain;
	chain.clear();
	const size_t maxChain = registry.storage<entt::entity>()->size();
	entt::entity current = iEntity;
	while (true) {
		const size_t index = denseIndex(current);
		if (index >= m_worldNodes.size())
			m_worldNodes.resize(std::max(index + 1, m_worldNodes.size() * 2));
		if (trustPass) {
			if (const auto& node = m_worldNodes[index]; node.handle == current && node.checkedPass == m_worldPass)
				break;
		}
		if (chain.size() > maxChain) {
			OWL_CORE_WARN("Scene: Circular hierarchy detected above entity {}.",
						  static_cast<uint64_t>(registry.get<component::ID>(iEntity).id))
			return nullptr;
		}
		const core::UUID parentId = hierarchies->get(current).parentId;
		if (parentId == core::UUID{0}) {
			chain.push_back({.handle = current, .parent = entt::null, .parentId = parentId});
			break;
		}
		if (parentId == iForbiddenAncestor)
			return nullptr;
		const entt::entity parent = parentOf(current, parentId);
		chain.push_back({.handle = current, .parent = parent, .parentId = parentId});
		if (parent == entt::null)
			break;
		current = parent;
	}
	// Back down from the top: an entry is recomposed only if its local transform, parent or parent's world changed.
	for (const auto& [handle, parent, parentId]: chain | std::views::reverse) {
		auto& node = m_worldNodes[denseIndex(handle)];
		const WorldNode* parentNode = parent == entt::null ? nullptr : &m_worldNodes[denseIndex(parent)];
		refreshWorldNode(node, handle, parent, parentNode, transforms->get(handle).transform, parentId, visibilities);
		if (m_preparingWorlds && !m_deferWorldSlots && node.slotPass != m_worldPass) {
			node.slot = static_cast<uint32_t>(m_worldMatrices.size());
			node.slotPass = m_worldPass;
			m_worldMatrices.push_back(node.world);
		}
	}
	return &m_worldNodes[denseIndex(iEntity)];
}

auto Scene::composeWorldMatrix(const Entity& iEntity, const core::UUID iForbiddenAncestor) const
		-> std::optional<math::mat4> {
	if (const auto* node = resolveWorldNode(static_cast<entt::entity>(iEntity), iForbiddenAncestor); node != nullptr)
		return node->world;
	return std::nullopt;
}

auto Scene::getWorldTransform(const Entity& iEntity) const -> math::Transform {
	const auto& local = iEntity.getComponent<component::Transform>().transform;
	if (iEntity.getComponent<component::Hierarchy>().parentId == core::UUID{0})
		return local;
	auto* node = resolveWorldNode(static_cast<entt::entity>(iEntity), core::UUID{0});
	// A root's world is its local transform: hand it back as is rather than a decomposition of its matrix.
	if (node == nullptr || node->parent == entt::null)
		return local;
	if (node->decomposedStamp != node->stamp) {
		node->worldTransform = math::Transform{node->world};
		node->decomposedStamp = node->stamp;
	}
	return node->worldTransform;
}

void Scene::refreshWorldNode(WorldNode& ioNode, const entt::entity iHandle, const entt::entity iParent,
							 const WorldNode* iParentNode, const math::Transform& iLocal, const core::UUID iParentId,
							 const entt::storage_for_t<const component::Visibility>* iVisibilities) const {
	const uint64_t parentStamp = iParentNode == nullptr ? 0 : iParentNode->stamp;
	if (ioNode.stamp == 0 || ioNode.handle != iHandle || ioNode.parent != iParent ||
		ioNode.parentStamp != parentStamp || !sameTransform(ioNode.local, iLocal)) {
		if (ioNode.stamp == 0 || ioNode.handle != iHandle || ioNode.parent != iParent)
			std::atomic_ref<bool>{m_worldLayoutDirty}.store(true, std::memory_order_relaxed);
		ioNode.handle = iHandle;
		ioNode.parent = iParent;
		ioNode.local = iLocal;
		ioNode.world = iParentNode == nullptr ? iLocal() : iParentNode->world * iLocal();
		ioNode.parentStamp = parentStamp;
		ioNode.stamp = (static_cast<uint64_t>(denseIndex(iHandle)) << 32U) | ((ioNode.stamp & 0xFFFFFFFFU) + 1U);
		// Loaded first: once set the flag is only read, so the workers do not fight over its cache line.
		if (const std::atomic_ref<bool> moved{m_worldMoved}; !moved.load(std::memory_order_relaxed))
			moved.store(true, std::memory_order_relaxed);
	}
	ioNode.parentId = iParentId;
	ioNode.checkedPass = m_worldPass;
	if (m_preparingWorlds && ioNode.visibilityPass != m_worldPass) {
		// The parent was checked earlier in this prepare, so its visibility is current.
		ioNode.visible = selfVisibility(iVisibilities, iHandle);
		if (iParentNode != nullptr) {
			ioNode.visible[0] = ioNode.visible[0] && iParentNode->visible[0];
			ioNode.visible[1] = ioNode.visible[1] && iParentNode->visible[1];
		}
		ioNode.visibilityPass = m_worldPass;
	}
}

void Scene::prepareWorldTransforms() const {
	OWL_PROFILE_FUNCTION()

	++m_worldPass;
	m_preparingWorlds = true;
	// A scene of a few big subtrees (a wide hierarchy) gains nothing from one task per root.
	if (const auto* hierarchies = registry.storage<component::Hierarchy>();
		mp_taskScheduler != nullptr && hierarchies != nullptr && hierarchies->size() >= m_parallelMinEntities &&
		(m_worldRootCount == 0 || m_worldRootCount >= g_ParallelMinRoots))
		prepareWorldsParallel(*mp_taskScheduler);
	else
		prepareWorldsSerial();
	m_preparingWorlds = false;

	renderer::Renderer2D::setSceneWorlds(m_worldMatrices);
}

void Scene::prepareWorldsSerial() const {
	m_worldMatrices.clear();
	m_worldSlotsValid = false;
	// Each entity is checked once, after its ancestors: slots stay parents-before-children whatever the view order.
	const auto* visibilities = registry.storage<component::Visibility>();
	size_t roots = 0;
	for (const auto [entity, transform, hierarchy]:
		 registry.view<component::Transform, component::Hierarchy>().each()) {
		if (hierarchy.parentId != core::UUID{0}) {
			static_cast<void>(resolveWorldNode(entity, core::UUID{0}));
			continue;
		}
		++roots;
		const size_t index = denseIndex(entity);
		if (index >= m_worldNodes.size())
			m_worldNodes.resize(std::max(index + 1, m_worldNodes.size() * 2));
		auto& node = m_worldNodes[index];
		if (node.handle == entity && node.checkedPass == m_worldPass)
			continue;
		if (node.stamp != 0 && node.handle == entity && node.parent == entt::null &&
			sameTransform(node.local, transform.transform)) {
			// Unchanged root: the common case of a static scene stays free of the general check.
			node.parentId = core::UUID{0};
			node.checkedPass = m_worldPass;
			node.visible = selfVisibility(visibilities, entity);
			node.visibilityPass = m_worldPass;
		} else {
			refreshWorldNode(node, entity, entt::null, nullptr, transform.transform, core::UUID{0}, visibilities);
		}
		node.slot = static_cast<uint32_t>(m_worldMatrices.size());
		node.slotPass = m_worldPass;
		m_worldMatrices.push_back(node.world);
	}
	m_worldRootCount = roots;
}

struct Scene::WorldStorages {
	const entt::storage_for_t<const component::Hierarchy>* hierarchies = nullptr;
	const entt::storage_for_t<const component::Transform>* transforms = nullptr;
	const entt::storage_for_t<const component::ID>* ids = nullptr;
	const entt::storage_for_t<const component::Visibility>* visibilities = nullptr;
};

auto Scene::refreshWorldSubtree(const WorldStorages& iStorages, const entt::entity iRoot,
								std::vector<entt::entity>& ioStack) const -> size_t {
	const auto& [hierarchies, transforms, ids, visibilities] = iStorages;
	refreshWorldNode(m_worldNodes[denseIndex(iRoot)], iRoot, entt::null, nullptr, transforms->get(iRoot).transform,
					 core::UUID{0}, visibilities);
	size_t refreshed = 1;
	const auto& rootChildren = hierarchies->get(iRoot).childrenIds;
	if (rootChildren.empty())
		return refreshed;
	ioStack.clear();
	ioStack.push_back(iRoot);
	while (!ioStack.empty()) {
		const entt::entity parent = ioStack.back();
		ioStack.pop_back();
		const core::UUID parentId = ids->get(parent).id;
		const WorldNode& parentNode = m_worldNodes[denseIndex(parent)];
		for (const core::UUID childId: hierarchies->get(parent).childrenIds) {
			// Read-only lookup: a child the index does not know is left to the serial pass.
			const auto found = m_uuidIndex.find(childId);
			if (found == m_uuidIndex.end())
				continue;
			const entt::entity child = found->second;
			if (!registry.valid(child) || !transforms->contains(child) || !hierarchies->contains(child) ||
				!ids->contains(child) || ids->get(child).id != childId || hierarchies->get(child).parentId != parentId)
				continue;
			auto& node = m_worldNodes[denseIndex(child)];
			if (node.handle == child && node.checkedPass == m_worldPass)
				continue;
			refreshWorldNode(node, child, parent, &parentNode, transforms->get(child).transform, parentId,
							 visibilities);
			ioStack.push_back(child);
			++refreshed;
		}
	}
	return refreshed;
}

void Scene::assignWorldSlots() const {
	thread_local std::vector<entt::entity> chain;
	m_worldSlotOwners.clear();
	for (const auto entity: registry.view<component::Transform, component::Hierarchy>()) {
		if (const auto& first = m_worldNodes[denseIndex(entity)];
			first.handle != entity || first.checkedPass != m_worldPass || first.slotPass == m_worldPass)
			continue;
		chain.clear();
		for (entt::entity current = entity;;) {
			const auto& node = m_worldNodes[denseIndex(current)];
			if (node.slotPass == m_worldPass)
				break;
			chain.push_back(current);
			if (node.parent == entt::null)
				break;
			current = node.parent;
		}
		for (const auto current: chain | std::views::reverse) {
			auto& node = m_worldNodes[denseIndex(current)];
			node.slot = static_cast<uint32_t>(m_worldSlotOwners.size());
			node.slotPass = m_worldPass;
			m_worldSlotOwners.push_back(static_cast<uint32_t>(denseIndex(current)));
		}
	}
}

void Scene::prepareWorldsParallel(core::task::Scheduler& ioScheduler) const {
	const auto view = registry.view<component::Transform, component::Hierarchy>();
	const WorldStorages storages{.hierarchies = registry.storage<component::Hierarchy>(),
								 .transforms = registry.storage<component::Transform>(),
								 .ids = registry.storage<component::ID>(),
								 .visibilities = registry.storage<component::Visibility>()};
	if (const size_t capacity = registry.storage<entt::entity>()->size(); m_worldNodes.size() < capacity)
		m_worldNodes.resize(capacity);
	// A pool reordered without a signal (no construct nor destroy) changes the view order: its ends tell.
	const auto poolSignature = [&storages]() -> std::array<size_t, 8> {
		const auto ends = [](const auto* iStorage) -> std::array<size_t, 4> {
			const size_t size = iStorage->size();
			if (size == 0)
				return {0, 0, 0, 0};
			return {size, static_cast<size_t>(iStorage->data()[0]), static_cast<size_t>(iStorage->data()[size / 2]),
					static_cast<size_t>(iStorage->data()[size - 1])};
		};
		const auto transformEnds = ends(storages.transforms);
		const auto hierarchyEnds = ends(storages.hierarchies);
		return {transformEnds[0], transformEnds[1], transformEnds[2], transformEnds[3],
				hierarchyEnds[0], hierarchyEnds[1], hierarchyEnds[2], hierarchyEnds[3]};
	}();
	if (poolSignature != m_worldPoolSignature) {
		m_worldPoolSignature = poolSignature;
		m_worldLayoutDirty = true;
	}
	const bool slotsKept = m_worldSlotsValid && !m_worldLayoutDirty;
	m_worldLayoutDirty = false;
	m_deferWorldSlots = true;
	// Every entity of the view is counted, and counted again when a root's walk reaches it.
	std::atomic<size_t> expected{0};
	std::atomic<size_t> reached{0};
	std::atomic<size_t> roots{0};
	// One walk stack per chunk, kept by the scene: chunk starts are at least one minimum chunk apart.
	if (const size_t chunks = storages.hierarchies->size() / g_ParallelChunkEntities + 1;
		m_worldWalkStacks.size() < chunks)
		m_worldWalkStacks.resize(chunks);
	ioScheduler.parallelFor(storages.hierarchies->size(), g_ParallelChunkEntities,
							[&](const size_t iBegin, const size_t iEnd) -> void {
								auto& stack = m_worldWalkStacks[iBegin / g_ParallelChunkEntities];
								size_t chunkExpected = 0;
								size_t chunkReached = 0;
								size_t chunkRoots = 0;
								for (const entt::entity* entity = storages.hierarchies->data() + iBegin;
									 entity != storages.hierarchies->data() + iEnd; ++entity) {
									if (!storages.transforms->contains(*entity))
										continue;
									++chunkExpected;
									if (storages.hierarchies->get(*entity).parentId != core::UUID{0})
										continue;
									++chunkRoots;
									chunkReached += refreshWorldSubtree(storages, *entity, stack);
								}
								roots.fetch_add(chunkRoots, std::memory_order_relaxed);
								expected.fetch_add(chunkExpected, std::memory_order_relaxed);
								reached.fetch_add(chunkReached, std::memory_order_relaxed);
							});
	m_worldRootCount = roots.load(std::memory_order_relaxed);
	// Entities no root reached through `childrenIds` (stale child lists, missing parent, loops) take the serial walk.
	const bool complete = reached.load(std::memory_order_relaxed) == expected.load(std::memory_order_relaxed);
	if (!complete) {
		for (const auto entity: view) {
			if (const auto& node = m_worldNodes[denseIndex(entity)];
				node.handle != entity || node.checkedPass != m_worldPass)
				static_cast<void>(resolveWorldNode(entity, core::UUID{0}));
		}
	}
	m_deferWorldSlots = false;
	if (!slotsKept || m_worldLayoutDirty || !complete) {
		assignWorldSlots();
		m_worldSlotsValid = complete;
		m_worldLayoutDirty = false;
	}
	m_worldMatrices.resize(m_worldSlotOwners.size());
	ioScheduler.parallelFor(m_worldSlotOwners.size(), g_ParallelChunkEntities * 4,
							[this](const size_t iBegin, const size_t iEnd) -> void {
								for (size_t slot = iBegin; slot < iEnd; ++slot) {
									auto& node = m_worldNodes[m_worldSlotOwners[slot]];
									node.slotPass = m_worldPass;
									m_worldMatrices[slot] = node.world;
								}
							});
}

auto Scene::getWorldIndex(const Entity& iEntity) const -> uint32_t {
	const auto handle = static_cast<entt::entity>(iEntity);
	if (const size_t index = denseIndex(handle); index < m_worldNodes.size()) {
		if (const auto& node = m_worldNodes[index]; node.handle == handle && node.slotPass == m_worldPass)
			return node.slot;
	}
	return std::numeric_limits<uint32_t>::max();
}

auto Scene::getWorldMatrices() const -> std::span<const math::mat4> { return m_worldMatrices; }

void Scene::setEditorVoxelHighlight(const bool iShow, const math::vec3i& iBlock, const math::vec3i& iNormal) {
	m_hasVoxelHighlight = iShow;
	if (iShow) {
		m_voxelHighlightBlock = iBlock;
		m_voxelHighlightNormal = iNormal;
	}
}

auto Scene::wantsCursorCapture() const -> bool {
	const auto view = registry.view<component::VoxelPlayer>();
	const auto walkers = registry.view<component::FirstPersonController>();
	return std::ranges::any_of(view,
							   [&](const auto iEntity) -> bool {
								   return view.get<component::VoxelPlayer>(iEntity).captureCursor;
							   }) ||
		   std::ranges::any_of(walkers, [&](const auto iEntity) -> bool {
			   const auto& walker = walkers.get<component::FirstPersonController>(iEntity);
			   return walker.enabled && walker.captureCursor;
		   });
}

auto Scene::isEffectivelyVisible(const Entity& iEntity, const bool iEditorMode) const -> bool {
	const size_t mode = iEditorMode ? 1 : 0;
	const auto handle = static_cast<entt::entity>(iEntity);
	// In an update pass, prepareWorldTransforms() has just computed the inherited visibility of every entity.
	if (m_inUpdatePass) {
		if (const size_t index = denseIndex(handle); index < m_worldNodes.size()) {
			if (const auto& node = m_worldNodes[index]; node.handle == handle && node.visibilityPass == m_worldPass)
				return node.visible[mode];
		}
	}
	const auto* visibilities = registry.storage<component::Visibility>();
	const auto* hierarchies = registry.storage<component::Hierarchy>();
	const size_t maxChain = registry.storage<entt::entity>()->size();
	size_t visited = 0;
	auto current = handle;
	while (true) {
		if (!selfVisibility(visibilities, current)[mode])
			return false;
		if (hierarchies == nullptr || !hierarchies->contains(current))
			return true;
		if (++visited > maxChain) {
			OWL_CORE_WARN("Scene: Circular hierarchy detected above entity {}.",
						  static_cast<uint64_t>(iEntity.getUUID()))
			return true;
		}
		const core::UUID parentId = hierarchies->get(current).parentId;
		if (parentId == core::UUID{0})
			return true;
		const entt::entity parent = parentOf(current, parentId);
		if (parent == entt::null)
			return true;
		current = parent;
	}
}

void Scene::setParent(const Entity& iChild, const Entity& iNewParent) const {
	if (!iChild || !iNewParent)
		return;
	if (iChild == iNewParent) {
		OWL_CORE_WARN("setParent: cannot parent entity to itself.")
		return;
	}
	const core::UUID childUuid = iChild.getUUID();
	const auto newParentWorld = composeWorldMatrix(iNewParent, childUuid);
	if (!newParentWorld.has_value()) {
		OWL_CORE_WARN("setParent: circular hierarchy detected, refusing reparent.")
		return;
	}
	const math::mat4 currentWorld =
			composeWorldMatrix(iChild, core::UUID{0}).value_or(iChild.getComponent<component::Transform>().transform());
	auto& [parentId, childrenIds] = iChild.getComponent<component::Hierarchy>();
	// Remove from old parent.
	if (parentId != core::UUID{0}) {
		if (const Entity oldParent = findEntityByUUID(parentId); oldParent) {
			auto& [pid, c_ids] = oldParent.getComponent<component::Hierarchy>();
			std::erase(c_ids, childUuid);
		}
	}
	// Set new parent.
	parentId = iNewParent.getUUID();
	iNewParent.getComponent<component::Hierarchy>().childrenIds.push_back(childUuid);
	// Recompute local transform: local = inverse(newParentWorld) * currentWorld.
	const math::mat4 newLocal = math::inverse(*newParentWorld) * currentWorld;
	iChild.getComponent<component::Transform>().transform = math::Transform{newLocal};
}

void Scene::unparent(const Entity& iChild) const {
	if (!iChild)
		return;
	auto& [parentId, childrenIds] = iChild.getComponent<component::Hierarchy>();
	if (parentId == core::UUID{0})
		return;// Already root.
	// Compute world transform before unparenting.
	const math::Transform currentWorld = getWorldTransform(iChild);
	// Remove from parent's children list.
	if (const Entity parent = findEntityByUUID(parentId); parent) {
		auto& [p_id, c_ids] = parent.getComponent<component::Hierarchy>();
		std::erase(c_ids, iChild.getUUID());
	}
	parentId = core::UUID{0};
	// Store world transform as the new local transform.
	iChild.getComponent<component::Transform>().transform = currentWorld;
}

void Scene::destroyEntityWithChildren(Entity& ioEntity) {// NOLINT(misc-no-recursion)
	// Remove this entity from its parent's children list.
	if (const auto rootParentId = ioEntity.getComponent<component::Hierarchy>().parentId;
		rootParentId != core::UUID{0}) {
		if (const Entity parent = findEntityByUUID(rootParentId); parent)
			std::erase(parent.getComponent<component::Hierarchy>().childrenIds, ioEntity.getUUID());
	}
	// Collect the entire subtree iteratively.
	std::vector<entt::entity> toDestroy;
	std::vector<Entity> queue{ioEntity};
	while (!queue.empty()) {
		const Entity current = queue.back();
		queue.pop_back();
		toDestroy.push_back(static_cast<entt::entity>(current));
		for (const auto childId: current.getComponent<component::Hierarchy>().childrenIds) {
			if (const Entity child = findEntityByUUID(childId); child)
				queue.push_back(child);
		}
	}
	// The whole subtree is still intact when the scripts' `on_destroy` run.
	for (const auto handle: toDestroy) releaseRuntimeResources(Entity{handle, this});
	for (const auto handle: toDestroy) {
		if (m_primaryPlayerCache == handle)
			m_primaryPlayerCache = entt::null;
		if (const auto* idComp = registry.try_get<component::ID>(handle); idComp != nullptr)
			m_uuidIndex.erase(idComp->id);
		registry.destroy(handle);
	}
	ioEntity.m_entityHandle = entt::null;
}

void Scene::destroyEntityDeferred(const Entity& iEntity) {
	if (!iEntity || isPendingDestruction(iEntity))
		return;
	m_pendingDestructions.push_back(iEntity.getUUID());
}

auto Scene::isPendingDestruction(const Entity& iEntity) const -> bool {
	return iEntity && std::ranges::find(m_pendingDestructions, iEntity.getUUID()) != m_pendingDestructions.end();
}

auto Scene::isPendingDestructionInTree(const Entity& iEntity) const -> bool {
	Entity current = iEntity;
	// Bounded by the entity count so a corrupted (cyclic) hierarchy cannot loop forever.
	const size_t maxDepth = registry.view<component::ID>().size();
	for (size_t depth = 0; current && depth <= maxDepth; ++depth) {
		if (isPendingDestruction(current))
			return true;
		const auto parentId = current.getComponent<component::Hierarchy>().parentId;
		if (parentId == core::UUID{0})
			return false;
		current = findEntityByUUID(parentId);
	}
	return false;
}

void Scene::dispatchCollisionEvents() {
	const auto notify = [this](const core::UUID iSelf, const core::UUID iOther) -> void {
		const Entity self = findEntityByUUID(iSelf);
		if (!self || !self.hasComponent<component::LuaScript>() || isPendingDestructionInTree(self) ||
			!isEffectivelyVisible(self, /*iEditorMode=*/false))
			return;
		// Hold the instance itself: the callback may grow component storages and move the component.
		if (const auto* instance = self.getComponent<component::LuaScript>().instance.get();
			instance != nullptr && instance->isValid())
			instance->onCollision(iOther);
	};
	for (const auto& [entityA, entityB]: physics::PhysicCommand::takeCollisionEvents(*this)) {
		notify(entityA, entityB);
		notify(entityB, entityA);
	}
}

void Scene::flushPendingDestructions() {
	while (!m_pendingDestructions.empty()) {
		for (const auto batch = std::exchange(m_pendingDestructions, {}); const auto uuid: batch) {
			const Entity root = findEntityByUUID(uuid);
			if (!root)
				continue;
			for (const auto& member: collectSubtree(*this, root)) releaseRuntimeResources(member);
			if (Entity alive = findEntityByUUID(uuid); alive)
				destroyEntityWithChildren(alive);
		}
	}
}

auto Scene::duplicateSubtree(const Entity& iEntity) -> Entity {
	// Bind name-only links first, so the copies of a linked group link to each other, not to the originals.
	assignEntityLinkIds();
	// Duplicate the root entity.
	Entity newRoot = createEntity(iEntity.getName());
	copyCopiableComponents(newRoot, iEntity);
	// Reset hierarchy for the new root.
	auto& [parentId, childrenIds] = newRoot.getComponent<component::Hierarchy>();
	parentId = core::UUID{0};
	childrenIds.clear();
	std::unordered_map<core::UUID, core::UUID> remap{{iEntity.getUUID(), newRoot.getUUID()}};
	std::vector<Entity> copies{newRoot};
	std::vector<std::pair<Entity, Entity>> stack;
	for (const auto childId: iEntity.getComponent<component::Hierarchy>().childrenIds) {
		if (const Entity srcChild = findEntityByUUID(childId); srcChild)
			stack.emplace_back(srcChild, newRoot);
	}
	while (!stack.empty()) {
		const auto [srcEntity, dstParent] = stack.back();
		stack.pop_back();
		Entity newChild = createEntity(srcEntity.getName());
		copyCopiableComponents(newChild, srcEntity);
		auto& [pid, c_ids] = newChild.getComponent<component::Hierarchy>();
		pid = dstParent.getUUID();
		c_ids.clear();
		dstParent.getComponent<component::Hierarchy>().childrenIds.push_back(newChild.getUUID());
		remap.emplace(srcEntity.getUUID(), newChild.getUUID());
		copies.push_back(newChild);
		// Push source children for further duplication.
		for (const auto grandChildId: srcEntity.getComponent<component::Hierarchy>().childrenIds) {
			if (const Entity srcGrandChild = findEntityByUUID(grandChildId); srcGrandChild)
				stack.emplace_back(srcGrandChild, newChild);
		}
	}
	// Links between members of the subtree follow the copies; links to outside entities are kept.
	remapEntityLinks(copies, remap);
	return newRoot;
}

void Scene::rebuildHierarchyChildren() {
	std::unordered_map<core::UUID, entt::entity> byUuid;
	for (const auto view = registry.view<component::Hierarchy, component::ID>(); const auto entity: view) {
		view.get<component::Hierarchy>(entity).childrenIds.clear();
		byUuid.emplace(view.get<component::ID>(entity).id, entity);
	}
	for (const auto view = registry.view<component::Hierarchy, component::ID>(); const auto entity: view) {
		if (auto& parentId = view.get<component::Hierarchy>(entity).parentId;
			parentId != core::UUID{0} && !byUuid.contains(parentId)) {
			OWL_CORE_WARN("Scene: Parent {} of entity {} not found, entity moved to the root.",
						  static_cast<uint64_t>(parentId), static_cast<uint64_t>(view.get<component::ID>(entity).id))
			parentId = core::UUID{0};
		}
	}
	breakHierarchyCycles(byUuid);
	for (const auto view = registry.view<component::Hierarchy, component::ID>(); const auto entity: view) {
		if (const auto parentId = view.get<component::Hierarchy>(entity).parentId; parentId != core::UUID{0})
			registry.get<component::Hierarchy>(byUuid.at(parentId))
					.childrenIds.push_back(view.get<component::ID>(entity).id);
	}
}

void Scene::breakHierarchyCycles(const std::unordered_map<core::UUID, entt::entity>& iByUuid) {
	enum struct Mark : uint8_t { InProgress, Done };
	std::unordered_map<core::UUID, Mark> marks;
	marks.reserve(iByUuid.size());
	std::vector<core::UUID> path;
	for (const auto& [rootUuid, rootEntity]: iByUuid) {
		path.clear();
		core::UUID current = rootUuid;
		while (current != core::UUID{0} && !marks.contains(current)) {
			marks.emplace(current, Mark::InProgress);
			path.push_back(current);
			current = registry.get<component::Hierarchy>(iByUuid.at(current)).parentId;
		}
		if (current != core::UUID{0} && marks.at(current) == Mark::InProgress) {
			const core::UUID cutAt = path.back();
			OWL_CORE_WARN("Scene: Hierarchy cycle through entity {} broken, entity moved to the root.",
						  static_cast<uint64_t>(cutAt))
			registry.get<component::Hierarchy>(iByUuid.at(cutAt)).parentId = core::UUID{0};
		}
		for (const auto uuid: path) marks.at(uuid) = Mark::Done;
	}
}

template<>
OWL_API void Scene::onComponentAdded<component::ID>([[maybe_unused]] const Entity& iEntity,
													[[maybe_unused]] component::ID& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Tag>([[maybe_unused]] const Entity& iEntity,
													 [[maybe_unused]] component::Tag& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Transform>([[maybe_unused]] const Entity& iEntity,
														   [[maybe_unused]] component::Transform& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Camera>([[maybe_unused]] const Entity& iEntity,
														component::Camera& ioComponent) {
	if (m_viewportSize.surface() > 0)
		ioComponent.camera.setViewportSize(m_viewportSize);
}

template<>
OWL_API void
Scene::onComponentAdded<component::SpriteRenderer>([[maybe_unused]] const Entity& iEntity,
												   [[maybe_unused]] component::SpriteRenderer& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::AnimatedSpriteRenderer>(
		[[maybe_unused]] const Entity& iEntity, [[maybe_unused]] component::AnimatedSpriteRenderer& ioComponent) {}

template<>
OWL_API void
Scene::onComponentAdded<component::CircleRenderer>([[maybe_unused]] const Entity& iEntity,
												   [[maybe_unused]] component::CircleRenderer& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::MeshRenderer>([[maybe_unused]] const Entity& iEntity,
															  [[maybe_unused]] component::MeshRenderer& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Light2D>([[maybe_unused]] const Entity& iEntity,
														 [[maybe_unused]] component::Light2D& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Light>([[maybe_unused]] const Entity& iEntity,
													   [[maybe_unused]] component::Light& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::LightFlicker>([[maybe_unused]] const Entity& iEntity,
															  [[maybe_unused]] component::LightFlicker& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Oscillator>([[maybe_unused]] const Entity& iEntity,
															[[maybe_unused]] component::Oscillator& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Patrol>([[maybe_unused]] const Entity& iEntity,
														[[maybe_unused]] component::Patrol& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Drift>([[maybe_unused]] const Entity& iEntity,
													   [[maybe_unused]] component::Drift& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::FollowTarget>([[maybe_unused]] const Entity& iEntity,
															  [[maybe_unused]] component::FollowTarget& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::RenderTarget>([[maybe_unused]] const Entity& iEntity,
															  [[maybe_unused]] component::RenderTarget& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::ProceduralSky>([[maybe_unused]] const Entity& iEntity,
															   [[maybe_unused]] component::ProceduralSky& ioComponent) {
}

template<>
OWL_API void Scene::onComponentAdded<component::DayNightCycle>([[maybe_unused]] const Entity& iEntity,
															   [[maybe_unused]] component::DayNightCycle& ioComponent) {
}

template<>
OWL_API void Scene::onComponentAdded<component::FirstPersonController>(
		[[maybe_unused]] const Entity& iEntity, [[maybe_unused]] component::FirstPersonController& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Environment>([[maybe_unused]] const Entity& iEntity,
															 [[maybe_unused]] component::Environment& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::NativeScript>([[maybe_unused]] const Entity& iEntity,
															  [[maybe_unused]] component::NativeScript& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Text>([[maybe_unused]] const Entity& iEntity,
													  [[maybe_unused]] component::Text& ioComponent) {
	if (ioComponent.font == nullptr) {
		if (app::Application::instanced()) {
			ioComponent.font = app::Application::get().getFontLibrary().getDefaultFont();
		}
	}
}

template<>
OWL_API void Scene::onComponentAdded<component::PhysicBody>([[maybe_unused]] const Entity& iEntity,
															[[maybe_unused]] component::PhysicBody& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Player>([[maybe_unused]] const Entity& iEntity,
														[[maybe_unused]] component::Player& ioComponent) {
	invalidatePrimaryPlayerCache();
}

template<>
OWL_API void Scene::onComponentAdded<component::Trigger>([[maybe_unused]] const Entity& iEntity,
														 [[maybe_unused]] component::Trigger& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::EntityLink>([[maybe_unused]] const Entity& iEntity,
															[[maybe_unused]] component::EntityLink& ioComponent) {}

template<>
OWL_API void
Scene::onComponentAdded<component::BackgroundTexture>([[maybe_unused]] const Entity& iEntity,
													  [[maybe_unused]] component::BackgroundTexture& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Visibility>([[maybe_unused]] const Entity& iEntity,
															[[maybe_unused]] component::Visibility& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Hierarchy>([[maybe_unused]] const Entity& iEntity,
														   [[maybe_unused]] component::Hierarchy& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::SoundSource>([[maybe_unused]] const Entity& iEntity,
															 [[maybe_unused]] component::SoundSource& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::SoundListener>([[maybe_unused]] const Entity& iEntity,
															   [[maybe_unused]] component::SoundListener& ioComponent) {
}

template<>
OWL_API void Scene::onComponentAdded<component::LuaScript>([[maybe_unused]] const Entity& iEntity,
														   [[maybe_unused]] component::LuaScript& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Canvas>([[maybe_unused]] const Entity& iEntity,
														[[maybe_unused]] component::Canvas& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiRect>([[maybe_unused]] const Entity& iEntity,
														[[maybe_unused]] component::UiRect& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiText>([[maybe_unused]] const Entity& iEntity,
														[[maybe_unused]] component::UiText& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiImage>([[maybe_unused]] const Entity& iEntity,
														 [[maybe_unused]] component::UiImage& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiPanel>([[maybe_unused]] const Entity& iEntity,
														 [[maybe_unused]] component::UiPanel& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiButton>([[maybe_unused]] const Entity& iEntity,
														  [[maybe_unused]] component::UiButton& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiSlider>([[maybe_unused]] const Entity& iEntity,
														  [[maybe_unused]] component::UiSlider& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiProgressBar>([[maybe_unused]] const Entity& iEntity,
															   [[maybe_unused]] component::UiProgressBar& ioComponent) {
}

template<>
OWL_API void Scene::onComponentAdded<component::UiLayout>([[maybe_unused]] const Entity& iEntity,
														  [[maybe_unused]] component::UiLayout& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiWindow>([[maybe_unused]] const Entity& iEntity,
														  [[maybe_unused]] component::UiWindow& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiTooltip>([[maybe_unused]] const Entity& iEntity,
														   [[maybe_unused]] component::UiTooltip& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiToolCursor>([[maybe_unused]] const Entity& iEntity,
															  [[maybe_unused]] component::UiToolCursor& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiFocus>([[maybe_unused]] const Entity& iEntity,
														 [[maybe_unused]] component::UiFocus& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiScrollView>([[maybe_unused]] const Entity& iEntity,
															  [[maybe_unused]] component::UiScrollView& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UiList>([[maybe_unused]] const Entity& iEntity,
														[[maybe_unused]] component::UiList& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::PrefabLink>([[maybe_unused]] const Entity& iEntity,
															[[maybe_unused]] component::PrefabLink& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::RendererTag>([[maybe_unused]] const Entity& iEntity,
															 [[maybe_unused]] component::RendererTag& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Tilemap>([[maybe_unused]] const Entity& iEntity,
														 [[maybe_unused]] component::Tilemap& ioComponent) {
	m_tilemapAssetsDirty = true;
}

template<>
OWL_API void
Scene::onComponentAdded<component::IsometricObject>([[maybe_unused]] const Entity& iEntity,
													[[maybe_unused]] component::IsometricObject& ioComponent) {
	m_tilemapAssetsDirty = true;
}

template<>
OWL_API void Scene::onComponentAdded<component::RaycastDoor>([[maybe_unused]] const Entity& iEntity,
															 [[maybe_unused]] component::RaycastDoor& ioComponent) {
	m_tilemapAssetsDirty = true;
}

template<>
OWL_API void
Scene::onComponentAdded<component::RaycastPushWall>([[maybe_unused]] const Entity& iEntity,
													[[maybe_unused]] component::RaycastPushWall& ioComponent) {
	m_tilemapAssetsDirty = true;
}

template<>
OWL_API void Scene::onComponentAdded<component::VoxelWorld>([[maybe_unused]] const Entity& iEntity,
															[[maybe_unused]] component::VoxelWorld& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::FlyCamera>([[maybe_unused]] const Entity& iEntity,
														   [[maybe_unused]] component::FlyCamera& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::VoxelPlayer>([[maybe_unused]] const Entity& iEntity,
															 [[maybe_unused]] component::VoxelPlayer& ioComponent) {}

}// namespace owl::scene
