/**
 * @file ScriptEngine.h
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace owl::scene {

class Scene;
}

/**
 * @brief
 *  Namespace for scripting.
 */
namespace owl::script {
/**
 * @brief
 *  Types of exposed script properties.
 */
enum struct ScriptPropertyType : uint8_t {
	Float,///< Floating point property.
	Int,///< Integer property.
	String,///< String property.
	Bool,///< Boolean property.
};

/**
 * @brief
 *  A single exposed script property.
 */
struct OWL_API ScriptProperty {
	/// Property name.
	std::string name;
	/// Property type.
	ScriptPropertyType type = ScriptPropertyType::Float;
	/// Property value.
	std::variant<float, int64_t, std::string, bool> value = 0.0f;
};

/**
 * @brief
 *  Global script engine manager (singleton pattern).
 *
 * Manages the shared Lua state, script loading/caching, and provides
 * the bridge between the engine and Lua scripts.
 */
class OWL_API ScriptEngine final {
public:
	ScriptEngine() = delete;

	~ScriptEngine() = delete;

	ScriptEngine(const ScriptEngine&) = delete;

	ScriptEngine(ScriptEngine&&) = delete;

	auto operator=(const ScriptEngine&) -> ScriptEngine& = delete;

	auto operator=(ScriptEngine&&) -> ScriptEngine& = delete;

	/**
	 * @brief
	 *  Initialize the scripting engine.
	 * @param[in] iScene The active scene (used by Lua bindings).
	 */
	static void init(scene::Scene* iScene);

	/**
	 * @brief
	 *  Shut down the scripting engine and release resources.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Check whether the engine is initialized.
	 * @return True if initialized.
	 */
	[[nodiscard]] static auto isInitialized() -> bool;

	/**
	 * @brief
	 *  Load a Lua script from a file.
	 * @param[in] iPath Path to the .lua file.
	 * @return True on success.
	 */
	[[nodiscard]] static auto loadScript(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Load a Lua script from a memory buffer.
	 * @param[in] iData Buffer containing the Lua source code.
	 * @param[in] iName Chunk name for error messages.
	 * @return True on success.
	 */
	[[nodiscard]] static auto loadScriptFromBuffer(const std::vector<uint8_t>& iData, const std::string& iName) -> bool;

	/**
	 * @brief
	 *  Parse a script file to extract declared properties.
	 * @param[in] iPath Path to the .lua file.
	 * @return List of extracted properties.
	 */
	[[nodiscard]] static auto extractProperties(const std::filesystem::path& iPath) -> std::vector<ScriptProperty>;

	/**
	 * @brief
	 *  Parse a script buffer to extract declared properties.
	 * @param[in] iData Buffer containing the Lua source code.
	 * @param[in] iName Chunk name for error messages.
	 * @return List of extracted properties.
	 */
	[[nodiscard]] static auto extractPropertiesFromBuffer(const std::vector<uint8_t>& iData, const std::string& iName)
			-> std::vector<ScriptProperty>;

	/**
	 * @brief
	 *  Access the active scene (used internally by Lua bindings).
	 * @return The active scene, or nullptr.
	 */
	[[nodiscard]] static auto getActiveScene() -> scene::Scene*;

private:
	/// Forward-declared implementation.
	class Impl;
	/// The implementation.
	static uniq<Impl> s_impl;
};

}// namespace owl::script
