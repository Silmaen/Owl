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
#include "scene/Entity.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"

#include "core/Application.h"
#include "input/Input.h"
#include "physic/PhysicCommand.h"
#include "scene/ScreenTransition.h"
#include "scene/component/components.h"
#include "script/ScriptEngine.h"
#include "script/ScriptInstance.h"
#include "sound/SoundCommand.h"
#include "sound/SoundSystem.h"

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

/**
 * @brief
 *  Compute the natural aspect ratio of a text string for a given font.
 */
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

/**
 * @brief
 *  Render a single UI child entity's visual components.
 */
void renderUIChild(const Entity& iChild, const math::Transform& iTransform, const float iPxScaleY) {
	const int entId = static_cast<int>(static_cast<entt::entity>(iChild));
	// Sprite.
	if (iChild.hasComponent<component::SpriteRenderer>()) {
		const auto& [color, texture, tilingFactor] = iChild.getComponent<component::SpriteRenderer>();

		renderer::Renderer2D::drawQuad({.transform = iTransform,
										.color = color,
										.texture = texture,
										.tilingFactor = tilingFactor,
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
		if (!font && core::Application::instanced())
			font = core::Application::get().getFontLibrary().getDefaultFont();
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

/**
 * @brief
 *  Advance one animated sprite by `iTimeStep`, applying `speedCurve` if set.
 */
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
	registry.destroy(ioEntity.m_entityHandle);
	ioEntity.m_entityHandle = entt::null;
}

void Scene::onStartRuntime() {
	OWL_PROFILE_FUNCTION()

	status = Status::Playing;

	// Tilemap tilesets are normally resolved on first render; we need them
	// before physics init so collidable cells generate static fixtures.
	resolveAllTilemapAssets();

	physic::PhysicCommand::init(this);

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

	// Initialize Lua scripting
	script::ScriptEngine::init(this);
	for (const auto view = registry.view<component::LuaScript>(); const auto entity: view) {
		auto& luaScript = view.get<component::LuaScript>(entity);
		if (luaScript.scriptPath.empty())
			continue;
		luaScript.instance = mkUniq<script::ScriptInstance>();
		const auto uuid = static_cast<uint64_t>(registry.get<component::ID>(entity).id);
		bool loaded = false;
		if (core::Application::instanced()) {
			auto& app = core::Application::get();
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
	}

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

	physic::PhysicCommand::destroy();
	status = Status::Editing;
}

void Scene::onUpdateRuntime(const core::Timestep& iTimeStep, const bool iRender) {
	OWL_PROFILE_FUNCTION()

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
			const auto font = core::Application::get().getFontLibrary().getDefaultFont();
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
			const auto font = core::Application::get().getFontLibrary().getDefaultFont();
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
	// update native scripts
	registry.view<component::NativeScript>().each([iTimeStep, this](auto ioEntity, auto& ioNsc) -> auto {
		if (!ioNsc.instance) {
			ioNsc.instance = ioNsc.instantiateScript();
			ioNsc.instance->entity = Entity{ioEntity, this};
			ioNsc.instance->onCreate();
		}
		ioNsc.instance->onUpdate(iTimeStep);
	});

	// update Lua scripts
	for (const auto view = registry.view<component::LuaScript>(); const auto entity: view) {
		if (auto& luaScript = view.get<component::LuaScript>(entity);
			luaScript.instance && luaScript.instance->isValid())
			luaScript.instance->onUpdate(iTimeStep.getSeconds());
	}
	// Inputs
	if (const Entity player = getPrimaryPlayer()) {
		auto& [primary, iplayer] = player.getComponent<component::Player>();
		iplayer.parseInputs(player);
	}

	// Physics
	physic::PhysicCommand::frame(iTimeStep);

	// links
	for (const auto view = registry.view<component::Transform, component::EntityLink>(); const auto entity: view) {
		auto [transform, link] = view.get<component::Transform, component::EntityLink>(entity);
		if (!link.linkedEntity || link.linkedEntity.getComponent<component::Tag>().tag != link.linkedEntityName) {
			for (const auto view2 = registry.view<component::Tag>(); const auto entity2: view2) {
				if (view2.get<component::Tag>(entity2).tag == link.linkedEntityName) {
					link.linkedEntity = {entity2, this};
					break;
				}
			}
		}
		// Copy world position from linked entity, converting to local space.
		const Entity thisEntity{entity, this};
		const math::Transform linkedWorld = getWorldTransform(link.linkedEntity);
		if (const auto& [parentId, childrenIds] = thisEntity.getComponent<component::Hierarchy>();
			parentId != core::UUID{0}) {
			if (const Entity parent = findEntityByUUID(parentId); parent) {
				const math::mat4 parentWorldInv = math::inverse(getWorldTransform(parent)());
				const math::vec4 localPos =
						parentWorldInv * math::vec4{linkedWorld.translation().x(), linkedWorld.translation().y(),
													linkedWorld.translation().z(), 1.0f};
				transform.transform.translation().x() = localPos.x();
				transform.transform.translation().y() = localPos.y();
				transform.transform.translation().z() = localPos.z();
			} else {
				transform.transform.translation() = linkedWorld.translation();
			}
		} else {
			transform.transform.translation() = linkedWorld.translation();
		}
	}
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

	// Triggers: edge detection (enter/exit/stay) + timer updates.
	// An entity hidden via `Visibility` (or whose ancestor is hidden) is treated as
	// disabled for gameplay purposes — its triggers do not fire and any prior overlap
	// state is reset so the next time it becomes visible the player has to re-enter.
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
		// Re-read the camera transform AFTER scripts have run — Lua follow-cam logic
		// updates the Camera entity's Transform via transform.set_position(); without
		// this refresh, the renderer would use the position from the start of the
		// frame, which causes the camera to appear locked or laggy.
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
}

void Scene::onRenderRuntime() {
	OWL_PROFILE_FUNCTION()

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
			const auto font = core::Application::get().getFontLibrary().getDefaultFont();
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
			const auto font = core::Application::get().getFontLibrary().getDefaultFont();
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
}

void Scene::onUpdateEditor([[maybe_unused]] const core::Timestep& iTimeStep, const renderer::Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	// Compute inverse(projection * viewRotation) for skybox (includes FOV/aspect ratio)
	math::mat4 viewRotation = iCamera.getView();
	viewRotation(0, 3) = 0.0f;
	viewRotation(1, 3) = 0.0f;
	viewRotation(2, 3) = 0.0f;
	m_inverseViewRotation = inverse(iCamera.getProjection() * viewRotation);
	renderWithStack(iCamera);
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

	auto* self = const_cast<Scene*>(this);
	const auto matches = [&](const entt::entity e) -> bool {
		const Entity ent{e, self};
		if (!isEffectivelyVisible(ent, /*iEditorMode=*/false))
			return false;
		if (!ent.hasComponent<component::RendererTag>())
			return iIsFirst;
		return ent.getComponent<component::RendererTag>().rendererName == iLayerName;
	};
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
}

void Scene::renderWithStack(const renderer::Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	const auto& stack = renderer::Renderer::getRenderStack();
	// Editor mode: ignore the renderer stack entirely and fall back to a single 2D
	// pass. Editing a tilemap from inside a first-person raycast view is unusable
	// (the level designer needs the top-down grid), so the editor always sees the
	// 2D rendering of every entity regardless of `RendererTag` / `EnabledRenderers`.
	// The multi-layer pipeline kicks back in as soon as the scene enters Play mode.
	const bool editorMode = (status == Status::Editing);
	if (editorMode || stack.isEmpty()) {
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
		// Skip layers with no entity routed to them — running an empty
		// `beginScene/endScene` pair otherwise costs a Vulkan render-pass per
		// layer and causes the neighbouring layers to flicker (the empty pass
		// clears state mid-frame). Untagged renderables default to the first
		// layer, so if it has zero matching entities the entire layer is dead.
		if (!layerHasContent(layer->getName(), isFirst))
			continue;
		layer->setViewport(m_viewportSize);
		layer->onBeginFrame(iCamera);
		m_currentLayerName = layer->getName();
		m_currentLayerIsFirst = isFirst;
		mp_currentLayer = layer.get();
		render();
		// Hand the layer's effective VP (world camera for sprite layers, pixel
		// ortho for raycast / screen-overlay layers) to `renderUI` so the HUD
		// stays aligned with whatever frame the layer is currently drawing in.
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

	// Draw background (only the first entity with this component, and only on the first layer
	// when running through the stack — backgrounds always sit behind every layer).
	if (m_currentLayerIsFirst) {
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

	// Draw tilemaps first so they sit behind every sprite / circle / text in the scene.
	// Renderer2D batches do not depth-sort within a frame; the painter's order wins,
	// hence tilemaps are drawn before the foreground entities.
	resolveAllTilemapAssets();
	for (const auto view = registry.view<component::Transform, component::Tilemap>(); auto entity: view) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
			continue;
		if (!layerAccepts(ent))
			continue;
		auto& tilemap = view.get<component::Tilemap>(entity);
		if (!tilemap.asset || !tilemap.asset->tileset || !tilemap.asset->tileset->texture)
			continue;
		const auto& assetData = *tilemap.asset;
		const math::Transform worldTransform = getWorldTransform(ent);
		const int entityId = static_cast<int>(entity);
		// When the active layer is a raycaster, route the tilemap to the DDA pipeline
		// instead of emitting per-cell 2D quads. The 2D path keeps running for any
		// layer whose type isn't "RendererRaycast" (legacy and HUD tilemaps included).
		if (mp_currentLayer != nullptr && std::string_view{mp_currentLayer->getTypeKey()} == "RendererRaycast") {
			renderer::RendererRaycast::drawTilemapWalls(assetData, worldTransform, entityId);
			continue;
		}
		const float cellSize = assetData.cellSize;
		const float originX = -static_cast<float>(assetData.width - 1) * 0.5f * cellSize;
		const float originY = static_cast<float>(assetData.height - 1) * 0.5f * cellSize;
		for (const auto& layer: assetData.layers) {
			if (!layer.visible)
				continue;
			for (uint32_t y = 0; y < assetData.height; ++y) {
				for (uint32_t x = 0; x < assetData.width; ++x) {
					const size_t flat = static_cast<size_t>(y) * assetData.width + x;
					if (flat >= layer.tiles.size())
						continue;
					const int32_t tileIdx = layer.tiles[flat];
					if (tileIdx < 0)
						continue;
					math::Transform cellTransform = worldTransform;
					cellTransform.translation().x() += (originX + static_cast<float>(x) * cellSize);
					cellTransform.translation().y() += (originY - static_cast<float>(y) * cellSize);
					// Slight overscale (0.5%) so adjacent tiles overlap at rasterisation
					// boundaries — without it, sub-pixel motion of the camera causes the
					// shared edge between two tiles to flicker.
					constexpr float kSeamOverlap = 1.005f;
					cellTransform.scale().x() *= cellSize * kSeamOverlap;
					cellTransform.scale().y() *= cellSize * kSeamOverlap;

					renderer::Renderer2D::drawQuad(
							{.transform = cellTransform,
							 .color = math::vec4{1.f, 1.f, 1.f, 1.f},
							 .texture = assetData.tileset->texture,
							 .textureCoords = assetData.tileset->getTileUv(static_cast<uint32_t>(tileIdx)),
							 .entityId = entityId});
				}
			}
		}
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
		const math::Transform worldTransform = getWorldTransform(ent);

		renderer::Renderer2D::drawQuad({.transform = worldTransform,
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
		const math::Transform worldTransform = getWorldTransform(ent);
		const uint32_t safeCols = std::max(anim.columns, 1u);
		const uint32_t safeRows = std::max(anim.rows, 1u);
		const uint32_t frame = std::clamp(anim.m_currentFrame, anim.firstFrame, anim.lastFrame);
		const uint32_t col = frame % safeCols;
		const uint32_t row = frame / safeCols;
		const float uMin = static_cast<float>(col) / static_cast<float>(safeCols);
		const float uMax = static_cast<float>(col + 1) / static_cast<float>(safeCols);
		const float vMax = 1.0f - static_cast<float>(row) / static_cast<float>(safeRows);
		const float vMin = 1.0f - static_cast<float>(row + 1) / static_cast<float>(safeRows);

		renderer::Renderer2D::drawQuad({.transform = worldTransform,
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
		const math::Transform worldTransform = getWorldTransform(ent);

		renderer::Renderer2D::drawCircle({.transform = worldTransform,
										  .color = circle.color,
										  .thickness = circle.thickness,
										  .fade = circle.fade,
										  .entityId = static_cast<int>(entity)});
	}
	// Draw text
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
}

void Scene::resolveAllTilemapAssets() {
	if (!core::Application::instanced())
		return;
	const auto& app = core::Application::get();
	const auto& assetDirs = app.getAssetDirectories();

	const auto loadFromAssetDirs = [&](const std::filesystem::path& iRelative, const auto& iLoader) -> bool {
		for (const auto& [title, assetsPath]: assetDirs) {
			if (const auto fullPath = assetsPath / iRelative; exists(fullPath)) {
				if (iLoader(fullPath))
					return true;
			}
		}
		return exists(iRelative) && iLoader(iRelative);
	};

	// Phase 1 — resolve the `.owltilemap` asset from `tilemapPath` (skip components that
	// already carry an in-memory asset, e.g. legacy inline tilemaps).
	for (const auto view = registry.view<component::Tilemap>(); auto entity: view) {
		auto& tilemap = view.get<component::Tilemap>(entity);
		if (tilemap.asset || tilemap.tilemapPath.empty())
			continue;
		auto resolved = mkShared<TilemapAsset>();
		const bool loaded = loadFromAssetDirs(tilemap.tilemapPath, [&](const std::filesystem::path& iPath) -> bool {
			return resolved->loadFromFile(iPath);
		});
		if (loaded)
			tilemap.asset = std::move(resolved);
	}

	// Phase 2 — resolve the asset's `.owltileset` (only for assets that don't already carry
	// a tileset).
	for (const auto view = registry.view<component::Tilemap>(); auto entity: view) {
		auto& tilemap = view.get<component::Tilemap>(entity);
		if (!tilemap.asset || tilemap.asset->tileset || tilemap.asset->tilesetPath.empty())
			continue;
		auto resolved = mkShared<Tileset>();
		const bool loaded =
				loadFromAssetDirs(tilemap.asset->tilesetPath, [&](const std::filesystem::path& iPath) -> bool {
					return resolved->loadFromFile(iPath);
				});
		if (loaded)
			tilemap.asset->tileset = std::move(resolved);
	}
}

void Scene::renderUI(const math::mat4& iEffectiveViewProjection) {
	OWL_PROFILE_FUNCTION()

	if (m_viewportSize.x() == 0 || m_viewportSize.y() == 0)
		return;

	const bool editorMode = status == Status::Editing;
	const auto vpWidth = static_cast<float>(m_viewportSize.x());
	const auto vpHeight = static_cast<float>(m_viewportSize.y());

	// Collect Canvas entities, sorted by sortOrder.
	struct CanvasEntry {
		entt::entity entity;
		int32_t sortOrder;
	};
	std::vector<CanvasEntry> canvases;
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

	// Convert pixel coordinates into the same frame the active layer's
	// `Renderer2D` is bound to. The layer reports it via
	// `RenderLayer::getEffectiveViewProjection` (world VP for sprite layers,
	// pixel-space ortho VP for raycast / screen-overlay layers). Project the
	// two opposite NDC corners back into world space and take min/max — this
	// is robust to the Vulkan-style Y-flip in our projections (where the
	// "bottom-left" NDC corner actually unprojects to the world *top-left*),
	// because picking the min Y / max Y of both corners always gives the
	// AABB regardless of which way the projection flipped.
	//
	// HUDs that should follow the camera's screen frame (rather than world
	// axes) belong on a `Renderer2DLayer` configured with `Space: Screen`,
	// which binds a pixel-space ortho instead of the rotated scene camera —
	// see the sample project's `ui` layer.
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

auto Scene::getPrimaryCamera() -> Entity {
	for (const auto view = registry.view<component::Camera>(); const auto entity: view) {
		if (view.get<component::Camera>(entity).primary)
			return Entity{entity, this};
	}
	return {};
}

auto Scene::getPrimaryPlayer() -> Entity {
	for (const auto view = registry.view<component::Player>(); const auto entity: view) {
		if (view.get<component::Player>(entity).primary)
			return Entity{entity, this};
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
	for (const auto view = registry.view<component::ID>(); const auto entity: view) {
		if (view.get<component::ID>(entity).id == iUuid)
			return Entity{entity, const_cast<Scene*>(this)};// NOLINT(cppcoreguidelines-pro-type-const-cast)
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
	constexpr uint32_t maxDepth = 64;
	const auto& localTransform = iEntity.getComponent<component::Transform>().transform;
	const auto& [parentId, childrenIds] = iEntity.getComponent<component::Hierarchy>();
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
}

auto Scene::isEffectivelyVisible(const Entity& iEntity, const bool iEditorMode) const -> bool {
	constexpr uint32_t maxDepth = 64;
	if (const auto* vis = registry.try_get<component::Visibility>(static_cast<entt::entity>(iEntity)); vis != nullptr) {
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
	for (const auto handle: toDestroy) registry.destroy(handle);
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
	// Iteratively duplicate children (avoid recursion for clang-tidy misc-no-recursion).
	// Stack of (source entity, destination parent entity) pairs.
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
		if (core::Application::instanced()) {
			ioComponent.font = core::Application::get().getFontLibrary().getDefaultFont();
		}
	}
}

template<>
OWL_API void Scene::onComponentAdded<component::PhysicBody>([[maybe_unused]] const Entity& iEntity,
															[[maybe_unused]] component::PhysicBody& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::Player>([[maybe_unused]] const Entity& iEntity,
														[[maybe_unused]] component::Player& ioComponent) {}

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
														 [[maybe_unused]] component::Tilemap& ioComponent) {}

}// namespace owl::scene
