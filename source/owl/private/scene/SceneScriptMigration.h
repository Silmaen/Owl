/**
 * @file SceneScriptMigration.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "core/YamlNode.h"

#include <filesystem>
#include <string>

namespace owl::scene {

/**
 * @brief
 *  Migration step of the scene format (6 to 7): the script of the primary camera moves to the `SceneScript` key of
 *  the scene when it only drives the scene, that is when its source never names `entity_id`. A script that cannot be
 *  found, or that moves its entity, stays on the camera.
 * @param[in] ioDocument The document root (with an `Entities` sequence).
 * @return Always true: a document without such a script has nothing to convert.
 */
auto moveCameraSceneScript(const core::Serializer& ioDocument) -> bool;

/**
 * @brief
 *  Check whether an entity node is a primary camera carrying a script `moveCameraSceneScript` may move.
 * @param[in] iEntity The entity node.
 * @return True when it has a primary `Camera` and a `LuaScript`.
 */
[[nodiscard]] OWL_API auto hasCameraScript(const core::YamlNode& iEntity) -> bool;

/**
 * @brief
 *  Names the file a scene is read from while it migrates, so `moveCameraSceneScript` finds the scripts next to it.
 *
 * The scripts are searched in the asset folders of the application, then in the folder of the scene and its parents.
 */
class OWL_API SceneScriptMigrationSource final {
public:
	SceneScriptMigrationSource(const SceneScriptMigrationSource&) = delete;

	SceneScriptMigrationSource(SceneScriptMigrationSource&&) = delete;

	auto operator=(const SceneScriptMigrationSource&) -> SceneScriptMigrationSource& = delete;

	auto operator=(SceneScriptMigrationSource&&) -> SceneScriptMigrationSource& = delete;

	/**
	 * @brief
	 *  Set the scene file of the current thread.
	 * @param[in] iSceneName Path of the scene file (a name that is no path finds nothing next to it).
	 */
	explicit SceneScriptMigrationSource(const std::string& iSceneName);

	/**
	 * @brief
	 *  Forget the scene file.
	 */
	~SceneScriptMigrationSource();
};

}// namespace owl::scene
