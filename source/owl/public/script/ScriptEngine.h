/**
 * @file ScriptEngine.h
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
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
	Color,///< sRGB colour with alpha, four components in 0..1 (`color` in the script).
	Vec2,///< Two-component vector (`vec2`).
	Vec3,///< Three-component vector (`vec3`).
	Entity,///< UUID of an entity of the scene, 0 for none (`entity`).
	Numbers,///< List of numbers (`numbers`).
	Strings,///< List of strings (`strings`).
};

/**
 * @brief
 *  Value of a script property: the alternative matching its `ScriptPropertyType`.
 *
 * `Color` holds four floats (r, g, b, a), `Vec2` two, `Vec3` three, `Entity` a UUID, `Numbers` and `Strings` lists.
 */
using ScriptValue = std::variant<float, int64_t, std::string, bool, std::array<float, 2>, std::array<float, 3>,
								 std::array<float, 4>, uint64_t, std::vector<double>, std::vector<std::string>>;

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
	ScriptValue value = 0.0f;
	/// Value declared by the script (`default`), what the inspector resets the field to.
	ScriptValue defaultValue = 0.0f;
	/// Smallest value of a number or vector component (`min`), when the script gives one.
	std::optional<float> min = std::nullopt;
	/// Largest value of a number or vector component (`max`), when the script gives one.
	std::optional<float> max = std::nullopt;
	/// Drag step of a number or vector component (`step`), when the script gives one.
	std::optional<float> step = std::nullopt;
};

/**
 * @brief
 *  A function a script defines at its top level.
 */
struct OWL_API ScriptFunction {
	/// Global name of the function.
	std::string name;
	/// One-based line of the `function` keyword in the script.
	uint32_t line = 0;
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
	 *  Name of a property type in a script (`float`, `int`, `string`, `bool`, `color`, `vec2`, `vec3`, `entity`,
	 *  `numbers`, `strings`) and in the scene file.
	 * @param[in] iType The type.
	 * @return The name.
	 */
	[[nodiscard]] static auto propertyTypeName(ScriptPropertyType iType) -> std::string_view;

	/**
	 * @brief
	 *  Find a property type by its name.
	 * @param[in] iName The name given by `propertyTypeName`.
	 * @return The type, or nothing for an unknown name.
	 */
	[[nodiscard]] static auto parsePropertyType(std::string_view iName) -> std::optional<ScriptPropertyType>;

	/**
	 * @brief
	 *  Value a property of a type holds before anything sets it (0, empty, white for a colour).
	 * @param[in] iType The type.
	 * @return The value, of the alternative matching `iType`.
	 */
	[[nodiscard]] static auto defaultValueFor(ScriptPropertyType iType) -> ScriptValue;

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
	 *  List the functions a script file defines at its top level (the engine hooks and the handlers it names).
	 * @param[in] iPath Path to the .lua file.
	 * @return The functions ordered by line; empty when the script does not load.
	 */
	[[nodiscard]] static auto extractFunctions(const std::filesystem::path& iPath) -> std::vector<ScriptFunction>;

	/**
	 * @brief
	 *  List the functions a script buffer defines at its top level.
	 * @param[in] iData Buffer containing the Lua source code.
	 * @param[in] iName Chunk name for error messages.
	 * @return The functions ordered by line; empty when the script does not load.
	 */
	[[nodiscard]] static auto extractFunctionsFromBuffer(const std::vector<uint8_t>& iData, const std::string& iName)
			-> std::vector<ScriptFunction>;

	/**
	 * @brief
	 *  Names of the functions the engine calls on a script: lifecycle, collision, trigger, timer and interaction hooks.
	 * @return The hook names, in the order the inspector lists them.
	 */
	[[nodiscard]] static auto getHookNames() -> const std::vector<std::string>&;

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
