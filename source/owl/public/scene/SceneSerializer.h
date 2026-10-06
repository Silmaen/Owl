/**
 * @file SceneSerializer.h
 * @author Silmaen
 * @date 27/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Scene.h"
#include "core/FormatVersion.h"
#include "core/Serializer.h"
#include "core/expected.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace owl::scene {

/**
 * @brief
 *  Reason why a scene could not be loaded.
 */
enum struct SceneLoadError : uint8_t {
	FileUnreadable,///< The scene file does not exist or cannot be read.
	InvalidYaml,///< The data is not valid YAML.
	NotAScene,///< The YAML has no `Scene` key, or its top-level layout is not a scene.
	InvalidEntity,///< An entity is malformed (missing `Entity` id, field of the wrong type...).
	InvalidFormatVersion,///< The `FormatVersion` field is not a positive integer.
	NewerFormatVersion,///< The file was created by a newer version of Owl.
	MigrationFailed,///< The file could not be migrated from its format version to the current one.
};

/**
 * @brief
 *  Human-readable description of a scene load error.
 * @param[in] iError The error.
 * @return A short sentence fragment describing the error.
 */
[[nodiscard]] OWL_API auto describe(SceneLoadError iError) -> std::string_view;

/// Result of a scene load: nothing on success, the failure reason otherwise.
using SceneLoadResult = expected<void, SceneLoadError>;

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
	/// Failure reason when `valid` is false.
	SceneLoadError failure = SceneLoadError::InvalidYaml;
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
	 *  Format descriptor of the scene files (`.owl`), with its migration chain.
	 * @return The scene format.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  Save the scene into a file, atomically (see `platform::writeFileAtomic`).
	 * @param[in] iFilepath The file where to save.
	 * @return True on success; on failure the previous file is left untouched.
	 */
	[[nodiscard]] auto serialize(const std::filesystem::path& iFilepath) const -> bool;

	/**
	 * @brief
	 *  Serialize the scene to a YAML string.
	 * @return The YAML string.
	 */
	[[nodiscard]] auto serializeToString() const -> std::string;

	/**
	 * @brief
	 *  Load the scene from a file.
	 *
	 * Same validation and rollback as `applyParsed`.
	 * @param[in] iFilepath The file to load.
	 * @return Nothing on success, the failure reason otherwise.
	 */
	[[nodiscard]] auto deserialize(const std::filesystem::path& iFilepath) const -> SceneLoadResult;

	/**
	 * @brief
	 *  Load the scene from a memory buffer.
	 * @param[in] iData The raw YAML data.
	 * @param[in] iSourceName Optional source name for error messages.
	 * @return Nothing on success, the failure reason otherwise.
	 */
	[[nodiscard]] auto deserializeFromBuffer(const std::vector<uint8_t>& iData,
											 const std::string& iSourceName = "<buffer>") const -> SceneLoadResult;

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
	 *
	 * An older format version is migrated first (see `core::upgradeDocument`), a newer one is refused.
	 * The data is validated on the way: a malformed entity aborts the load and
	 * removes every entity it already created (the bound scene is left as it was).
	 * Recoverable corruption is repaired with a warning: a duplicated UUID gets a
	 * fresh one, a missing parent or a hierarchy cycle is moved to the root
	 * (see `Scene::rebuildHierarchyChildren`).
	 * @param[in] iParsed The parsed YAML — typically produced on a worker.
	 * @return Nothing when the scene populated cleanly, the failure reason otherwise.
	 */
	[[nodiscard]] auto applyParsed(const ParsedScene& iParsed) const -> SceneLoadResult;

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

	/**
	 * @brief
	 *  Apply a serialized entity onto an existing entity, in place.
	 *
	 * Tag, Transform, Visibility and every optional component take the state stored in the YAML;
	 * optional components absent from it are removed, and a component whose YAML is unchanged is
	 * left untouched (its runtime state survives). The entity handle, its UUID and its `Hierarchy`
	 * component are preserved, so children stay attached and cached handles stay valid.
	 * @param[in] iEntity The entity to update.
	 * @param[in] iYamlData The YAML string (as produced by serializeEntityToString) of the same entity.
	 * @return True if successful; false when the YAML is invalid or describes another entity.
	 */
	[[nodiscard]] static auto applyEntityFromString(const Entity& iEntity, const std::string& iYamlData) -> bool;

private:
	/// Parent Scene.
	shared<Scene> mp_scene;
};
}// namespace owl::scene
