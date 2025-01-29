/**
 * @file SceneSerializer.h
 * @author Silmaen
 * @date 27/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Scene.h"

namespace owl::scene {
/**
 * @brief Class use to serialize of deserialize the scene.
 */
class OWL_API SceneSerializer {
public:
	/**
	 * @brief Constructor.
	 * @param[in] iScene The attached scene.
	 */
	explicit SceneSerializer(const shared<Scene>& iScene);

	/**
	 * @brief Save the scene into a file.
	 * @param[in] iFilepath The file where to save.
	 */
	void serialize(const std::filesystem::path& iFilepath) const;

	/**
	 * @brief Load the scene from a file.
	 * @param[in] iFilepath The file to load.
	 * @return True if everything works.
	 */
	[[nodiscard]] auto deserialize(const std::filesystem::path& iFilepath) const -> bool;

private:
	/// Parent Scene.
	shared<Scene> mp_scene;
};
}// namespace owl::scene
