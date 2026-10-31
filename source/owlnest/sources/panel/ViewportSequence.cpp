/**
 * @file ViewportSequence.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"
#include "Viewport.h"
#include "document/SceneDocument.h"
#include "document/SequenceDocument.h"
#include "document/SequencePreview.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <math/Transform.h>
#include <math/linAlgebra.h>
#include <math/trigonometry.h>
#include <renderer/Renderer2D.h>
#include <scene/SequencePlayback.h>
#include <scene/component/Camera.h>
#include <scene/component/SequencePlayer.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>

namespace owl::nest::panel {

namespace {

using data::sequence::Sequence;

auto transformPoint(const math::mat4& iMatrix, const math::vec3& iPoint) -> math::vec3 {
	const math::vec4 point = iMatrix * math::vec4{iPoint.x(), iPoint.y(), iPoint.z(), 1.f};
	return {point.x(), point.y(), point.z()};
}

// A camera frustum of the overlay: lines from the eye to the corners of a quad in front of it.
void drawFrustum(const math::mat4& iFrame, const float iSize, const math::vec4& iColor, const int iId) {
	const math::vec3 eye = transformPoint(iFrame, {0.f, 0.f, 0.f});
	std::array<math::vec3, 4> corners{};
	size_t index = 0;
	for (const auto& corner: {math::vec3{-0.6f, -0.4f, -1.f}, math::vec3{0.6f, -0.4f, -1.f},
							  math::vec3{0.6f, 0.4f, -1.f}, math::vec3{-0.6f, 0.4f, -1.f}})
		corners[index++] = transformPoint(iFrame, corner * iSize);
	for (size_t corner = 0; corner < corners.size(); ++corner) {
		renderer::Renderer2D::drawLine({.point1 = eye, .point2 = corners[corner], .color = iColor, .entityId = iId});
		renderer::Renderer2D::drawLine({.point1 = corners[corner],
										.point2 = corners[(corner + 1) % corners.size()],
										.color = iColor,
										.entityId = iId});
	}
}

}// namespace

auto Viewport::findPreviewSequence() const -> SequenceDocument* {
	if (m_parent == nullptr || mp_document == nullptr)
		return nullptr;
	const auto& documents = m_parent->getDocumentManager();
	if (auto* active = documents.getActive();
		active != nullptr && active->type() == DocumentType::Sequence &&
		static_cast<SequenceDocument*>(active)->getPreviewDocument() == mp_document)
		return static_cast<SequenceDocument*>(active);
	for (const auto& document: documents.list())
		if (document && document->type() == DocumentType::Sequence &&
			static_cast<SequenceDocument*>(document.get())->getPreviewDocument() == mp_document)
			return static_cast<SequenceDocument*>(document.get());
	return nullptr;
}

void Viewport::renderSequenceOverlay(const shared<scene::Scene>& iScene) const {
	if (m_parent == nullptr || mp_document == nullptr || mp_document->state() != SceneDocument::State::Edit)
		return;
	scene::Entity player;
	const Sequence* sequence = nullptr;
	shared<const Sequence> loaded;
	std::optional<float> playhead;
	if (const auto* preview = findPreviewSequence(); preview != nullptr && preview->getPreviewPlayer()) {
		player = preview->getPreviewPlayer();
		sequence = &preview->sequence();
		playhead = preview->getTime();
	} else if (const scene::Entity selected = m_parent->getSelectedEntity();
			   selected && selected.hasComponent<scene::component::SequencePlayer>() && app::Application::instanced()) {
		player = selected;
		loaded = app::Application::get().getEngineContext().getSequences().get(
				selected.getComponent<scene::component::SequencePlayer>().sequence);
		sequence = loaded.get();
	}
	if (!player || sequence == nullptr)
		return;
	const auto id = static_cast<int>(static_cast<uint32_t>(player));
	const math::mat4 anchor = getSequenceAnchor(*iScene, player);
	constexpr std::array palette{math::vec4{0.92f, 0.65f, 0.24f, 1.f}, math::vec4{0.35f, 0.64f, 0.94f, 1.f},
								 math::vec4{0.3f, 0.72f, 0.51f, 1.f}, math::vec4{0.9f, 0.33f, 0.29f, 1.f}};
	for (size_t index = 0; index < sequence->cameraTracks.size(); ++index) {
		const auto& track = sequence->cameraTracks[index];
		if (track.keys.empty())
			continue;
		auto color = palette[index % palette.size()];
		if (track.muted)
			color.w() = 0.35f;
		constexpr int steps = 64;
		math::vec3 previous = transformPoint(anchor, track.keys.front().position);
		for (int step = 1; step <= steps; ++step) {
			const float time = track.getStart() + (track.getEnd() - track.getStart()) * static_cast<float>(step) /
														  static_cast<float>(steps);
			const math::vec3 next = transformPoint(anchor, track.sample(time)->position);
			renderer::Renderer2D::drawLine({.point1 = previous, .point2 = next, .color = color, .entityId = id});
			previous = next;
		}
		for (const auto& key: track.keys) {
			const math::mat4 frame = anchor * math::Transform{key.position, key.rotation}();
			drawFrustum(frame, getLightMarkerRadius(transformPoint(frame, {0.f, 0.f, 0.f})), color, id);
		}
	}
	if (!playhead || m_sequenceView)
		return;
	if (const auto pose = scene::getSequenceCameraPose(player, *sequence, *playhead); pose)
		drawFrustum(pose->world, 1.6f * getLightMarkerRadius(transformPoint(pose->world, {0.f, 0.f, 0.f})),
					{1.f, 1.f, 1.f, 1.f}, id);
}

auto Viewport::updateSequenceView(const shared<scene::Scene>& iScene) -> bool {
	const auto* preview = findPreviewSequence();
	const scene::Entity player = preview != nullptr ? preview->getPreviewPlayer() : scene::Entity{};
	const auto pose = player && preview->isLookingThrough() && !m_isometric
							  ? scene::getSequenceCameraPose(player, preview->sequence(), preview->getTime())
							  : std::nullopt;
	if (!pose) {
		if (m_sequenceView) {
			m_sequenceView = false;
			const math::mat4 world = math::inverse(m_sequenceCamera.getView());
			const math::vec4 forward = world * math::vec4{0.f, 0.f, -1.f, 0.f};
			m_editorCamera.lookFrom(transformPoint(world, {0.f, 0.f, 0.f}), {forward.x(), forward.y(), forward.z()});
		}
		return false;
	}
	if (m_pilot.isActive())
		stopPiloting();
	const auto& component = player.getComponent<scene::component::SequencePlayer>();
	const scene::Entity camera =
			component.camera != core::UUID{0} ? iScene->findEntityByUUID(component.camera) : player;
	if (camera && camera.hasComponent<scene::component::Camera>())
		m_sequenceCamera = camera.getComponent<scene::component::Camera>().camera;
	else
		m_sequenceCamera = {};
	if (m_sequenceCamera.getProjectionType() == scene::SceneCamera::ProjectionType::Perspective)
		m_sequenceCamera.setPerspectiveVerticalFov(pose->fov);
	else
		m_sequenceCamera.setOrthographicSize(pose->size);
	m_sequenceCamera.setViewportSize(getSize());
	m_sequenceCamera.setTransform(pose->world);
	m_sequenceView = true;
	return true;
}

auto Viewport::addSequenceKeyFromView() -> bool {
	if (m_parent == nullptr || mp_document == nullptr)
		return false;
	const auto& scene = mp_document->getActiveScene();
	auto* sequence = findPreviewSequence();
	if (sequence == nullptr) {
		const scene::Entity selected = m_parent->getSelectedEntity();
		if (!scene || !selected || !selected.hasComponent<scene::component::SequencePlayer>())
			return false;
		const auto path = getSequenceDiskPath(selected.getComponent<scene::component::SequencePlayer>().sequence);
		if (!path) {
			OWL_WARN("Viewport: The sequence player names no sequence file.")
			return false;
		}
		m_parent->openSequenceFile(*path);
		sequence = findPreviewSequence();
	}
	if (sequence == nullptr || !scene || !sequence->getPreviewPlayer())
		return false;
	const math::mat4 view = math::inverse(getActiveCamera().getView());
	const math::Transform local{math::inverse(getSequenceAnchor(*scene, sequence->getPreviewPlayer())) * view};
	return sequence->setKeyAtPlayhead(local.translation(), local.rotation());
}

}// namespace owl::nest::panel
