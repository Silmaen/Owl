/**
 * @file render.h
 * @author Silmaen
 * @date 12/30/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "scene/component/components.h"
#include <core/Core.h>

/**
 * @brief
 *  Namespace defining functions for draw components in GUI.
 */
namespace owl::gui::component {
/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Transform& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Camera& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::SpriteRenderer& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::AnimatedSpriteRenderer& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::CircleRenderer& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Text& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::PhysicBody& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Player& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Trigger& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::RaycastDoor& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::RaycastPushWall& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::EntityLink& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::BackgroundTexture& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Visibility& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::SoundSource& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::SoundListener& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::LuaScript& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Canvas& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiRect& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiText& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiImage& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiPanel& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiButton& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiSlider& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiProgressBar& ioComponent);

/**
 * @brief
 *  Render a read-only Gui displaying the prefab link info.
 * @param ioComponent The component to display.
 */
OWL_API void renderProps(scene::component::PrefabLink& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Tilemap& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the renderer tag (dropdown of stack layers).
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::RendererTag& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the voxel world (lighting, palette / chunk summary).
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::VoxelWorld& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the fly-camera controller (movement / look speeds).
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::FlyCamera& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the voxel player (speeds, gravity, collision box).
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::VoxelPlayer& ioComponent);

/**
 * @brief
 *  List of components that have a render function.
 */
using DrawableComponents =
		std::tuple<scene::component::Transform, scene::component::Camera, scene::component::SpriteRenderer,
				   scene::component::AnimatedSpriteRenderer, scene::component::CircleRenderer, scene::component::Text,
				   scene::component::Tilemap, scene::component::PhysicBody, scene::component::Player,
				   scene::component::Trigger, scene::component::RaycastDoor, scene::component::RaycastPushWall,
				   scene::component::EntityLink, scene::component::BackgroundTexture, scene::component::Visibility,
				   scene::component::SoundSource, scene::component::SoundListener, scene::component::LuaScript,
				   scene::component::PrefabLink, scene::component::RendererTag, scene::component::VoxelWorld,
				   scene::component::FlyCamera, scene::component::VoxelPlayer, scene::component::Canvas,
				   scene::component::UiRect, scene::component::UiText, scene::component::UiImage,
				   scene::component::UiPanel, scene::component::UiButton, scene::component::UiSlider,
				   scene::component::UiProgressBar>;


}// namespace owl::gui::component
