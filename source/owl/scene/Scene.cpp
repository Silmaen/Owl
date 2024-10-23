/**
 * @file Scene.cpp
 * @author Silmaen
 * @date 22/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "Scene.h"

#include "Entity.h"
#include "renderer/Renderer2D.h"

#include "component/Camera.h"
#include "component/CircleRenderer.h"
#include "component/ID.h"
#include "component/NativeScript.h"
#include "component/SpriteRenderer.h"
#include "component/Tag.h"
#include "component/Text.h"
#include "component/Transform.h"
#include "core/Application.h"

namespace owl::scene {
namespace {
template<typename Component>
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

template<typename Component>
void copyComponentIfExists(Entity& oDst, const Entity& iSrc) {
	if (iSrc.hasComponent<Component>())
		oDst.addOrReplaceComponent<Component>(iSrc.getComponent<Component>());
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
	auto idView = srcSceneRegistry.view<component::ID>();
	for (const auto e: idView) {
		const core::UUID uuid = srcSceneRegistry.get<component::ID>(e).id;
		const auto& name = srcSceneRegistry.get<component::Tag>(e).tag;
		const Entity newEntity = newScene->createEntityWithUUID(uuid, name);
		enttMap[uuid] = static_cast<entt::entity>(newEntity);
	}

	// Copy components (except IDComponent and TagComponent)
	copyComponent<component::Transform>(dstSceneRegistry, srcSceneRegistry, enttMap);
	copyComponent<component::SpriteRenderer>(dstSceneRegistry, srcSceneRegistry, enttMap);
	copyComponent<component::CircleRenderer>(dstSceneRegistry, srcSceneRegistry, enttMap);
	copyComponent<component::Camera>(dstSceneRegistry, srcSceneRegistry, enttMap);
	copyComponent<component::NativeScript>(dstSceneRegistry, srcSceneRegistry, enttMap);
	copyComponent<component::Text>(dstSceneRegistry, srcSceneRegistry, enttMap);
	//copyComponent<component::Rigidbody2D>(dstSceneRegistry, srcSceneRegistry, enttMap);
	//copyComponent<component::BoxCollider2D>(dstSceneRegistry, srcSceneRegistry, enttMap);

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
	return entity;
}

void Scene::destroyEntity(Entity& ioEntity) {
	registry.destroy(ioEntity.m_entityHandle);
	ioEntity.m_entityHandle = entt::null;
}

void Scene::onUpdateRuntime(const core::Timestep& iTimeStep) {
	// update scripts
	registry.view<component::NativeScript>().each([iTimeStep, this](auto ioEntity, auto& ioNsc) {
		if (!ioNsc.instance) {
			ioNsc.instance = ioNsc.instantiateScript();
			ioNsc.instance->entity = Entity{ioEntity, this};
			ioNsc.instance->onCreate();
		}
		ioNsc.instance->onUpdate(iTimeStep);
	});

	// Render 2D
	renderer::Camera* mainCamera = nullptr;
	math::mat4 cameraTransform;
	for (const auto view = registry.view<component::Transform, component::Camera>(); const auto entity: view) {
		auto [transform, camera] = view.get<component::Transform, component::Camera>(entity);
		if (camera.primary) {
			mainCamera = &camera.camera;
			cameraTransform = transform.getTransform();
			break;
		}
	}

	if (mainCamera != nullptr) {
		mainCamera->setTransform(cameraTransform);
		renderer::Renderer2D::resetStats();
		renderer::Renderer2D::beginScene(*mainCamera);
		render();
		renderer::Renderer2D::endScene();
	}
}

void Scene::onUpdateEditor([[maybe_unused]] const core::Timestep& iTimeStep, const renderer::Camera& iCamera) {
	renderer::Renderer2D::resetStats();
	renderer::Renderer2D::beginScene(iCamera);
	render();
	renderer::Renderer2D::endScene();
}

void Scene::render() {
	// Draw sprites
	for (const auto group = registry.group<component::Transform>(entt::get<component::SpriteRenderer>);
		 auto entity: group) {
		auto [transform, sprite] = group.get<component::Transform, component::SpriteRenderer>(entity);
		renderer::Renderer2D::drawQuad({.transform = transform.getTransform(),
										.color = sprite.color,
										.texture = sprite.texture,
										.tilingFactor = sprite.tilingFactor,
										.entityId = static_cast<int>(entity)});
	}
	// Draw circles
	for (const auto view = registry.view<component::Transform, component::CircleRenderer>(); auto entity: view) {
		auto [transform, circle] = view.get<component::Transform, component::CircleRenderer>(entity);
		renderer::Renderer2D::drawCircle({.transform = transform.getTransform(),
										  .color = circle.color,
										  .thickness = circle.thickness,
										  .fade = circle.fade,
										  .entityId = static_cast<int>(entity)});
	}
	// Draw text
	for (const auto view = registry.view<component::Transform, component::Text>(); auto entity: view) {
		auto [transform, text] = view.get<component::Transform, component::Text>(entity);
		renderer::Renderer2D::drawString({.transform = transform.getTransform(),
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

auto Scene::duplicateEntity(const Entity& iEntity) -> Entity {
	const std::string name = iEntity.getName();
	Entity newEntity = createEntity(name);

	copyComponentIfExists<component::Transform>(newEntity, iEntity);
	copyComponentIfExists<component::SpriteRenderer>(newEntity, iEntity);
	copyComponentIfExists<component::CircleRenderer>(newEntity, iEntity);
	copyComponentIfExists<component::Camera>(newEntity, iEntity);
	copyComponentIfExists<component::NativeScript>(newEntity, iEntity);
	copyComponentIfExists<component::Text>(newEntity, iEntity);
	//copyComponentIfExists<component::Rigidbody2D>(newEntity, iEntity);
	//copyComponentIfExists<component::BoxCollider2D>(newEntity, iEntity);

	return newEntity;
}

auto Scene::getPrimaryCamera() -> Entity {
	for (const auto view = registry.view<component::Camera>(); const auto entity: view) {
		if (view.get<component::Camera>(entity).primary)
			return Entity{entity, this};
	}
	return {};
}

template<typename T>
void Scene::onComponentAdded([[maybe_unused]] const Entity& iEntity, [[maybe_unused]] T& ioComponent) {
	OWL_ASSERT(false, "Unknown component")
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

}// namespace owl::scene
