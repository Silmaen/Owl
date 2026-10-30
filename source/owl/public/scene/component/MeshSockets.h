/**
 * @file MeshSockets.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "math/Transform.h"

#include <string>
#include <string_view>
#include <vector>

namespace owl::scene::component {

/**
 * @brief
 *  Named attachment points of an entity, on top of the `socket_*` nodes of its mesh file.
 *
 * A socket is an offset from the entity origin or from a bone of its skinned mesh; an entity holding a
 * `SocketAttachment` under this one is snapped on it. A socket of the component hides a mesh socket of the same name.
 *
 * On-disk form:
 * ```yaml
 * MeshSockets:
 *   sockets:
 *     - {name: hand_r, bone: hand_r, translation: [0, -0.1, 0], rotation: [0, 0, 0], scale: [1, 1, 1]}
 * ```.
 */
struct OWL_API MeshSockets {
	/**
	 * @brief
	 *  One attachment point.
	 */
	struct Socket {
		/// Name of the socket.
		std::string name;
		/// Bone the socket follows, empty for the entity origin.
		std::string bone;
		/// Transform of the socket relative to the bone (or to the entity).
		math::Transform offset;
	};

	/// The sockets.
	std::vector<Socket> sockets;

	/**
	 * @brief
	 *  Find a socket by its name.
	 * @param[in] iName The name.
	 * @return The socket, null when absent.
	 */
	[[nodiscard]] auto find(std::string_view iName) const -> const Socket*;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Mesh Sockets"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "MeshSockets"; }

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

/**
 * @brief
 *  Snaps the entity on a socket of its parent: every frame its local transform is the one of the socket.
 *
 * `Scene::attachToSocket` parents the entity and adds this component; `Scene::detachFromSocket` removes it and keeps
 * the entity where it is. A socket bound to a bone carries the entity with the animation.
 *
 * On-disk form:
 * ```yaml
 * SocketAttachment:
 *   socket: hand_r
 * ```.
 */
struct OWL_API SocketAttachment {
	/// Name of the socket of the parent.
	std::string socket;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Socket Attachment"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "SocketAttachment"; }

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
