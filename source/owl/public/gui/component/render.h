/**
 * @file render.h
 * @author Silmaen
 * @date 12/30/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "scene/component/components.h"
#include <core/Core.h>

/**
 * @brief Namespace defining functions for draw components in GUI.
 */
namespace owl::gui::component {

/**
 * @brief Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Transform& ioComponent);

/**
 * @brief Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Camera& ioComponent);

/**
 * @brief Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::SpriteRenderer& ioComponent);

/**
 * @brief Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::CircleRenderer& ioComponent);

/**
 * @brief Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Text& ioComponent);

/**
 * @brief Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::PhysicBody& ioComponent);

/**
 * @brief Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Player& ioComponent);

/**
 * @brief Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Trigger& ioComponent);
/**
 * @brief Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::EntityLink& ioComponent);

/**
 * @brief Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::BackgroundTexture& ioComponent);

/**
 * @brief List of components that have a render function.
 */
using DrawableComponents =
		std::tuple<scene::component::Transform, scene::component::Camera, scene::component::SpriteRenderer,
				   scene::component::CircleRenderer, scene::component::Text, scene::component::PhysicBody,
				   scene::component::Player, scene::component::Trigger, scene::component::EntityLink,
				   scene::component::BackgroundTexture>;


}// namespace owl::gui::component
