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
#include "renderer/Renderer2D.h"
#include "scene/Entity.h"

#include "core/Application.h"
#include "input/Input.h"
#include "physic/PhysicCommand.h"
#include "scene/component/components.h"
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

}// namespace

Scene::Scene() = default;

Scene::~Scene() = default;

auto Scene::copy(const shared<Scene>& iOther) -> shared<Scene> {
	shared<Scene> newScene = mkShared<Scene>();

	newScene->m_viewportSize = iOther->m_viewportSize;

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
	if (sound::SoundSystem::getState() != sound::SoundSystem::State::Running &&
		sound::SoundSystem::getState() != sound::SoundSystem::State::Error)
		return;
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
		const sound::PlayParams params{.volume = soundComp.volume,
									   .pitch = soundComp.pitch,
									   .loop = soundComp.loop,
									   .spatial = soundComp.spatial,
									   .position = {wt.translation().x(), wt.translation().y(), wt.translation().z()},
									   .maxDistance = soundComp.maxDistance,
									   .rolloff = soundComp.rolloff};
		soundComp.runtimeHandle = sound::SoundCommand::play(soundData, params);
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
	// update scripts
	registry.view<component::NativeScript>().each([iTimeStep, this](auto ioEntity, auto& ioNsc) -> auto {
		if (!ioNsc.instance) {
			ioNsc.instance = ioNsc.instantiateScript();
			ioNsc.instance->entity = Entity{ioEntity, this};
			ioNsc.instance->onCreate();
		}
		ioNsc.instance->onUpdate(iTimeStep);
	});

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
		renderer::Renderer2D::endScene();
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
		renderer::Renderer2D::endScene();
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

}// namespace owl::scene
