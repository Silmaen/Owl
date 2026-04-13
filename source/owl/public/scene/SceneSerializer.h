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
	 * @brief Serialize the scene to a YAML string.
	 * @return The YAML string.
	 */
	[[nodiscard]] auto serializeToString() const -> std::string;

	/**
	 * @brief Load the scene from a file.
	 * @param[in] iFilepath The file to load.
	 * @return True if everything works.
	 */
	[[nodiscard]] auto deserialize(const std::filesystem::path& iFilepath) const -> bool;

	/**
	 * @brief Load the scene from a memory buffer.
	 * @param[in] iData The raw YAML data.
	 * @param[in] iSourceName Optional source name for error messages.
	 * @return True if everything works.
	 */
	[[nodiscard]] auto deserializeFromBuffer(const std::vector<uint8_t>& iData,
											 const std::string& iSourceName = "<buffer>") const -> bool;

	/**
	 * @brief Serialize a single entity to a YAML string.
	 * @param[in] iEntity The entity to serialize.
	 * @return The YAML string for this entity.
	 */
	static auto serializeEntityToString(const Entity& iEntity) -> std::string;

	/**
	 * @brief Deserialize a single entity from a YAML string into a scene.
	 * @param[in] ioScene The scene to create the entity in.
	 * @param[in] iYamlData The YAML string (as produced by serializeEntityToString).
	 * @return True if successful.
	 */
	static auto deserializeEntityFromString(const shared<Scene>& ioScene, const std::string& iYamlData) -> bool;

private:
	/// Parent Scene.
	shared<Scene> mp_scene;
};
}// namespace owl::scene
