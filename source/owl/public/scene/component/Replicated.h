/**
 * @file Replicated.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace owl::scene::component {

/**
 * @brief
 *  An entity a network session shares: the server sends its state to the clients that need it.
 *
 * The server decides an entity of `Authority::Server`; the clients show what it sends, interpolated, or predict the
 * entity they own. The owner of an entity of `Authority::Owner` decides it and the server passes it on. Ignored
 * outside a networked match ([Networking](networking.md#networking-replication)).
 */
struct OWL_API Replicated {
	/// Who decides the state of the entity.
	enum struct Authority : uint8_t {
		Server,///< The server simulates it; the clients receive it.
		Owner,///< Its owner simulates it and sends it to the server, which passes it on.
	};

	/// Who decides the state of the entity.
	Authority authority = Authority::Server;
	/// Player owning the entity (0 for the host): the one predicting it, or deciding it with `Authority::Owner`.
	uint32_t owner = 0;
	/// Whether the transform is sent.
	bool transform = true;
	/// Keys of the other components sent when they change (`Text`, `SpriteRenderer`...).
	std::vector<std::string> components;
	/// Whether a client shows the received transforms interpolated, else as they arrive.
	bool interpolate = true;
	/// Whether the owner simulates the entity itself and only corrects it when the server disagrees.
	bool predict = false;
	/// Distance within which a client receives the entity, from the entities it owns; 0 for everywhere.
	float relevance = 0.f;
	/// Share of the snapshot budget: an entity of twice the priority is sent twice as often when the budget is short.
	float priority = 1.f;
	/// Radius of the hit sphere the server rewinds to the time a player saw it (lag compensation); 0 for none.
	float hitRadius = 0.f;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Replicated"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Replicated"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
