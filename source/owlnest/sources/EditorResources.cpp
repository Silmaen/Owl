/**
 * @file EditorResources.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorResources.h"

#include <gui/IconBank.h>
#include <gui/UiLayer.h>
#include <owl.h>
#include <sound/SoundSystem.h>

#include <imgui.h>

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace owl::nest::utils {

void buildIconBank() {
	auto& iconBank = gui::IconBank::instance();

	// Resolve icon file paths: try SVG in assets_sources first, then PNG in assets.
	const auto& assetDirs = app::Application::get().getAssetDirectories();
	const auto resolve = [&](const std::string& iName) -> std::filesystem::path {
		for (const auto& dir: assetDirs) {
			// SVG sources in assets_sources/ (parallel to the assets/ directory).
			const auto svgDir = dir.assetsPath.parent_path() / "assets_sources";
			if (const auto p = svgDir / (iName + ".svg"); exists(p))
				return p;
			// PNG fallback in assets/.
			if (const auto p = dir.assetsPath / (iName + ".png"); exists(p))
				return p;
		}
		return {};
	};

	// clang-format off
	std::vector<std::pair<std::string, std::filesystem::path>> icons = {
		// Toolbar icons (playback + gizmo controls)
		{"ctrl_rotation",     resolve("icons/toolbar/ctrl_rotation")},
		{"ctrl_scale",        resolve("icons/toolbar/ctrl_scale")},
		{"ctrl_translation",  resolve("icons/toolbar/ctrl_translation")},
		{"snap_grid",         resolve("icons/toolbar/snap_grid")},
		{"eraser",            resolve("icons/actions/eraser")},
		{"move_up",           resolve("icons/actions/move_up")},
		{"move_down",         resolve("icons/actions/move_down")},
		{"PlayButton",        resolve("icons/toolbar/play")},
		{"PauseButton",       resolve("icons/toolbar/pause")},
		{"StopButton",        resolve("icons/toolbar/stop")},
		{"StepButton",        resolve("icons/toolbar/step")},
		// Ribbon icons (duotone: the accent marks the meaningful part)
		{"ribbon_save",     resolve("icons/ribbon/save")},
		{"ribbon_open",     resolve("icons/ribbon/open")},
		{"ribbon_new",      resolve("icons/ribbon/new")},
		{"ribbon_undo",     resolve("icons/ribbon/undo")},
		{"ribbon_redo",     resolve("icons/ribbon/redo")},
		{"ribbon_copy",     resolve("icons/ribbon/copy")},
		{"ribbon_flow",     resolve("icons/ribbon/flow")},
		{"ribbon_settings", resolve("icons/ribbon/settings")},
		{"ribbon_pack",     resolve("icons/ribbon/pack")},
		{"ribbon_panels",   resolve("icons/ribbon/panels")},
		{"ribbon_layout",   resolve("icons/ribbon/layout")},
		{"ribbon_stats",    resolve("icons/ribbon/stats")},
		{"ribbon_help",     resolve("icons/ribbon/help")},
		{"ribbon_tune",     resolve("icons/ribbon/tune")},
		{"ribbon_import",   resolve("icons/ribbon/import")},
		{"ribbon_close",    resolve("icons/ribbon/close")},
		{"ribbon_cameras",  resolve("icons/ribbon/cameras")},
		{"ribbon_visibility",resolve("icons/ribbon/visibility")},
		{"ribbon_play",     resolve("icons/ribbon/play")},
		{"ribbon_pause",    resolve("icons/ribbon/pause")},
		{"ribbon_stop",     resolve("icons/ribbon/stop")},
		{"ribbon_previous", resolve("icons/ribbon/previous")},
		{"ribbon_back",     resolve("icons/ribbon/back")},
		{"ribbon_folder_new", resolve("icons/ribbon/folder_new")},
		{"ribbon_exit",     resolve("icons/ribbon/exit")},
		{"ribbon_book",     resolve("icons/ribbon/book")},
		{"ribbon_next",     resolve("icons/ribbon/next")},
		{"ribbon_view_reset", resolve("icons/ribbon/view_reset")},
		{"ribbon_axes",     resolve("icons/ribbon/axes")},
		{"ribbon_frame_scene", resolve("icons/ribbon/frame_scene")},
		{"ribbon_viewpoint", resolve("icons/ribbon/viewpoint")},
		// Visibility icons
		{"camera_on",         resolve("icons/visibility/camera_on")},
		{"camera_off",        resolve("icons/visibility/camera_off")},
		{"eye_open",          resolve("icons/visibility/eye_open")},
		{"eye_closed",        resolve("icons/visibility/eye_closed")},
		// Trigger icons
		{"trigger_victory",   resolve("icons/triggers/victory")},
		{"trigger_death",     resolve("icons/triggers/death")},
		{"trigger_target",    resolve("icons/triggers/target")},
		{"trigger_teleport",  resolve("icons/triggers/teleport")},
		{"trigger_timer",     resolve("icons/triggers/timer")},
		{"trigger_interact",  resolve("icons/triggers/interaction")},
		{"trigger_lua",       resolve("icons/triggers/lua_callback")},
		// File browser icons
		{"folder_icon",       resolve("icons/browser/folder")},
		{"glsl_icon",         resolve("icons/browser/glsl")},
		{"jpg_icon",          resolve("icons/browser/jpg")},
		{"json_icon",         resolve("icons/browser/json")},
		{"owl_icon",          resolve("icons/browser/owl")},
		{"png_icon",          resolve("icons/browser/png")},
		{"svg_icon",          resolve("icons/browser/svg_file")},
		{"text_icon",         resolve("icons/browser/text")},
		{"ttf_icon",          resolve("icons/browser/ttf")},
		{"yml_icon",          resolve("icons/browser/yml")},
		{"lua_icon",          resolve("icons/browser/lua")},
		{"prefab_icon",       resolve("icons/browser/prefab")},
		{"owlhud_icon",       resolve("icons/browser/owlhud")},
		{"owlanim_icon",      resolve("icons/browser/owlanim")},
		{"owltileset_icon",   resolve("icons/browser/owltileset")},
		{"owltilemap_icon",   resolve("icons/browser/owltilemap")},
		{"wav_icon",          resolve("icons/browser/wav")},
		{"mp3_icon",          resolve("icons/browser/mp3")},
		{"ogg_icon",          resolve("icons/browser/ogg")},
		{"flac_icon",         resolve("icons/browser/flac")},
		{"obj_icon",          resolve("icons/browser/obj")},
		{"gltf_icon",         resolve("icons/browser/gltf")},
		{"glb_icon",          resolve("icons/browser/glb")},
		{"fbx_icon",          resolve("icons/browser/fbx")},
		{"py_icon",           resolve("icons/browser/py")},
		{"cpp_icon",          resolve("icons/browser/cpp")},
		{"h_icon",            resolve("icons/browser/h")},
		{"c_icon",            resolve("icons/browser/c")},
		{"md_icon",           resolve("icons/browser/md")},
		// Action icons (context menus, toolbar, etc.)
		{"delete",            resolve("icons/actions/delete")},
		{"rename",            resolve("icons/actions/rename")},
		{"new_folder",        resolve("icons/actions/new_folder")},
		{"import_file",       resolve("icons/actions/import_file")},
		{"import_folder",     resolve("icons/actions/import_folder")},
		{"add_entity",        resolve("icons/actions/add_entity")},
		{"add_child_entity",  resolve("icons/actions/add_child_entity")},
		{"add_component",     resolve("icons/actions/add_component")},
		{"delete_entity",     resolve("icons/actions/delete_entity")},
		{"delete_cascade",    resolve("icons/actions/delete_cascade")},
		{"unparent",          resolve("icons/actions/unparent")},
		{"save",              resolve("icons/actions/save")},
		{"open",              resolve("icons/actions/open")},
		{"new_scene",         resolve("icons/actions/new_scene")},
		{"duplicate",         resolve("icons/actions/duplicate")},
		{"undo",              resolve("icons/actions/undo")},
		{"redo",              resolve("icons/actions/redo")},
		{"settings",          resolve("icons/actions/settings")},
		{"search",            resolve("icons/actions/search")},
		{"pack",              resolve("icons/actions/pack")},
		// UI / navigation icons
		{"back",              resolve("icons/actions/back")},
		{"close",             resolve("icons/actions/close")},
		{"project",           resolve("icons/actions/project")},
		{"exit",              resolve("icons/actions/exit")},
		// Panel icons
		{"scene_hierarchy",   resolve("icons/panels/scene_hierarchy")},
		{"content_browser",   resolve("icons/panels/content_browser")},
		{"stats",             resolve("icons/panels/stats")},
		{"properties",        resolve("icons/panels/properties")},
		{"log",               resolve("icons/panels/log")},
		{"viewport",          resolve("icons/panels/viewport")},
		{"info",              resolve("icons/panels/info")},
		{"preview",           resolve("icons/panels/preview")},
		// Component icons
		{"comp_transform",    resolve("icons/components/transform")},
		{"comp_camera",       resolve("icons/components/camera")},
		{"comp_sprite",       resolve("icons/components/sprite")},
		{"comp_animated_sprite", resolve("icons/components/animated_sprite")},
		{"comp_circle",       resolve("icons/components/circle")},
		{"comp_text",         resolve("icons/components/text")},
		{"comp_physics",      resolve("icons/components/physics")},
		{"comp_script",       resolve("icons/components/script")},
		{"comp_lua_script",   resolve("icons/components/lua_script")},
		{"comp_sound",        resolve("icons/components/sound")},
		{"comp_trigger",      resolve("icons/components/trigger")},
		{"comp_player",       resolve("icons/components/player")},
		{"comp_link",         resolve("icons/components/link")},
		{"comp_background",   resolve("icons/components/background")},
		{"comp_visibility",   resolve("icons/components/visibility")},
		{"comp_canvas",       resolve("icons/components/canvas")},
		{"comp_ui_rect",      resolve("icons/components/ui_rect")},
		{"comp_ui_text",      resolve("icons/components/ui_text")},
		{"comp_ui_image",     resolve("icons/components/ui_image")},
		{"comp_ui_panel",     resolve("icons/components/ui_panel")},
		{"comp_ui_button",    resolve("icons/components/ui_button")},
		{"comp_ui_slider",    resolve("icons/components/ui_slider")},
		{"comp_ui_progress",  resolve("icons/components/ui_progress")},
		{"comp_voxel_world",  resolve("icons/components/voxel_world")},
		{"comp_fly_camera",   resolve("icons/components/fly_camera")},
	};
	// clang-format on

	// Remove entries with empty paths
	std::erase_if(icons, [](const auto& iEntry) -> auto { return iEntry.second.empty(); });

	const auto& theme = gui::UiLayer::getTheme();
	iconBank.build(icons, 64, {.primary = theme.text, .secondary = theme.accent});
}

void applyTheme(const gui::Theme& iTheme) {
	gui::UiLayer::setTheme(iTheme);
	gui::IconBank::instance().rebuild({.primary = iTheme.text, .secondary = iTheme.accent});
}

void loadTriggerTextures() {
	auto& textureLibrary = renderer::Renderer::getTextureLibrary();
	textureLibrary.load("icons/triggers/victory");
	textureLibrary.load("icons/triggers/death");
	textureLibrary.load("icons/triggers/target");
	textureLibrary.load("icons/triggers/teleport");
	textureLibrary.load("icons/triggers/timer");
	textureLibrary.load("icons/triggers/interaction");
	textureLibrary.load("icons/triggers/lua_callback");
	textureLibrary.load("icons/triggers/camera");
}

void loadSounds() {
	auto& soundLibrary = sound::SoundSystem::getSoundLibrary();
	soundLibrary.load("clic.wav");
}
}// namespace owl::nest::utils
