/**
 * @file Scene.cpp
 * @author Silmaen
 * @date 22/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/Scene.h"

#include "renderer/BackgroundRenderer.h"
#include "renderer/CameraOrtho.h"
#include "renderer/Renderer.h"
#include "renderer/Renderer2D.h"
#include "renderer/RendererRaycast.h"
#include "renderer/RendererRaycastLayer.h"
#include "renderer/RendererTilemap.h"
#include "renderer/gpu/StorageBuffer.h"
#include "renderer/utils/WorldTransformPass.h"
#include "scene/Entity.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"

#include "app/Application.h"
#include "input/Input.h"
#include "physics/PhysicCommand.h"
#include "scene/ScreenTransition.h"
#include "scene/component/components.h"
#include "script/ScriptEngine.h"
#include "script/ScriptInstance.h"
#include "sound/SoundCommand.h"
#include "sound/SoundSystem.h"

#include <limits>

namespace owl::scene {

namespace {
template<component::isComponent Component>
void copyComponent(entt::registry& oDst, const entt::registry& iSrc,
				   const std::unordered_map<core::UUID, entt::entity>& iEnttMap) {
	for (auto view = iSrc.view<Component>(); auto e: view) {
		const core::UUID uuid = iSrc.get<component::ID>(e).id;
		OWL_CORE_ASSERT(iEnttMap.contains(uuid), "Error: Component not found in map.")
		const entt::entity dstEnttId = iEnttMap.at(uuid);
		auto& component = iSrc.get<Component>(e);
		oDst.emplace_or_replace<Component>(dstEnttId, component);
	}
}

template<typename... Components>
void copyComponentFromTuple(entt::registry& oDst, const entt::registry& iSrc,
							const std::unordered_map<core::UUID, entt::entity>& iEnttMap,
							const std::tuple<Components...>&) {
	(..., copyComponent<Components>(oDst, iSrc, iEnttMap));
}

template<component::isComponent Component>
void copyComponentIfExists(Entity& oDst, const Entity& iSrc) {
	if (iSrc.hasComponent<Component>())
		oDst.addOrReplaceComponent<Component>(iSrc.getComponent<Component>());
}

template<typename... Components>
void copyComponentIfExistsFromTuple(Entity& oDst, const Entity& iSrc, const std::tuple<Components...>&) {
	(..., copyComponentIfExists<Components>(oDst, iSrc));
}

auto getColliderBox(const Entity& iEntity, const math::Transform& iWorldTransform) -> math::box2f {
	auto halfDiag = math::vec2f{iWorldTransform.scale().x() * 0.5f, iWorldTransform.scale().y() * 0.5f};
	if (iEntity.hasComponent<component::PhysicBody>()) {
		auto& [body] = iEntity.getComponent<component::PhysicBody>();
		halfDiag.x() *= body.colliderSize.x();
		halfDiag.y() *= body.colliderSize.y();
	}
	const math::vec2f center = {iWorldTransform.translation().x(), iWorldTransform.translation().y()};
	return {center - halfDiag, center + halfDiag};
}

auto computeTextAspect(const shared<data::fonts::Font>& iFont, const std::string& iText, const float iKerning,
					   const float iLineSpacing) -> float {
	if (!iFont || iText.empty())
		return 1.f;
	math::box2f extents;
	math::vec2 cursor{0.f, 0.f};
	for (size_t ci = 0; ci < iText.size(); ++ci) {
		char ch = iText[ci];
		if (isascii(ch) == 0)
			ch = '?';
		if (ch == '\n') {
			cursor.x() = 0;
			cursor.y() -= iFont->getScaledLineHeight() + iLineSpacing;
			continue;
		}
		if (ch == '\r')
			continue;
		auto [quad, uv] = iFont->getGlyphBox(ch);
		quad.translate(cursor);
		extents.update(quad);
		if (ci < iText.size() - 1) {
			char next = iText[ci + 1];
			if (isascii(next) == 0)
				next = '?';
			cursor.x() += iFont->getAdvance(ch, next) + iKerning;
		}
	}
	const auto diag = extents.diagonal();
	return diag.y() > 0.f ? diag.x() / diag.y() : 1.f;
}

void renderUIChild(const Entity& iChild, const math::Transform& iTransform, const float iPxScaleY) {
	const int entId = static_cast<int>(static_cast<entt::entity>(iChild));
	// Sprite.
	if (iChild.hasComponent<component::SpriteRenderer>()) {
		const auto& sprite = iChild.getComponent<component::SpriteRenderer>();

		renderer::Renderer2D::drawQuad({.transform = iTransform,
										.color = sprite.color,
										.texture = sprite.texture,
										.tilingFactor = sprite.tilingFactor,
										.entityId = entId});
	}
	// Text (world-space component on a UI entity).
	if (iChild.hasComponent<component::Text>()) {
		const auto& [text, font, color, kerning, lineSpacing] = iChild.getComponent<component::Text>();

		renderer::Renderer2D::drawString({.transform = iTransform,
										  .text = text,
										  .font = font,
										  .color = color,
										  .kerning = kerning,
										  .lineSpacing = lineSpacing,
										  .entityId = entId});
	}
	// Circle.
	if (iChild.hasComponent<component::CircleRenderer>()) {
		const auto& [color, thickness, fade] = iChild.getComponent<component::CircleRenderer>();

		renderer::Renderer2D::drawCircle(
				{.transform = iTransform, .color = color, .thickness = thickness, .fade = fade, .entityId = entId});
	}
	// UiPanel.
	if (iChild.hasComponent<component::UiPanel>()) {
		const auto& panel = iChild.getComponent<component::UiPanel>();

		renderer::Renderer2D::drawQuad({.transform = iTransform, .color = panel.backgroundColor, .entityId = entId});
	}
	// UiImage.
	if (iChild.hasComponent<component::UiImage>()) {
		const auto& [texture, tint] = iChild.getComponent<component::UiImage>();

		renderer::Renderer2D::drawQuad({.transform = iTransform, .color = tint, .texture = texture, .entityId = entId});
	}
	// UiText.
	if (iChild.hasComponent<component::UiText>()) {
		const auto& uiText = iChild.getComponent<component::UiText>();
		shared<data::fonts::Font> font = uiText.font;
		if (!font && app::Application::instanced())
			font = app::Application::get().getFontLibrary().getDefaultFont();
		const float textAspect = computeTextAspect(font, uiText.text, uiText.kerning, uiText.lineSpacing);
		math::Transform textTransform;
		textTransform.translation() = iTransform.translation();
		const float baseScale = uiText.fontSize * iPxScaleY;
		textTransform.scale() = {baseScale * textAspect, baseScale, 1.f};

		renderer::Renderer2D::drawString({.transform = textTransform,
										  .text = uiText.text,
										  .font = font,
										  .color = uiText.color,
										  .kerning = uiText.kerning,
										  .lineSpacing = uiText.lineSpacing,
										  .entityId = entId});
	}
	// UiButton.
	if (iChild.hasComponent<component::UiButton>()) {
		const auto& button = iChild.getComponent<component::UiButton>();

		renderer::Renderer2D::drawQuad({.transform = iTransform, .color = button.getCurrentColor(), .entityId = entId});
	}
	// UiProgressBar.
	if (iChild.hasComponent<component::UiProgressBar>()) {
		const auto& [value, backgroundColor, fillColor] = iChild.getComponent<component::UiProgressBar>();
		const float worldWidth = iTransform.scale().x();

		renderer::Renderer2D::drawQuad({.transform = iTransform, .color = backgroundColor, .entityId = entId});
		if (const float fillFraction = std::clamp(value, 0.f, 1.f); fillFraction > 0.f) {
			math::Transform fillTransform = iTransform;
			fillTransform.scale().x() *= fillFraction;
			fillTransform.translation().x() -= worldWidth * (1.f - fillFraction) * 0.5f;

			renderer::Renderer2D::drawQuad({.transform = fillTransform, .color = fillColor, .entityId = entId});
		}
	}
	// UiSlider.
	if (iChild.hasComponent<component::UiSlider>()) {
		const auto& slider = iChild.getComponent<component::UiSlider>();
		const float worldWidth = iTransform.scale().x();
		const float worldHeight = iTransform.scale().y();

		renderer::Renderer2D::drawQuad({.transform = iTransform, .color = slider.trackColor, .entityId = entId});
		const float norm = slider.getNormalized();
		if (norm > 0.f) {
			math::Transform fillTransform = iTransform;
			fillTransform.scale().x() *= norm;
			fillTransform.translation().x() -= worldWidth * (1.f - norm) * 0.5f;

			renderer::Renderer2D::drawQuad({.transform = fillTransform, .color = slider.fillColor, .entityId = entId});
		}
		math::Transform handleTransform = iTransform;
		handleTransform.scale() = {worldHeight, worldHeight, 1.f};
		handleTransform.translation().x() = iTransform.translation().x() - worldWidth * 0.5f + norm * worldWidth;

		renderer::Renderer2D::drawQuad({.transform = handleTransform, .color = slider.handleColor, .entityId = entId});
	}
}

void updateAnimatedSprite(component::AnimatedSpriteRenderer& ioAnim, const core::Timestep& iTimeStep) {
	if (!ioAnim.m_playing || ioAnim.columns == 0 || ioAnim.rows == 0 || ioAnim.frameDuration <= 0.0f)
		return;
	const uint32_t totalFrames = ioAnim.lastFrame >= ioAnim.firstFrame ? ioAnim.lastFrame - ioAnim.firstFrame + 1 : 1;
	float deltaSeconds = iTimeStep.getSeconds();
	if (!ioAnim.speedCurve.empty()) {
		const float progress = totalFrames > 1 ? static_cast<float>(ioAnim.m_currentFrame - ioAnim.firstFrame) /
														 static_cast<float>(totalFrames - 1)
											   : 0.f;
		deltaSeconds *= ioAnim.speedCurve.evaluate(progress);
	}
	ioAnim.m_elapsedTime += deltaSeconds;
	if (ioAnim.m_elapsedTime < ioAnim.frameDuration)
		return;
	const auto framesToAdvance = static_cast<uint32_t>(ioAnim.m_elapsedTime / ioAnim.frameDuration);
	ioAnim.m_elapsedTime -= static_cast<float>(framesToAdvance) * ioAnim.frameDuration;
	if (ioAnim.loop) {
		ioAnim.m_currentFrame =
				ioAnim.firstFrame + (ioAnim.m_currentFrame - ioAnim.firstFrame + framesToAdvance) % totalFrames;
		return;
	}
	const uint32_t newFrame = ioAnim.m_currentFrame + framesToAdvance;
	ioAnim.m_currentFrame = std::min(newFrame, ioAnim.lastFrame);
	if (ioAnim.m_currentFrame >= ioAnim.lastFrame)
		ioAnim.m_playing = false;
}

}// namespace

Scene::Scene() = default;

Scene::~Scene() = default;

auto Scene::copy(const shared<Scene>& iOther) -> shared<Scene> {
	shared<Scene> newScene = mkShared<Scene>();

	newScene->m_viewportSize = iOther->m_viewportSize;
	newScene->m_gameState = iOther->m_gameState;
	newScene->m_enabledRenderers = iOther->m_enabledRenderers;

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
	copyComponentFromTuple(dstSceneRegistry, srcSceneRegistry, enttMap, component::CopiableComponents{});

	return newScene;
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
	auto& [parentId, childrenIds] = ioEntity.getComponent<component::Hierarchy>();
	const core::UUID grandParentId = parentId;
	// Reparent children to this entity's parent (or root if no parent).
	for (const auto childId: childrenIds) {
		if (const Entity child = findEntityByUUID(childId); child) {
			auto& [uuid, ids] = child.getComponent<component::Hierarchy>();
			uuid = grandParentId;
			if (grandParentId != core::UUID{0}) {
				if (const Entity grandParent = findEntityByUUID(grandParentId); grandParent) {
					auto& [pid, v_uuid] = grandParent.getComponent<component::Hierarchy>();
					v_uuid.push_back(childId);
				}
			}
		}
	}
	// Remove this entity from its parent's children list.
	if (grandParentId != core::UUID{0}) {
		if (const Entity parent = findEntityByUUID(grandParentId); parent) {
			auto& [pid, uuids] = parent.getComponent<component::Hierarchy>();
			std::erase(uuids, ioEntity.getUUID());
		}
	}
	if (m_primaryPlayerCache == ioEntity.m_entityHandle)
		m_primaryPlayerCache = entt::null;
	m_uuidIndex.erase(ioEntity.getUUID());
	registry.destroy(ioEntity.m_entityHandle);
	ioEntity.m_entityHandle = entt::null;
}

void Scene::onStartRuntime() {
	OWL_PROFILE_FUNCTION()

	status = Status::Playing;

	using clk = std::chrono::steady_clock;
	const auto runtimeStart = clk::now();
	const auto ms = [](const clk::duration iDur) -> double {
		return std::chrono::duration<double, std::milli>{iDur}.count();
	};
	OWL_CORE_INFO("Scene::onStartRuntime: begin.")

	const auto tilesetStart = clk::now();
	resolveAllTilemapAssets();
	OWL_CORE_INFO("Scene::onStartRuntime: tileset resolve {:.1f} ms.", ms(clk::now() - tilesetStart))

	const auto linksStart = clk::now();
	resolveAllEntityLinks();
	OWL_CORE_INFO("Scene::onStartRuntime: entity-link resolve {:.1f} ms.", ms(clk::now() - linksStart))

	const auto physicsStart = clk::now();
	physics::PhysicCommand::init(this);
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
			const auto soundData = soundLibrary.get(soundComp.soundAsset);
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

	const auto luaStart = clk::now();
	size_t luaCount = 0;
	// Initialize Lua scripting
	script::ScriptEngine::init(this);
	for (const auto view = registry.view<component::LuaScript>(); const auto entity: view) {
		auto& luaScript = view.get<component::LuaScript>(entity);
		if (luaScript.scriptPath.empty())
			continue;
		luaScript.instance = mkUniq<script::ScriptInstance>();
		const auto uuid = static_cast<uint64_t>(registry.get<component::ID>(entity).id);
		bool loaded = false;
		if (app::Application::instanced()) {
			auto& app = app::Application::get();
			// Try pack first.
			if (app.packContains(luaScript.scriptPath))
				if (const auto data = app.loadFromPack(luaScript.scriptPath))
					loaded = luaScript.instance->createFromBuffer(*data, luaScript.scriptPath, uuid);
			// Resolve against asset directories.
			if (!loaded) {
				for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
					if (const auto resolved = assetsPath / luaScript.scriptPath; exists(resolved)) {
						loaded = luaScript.instance->create(resolved.string(), uuid);
						break;
					}
				}
			}
		}
		// Fallback: try raw path (absolute or CWD-relative).
		if (!loaded)
			loaded = luaScript.instance->create(luaScript.scriptPath, uuid);
		if (!loaded)
			OWL_CORE_ERROR("onStartRuntime: FAILED to load script '{}'.", luaScript.scriptPath)
		if (loaded) {
			for (const auto& [name, type, value]: luaScript.properties) {
				switch (type) {
					case script::ScriptPropertyType::Float:
						luaScript.instance->setProperty(name, std::get<float>(value));
						break;
					case script::ScriptPropertyType::Int:
						luaScript.instance->setProperty(name, std::get<int64_t>(value));
						break;
					case script::ScriptPropertyType::String:
						luaScript.instance->setProperty(name, std::get<std::string>(value));
						break;
					case script::ScriptPropertyType::Bool:
						luaScript.instance->setProperty(name, std::get<bool>(value));
						break;
				}
			}
			luaScript.instance->onCreate();
		} else {
			luaScript.instance.reset();
		}
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
	OWL_CORE_INFO("Scene::onStartRuntime: total {:.1f} ms.", ms(clk::now() - runtimeStart))
}

void Scene::onEndRuntime() {
	OWL_PROFILE_FUNCTION()

	// Stop all active sounds (both component-based and Lua-created).
	sound::SoundCommand::stopAll();

	// Destroy Lua scripts
	for (const auto view = registry.view<component::LuaScript>(); const auto entity: view) {
		auto& luaScript = view.get<component::LuaScript>(entity);
		if (luaScript.instance && luaScript.instance->isValid())
			luaScript.instance->onDestroy();
		luaScript.instance.reset();
	}
	script::ScriptEngine::shutdown();

	physics::PhysicCommand::destroy();
	status = Status::Editing;
}

void Scene::onUpdateRuntime(const core::Timestep& iTimeStep, const bool iRender) {
	OWL_PROFILE_FUNCTION()

	m_visibilityCache.clear();
	m_layerContentCacheFirst.clear();
	m_layerContentCacheNotFirst.clear();
	m_inUpdatePass = true;

	// find camera
	renderer::Camera* mainCamera = nullptr;
	math::mat4 cameraTransform;
	math::Transform camTransform;
	entt::entity primaryCameraEntity = entt::null;
	for (const auto view = registry.view<component::Transform, component::Camera>(); const auto entity: view) {
		auto [transform, camera] = view.get<component::Transform, component::Camera>(entity);
		if (camera.primary) {
			mainCamera = &camera.camera;
			primaryCameraEntity = entity;
			const Entity camEntity{entity, this};
			camTransform = getWorldTransform(camEntity);
			cameraTransform = camTransform();
			break;
		}
	}
	if (status == Status::Victory) {
		if (iRender && mainCamera != nullptr) {
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
		if (iRender && mainCamera != nullptr) {
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
	registry.view<component::NativeScript>().each([iTimeStep, this](auto ioEntity, auto& ioNsc) -> auto {
		if (!isEffectivelyVisible(Entity{ioEntity, this}, /*iEditorMode=*/false))
			return;
		if (!ioNsc.instance) {
			ioNsc.instance = ioNsc.instantiateScript();
			ioNsc.instance->entity = Entity{ioEntity, this};
			ioNsc.instance->onCreate();
		}
		ioNsc.instance->onUpdate(iTimeStep);
	});

	for (const auto view = registry.view<component::LuaScript>(); const auto entity: view) {
		if (!isEffectivelyVisible(Entity{entity, this}, /*iEditorMode=*/false))
			continue;
		if (auto& luaScript = view.get<component::LuaScript>(entity);
			luaScript.instance && luaScript.instance->isValid())
			luaScript.instance->onUpdate(iTimeStep.getSeconds());
	}

	updateRaycastDynamicWalls(iTimeStep.getSeconds());

	// Inputs
	if (const Entity player = getPrimaryPlayer()) {
		auto& [primary, iplayer] = player.getComponent<component::Player>();
		iplayer.parseInputs(player);
	}

	// Physics
	physics::PhysicCommand::frame(iTimeStep);

	updateEntityLinks();
	m_worldTransformCache.clear();
	m_worldTransformCacheActive = true;
	prepareWorldTransforms();
	renderer::Renderer2D::setSceneWorldsBuffer(getWorldsBuffer());
	// Sound: update listener and spatial source positions
	for (const auto view = registry.view<component::Transform, component::SoundListener>(); const auto entity: view) {
		if (const auto& [transform, listener] = view.get<component::Transform, component::SoundListener>(entity);
			listener.primary) {
			const Entity ent{entity, this};
			const auto wt = getWorldTransform(ent);

			sound::SoundCommand::setListenerPosition(
					{wt.translation().x(), wt.translation().y(), wt.translation().z()});
			const float rotZ = wt.rotation().z();

			sound::SoundCommand::setListenerOrientation({std::sin(rotZ), std::cos(rotZ), 0.0f}, {0.0f, 0.0f, 1.0f});
			break;
		}
	}
	for (const auto view = registry.view<component::Transform, component::SoundSource>(); const auto entity: view) {
		auto& [soundComp] = view.get<component::SoundSource>(entity);
		if (soundComp.runtimeHandle == sound::invalidSoundHandle || !soundComp.spatial)
			continue;
		const Entity ent{entity, this};
		const auto wt = getWorldTransform(ent);

		sound::SoundCommand::setPosition(soundComp.runtimeHandle,
										 {wt.translation().x(), wt.translation().y(), wt.translation().z()});
	}

	{
		auto player = getPrimaryPlayer();
		for (const auto view = registry.view<component::Trigger>(); const auto ent: view) {
			const Entity entity{ent, this};
			auto& trigger = entity.getComponent<component::Trigger>().trigger;
			if (!isEffectivelyVisible(entity, /*iEditorMode=*/false)) {
				// Hidden trigger: cancel any in-progress timer and clear overlap state.
				if (trigger.type == SceneTrigger::TriggerType::Timer)
					trigger.stopTimer();
				if (trigger.wasOverlapping() && player)
					trigger.onTriggerExit(player, entity);
				trigger.setOverlapping(false);
				continue;
			}
			// Timer triggers: update independently of overlap.
			if (trigger.type == SceneTrigger::TriggerType::Timer) {
				trigger.updateTimer(iTimeStep.getSeconds(), entity);
				continue;
			}
			// Overlap-based triggers.
			if (!player)
				continue;
			const bool overlapping = getColliderBox(entity, getWorldTransform(entity))
											 .intersect(getColliderBox(player, getWorldTransform(player)));
			if (overlapping && !trigger.wasOverlapping())
				trigger.onTriggerEnter(player, entity);
			if (overlapping)
				trigger.onTriggered(player, entity);
			if (!overlapping && trigger.wasOverlapping())
				trigger.onTriggerExit(player, entity);
			trigger.setOverlapping(overlapping);
		}
	}
	// Update animated sprites
	for (const auto view = registry.view<component::AnimatedSpriteRenderer>(); const auto entity: view)

		updateAnimatedSprite(view.get<component::AnimatedSpriteRenderer>(entity), iTimeStep);

	// Render 2D
	if (iRender && mainCamera != nullptr) {
		if (primaryCameraEntity != entt::null) {
			const Entity camEntity{primaryCameraEntity, this};
			camTransform = getWorldTransform(camEntity);
			cameraTransform = camTransform();
		}
		mainCamera->setTransform(cameraTransform);
		// Compute inverse(projection * viewRotation) for skybox (includes FOV/aspect ratio)
		math::mat4 viewRotation = mainCamera->getView();

		viewRotation(0, 3) = 0.0f;

		viewRotation(1, 3) = 0.0f;

		viewRotation(2, 3) = 0.0f;
		m_inverseViewRotation = inverse(mainCamera->getProjection() * viewRotation);

		renderWithStack(*mainCamera);

		ScreenTransition::update(iTimeStep.getSeconds());

		ScreenTransition::render(static_cast<float>(m_viewportSize.x()), static_cast<float>(m_viewportSize.y()));
	}
	// Disarm per-pass caches; the next tick repopulates from scratch.
	m_inUpdatePass = false;
	m_worldTransformCacheActive = false;
}

void Scene::onRenderRuntime() {
	OWL_PROFILE_FUNCTION()

	m_worldTransformCache.clear();
	m_worldTransformCacheActive = true;
	prepareWorldTransforms();
	renderer::Renderer2D::setSceneWorldsBuffer(getWorldsBuffer());

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
		renderWithStack(*mainCamera);
		ScreenTransition::render(static_cast<float>(m_viewportSize.x()), static_cast<float>(m_viewportSize.y()));
	}
	m_worldTransformCacheActive = false;
}

void Scene::onUpdateEditor([[maybe_unused]] const core::Timestep& iTimeStep, const renderer::Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	m_visibilityCache.clear();
	m_layerContentCacheFirst.clear();
	m_layerContentCacheNotFirst.clear();
	m_worldTransformCache.clear();
	m_inUpdatePass = true;
	m_worldTransformCacheActive = true;
	prepareWorldTransforms();
	renderer::Renderer2D::setSceneWorldsBuffer(getWorldsBuffer());

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

auto Scene::layerAccepts(const Entity& iEntity) const -> bool {
	if (m_currentLayerName.empty())
		return true;// legacy single-pass — every entity is drawn.
	if (!iEntity.hasComponent<component::RendererTag>())
		return m_currentLayerIsFirst;// untagged entities default to the first layer.
	return iEntity.getComponent<component::RendererTag>().rendererName == m_currentLayerName;
}

auto Scene::layerHasContent(const std::string& iLayerName, const bool iIsFirst) const -> bool {
	OWL_PROFILE_FUNCTION()

	auto& cache = iIsFirst ? m_layerContentCacheFirst : m_layerContentCacheNotFirst;
	if (m_inUpdatePass) {
		if (const auto it = cache.find(iLayerName); it != cache.end())
			return it->second;
	}

	auto* self = const_cast<Scene*>(this);// NOLINT(cppcoreguidelines-pro-type-const-cast)
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
		for (const auto e: registry.view<component::Canvas>())
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

	const auto& stack = renderer::Renderer::getRenderStack();
	if (const bool editorMode = (status == Status::Editing); editorMode || stack.isEmpty()) {
		renderer::Renderer2D::resetStats();
		renderer::Renderer2D::beginScene(iCamera);
		m_currentLayerName.clear();
		m_currentLayerIsFirst = true;
		mp_currentLayer = nullptr;
		render();
		renderUI(iCamera.getViewProjection());
		renderer::Renderer2D::endScene();
		return;
	}
	renderer::Renderer2D::resetStats();
	bool first = true;
	for (const auto& layer: stack.getLayers()) {
		const bool isFirst = first;
		first = false;
		if (!layerHasContent(layer->getName(), isFirst))
			continue;
		layer->setViewport(m_viewportSize);
		layer->onBeginFrame(iCamera);
		m_currentLayerName = layer->getName();
		m_currentLayerIsFirst = isFirst;
		mp_currentLayer = layer.get();
		render();
		renderUI(layer->getEffectiveViewProjection(iCamera));
		layer->onEndFrame();
	}
	m_currentLayerName.clear();
	m_currentLayerIsFirst = true;
	mp_currentLayer = nullptr;
}

void Scene::render() {
	OWL_PROFILE_FUNCTION()

	const bool editorMode = status == Status::Editing;
	const bool raycastLayer =
			mp_currentLayer != nullptr && std::string_view{mp_currentLayer->getTypeKey()} == "RendererRaycast";

	if (m_currentLayerIsFirst && !raycastLayer) {
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

	resolveAllTilemapAssets();
	renderTilemaps(editorMode, raycastLayer);

	if (raycastLayer) {
		renderRaycastDynamicWalls(editorMode);
		renderRaycastSprites(editorMode);
		return;
	}

	// Draw sprites
	for (const auto group = registry.group<component::Transform>(entt::get<component::SpriteRenderer>);
		 auto entity: group) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		auto [transform, sprite] = group.get<component::Transform, component::SpriteRenderer>(entity);
		const auto worldIndex = static_cast<int32_t>(getWorldIndex(ent));

		renderer::Renderer2D::drawQuad({.transform = worldIndex < 0 ? getWorldTransform(ent) : math::Transform{},
										.worldIndex = worldIndex,
										.color = sprite.color,
										.texture = sprite.texture,
										.tilingFactor = sprite.tilingFactor,
										.entityId = static_cast<int>(entity)});
	}
	// Draw animated sprites
	for (const auto view = registry.view<component::Transform, component::AnimatedSpriteRenderer>();
		 auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		auto [transform, anim] = view.get<component::Transform, component::AnimatedSpriteRenderer>(entity);
		const auto worldIndex = static_cast<int32_t>(getWorldIndex(ent));
		const uint32_t safeCols = std::max(anim.columns, 1u);
		const uint32_t safeRows = std::max(anim.rows, 1u);
		const uint32_t frame = std::clamp(anim.m_currentFrame, anim.firstFrame, anim.lastFrame);
		const uint32_t col = frame % safeCols;
		const uint32_t row = frame / safeCols;
		const float uMin = static_cast<float>(col) / static_cast<float>(safeCols);
		const float uMax = static_cast<float>(col + 1) / static_cast<float>(safeCols);
		const float vMax = 1.0f - static_cast<float>(row) / static_cast<float>(safeRows);
		const float vMin = 1.0f - static_cast<float>(row + 1) / static_cast<float>(safeRows);

		renderer::Renderer2D::drawQuad({.transform = worldIndex < 0 ? getWorldTransform(ent) : math::Transform{},
										.worldIndex = worldIndex,
										.color = anim.color,
										.texture = anim.texture,
										.textureCoords = {math::vec2{uMin, vMin}, math::vec2{uMax, vMin},
														  math::vec2{uMax, vMax}, math::vec2{uMin, vMax}},
										.entityId = static_cast<int>(entity)});
	}
	// Draw circles
	for (const auto view = registry.view<component::Transform, component::CircleRenderer>(); auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		auto [transform, circle] = view.get<component::Transform, component::CircleRenderer>(entity);
		const auto worldIndex = static_cast<int32_t>(getWorldIndex(ent));

		renderer::Renderer2D::drawCircle({.transform = worldIndex < 0 ? getWorldTransform(ent) : math::Transform{},
										  .worldIndex = worldIndex,
										  .color = circle.color,
										  .thickness = circle.thickness,
										  .fade = circle.fade,
										  .entityId = static_cast<int>(entity)});
	}
	// drawString always uses the transient buffer (per-glyph local matrices), so worldIndex is left default.
	for (const auto view = registry.view<component::Transform, component::Text>(); auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		auto [transform, text] = view.get<component::Transform, component::Text>(entity);
		const math::Transform worldTransform = getWorldTransform(ent);

		renderer::Renderer2D::drawString({.transform = worldTransform,
										  .text = text.text,
										  .font = text.font,
										  .color = text.color,
										  .kerning = text.kerning,
										  .lineSpacing = text.lineSpacing,
										  .entityId = static_cast<int>(entity)});
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

void Scene::renderTilemaps(const bool iEditorMode, const bool iRaycastLayer) {
	OWL_PROFILE_FUNCTION()

	for (const auto view = registry.view<component::Transform, component::Tilemap>(); auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, iEditorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		auto& tilemap = view.get<component::Tilemap>(entity);
		if (!tilemap.asset || !tilemap.asset->tileset || !tilemap.asset->tileset->texture)
			continue;
		const auto& assetData = *tilemap.asset;
		const math::Transform worldTransform = getWorldTransform(ent);
		const int entityId = static_cast<int>(entity);
		if (iRaycastLayer) {
			renderer::RendererRaycast::drawTilemapWalls(assetData, worldTransform, entityId);
			continue;
		}
		renderer::RendererTilemap::drawTilemap(assetData, worldTransform, entityId);
	}
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
}

void Scene::updateRaycastDynamicWalls(const float iTimeStep) {
	OWL_PROFILE_FUNCTION()

	// Find primary player position once (used for built-in proximity activation).
	math::vec2 playerWorldXY{0.f, 0.f};
	bool hasPlayer = false;
	if (const Entity player = getPrimaryPlayer()) {
		const auto wt = getWorldTransform(player);
		playerWorldXY = {wt.translation().x(), wt.translation().y()};
		hasPlayer = true;
	}

	for (const auto view = registry.view<component::Transform, component::RaycastDoor>(); const auto entity: view) {
		const Entity ent{entity, this};
		auto& door = view.get<component::RaycastDoor>(entity);
		auto& [transform] = view.get<component::Transform>(entity);

		// Built-in activation (proximity + key edge). `interactionKey == 0` disables.
		if (door.state == component::RaycastDoor::State::Idle && door.interactionKey != 0 && hasPlayer) {
			const float dx = playerWorldXY.x() - transform.translation().x();
			const float dy = playerWorldXY.y() - transform.translation().y();
			const float distSq = dx * dx + dy * dy;
			const bool keyHeld = input::Input::isKeyPressed(door.interactionKey);
			const bool keyEdge = keyHeld && !door.keyHeldLastTick;
			if (keyEdge && distSq <= door.interactionRange * door.interactionRange)
				door.state = component::RaycastDoor::State::Opening;
			door.keyHeldLastTick = keyHeld;
		} else if (door.interactionKey != 0) {
			door.keyHeldLastTick = input::Input::isKeyPressed(door.interactionKey);
		}

		const float prevOffset = door.currentOffset;
		switch (door.state) {
			case component::RaycastDoor::State::Idle:
				break;
			case component::RaycastDoor::State::Opening:
				door.currentOffset += std::max(0.f, door.slideSpeed) * iTimeStep;
				if (door.currentOffset >= 1.f) {
					door.currentOffset = 1.f;
					door.state = component::RaycastDoor::State::Open;
					door.holdTimer = std::max(0.f, door.holdTime);
				}
				break;
			case component::RaycastDoor::State::Open:
				door.holdTimer -= iTimeStep;
				if (door.holdTimer <= 0.f)
					door.state = component::RaycastDoor::State::Closing;
				break;
			case component::RaycastDoor::State::Closing:
				door.currentOffset -= std::max(0.f, door.closeSpeed) * iTimeStep;
				if (door.currentOffset <= 0.f) {
					door.currentOffset = 0.f;
					door.state = component::RaycastDoor::State::Idle;
				}
				break;
		}
		const float delta = door.currentOffset - prevOffset;
		if (std::abs(delta) > 1e-6f) {
			using OD = component::RaycastDoor::OpeningDirection;
			float dx = 0.f;
			float dy = 0.f;
			switch (door.openingDirection) {
				case OD::East:
					dx = 1.f;
					break;
				case OD::West:
					dx = -1.f;
					break;
				case OD::North:
					dy = 1.f;
					break;
				case OD::South:
					dy = -1.f;
					break;
			}
			const math::Transform wt = getWorldTransform(ent);
			const float plateX = wt.translation().x() + dx * door.currentOffset;
			const float plateY = wt.translation().y() + dy * door.currentOffset;
			physics::PhysicCommand::setTransform(ent, math::vec2f{plateX, plateY}, wt.rotation().z());
		}
	}

	// Pushwalls — Idle → Moving → Final, one-shot.
	for (const auto view = registry.view<component::Transform, component::RaycastPushWall>(); const auto entity: view) {
		const Entity ent{entity, this};
		auto& push = view.get<component::RaycastPushWall>(entity);
		auto& [transform] = view.get<component::Transform>(entity);

		if (push.state == component::RaycastPushWall::State::Idle && push.interactionKey != 0 && hasPlayer) {
			const float dx = playerWorldXY.x() - transform.translation().x();
			const float dy = playerWorldXY.y() - transform.translation().y();
			const float distSq = dx * dx + dy * dy;
			const bool keyHeld = input::Input::isKeyPressed(push.interactionKey);
			if (const bool keyEdge = keyHeld && !push.keyHeldLastTick;
				keyEdge && distSq <= push.interactionRange * push.interactionRange)
				push.state = component::RaycastPushWall::State::Moving;
			push.keyHeldLastTick = keyHeld;
		} else if (push.interactionKey != 0) {
			push.keyHeldLastTick = input::Input::isKeyPressed(push.interactionKey);
		}

		const float prevOffset = push.currentOffset;
		if (push.state == component::RaycastPushWall::State::Moving) {
			push.currentOffset += std::max(0.f, push.slideSpeed) * iTimeStep;
			if (push.currentOffset >= push.slideDistance) {
				push.currentOffset = push.slideDistance;
				push.state = component::RaycastPushWall::State::Final;
			}
		}
		if (const float delta = push.currentOffset - prevOffset; std::abs(delta) > 1e-6f) {
			transform.translation().x() += push.slideDirection.x() * delta;
			transform.translation().y() += push.slideDirection.y() * delta;
			const math::Transform wt = getWorldTransform(ent);
			physics::PhysicCommand::setTransform(ent, math::vec2f{wt.translation().x(), wt.translation().y()},
												 wt.rotation().z());
		}
	}
}

void Scene::resolveAllTilemapAssets() {
	if (!m_tilemapAssetsDirty)
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
		auto& [tilemapPath, asset] = view.get<component::Tilemap>(entity);
		if (asset || tilemapPath.empty())
			continue;
		auto resolved = mkShared<TilemapAsset>();
		const bool loaded = loadAsset(
				tilemapPath,
				[&](const std::string_view iYaml) -> bool { return resolved->deserializeFromString(iYaml); },
				[&](const std::filesystem::path& iPath) -> bool { return resolved->loadFromFile(iPath); });
		if (loaded)
			asset = std::move(resolved);
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
		if (!loaded)
			return nullptr;
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

	m_tilemapAssetsDirty = false;
}

void Scene::resolveAllEntityLinks() {
	OWL_PROFILE_FUNCTION()

	std::unordered_map<std::string, entt::entity> tagIndex;
	for (const auto view = registry.view<component::Tag>(); const auto entity: view)
		tagIndex.emplace(view.get<component::Tag>(entity).tag, entity);
	for (const auto view = registry.view<component::EntityLink>(); const auto entity: view) {
		auto& link = view.get<component::EntityLink>(entity);
		if (link.linkedEntityName.empty()) {
			link.linkedEntity = {};
			continue;
		}
		if (const auto it = tagIndex.find(link.linkedEntityName); it != tagIndex.end())
			link.linkedEntity = Entity{it->second, this};
		else
			link.linkedEntity = {};
	}
}

void Scene::updateEntityLinks() {
	OWL_PROFILE_FUNCTION()

	const auto rescanTag = [this](component::EntityLink& ioLink) -> void {
		for (const auto view = registry.view<component::Tag>(); const auto entity: view) {
			if (view.get<component::Tag>(entity).tag == ioLink.linkedEntityName) {
				ioLink.linkedEntity = {entity, this};
				return;
			}
		}
	};
	const auto applyLocalFromWorld = [this](component::Transform& ioTransform, const Entity& iHost,
											const math::Transform& iLinkedWorld) -> void {
		const auto& [parentId, childrenIds] = iHost.getComponent<component::Hierarchy>();
		if (parentId == core::UUID{0}) {
			ioTransform.transform.translation() = iLinkedWorld.translation();
			return;
		}
		const Entity parent = findEntityByUUID(parentId);
		if (!parent) {
			ioTransform.transform.translation() = iLinkedWorld.translation();
			return;
		}
		const math::mat4 parentWorldInv = math::inverse(getWorldTransform(parent)());
		const math::vec4 localPos =
				parentWorldInv * math::vec4{iLinkedWorld.translation().x(), iLinkedWorld.translation().y(),
											iLinkedWorld.translation().z(), 1.0f};
		ioTransform.transform.translation().x() = localPos.x();
		ioTransform.transform.translation().y() = localPos.y();
		ioTransform.transform.translation().z() = localPos.z();
	};
	for (const auto view = registry.view<component::Transform, component::EntityLink>(); const auto entity: view) {
		const Entity host{entity, this};
		if (!isEffectivelyVisible(host, /*iEditorMode=*/false))
			continue;
		auto [transform, link] = view.get<component::Transform, component::EntityLink>(entity);
		if (!link.linkedEntity || link.linkedEntity.getComponent<component::Tag>().tag != link.linkedEntityName)
			rescanTag(link);
		applyLocalFromWorld(transform, host, getWorldTransform(link.linkedEntity));
	}
}

void Scene::renderUI(const math::mat4& iEffectiveViewProjection) {
	OWL_PROFILE_FUNCTION()

	if (m_viewportSize.x() == 0 || m_viewportSize.y() == 0)
		return;

	const bool editorMode = status == Status::Editing;
	const auto vpWidth = static_cast<float>(m_viewportSize.x());
	const auto vpHeight = static_cast<float>(m_viewportSize.y());

	struct CanvasEntry {
		entt::entity entity;
		int32_t sortOrder;
	};
	thread_local std::vector<CanvasEntry> canvases;
	canvases.clear();
	for (const auto view = registry.view<component::Canvas>(); const auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		canvases.push_back({entity, view.get<component::Canvas>(entity).sortOrder});
	}
	if (canvases.empty())
		return;
	std::ranges::sort(canvases, [](const auto& iA, const auto& iB) -> auto { return iA.sortOrder < iB.sortOrder; });

	const math::mat4 invVP = math::inverse(iEffectiveViewProjection);
	const math::vec4 cornerA4 = invVP * math::vec4{-1.f, -1.f, 0.f, 1.f};
	const math::vec4 cornerB4 = invVP * math::vec4{1.f, 1.f, 0.f, 1.f};
	const math::vec2 cornerA = {cornerA4.x() / cornerA4.w(), cornerA4.y() / cornerA4.w()};
	const math::vec2 cornerB = {cornerB4.x() / cornerB4.w(), cornerB4.y() / cornerB4.w()};
	const math::vec2 worldMin = {std::min(cornerA.x(), cornerB.x()), std::min(cornerA.y(), cornerB.y())};
	const math::vec2 worldMax = {std::max(cornerA.x(), cornerB.x()), std::max(cornerA.y(), cornerB.y())};
	const float worldW = worldMax.x() - worldMin.x();
	const float worldH = worldMax.y() - worldMin.y();
	// Pixel (0,0)=bottom-left, (vpW,vpH)=top-right → world (worldMin → worldMax).
	const auto pixelToWorld = [&](const float iPx, const float iPy) -> math::vec2 {
		return {worldMin.x() + (iPx / vpWidth) * worldW, worldMin.y() + (iPy / vpHeight) * worldH};
	};
	const float pxScaleX = worldW / vpWidth;
	const float pxScaleY = worldH / vpHeight;

	for (const auto& [canvasEnt, sortOrder]: canvases) {
		const Entity canvasEntity{canvasEnt, this};
		const math::vec2 canvasSize = {vpWidth, vpHeight};

		// Render children of the Canvas that have UiRect + visual components.
		for (const auto& [parentId, childrenIds] = canvasEntity.getComponent<component::Hierarchy>();
			 const auto childId: childrenIds) {
			const Entity child = findEntityByUUID(childId);
			if (!child || !child.hasComponent<component::UiRect>())
				continue;
			if (!isEffectivelyVisible(child, editorMode))
				continue;

			const auto& rect = child.getComponent<component::UiRect>();
			const math::vec2 posPx = rect.computePosition(canvasSize);
			const math::vec2 posWorld = pixelToWorld(posPx.x(), posPx.y());

			math::Transform uiTransform;
			uiTransform.translation() = {posWorld.x(), posWorld.y(), 0.5f};
			uiTransform.scale() = {rect.size.x() * pxScaleX, rect.size.y() * pxScaleY, 1.f};

			renderUIChild(child, uiTransform, pxScaleY);
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
	for (auto&& [e]: registry.storage<entt::entity>()->each()) {
		entities.emplace_back(e, const_cast<Scene*>(this));
	}// NOLINT(cppcoreguidelines-pro-type-const-cast)
	return entities;
}

auto Scene::duplicateEntity(const Entity& iEntity) -> Entity {
	const std::string name = iEntity.getName();
	Entity newEntity = createEntity(name);
	copyComponentIfExistsFromTuple(newEntity, iEntity, component::CopiableComponents{});
	// Reset hierarchy: duplicate is always a root entity with no children.
	auto& [parentId, childrenIds] = newEntity.getComponent<component::Hierarchy>();
	parentId = core::UUID{0};
	childrenIds.clear();
	return newEntity;
}

auto Scene::getPrimaryCamera() const -> Entity {
	auto* self = const_cast<Scene*>(this);// NOLINT(cppcoreguidelines-pro-type-const-cast)
	for (const auto view = registry.view<component::Camera>(); const auto entity: view) {
		if (view.get<component::Camera>(entity).primary)
			return Entity{entity, self};
	}
	return {};
}

auto Scene::getPrimaryPlayer() const -> Entity {
	auto* self = const_cast<Scene*>(this);// NOLINT(cppcoreguidelines-pro-type-const-cast)

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
	const auto* st = registry.storage<Entity>();
	if (st == nullptr)
		return 0;
	return static_cast<uint32_t>(st->size());
}

auto Scene::findEntityByUUID(const core::UUID iUuid) const -> Entity {
	auto* self = const_cast<Scene*>(this);// NOLINT(cppcoreguidelines-pro-type-const-cast)
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

auto Scene::getWorldTransform(const Entity& iEntity) const -> math::Transform {
	const auto handle = static_cast<entt::entity>(iEntity);
	if (m_worldTransformCacheActive) {
		if (const auto it = m_worldTransformCache.find(handle); it != m_worldTransformCache.end())
			return it->second;
	}
	constexpr uint32_t maxDepth = 64;
	const auto& localTransform = iEntity.getComponent<component::Transform>().transform;
	const auto& [parentId, childrenIds] = iEntity.getComponent<component::Hierarchy>();
	const auto compute = [&]() -> math::Transform {
		if (parentId == core::UUID{0})
			return localTransform;
		// Walk the parent chain, collecting transforms.
		math::mat4 worldMat = localTransform();
		core::UUID currentParentId = parentId;
		uint32_t depth = 0;
		while (currentParentId != core::UUID{0} && depth < maxDepth) {
			const Entity parent = findEntityByUUID(currentParentId);
			if (!parent)
				break;
			const auto& parentTransform = parent.getComponent<component::Transform>().transform;
			worldMat = parentTransform() * worldMat;
			currentParentId = parent.getComponent<component::Hierarchy>().parentId;
			++depth;
		}
		if (depth >= maxDepth)
			OWL_CORE_WARN("getWorldTransform: depth limit reached, possible circular hierarchy.")
		return math::Transform{worldMat};
	};
	const math::Transform result = compute();
	if (m_worldTransformCacheActive)
		m_worldTransformCache.emplace(handle, result);
	return result;
}

void Scene::prepareWorldTransforms() const {
	OWL_PROFILE_FUNCTION()

	if (!mp_worldTransformPass) {
		mp_worldTransformPass = mkUniq<renderer::utils::WorldTransformPass>();
		mp_worldTransformPass->init();
	}

	m_entityToWorldIndex.clear();

	thread_local std::vector<renderer::utils::WorldTransformPass::Entry> entries;
	thread_local std::vector<math::mat4> cpuWorlds;
	entries.clear();
	cpuWorlds.clear();

	// Pre-order traversal stack — children pushed in reverse so siblings are visited left-to-right.
	thread_local std::vector<entt::entity> stack;
	stack.clear();
	for (const auto e: registry.view<component::Transform, component::Hierarchy>()) {
		if (const auto& [parentId, childrenIds] = registry.get<component::Hierarchy>(e); parentId == core::UUID{0})
			stack.push_back(e);
	}

	while (!stack.empty()) {
		const entt::entity e = stack.back();
		stack.pop_back();

		const auto& localTransform = registry.get<component::Transform>(e).transform;
		const auto& hierarchy = registry.get<component::Hierarchy>(e);

		const math::mat4 localMat = localTransform();
		int32_t parentIdx = -1;
		math::mat4 worldMat = localMat;
		if (hierarchy.parentId != core::UUID{0}) {
			if (const Entity parentEnt = findEntityByUUID(hierarchy.parentId); parentEnt) {
				const auto parentHandle = static_cast<entt::entity>(parentEnt);
				if (const auto it = m_entityToWorldIndex.find(parentHandle); it != m_entityToWorldIndex.end()) {
					parentIdx = static_cast<int32_t>(it->second);
					worldMat = cpuWorlds[it->second] * localMat;
				}
			}
		}

		const auto slot = static_cast<uint32_t>(entries.size());
		m_entityToWorldIndex.emplace(e, slot);
		entries.push_back({.local = localMat, .parentIdx = parentIdx});
		cpuWorlds.push_back(worldMat);
		m_worldTransformCache.insert_or_assign(e, math::Transform{worldMat});

		for (const auto& childId: hierarchy.childrenIds | std::views::reverse) {
			if (const Entity childEnt = findEntityByUUID(childId); childEnt)
				stack.push_back(static_cast<entt::entity>(childEnt));
		}
	}

	mp_worldTransformPass->compute(entries);
}

auto Scene::getWorldIndex(const Entity& iEntity) const -> uint32_t {
	const auto handle = static_cast<entt::entity>(iEntity);
	if (const auto it = m_entityToWorldIndex.find(handle); it != m_entityToWorldIndex.end())
		return it->second;
	return std::numeric_limits<uint32_t>::max();
}

auto Scene::getWorldsBuffer() const -> shared<renderer::gpu::StorageBuffer> {
	if (!mp_worldTransformPass)
		return nullptr;
	return mp_worldTransformPass->getWorldBuffer();
}

auto Scene::isEffectivelyVisible(const Entity& iEntity, const bool iEditorMode) const -> bool {
	const auto handle = static_cast<entt::entity>(iEntity);
	const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(handle)) << 1) | (iEditorMode ? 1ULL : 0ULL);
	if (m_inUpdatePass) {
		if (const auto it = m_visibilityCache.find(key); it != m_visibilityCache.end())
			return it->second;
	}

	constexpr uint32_t maxDepth = 64;
	const auto compute = [&]() -> bool {
		if (const auto* vis = registry.try_get<component::Visibility>(handle); vis != nullptr) {
			if (const bool visible = iEditorMode ? vis->editorVisible : vis->gameVisible; !visible)
				return false;
		}
		core::UUID currentParentId = iEntity.getComponent<component::Hierarchy>().parentId;
		uint32_t depth = 0;
		while (currentParentId != core::UUID{0} && depth < maxDepth) {
			const Entity parent = findEntityByUUID(currentParentId);
			if (!parent)
				break;
			if (const auto* parentVis = registry.try_get<component::Visibility>(static_cast<entt::entity>(parent));
				parentVis != nullptr) {
				if (const bool parentVisible = iEditorMode ? parentVis->editorVisible : parentVis->gameVisible;
					!parentVisible)
					return false;
			}
			currentParentId = parent.getComponent<component::Hierarchy>().parentId;
			++depth;
		}
		return true;
	};
	const bool result = compute();
	if (m_inUpdatePass)
		m_visibilityCache.emplace(key, result);
	return result;
}

void Scene::setParent(const Entity& iChild, const Entity& iNewParent) const {
	if (!iChild || !iNewParent)
		return;
	if (iChild == iNewParent) {
		OWL_CORE_WARN("setParent: cannot parent entity to itself.")
		return;
	}
	// Check for circular reference: walk iNewParent's ancestor chain.
	const core::UUID childUuid = iChild.getUUID();
	core::UUID ancestorId = iNewParent.getComponent<component::Hierarchy>().parentId;
	uint32_t depth = 0;
	while (ancestorId != core::UUID{0} && depth < 64) {
		if (ancestorId == childUuid) {
			OWL_CORE_WARN("setParent: circular hierarchy detected, refusing reparent.")
			return;
		}
		const Entity ancestor = findEntityByUUID(ancestorId);
		if (!ancestor)
			break;
		ancestorId = ancestor.getComponent<component::Hierarchy>().parentId;
		++depth;
	}
	// Compute current world transform before reparenting.
	const math::Transform currentWorld = getWorldTransform(iChild);
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
	const math::Transform newParentWorld = getWorldTransform(iNewParent);
	const math::mat4 newLocal = math::inverse(newParentWorld()) * currentWorld();
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
	for (const auto handle: toDestroy) {
		if (m_primaryPlayerCache == handle)
			m_primaryPlayerCache = entt::null;
		if (const auto* idComp = registry.try_get<component::ID>(handle); idComp != nullptr)
			m_uuidIndex.erase(idComp->id);
		registry.destroy(handle);
	}
	ioEntity.m_entityHandle = entt::null;
}

auto Scene::duplicateSubtree(const Entity& iEntity) -> Entity {
	// Duplicate the root entity.
	Entity newRoot = createEntity(iEntity.getName());
	copyComponentIfExistsFromTuple(newRoot, iEntity, component::CopiableComponents{});
	// Reset hierarchy for the new root.
	auto& [parentId, childrenIds] = newRoot.getComponent<component::Hierarchy>();
	parentId = core::UUID{0};
	childrenIds.clear();
	std::vector<std::pair<Entity, Entity>> stack;
	for (const auto childId: iEntity.getComponent<component::Hierarchy>().childrenIds) {
		if (const Entity srcChild = findEntityByUUID(childId); srcChild)
			stack.emplace_back(srcChild, newRoot);
	}
	while (!stack.empty()) {
		const auto [srcEntity, dstParent] = stack.back();
		stack.pop_back();
		Entity newChild = createEntity(srcEntity.getName());
		copyComponentIfExistsFromTuple(newChild, srcEntity, component::CopiableComponents{});
		auto& [pid, c_ids] = newChild.getComponent<component::Hierarchy>();
		pid = dstParent.getUUID();
		c_ids.clear();
		dstParent.getComponent<component::Hierarchy>().childrenIds.push_back(newChild.getUUID());
		// Push source children for further duplication.
		for (const auto grandChildId: srcEntity.getComponent<component::Hierarchy>().childrenIds) {
			if (const Entity srcGrandChild = findEntityByUUID(grandChildId); srcGrandChild)
				stack.emplace_back(srcGrandChild, newChild);
		}
	}
	return newRoot;
}

void Scene::rebuildHierarchyChildren() {
	// Clear all children lists.
	for (const auto view = registry.view<component::Hierarchy>(); const auto entity: view) {
		view.get<component::Hierarchy>(entity).childrenIds.clear();
	}
	// Rebuild from parentId references.
	for (const auto view = registry.view<component::Hierarchy, component::ID>(); const auto entity: view) {
		if (auto& [parentId, childrenIds] = view.get<component::Hierarchy>(entity); parentId != core::UUID{0}) {
			if (const Entity parent = findEntityByUUID(parentId)) {
				parent.getComponent<component::Hierarchy>().childrenIds.push_back(view.get<component::ID>(entity).id);
			} else {
				// Parent not found (corrupted data), orphan this entity.
				OWL_CORE_WARN("rebuildHierarchyChildren: parent {} not found, orphaning entity.",
							  static_cast<uint64_t>(parentId))
				parentId = core::UUID{0};
			}
		}
	}
}

template<typename T>
void Scene::onComponentAdded([[maybe_unused]] const Entity& iEntity, [[maybe_unused]] T& ioComponent) {
	OWL_CORE_ASSERT(false, "Unknown component")
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

}// namespace owl::scene
