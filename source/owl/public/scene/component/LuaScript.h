/**
 * @file LuaScript.h
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "script/ScriptEngine.h"
#include "script/ScriptInstance.h"

namespace owl::scene::component {
/**
 * @brief
 *  Lua script component.
 *
 * Attaches a Lua script to an entity. The scriptPath and properties are serialized;
 * the instance is runtime-only and created during onStartRuntime.
 */
struct OWL_API LuaScript {
	LuaScript() = default;

	/**
	 * @brief
	 *  Copy constructor — copies script data but not the runtime instance.
	 * @param[in] iOther The component to copy from.
	 */
	LuaScript(const LuaScript& iOther)
		: scriptPath{iOther.scriptPath}, properties{iOther.properties}, instance{nullptr} {}

	/**
	 * @brief
	 *  Move constructor.
	 */
	LuaScript(LuaScript&&) noexcept = default;

	/**
	 * @brief
	 *  Copy assignment — copies script data but not the runtime instance.
	 * @param[in] iOther The component to copy from.
	 * @return A reference to this component.
	 */
	auto operator=(const LuaScript& iOther) -> LuaScript& {
		if (this != &iOther) {
			scriptPath = iOther.scriptPath;
			properties = iOther.properties;
			instance = nullptr;
		}
		return *this;
	}

	/**
	 * @brief
	 *  Move assignment.
	 * @return A reference to this component.
	 */
	auto operator=(LuaScript&&) noexcept -> LuaScript& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~LuaScript() = default;

	/// Path to the .lua script file (relative to assets).
	std::string scriptPath;
	/// Exposed properties editable in the inspector.
	std::vector<script::ScriptProperty> properties;
	/// Runtime script instance (not serialized, created at runtime).
	uniq<script::ScriptInstance> instance;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() noexcept -> const char* { return "Lua Script"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() noexcept -> const char* { return "LuaScript"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
