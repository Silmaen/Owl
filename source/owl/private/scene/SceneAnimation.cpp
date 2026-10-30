/**
 * @file SceneAnimation.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/MeshCache.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace owl::scene {

namespace {

struct SocketSource {
	std::string bone;
	math::mat4 offset = math::identity<float, 4>();
};

auto findSocket(const Entity& iOwner, const std::string_view iSocket) -> std::optional<SocketSource> {
	if (iOwner.hasComponent<component::MeshSockets>()) {
		if (const auto* socket = iOwner.getComponent<component::MeshSockets>().find(iSocket); socket != nullptr)
			return SocketSource{.bone = socket->bone, .offset = socket->offset()};
	}
	if (iOwner.hasComponent<component::MeshRenderer>()) {
		if (const auto& mesh = iOwner.getComponent<component::MeshRenderer>().mesh; mesh) {
			const auto& sockets = mesh->getSockets();
			if (const auto found = std::ranges::find(sockets, iSocket, &data::geometry::MeshSocket::name);
				found != sockets.end())
				return SocketSource{.bone = found->bone, .offset = found->offset};
		}
	}
	return std::nullopt;
}

}// namespace

auto Scene::getSocketTransform(const Entity& iOwner, const std::string_view iSocket) const
		-> std::optional<math::mat4> {
	if (!iOwner)
		return std::nullopt;
	const auto socket = findSocket(iOwner, iSocket);
	if (!socket)
		return std::nullopt;
	if (socket->bone.empty() || !iOwner.hasComponent<component::MeshRenderer>())
		return socket->offset;
	const auto& mesh = iOwner.getComponent<component::MeshRenderer>().mesh;
	const auto skeleton = mesh ? mesh->getSkeleton() : nullptr;
	if (!skeleton)
		return socket->offset;
	const auto bone = skeleton->findBone(socket->bone);
	if (!bone)
		return socket->offset;
	if (iOwner.hasComponent<component::Animator>()) {
		if (const auto& animator = iOwner.getComponent<component::Animator>();
			animator.hasPose(skeleton->getBoneCount()))
			return animator.boneMatrices[*bone] * socket->offset;
	}
	std::vector<math::mat4> rest(skeleton->getBoneCount());
	skeleton->computeModelMatrices(skeleton->getRestPose(), rest);
	return rest[*bone] * socket->offset;
}

auto Scene::getSocketNames(const Entity& iOwner) const -> std::vector<std::string> {
	std::vector<std::string> names;
	if (!iOwner)
		return names;
	if (iOwner.hasComponent<component::MeshSockets>()) {
		for (const auto& socket: iOwner.getComponent<component::MeshSockets>().sockets) names.push_back(socket.name);
	}
	if (iOwner.hasComponent<component::MeshRenderer>()) {
		if (const auto& mesh = iOwner.getComponent<component::MeshRenderer>().mesh; mesh) {
			for (const auto& socket: mesh->getSockets()) {
				if (std::ranges::find(names, socket.name) == names.end())
					names.push_back(socket.name);
			}
		}
	}
	return names;
}

auto Scene::attachToSocket(const Entity& iEntity, const Entity& iOwner, const std::string& iSocket) -> bool {
	if (!iEntity || !iOwner || iEntity == iOwner ||
		isInSubtree(static_cast<entt::entity>(iOwner), static_cast<entt::entity>(iEntity))) {
		OWL_CORE_WARN("Scene: Cannot attach an entity to the socket {} of itself or of a child.", iSocket)
		return false;
	}
	const auto socket = getSocketTransform(iOwner, iSocket);
	if (!socket) {
		OWL_CORE_WARN("Scene: Entity {} has no socket {}.", iOwner.getName(), iSocket)
		return false;
	}
	if (iEntity.getComponent<component::Hierarchy>().parentId != iOwner.getUUID())
		setParent(iEntity, iOwner);
	iEntity.getComponent<component::Transform>().transform = math::Transform{*socket};
	if (iEntity.hasComponent<component::SocketAttachment>())
		iEntity.getComponent<component::SocketAttachment>().socket = iSocket;
	else
		std::ignore = registry.emplace<component::SocketAttachment>(static_cast<entt::entity>(iEntity),
																	component::SocketAttachment{.socket = iSocket});
	return true;
}

auto Scene::detachFromSocket(const Entity& iEntity) -> bool {
	if (!iEntity || !iEntity.hasComponent<component::SocketAttachment>()) {
		OWL_CORE_WARN("Scene: The entity is attached to no socket.")
		return false;
	}
	registry.remove<component::SocketAttachment>(static_cast<entt::entity>(iEntity));
	return true;
}

void Scene::updateAnimators(const float iSeconds, const bool iPreviewOnly) {
	OWL_PROFILE_FUNCTION()

	for (const auto view = registry.view<component::Animator, component::MeshRenderer>(); const auto handle: view) {
		auto& animator = view.get<component::Animator>(handle);
		const auto& mesh = view.get<component::MeshRenderer>(handle).mesh;
		if (!mesh || !mesh->isSkinned()) {
			if (animator.source != nullptr)
				animator.invalidate();
			continue;
		}
		if (iPreviewOnly && !animator.preview)
			continue;
		const auto& skeleton = *mesh->getSkeleton();
		const auto& clips = mesh->getClips();
		if (!animator.player.isReady() || animator.source != mesh.get()) {
			animator.player.reset(animator.machine, clips);
			animator.source = mesh.get();
		}
		if (animator.playing)
			animator.player.update(animator.machine, clips, iSeconds * animator.speed);
		animator.player.evaluate(animator.machine, skeleton, clips, animator.pose);
		animator.boneMatrices.resize(skeleton.getBoneCount());
		animator.skinMatrices.resize(skeleton.getBoneCount());
		skeleton.computeModelMatrices(animator.pose, animator.boneMatrices);
		skeleton.computeSkinMatrices(animator.boneMatrices, animator.skinMatrices);
	}
}

void Scene::updateSocketAttachments() {
	OWL_PROFILE_FUNCTION()

	for (const auto view = registry.view<component::SocketAttachment, component::Hierarchy, component::Transform>();
		 const auto handle: view) {
		const auto parentId = view.get<component::Hierarchy>(handle).parentId;
		if (parentId == core::UUID{0})
			continue;
		const Entity owner = findEntityByUUID(parentId);
		if (const auto socket = getSocketTransform(owner, view.get<component::SocketAttachment>(handle).socket); socket)
			view.get<component::Transform>(handle).transform = math::Transform{*socket};
	}
}

}// namespace owl::scene
