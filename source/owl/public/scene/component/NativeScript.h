/**
 * @file NativeScript.h
 * @author Silmaen
 * @date 26/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "scene/ScriptableEntity.h"

namespace owl::scene::component {
/**
 * @brief
 *  Class allowing the use of C++ scripts.
 */
struct OWL_API NativeScript {
	/// Script instance.
	ScriptableEntity* instance = nullptr;

	/**
	 * @brief
	 *  Run the script.
	 * @return the script instance.
	 */
	ScriptableEntity* (*instantiateScript)(){};

	/**
	 * @brief
	 *  Run destroy script.
	 */
	[[maybe_unused]] void (*destroyScript)(NativeScript*){};

	/**
	 * @brief
	 *  Bind the script.
	 * @tparam T New script.
	 */
	// NOLINTBEGIN(cppcoreguidelines-owning-memory)
	template<typename T>
	/**
	 * @brief
	 *  Bind.
	 */
	void bind() {
		instantiateScript = []() -> ScriptableEntity* { return static_cast<ScriptableEntity*>(new T()); };
		destroyScript = [](NativeScript* ioNsc) -> void {
			delete ioNsc->instance;
			ioNsc->instance = nullptr;
		};
	}

	// NOLINTEND(cppcoreguidelines-owning-memory)
	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Native Script"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "NativeScript"; }
};
}// namespace owl::scene::component
