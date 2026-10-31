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

#include <app/Application.h>
#include <app/EngineContext.h>
#include <math/Transform.h>
#include <math/linAlgebra.h>
#include <math/trigonometry.h>
#include <renderer/Renderer2D.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/SequencePlayer.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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

// The `.owlseq` of a player on disk, in the asset folders of the application.
auto diskPathOf(const std::string& iAsset) -> std::optional<std::filesystem::path> {
	if (iAsset.empty())
		return std::nullopt;
	if (app::Application::instanced())
		for (const auto& [title, folder]: app::Application::get().getAssetDirectories())
			if (const auto candidate = folder / iAsset; exists(candidate))
				return candidate.lexically_normal();
	if (exists(std::filesystem::path{iAsset}))
		return std::filesystem::path{iAsset}.lexically_normal();
	return std::nullopt;
}

// The space of the keys: the player, or its parent when it drives its own camera.
auto anchorOf(const scene::Scene& iScene, const scene::Entity& iPlayer) -> math::mat4 {
	if (iPlayer.getComponent<scene::component::SequencePlayer>().camera != core::UUID{0})
		return iScene.getWorldTransform(iPlayer)();
	if (const auto parentId = iPlayer.getComponent<scene::component::Hierarchy>().parentId; parentId != core::UUID{0})
		if (const auto parent = iScene.findEntityByUUID(parentId); parent)
			return iScene.getWorldTransform(parent)();
	return math::identity<float, 4>();
}

}// namespace

void Viewport::renderSequenceOverlay(const shared<scene::Scene>& iScene) const {
	if (m_parent == nullptr || mp_document == nullptr || mp_document->state() != SceneDocument::State::Edit)
		return;
	const scene::Entity selected = m_parent->getSelectedEntity();
	if (!selected || !selected.hasComponent<scene::component::SequencePlayer>())
		return;
	const auto& asset = selected.getComponent<scene::component::SequencePlayer>().sequence;
	const Sequence* sequence = nullptr;
	shared<const Sequence> loaded;
	if (const auto path = diskPathOf(asset); path) {
		for (const auto& document: m_parent->getDocumentManager().list())
			if (document && document->type() == DocumentType::Sequence &&
				document->filePath().lexically_normal() == *path)
				sequence = &static_cast<const SequenceDocument*>(document.get())->sequence();
	}
	if (sequence == nullptr && !asset.empty() && app::Application::instanced()) {
		loaded = app::Application::get().getEngineContext().getSequences().get(asset);
		sequence = loaded.get();
	}
	if (sequence == nullptr)
		return;
	const auto id = static_cast<int>(static_cast<uint32_t>(selected));
	const math::mat4 anchor = anchorOf(*iScene, selected);
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
			const math::vec3 eye = transformPoint(frame, {0.f, 0.f, 0.f});
			const float size = getLightMarkerRadius(eye);
			for (const auto& corner: {math::vec3{-0.6f, -0.4f, -1.f}, math::vec3{0.6f, -0.4f, -1.f},
									  math::vec3{0.6f, 0.4f, -1.f}, math::vec3{-0.6f, 0.4f, -1.f}})
				renderer::Renderer2D::drawLine({.point1 = eye,
												.point2 = transformPoint(frame, corner * size),
												.color = color,
												.entityId = id});
		}
	}
}

auto Viewport::addSequenceKeyFromView() -> bool {
	if (m_parent == nullptr || mp_document == nullptr)
		return false;
	const scene::Entity selected = m_parent->getSelectedEntity();
	const auto& scene = mp_document->getActiveScene();
	if (!selected || !scene || !selected.hasComponent<scene::component::SequencePlayer>())
		return false;
	const auto path = diskPathOf(selected.getComponent<scene::component::SequencePlayer>().sequence);
	if (!path) {
		OWL_WARN("Viewport: The sequence player names no sequence file.")
		return false;
	}
	auto* editor = m_parent->openSequence(*path);
	if (editor == nullptr)
		return false;
	const math::mat4 view = math::inverse(getActiveCamera().getView());
	const math::Transform local{math::inverse(anchorOf(*scene, selected)) * view};
	const auto& sequence = editor->sequence();
	int64_t track = static_cast<int64_t>(sequence.cameraTracks.size()) - 1;
	if (track < 0) {
		if (!m_parent->execute("sequence.add_track", {{"path", path->generic_string()}},
							   commands::CommandSource::Gesture))
			return false;
		track = 0;
	}
	const auto& keys = editor->sequence().cameraTracks[static_cast<size_t>(track)].keys;
	const float time = keys.empty() ? 0.f : keys.back().time + 1.f;
	if (time > editor->sequence().duration)
		std::ignore = m_parent->execute("sequence.set_duration",
										{{"path", path->generic_string()}, {"duration", static_cast<double>(time)}},
										commands::CommandSource::Gesture);
	const bool done = m_parent->execute("sequence.add_key",
										{{"path", path->generic_string()},
										 {"track", track},
										 {"time", static_cast<double>(time)},
										 {"position", local.translation()},
										 {"rotation", math::degrees(local.rotation())}},
										commands::CommandSource::Gesture)
							  .has_value();
	m_parent->getDocumentManager().setActive(mp_document);
	mp_document->requestFocus();
	return done;
}

}// namespace owl::nest::panel
