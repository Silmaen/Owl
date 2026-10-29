/**
 * @file MeshSockets.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "core/YamlNode.h"
#include "math/YamlSerializers.h"
#include "scene/component/MeshSockets.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace owl::scene::component {

auto MeshSockets::find(const std::string_view iName) const -> const Socket* {
	const auto found = std::ranges::find(sockets, iName, &Socket::name);
	return found == sockets.end() ? nullptr : &*found;
}

void MeshSockets::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// MeshSockets
	out << YAML::Key << "sockets" << YAML::Value << YAML::BeginSeq;
	for (const auto& socket: sockets) {
		out << YAML::BeginMap;
		out << YAML::Key << "name" << YAML::Value << socket.name;
		out << YAML::Key << "bone" << YAML::Value << socket.bone;
		out << YAML::Key << "translation" << YAML::Value << socket.offset.translation();
		out << YAML::Key << "rotation" << YAML::Value << socket.offset.rotation();
		out << YAML::Key << "scale" << YAML::Value << socket.offset.scale();
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;// MeshSockets
}

void MeshSockets::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	sockets.clear();
	if (const auto items = node["sockets"]; items.isSequence()) {
		for (const auto item: items) {
			Socket socket;
			socket.name = item["name"].as<std::string>(std::string{});
			socket.bone = item["bone"].as<std::string>(std::string{});
			socket.offset.translation() = item["translation"].as<math::vec3>(math::vec3{0.f, 0.f, 0.f});
			socket.offset.rotation() = item["rotation"].as<math::vec3>(math::vec3{0.f, 0.f, 0.f});
			socket.offset.scale() = item["scale"].as<math::vec3>(math::vec3{1.f, 1.f, 1.f});
			sockets.push_back(std::move(socket));
		}
	}
}

void SocketAttachment::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// SocketAttachment
	out << YAML::Key << "socket" << YAML::Value << socket;
	out << YAML::EndMap;// SocketAttachment
}

void SocketAttachment::deserialize(const core::Serializer& iNode) {
	socket = iNode.getImpl()->node["socket"].as<std::string>(std::string{});
}

}// namespace owl::scene::component
