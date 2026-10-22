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

#include <cstdint>
#include <filesystem>
#include <functional>
#include <tuple>

namespace owl::scene {
class Scene;
}// namespace owl::scene

/**
 * @brief
 *  Namespace defining functions for draw components in GUI.
 */
namespace owl::gui::component {

/**
 * @brief
 *  Set the function the Open buttons of the inspector call to open an asset in its editor (a tilemap, a tileset, a
 *  script); the editor sets it, a game leaves it empty.
 * @param[in] iHandler Called with the path as stored in the component (relative to an asset folder).
 */
OWL_API void setOpenAssetHandler(std::function<void(const std::filesystem::path&)> iHandler);

/**
 * @brief
 *  Set the function that opens a script at a line (the editor sets it, a game leaves it empty).
 * @param[in] iHandler Called with the path as stored in the component and a one-based line.
 */
OWL_API void setOpenAssetAtLineHandler(std::function<void(const std::filesystem::path&, uint32_t)> iHandler);

/**
 * @brief
 *  Ask for a script to be opened at a line, or just opened when only `setOpenAssetHandler` is set.
 * @param[in] iPath The path as stored in the component.
 * @param[in] iLine One-based line to reveal.
 * @return True when a handler took the request.
 */
OWL_API auto openAssetAtLine(const std::filesystem::path& iPath, uint32_t iLine) -> bool;

/**
 * @brief
 *  Set the scene the inspector reads when it checks that a named Lua function exists (HUD, trigger and timer
 *  handlers); null when no scene is inspected.
 * @param[in] iScene The scene whose scripts define the handlers.
 */
OWL_API void setInspectedScene(const scene::Scene* iScene);

/**
 * @brief
 *  Ask for an asset to be opened in its editor.
 * @param[in] iPath The path as stored in the component.
 * @return True when a handler is set and the path is not empty.
 */
OWL_API auto openAsset(const std::filesystem::path& iPath) -> bool;
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
 *  Render a Gui for editing the static mesh (file, tint, texture override, blending) and its loaded summary.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::MeshRenderer& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the environment (HDRI, intensity, rotation, sky).
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Environment& ioComponent);

/**
 * @brief
 *  Render the fields of a material (surface, maps, emission, layers, toon, alpha), shared by the mesh slots and the
 *  material document.
 *
 * With `iShowOverrides`, every field gets an override box and `ioFields` tells which fields are overridden: the
 * others show the inherited value greyed and keep `ioValues` untouched.
 * @param[in,out] ioValues The values being edited.
 * @param[in,out] ioFields Mask of the `renderer::MaterialField` bits that are overridden (used with the boxes only).
 * @param[in] iShowOverrides True to show the override boxes.
 * @param[in] iInherited Values shown for the fields that are not overridden, null to show `ioValues`.
 * @return True when something changed.
 */
OWL_API auto renderMaterialFields(renderer::MaterialParameters& ioValues, uint32_t& ioFields, bool iShowOverrides,
								  const renderer::MaterialParameters* iInherited) -> bool;

/**
 * @brief
 *  Render a Gui for editing the light (kind, colour, intensity, range, spot cone, directional shadows).
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Light& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the 2D light (kind, colour, intensity, radius, falloff, height, spot cone, shadows).
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Light2D& ioComponent);

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
 *  Render a Gui for editing the component; changing the unit of a size or offset number converts its value so the
 *  widget does not move on screen (from the HUD layout of the owner's scene).
 * @param ioComponent The component to edit.
 * @param[in] iOwner The entity holding the component.
 */
OWL_API void renderProps(scene::component::UiRect& ioComponent, const scene::Entity& iOwner);

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
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiWindow& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiLayout& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiScrollView& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component: the model size, the rows, the colours and the callbacks.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiList& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiTooltip& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::UiToolCursor& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component; the neighbours are picked among the UI entities of the owner's scene.
 * @param ioComponent The component to edit.
 * @param[in] iOwner The entity holding the component.
 */
OWL_API void renderProps(scene::component::UiFocus& ioComponent, const scene::Entity& iOwner);

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
 *  The switch of the navigation graph overlay of the viewport, drawn as the "Show graph" button of the Tilemap
 *  inspector.
 */
struct NavigationGraphSwitch {
	/// Whether the overlay is shown.
	std::function<bool()> isShown;
	/// Show or hide the overlay.
	std::function<void(bool)> setShown;
};

/**
 * @brief
 *  Give the Tilemap inspector the switch of the graph overlay (the button is hidden without it).
 * @param[in] iSwitch The switch, empty functions to remove it.
 */
OWL_API void setNavigationGraphSwitch(NavigationGraphSwitch iSwitch);

/**
 * @brief
 *  Render a Gui for editing an isometric object (sheet, building, tint).
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::IsometricObject& ioComponent);

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
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::LightFlicker& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Oscillator& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Patrol& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::Drift& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::FollowTarget& ioComponent);

/**
 * @brief
 *  Render a Gui for editing the component.
 * @param ioComponent The component to edit.
 */
OWL_API void renderProps(scene::component::RenderTarget& ioComponent);

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
				   scene::component::AnimatedSpriteRenderer, scene::component::CircleRenderer,
				   scene::component::MeshRenderer, scene::component::Light, scene::component::Environment,
				   scene::component::Text, scene::component::Tilemap, scene::component::IsometricObject,
				   scene::component::PhysicBody, scene::component::Player, scene::component::Trigger,
				   scene::component::RaycastDoor, scene::component::RaycastPushWall, scene::component::EntityLink,
				   scene::component::BackgroundTexture, scene::component::Visibility, scene::component::SoundSource,
				   scene::component::SoundListener, scene::component::LuaScript, scene::component::PrefabLink,
				   scene::component::RendererTag, scene::component::VoxelWorld, scene::component::FlyCamera,
				   scene::component::LightFlicker, scene::component::Oscillator, scene::component::Patrol,
				   scene::component::Drift, scene::component::FollowTarget, scene::component::RenderTarget,
				   scene::component::VoxelPlayer, scene::component::Canvas,
				   scene::component::UiRect, scene::component::UiText, scene::component::UiImage,
				   scene::component::UiPanel, scene::component::UiButton, scene::component::UiSlider,
				   scene::component::UiProgressBar, scene::component::UiWindow, scene::component::UiLayout,
				   scene::component::UiTooltip, scene::component::UiToolCursor, scene::component::UiFocus,
				   scene::component::UiScrollView, scene::component::UiList>;


}// namespace owl::gui::component
