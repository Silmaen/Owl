/**
 * @file components.h
 * @author Silmaen
 * @date 10/24/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "AnimatedSpriteRenderer.h"
#include "BackgroundTexture.h"
#include "Camera.h"
#include "Canvas.h"
#include "CircleRenderer.h"
#include "EntityLink.h"
#include "Hierarchy.h"
#include "ID.h"
#include "LuaScript.h"
#include "NativeScript.h"
#include "PhysicBody.h"
#include "Player.h"
#include "PrefabLink.h"
#include "RendererTag.h"
#include "SoundListener.h"
#include "SoundSource.h"
#include "SpriteRenderer.h"
#include "Tag.h"
#include "Text.h"
#include "Tilemap.h"
#include "Transform.h"
#include "Trigger.h"
#include "UiButton.h"
#include "UiImage.h"
#include "UiPanel.h"
#include "UiProgressBar.h"
#include "UiRect.h"
#include "UiSlider.h"
#include "UiText.h"
#include "Visibility.h"

namespace owl::scene::component {
/**
 * @brief
 *  Concept that type is a component.
 */
template<typename Component>
concept isComponent =
		std::is_same_v<Component, AnimatedSpriteRenderer> || std::is_same_v<Component, BackgroundTexture> ||
		std::is_same_v<Component, Camera> || std::is_same_v<Component, Canvas> ||
		std::is_same_v<Component, CircleRenderer> || std::is_same_v<Component, EntityLink> ||
		std::is_same_v<Component, Hierarchy> || std::is_same_v<Component, ID> || std::is_same_v<Component, LuaScript> ||
		std::is_same_v<Component, NativeScript> || std::is_same_v<Component, PhysicBody> ||
		std::is_same_v<Component, Player> || std::is_same_v<Component, PrefabLink> ||
		std::is_same_v<Component, RendererTag> || std::is_same_v<Component, SoundListener> ||
		std::is_same_v<Component, SoundSource> || std::is_same_v<Component, SpriteRenderer> ||
		std::is_same_v<Component, Tag> || std::is_same_v<Component, Text> || std::is_same_v<Component, Tilemap> ||
		std::is_same_v<Component, Transform> || std::is_same_v<Component, Trigger> ||
		std::is_same_v<Component, UiButton> || std::is_same_v<Component, UiImage> ||
		std::is_same_v<Component, UiPanel> || std::is_same_v<Component, UiProgressBar> ||
		std::is_same_v<Component, UiRect> || std::is_same_v<Component, UiSlider> || std::is_same_v<Component, UiText> ||
		std::is_same_v<Component, Visibility>;

/**
 * @brief
 *  Concept that type has a name() method.
 */
template<typename Component>
concept isNamedComponent = isComponent<Component> && requires {
	{ Component::name() } -> std::convertible_to<const char*>;
};

/**
 * @brief
 *  Concept that type is serializable.
 */
template<typename Component>
concept isSerializableComponent =
		isComponent<Component> && requires(const Component& iComponent, const core::Serializer& iOut) {
			{ iComponent.serialize(iOut) } -> std::same_as<void>;
			{ Component::key() } -> std::convertible_to<const char*>;
		};

/**
 * @brief
 *  Concept that type is deserializable.
 */
template<typename Component>
concept isDeserializableComponent =
		isComponent<Component> && requires(Component& iComponent, const core::Serializer& iNode) {
			{ iComponent.deserialize(iNode) } -> std::same_as<void>;
			{ Component::key() } -> std::convertible_to<const char*>;
		};

/**
 * @brief
 *  List of copiable components.
 * @note All except ID and Tag.
 */
using CopiableComponents = std::tuple<Transform, Camera, Canvas, SpriteRenderer, AnimatedSpriteRenderer, CircleRenderer,
									  Text, Tilemap, PhysicBody, Player, Trigger, EntityLink, BackgroundTexture,
									  Visibility, Hierarchy, SoundSource, SoundListener, LuaScript, PrefabLink,
									  RendererTag, UiButton, UiImage, UiPanel, UiProgressBar, UiRect, UiSlider, UiText>;

/**
 * @brief
 *  List all serializable components.
 * @note All except ID which is serialized directly in the entity.
 */
using SerializableComponents =
		std::tuple<Tag, Transform, Camera, Canvas, SpriteRenderer, AnimatedSpriteRenderer, CircleRenderer, Text,
				   Tilemap, PhysicBody, Player, Trigger, EntityLink, BackgroundTexture, Visibility, Hierarchy,
				   SoundSource, SoundListener, LuaScript, PrefabLink, RendererTag, UiButton, UiImage, UiPanel,
				   UiProgressBar, UiRect, UiSlider, UiText>;

/**
 * @brief
 *  List all optional components.
 * @note All except ID, Tag & Transform that are mandatory.
 */
using OptionalComponents =
		std::tuple<Camera, Canvas, SpriteRenderer, AnimatedSpriteRenderer, CircleRenderer, Text, Tilemap, PhysicBody,
				   Player, Trigger, EntityLink, BackgroundTexture, SoundSource, SoundListener, LuaScript, PrefabLink,
				   RendererTag, UiButton, UiImage, UiPanel, UiProgressBar, UiRect, UiSlider, UiText>;

}// namespace owl::scene::component
