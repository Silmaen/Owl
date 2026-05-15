/**
 * @file SceneSerializer.h
 * @author Silmaen
 * @date 27/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Scene.h"
#include "core/Serializer.h"

namespace owl::scene {

/**
 * @brief
 *  Outcome of the CPU-only YAML parse phase used by the async scene-load path.
 *
 * `SceneSerializer::parseBuffer` produces one of these on a worker thread
 * (no engine, no entity, no GPU touch). The main thread then feeds it
 * back to `SceneSerializer::applyParsed`, which walks the parsed tree and
 * actually populates the bound scene.
 */
struct OWL_API ParsedScene {
	/// Opaque handle to the parsed YAML root. Empty when `valid` is false.
	shared<core::Serializer> serializer;
	/// Scene name extracted from the YAML header (informational; may be empty).
	std::string sceneName;
	/// True when the buffer parsed cleanly and looked like a scene file.
	bool valid = false;
	/// Optional human-readable error message when `valid` is false.
	std::string error;
};
/**
 * @brief
 *  Class use to serialize of deserialize the scene.
 */
class OWL_API SceneSerializer {
public:
	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iScene The attached scene.
	 */
	explicit SceneSerializer(const shared<Scene>& iScene);

	/**
	 * @brief
	 *  Save the scene into a file.
	 * @param[in] iFilepath The file where to save.
	 */
	void serialize(const std::filesystem::path& iFilepath) const;

	/**
	 * @brief
	 *  Serialize the scene to a YAML string.
	 * @return The YAML string.
	 */
	[[nodiscard]] auto serializeToString() const -> std::string;

	/**
	 * @brief
	 *  Load the scene from a file.
	 * @param[in] iFilepath The file to load.
	 * @return True if everything works.
	 */
	[[nodiscard]] auto deserialize(const std::filesystem::path& iFilepath) const -> bool;

	/**
	 * @brief
	 *  Load the scene from a memory buffer.
	 * @param[in] iData The raw YAML data.
	 * @param[in] iSourceName Optional source name for error messages.
	 * @return True if everything works.
	 */
	[[nodiscard]] auto deserializeFromBuffer(const std::vector<uint8_t>& iData,
											 const std::string& iSourceName = "<buffer>") const -> bool;

	/**
	 * @brief
	 *  Parse the YAML buffer on a worker thread. CPU-only — does NOT touch
	 *  entities, GPU textures or any engine global; safe to call from any
	 *  Taskflow worker. The result feeds into `applyParsed` on the main
	 *  thread to actually populate the scene.
	 * @param[in] iData The raw YAML data.
	 * @param[in] iSourceName Optional source name for error messages.
	 * @return A `ParsedScene` whose `valid` flag tells whether the parse
	 *         succeeded; the `error` field carries the failure reason on miss.
	 */
	[[nodiscard]] static auto parseBuffer(const std::vector<uint8_t>& iData,
										  const std::string& iSourceName = "<buffer>") -> ParsedScene;

	/**
	 * @brief
	 *  Apply a `ParsedScene` produced by `parseBuffer` to the bound scene.
	 *  Must run on the main thread (creates entities and may create GPU
	 *  textures via the async texture path).
	 * @param[in] iParsed The parsed YAML — typically produced on a worker.
	 * @return True when the scene populated cleanly.
	 */
	[[nodiscard]] auto applyParsed(const ParsedScene& iParsed) const -> bool;

	/**
	 * @brief
	 *  Serialize a single entity to a YAML string.
	 * @param[in] iEntity The entity to serialize.
	 * @return The YAML string for this entity.
	 */
	[[nodiscard]] static auto serializeEntityToString(const Entity& iEntity) -> std::string;

	/**
	 * @brief
	 *  Deserialize a single entity from a YAML string into a scene.
	 * @param[in] ioScene The scene to create the entity in.
	 * @param[in] iYamlData The YAML string (as produced by serializeEntityToString).
	 * @return True if successful.
	 */
	[[nodiscard]] static auto deserializeEntityFromString(const shared<Scene>& ioScene, const std::string& iYamlData)
			-> bool;

private:
	/// Parent Scene.
	shared<Scene> mp_scene;
};
}// namespace owl::scene
