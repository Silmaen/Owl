/**
 * @file Scene.cpp
 * @author Silmaen
 * @date 22/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/Scene.h"

#include "renderer/BackgroundRenderer.h"
#include "renderer/CameraOrtho.h"
#include "renderer/RenderCommand.h"
#include "renderer/Renderer2D.h"
#include "scene/Entity.h"

#include "core/Application.h"
#include "input/Input.h"
#include "physic/PhysicCommand.h"
#include "scene/ScreenTransition.h"
#include "scene/UIInputSystem.h"
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

/// Compute the natural aspect ratio of a text string for a given font.
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

/// Render a single UI child entity's visual components.
void renderUIChild(const Entity& iChild, const math::Transform& iTransform, const float iPxScaleY) {
	const int entId = static_cast<int>(static_cast<entt::entity>(iChild));
	// Sprite.
	if (iChild.hasComponent<component::SpriteRenderer>()) {
		const auto& sprite = iChild.getComponent<component::SpriteRenderer>();
		renderer::Renderer2D::drawQuad(
				{.transform = iTransform, .color = sprite.color, .texture = sprite.texture,
				 .tilingFactor = sprite.tilingFactor, .entityId = entId});
	}
	// Text (world-space component on a UI entity).
	if (iChild.hasComponent<component::Text>()) {
		const auto& text = iChild.getComponent<component::Text>();
		renderer::Renderer2D::drawString(
				{.transform = iTransform, .text = text.text, .font = text.font, .color = text.color,
				 .kerning = text.kerning, .lineSpacing = text.lineSpacing, .entityId = entId});
	}
	// Circle.
	if (iChild.hasComponent<component::CircleRenderer>()) {
		const auto& circle = iChild.getComponent<component::CircleRenderer>();
		renderer::Renderer2D::drawCircle(
				{.transform = iTransform, .color = circle.color, .thickness = circle.thickness,
				 .fade = circle.fade, .entityId = entId});
	}
	// UIPanel.
	if (iChild.hasComponent<component::UIPanel>()) {
		const auto& panel = iChild.getComponent<component::UIPanel>();
		renderer::Renderer2D::drawQuad({.transform = iTransform, .color = panel.backgroundColor, .entityId = entId});
	}
	// UIImage.
	if (iChild.hasComponent<component::UIImage>()) {
		const auto& img = iChild.getComponent<component::UIImage>();
		renderer::Renderer2D::drawQuad(
				{.transform = iTransform, .color = img.tint, .texture = img.texture, .entityId = entId});
	}
	// UIText.
	if (iChild.hasComponent<component::UIText>()) {
		const auto& uiText = iChild.getComponent<component::UIText>();
		shared<data::fonts::Font> font = uiText.font;
		if (!font && core::Application::instanced())
			font = core::Application::get().getFontLibrary().getDefaultFont();
		const float textAspect = computeTextAspect(font, uiText.text, uiText.kerning, uiText.lineSpacing);
		math::Transform textTransform;
		textTransform.translation() = iTransform.translation();
		const float baseScale = uiText.fontSize * iPxScaleY;
		textTransform.scale() = {baseScale * textAspect, baseScale, 1.f};
		renderer::Renderer2D::drawString(
				{.transform = textTransform, .text = uiText.text, .font = font, .color = uiText.color,
				 .kerning = uiText.kerning, .lineSpacing = uiText.lineSpacing, .entityId = entId});
	}
	// UIButton.
	if (iChild.hasComponent<component::UIButton>()) {
		const auto& button = iChild.getComponent<component::UIButton>();
		renderer::Renderer2D::drawQuad({.transform = iTransform, .color = button.getCurrentColor(), .entityId = entId});
	}
	// UIProgressBar.
	if (iChild.hasComponent<component::UIProgressBar>()) {
		const auto& bar = iChild.getComponent<component::UIProgressBar>();
		const float worldWidth = iTransform.scale().x();
		renderer::Renderer2D::drawQuad({.transform = iTransform, .color = bar.backgroundColor, .entityId = entId});
		const float fillFraction = std::clamp(bar.value, 0.f, 1.f);
		if (fillFraction > 0.f) {
			math::Transform fillTransform = iTransform;
			fillTransform.scale().x() *= fillFraction;
			fillTransform.translation().x() -= worldWidth * (1.f - fillFraction) * 0.5f;
			renderer::Renderer2D::drawQuad({.transform = fillTransform, .color = bar.fillColor, .entityId = entId});
		}
	}
	// UISlider.
	if (iChild.hasComponent<component::UISlider>()) {
		const auto& slider = iChild.getComponent<component::UISlider>();
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
		renderer::Renderer2D::drawQuad(
				{.transform = handleTransform, .color = slider.handleColor, .entityId = entId});
	}
}

}// namespace

Scene::Scene() = default;

Scene::~Scene() = default;

auto Scene::copy(const shared<Scene>& iOther) -> shared<Scene> {
	shared<Scene> newScene = mkShared<Scene>();

	newScene->m_viewportSize = iOther->m_viewportSize;
	newScene->m_gameState = iOther->m_gameState;

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
		for (const auto view = registry.view<component::Transform, component::SoundSource>();
			 const auto entity: view) {
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
			OWL_CORE_ERROR("onStartRuntime: FAILED to load script '{}'", luaScript.scriptPath)
		if (loaded) {
			for (const auto& prop: luaScript.properties) {
				switch (prop.type) {
					case script::ScriptPropertyType::Float:
						luaScript.instance->setProperty(prop.name, std::get<float>(prop.value));
						break;
					case script::ScriptPropertyType::Int:
						luaScript.instance->setProperty(prop.name, std::get<int64_t>(prop.value));
						break;
					case script::ScriptPropertyType::String:
						luaScript.instance->setProperty(prop.name, std::get<std::string>(prop.value));
						break;
					case script::ScriptPropertyType::Bool:
						luaScript.instance->setProperty(prop.name, std::get<bool>(prop.value));
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
}

void Scene::onEndRuntime() {
	OWL_PROFILE_FUNCTION()

	// Stop all active sounds
	for (const auto view = registry.view<component::SoundSource>(); const auto entity: view) {
		if (auto& [soundComp] = view.get<component::SoundSource>(entity);
			soundComp.runtimeHandle != sound::invalidSoundHandle) {
			sound::SoundCommand::stop(soundComp.runtimeHandle);
			soundComp.runtimeHandle = sound::invalidSoundHandle;
		}
	}

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

void Scene::onUpdateRuntime(const core::Timestep& iTimeStep) {
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
		auto& luaScript = view.get<component::LuaScript>(entity);
		if (luaScript.instance && luaScript.instance->isValid())
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

	// Trigger
	for (const auto view = registry.view<component::Trigger>(); const auto ent: view) {
		const Entity entity{ent, this};
		if (Entity player = getPrimaryPlayer(); getColliderBox(entity, getWorldTransform(entity))
														.intersect(getColliderBox(player, getWorldTransform(player)))) {
			entity.getComponent<component::Trigger>().trigger.onTriggered(player, entity);
		}
	}

	// Update animated sprites
	for (const auto view = registry.view<component::AnimatedSpriteRenderer>(); const auto entity: view) {
		auto& anim = view.get<component::AnimatedSpriteRenderer>(entity);
		if (!anim.m_playing || anim.columns == 0 || anim.rows == 0 || anim.frameDuration <= 0.0f)
			continue;
		const uint32_t totalFrames =
				anim.lastFrame >= anim.firstFrame ? anim.lastFrame - anim.firstFrame + 1 : 1;
		anim.m_elapsedTime += iTimeStep.getSeconds();
		if (anim.m_elapsedTime >= anim.frameDuration) {
			const auto framesToAdvance = static_cast<uint32_t>(anim.m_elapsedTime / anim.frameDuration);
			anim.m_elapsedTime -= static_cast<float>(framesToAdvance) * anim.frameDuration;
			if (anim.loop) {
				anim.m_currentFrame = anim.firstFrame +
									  (anim.m_currentFrame - anim.firstFrame + framesToAdvance) % totalFrames;
			} else {
				const uint32_t newFrame = anim.m_currentFrame + framesToAdvance;
				anim.m_currentFrame = std::min(newFrame, anim.lastFrame);
				if (anim.m_currentFrame >= anim.lastFrame)
					anim.m_playing = false;
			}
		}
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
		renderer::Renderer2D::resetStats();
		renderer::Renderer2D::beginScene(*mainCamera);
		render();
		renderUI(*mainCamera);
		renderer::Renderer2D::endScene();
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
		renderer::Renderer2D::resetStats();
		renderer::Renderer2D::beginScene(*mainCamera);
		render();
		renderUI(*mainCamera);
		renderer::Renderer2D::endScene();
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

	renderer::Renderer2D::resetStats();
	renderer::Renderer2D::beginScene(iCamera);
	render();
	renderUI(iCamera);
	renderer::Renderer2D::endScene();
}

void Scene::render() {
	OWL_PROFILE_FUNCTION()

	const bool editorMode = status == Status::Editing;

	// Draw background (only the first entity with this component)
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

	// Draw sprites
	for (const auto group = registry.group<component::Transform>(entt::get<component::SpriteRenderer>);
		 auto entity: group) {
		const Entity ent{entity, this};
		if (!isEffectivelyVisible(ent, editorMode))
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

void Scene::renderUI(const renderer::Camera& iCamera) {
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
		canvases.push_back({entity, view.get<component::Canvas>(entity).sortOrder});
	}
	if (canvases.empty())
		return;
	std::ranges::sort(canvases, [](const auto& iA, const auto& iB) -> auto { return iA.sortOrder < iB.sortOrder; });

	// Convert pixel coordinates to world coordinates using the inverse VP matrix.
	// Determine world-space viewport bounds using min/max to handle Y-flip (Vulkan vs OpenGL).
	const math::mat4 invVP = math::inverse(iCamera.getViewProjection());
	const math::vec4 cornerA4 = invVP * math::vec4{-1.f, -1.f, 0.f, 1.f};
	const math::vec4 cornerB4 = invVP * math::vec4{1.f, 1.f, 0.f, 1.f};
	// Perspective divide (for robustness, though w should be 1 for ortho).
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

		// Render children of the Canvas that have UIRect + visual components.
		const auto& [parentId, childrenIds] = canvasEntity.getComponent<component::Hierarchy>();
		for (const auto childId: childrenIds) {
			const Entity child = findEntityByUUID(childId);
			if (!child || !child.hasComponent<component::UIRect>())
				continue;
			if (!isEffectivelyVisible(child, editorMode))
				continue;

			const auto& rect = child.getComponent<component::UIRect>();
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
		OWL_CORE_WARN("getWorldTransform: depth limit reached, possible circular hierarchy")
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
		OWL_CORE_WARN("setParent: cannot parent entity to itself")
		return;
	}
	// Check for circular reference: walk iNewParent's ancestor chain.
	const core::UUID childUuid = iChild.getUUID();
	core::UUID ancestorId = iNewParent.getComponent<component::Hierarchy>().parentId;
	uint32_t depth = 0;
	while (ancestorId != core::UUID{0} && depth < 64) {
		if (ancestorId == childUuid) {
			OWL_CORE_WARN("setParent: circular hierarchy detected, refusing reparent")
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
				OWL_CORE_WARN("rebuildHierarchyChildren: parent {} not found, orphaning entity",
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
OWL_API void Scene::onComponentAdded<component::UIRect>([[maybe_unused]] const Entity& iEntity,
														 [[maybe_unused]] component::UIRect& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UIText>([[maybe_unused]] const Entity& iEntity,
														 [[maybe_unused]] component::UIText& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UIImage>([[maybe_unused]] const Entity& iEntity,
														  [[maybe_unused]] component::UIImage& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UIPanel>([[maybe_unused]] const Entity& iEntity,
														  [[maybe_unused]] component::UIPanel& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UIButton>([[maybe_unused]] const Entity& iEntity,
														   [[maybe_unused]] component::UIButton& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UISlider>([[maybe_unused]] const Entity& iEntity,
														   [[maybe_unused]] component::UISlider& ioComponent) {}

template<>
OWL_API void Scene::onComponentAdded<component::UIProgressBar>([[maybe_unused]] const Entity& iEntity,
																[[maybe_unused]] component::UIProgressBar& ioComponent) {
}

template<>
OWL_API void Scene::onComponentAdded<component::PrefabLink>([[maybe_unused]] const Entity& iEntity,
															 [[maybe_unused]] component::PrefabLink& ioComponent) {
}

}// namespace owl::scene
