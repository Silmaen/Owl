/**
 * @file Player.h
 * @author Silmaen
 * @date 30/12/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/YamlSerializers.h"
#include "scene/ScenePlayer.h"

namespace owl::scene::component {

/**
 * @brief component for player informations.
 */
struct OWL_API Player {
	/// If player is the primary one.
	bool primary = true;
	/// The scene player.
	ScenePlayer player;
	/**
	 * @brief Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Player"; }
	/**
	 * @brief Get the YAML key for this component
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Player"; }

	/**
	 * @brief Write this component to a YAML context.
	 * @param ioOut The YAML context.
	 */
	void serialize(YAML::Emitter& ioOut) const {
		ioOut << YAML::Key << key();
		ioOut << YAML::BeginMap;
		ioOut << YAML::Key << "primary" << YAML::Value << primary;
		ioOut << YAML::Key << "linearImpulse" << YAML::Value << player.linearImpulse;
		ioOut << YAML::Key << "jumpImpulse" << YAML::Value << player.jumpImpulse;
		ioOut << YAML::Key << "canJump" << YAML::Value << player.canJump;
		ioOut << YAML::EndMap;
	}

	/**
	 * @brief Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const YAML::Node& iNode) {
		if (iNode["primary"])
			primary = iNode["primary"].as<bool>();
		if (iNode["linearImpulse"]) {
			player.linearImpulse = iNode["linearImpulse"].as<float>();
		}
		if (iNode["jumpImpulse"]) {
			player.jumpImpulse = iNode["jumpImpulse"].as<float>();
		}
		if (iNode["canJump"]) {
			player.canJump = iNode["canJump"].as<bool>();
		}
	}
};

}// namespace owl::scene::component
