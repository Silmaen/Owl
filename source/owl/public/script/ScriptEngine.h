/**
 * @file ScriptEngine.h
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

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
 *  Resource limits of one Lua state (one per script instance).
 *
 * A limit set to zero is disabled. The defaults are generous: a gameplay script stays orders of magnitude
 * below them, a runaway one is stopped before it freezes the frame for long or exhausts the host memory.
 */
struct OWL_API ScriptQuotas {
	/// Heap ceiling of the state, in bytes (64 MiB by default).
	size_t memoryBytes = size_t{64} * 1024 * 1024;
	/// Wall-clock time allowed per host call (`on_update`, chunk execution...), in milliseconds (250 by default).
	uint32_t timePerCallMs = 250;
};

/**
 * @brief
 *  Script services shared by every scene: default quotas and property parser.
 *
 * It holds no scene and no Lua state: every ScriptInstance owns its state and acts on the scene it is bound
 * to (`ScriptInstance::setScene`), so several scenes run their scripts side by side.
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
	 *  Set the quotas given to every Lua state created afterwards.
	 * @param[in] iQuotas Memory and time limits.
	 */
	static void setDefaultQuotas(const ScriptQuotas& iQuotas);

	/**
	 * @brief
	 *  Get the quotas given to new Lua states.
	 * @return The default memory and time limits.
	 */
	[[nodiscard]] static auto getDefaultQuotas() -> ScriptQuotas;
};

}// namespace owl::script
