/**
 * @file UiRect.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/UiRect.h"

#include "Vec2Yaml.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

namespace owl::scene::component {

namespace {

auto scaled(const math::vec2& iValue, const math::vec2& iFactor) -> math::vec2 {
	return {iValue.x() * iFactor.x(), iValue.y() * iFactor.y()};
}

}// namespace

auto UiRect::Bounds::contains(const math::vec2& iPoint) const -> bool {
	return iPoint.x() >= position.x() && iPoint.x() <= position.x() + size.x() && iPoint.y() >= position.y() &&
		   iPoint.y() <= position.y() + size.y();
}

auto UiRect::Bounds::intersect(const Bounds& iOther) const -> Bounds {
	const float left = std::max(position.x(), iOther.position.x());
	const float top = std::max(position.y(), iOther.position.y());
	const float right = std::min(position.x() + size.x(), iOther.position.x() + iOther.size.x());
	const float bottom = std::min(position.y() + size.y(), iOther.position.y() + iOther.size.y());
	return {.position = {left, top}, .size = {std::max(0.f, right - left), std::max(0.f, bottom - top)}};
}

auto UiRect::Bounds::getCenter() const -> math::vec2 { return position + size * 0.5f; }

auto UiRect::getAnchorPoint(const Anchor iAnchor) -> math::vec2 {
	switch (iAnchor) {
		case Anchor::TopLeft:
			return {0.f, 0.f};
		case Anchor::TopCenter:
			return {0.5f, 0.f};
		case Anchor::TopRight:
			return {1.f, 0.f};
		case Anchor::MiddleLeft:
			return {0.f, 0.5f};
		case Anchor::Center:
			return {0.5f, 0.5f};
		case Anchor::MiddleRight:
			return {1.f, 0.5f};
		case Anchor::BottomLeft:
			return {0.f, 1.f};
		case Anchor::BottomCenter:
			return {0.5f, 1.f};
		case Anchor::BottomRight:
			return {1.f, 1.f};
	}
	return {0.5f, 0.5f};
}

void UiRect::setAnchor(const Anchor iAnchor) {
	anchorMin = getAnchorPoint(iAnchor);
	anchorMax = anchorMin;
}

auto UiRect::getAnchor() const -> std::optional<Anchor> {
	constexpr float tolerance = 1e-4f;
	const auto near = [](const math::vec2& iA, const math::vec2& iB) -> bool {
		return std::abs(iA.x() - iB.x()) < tolerance && std::abs(iA.y() - iB.y()) < tolerance;
	};
	if (!near(anchorMin, anchorMax))
		return std::nullopt;
	for (const auto preset: magic_enum::enum_values<Anchor>()) {
		if (near(anchorMin, getAnchorPoint(preset)))
			return preset;
	}
	return std::nullopt;
}

auto UiRect::computeBounds(const Bounds& iParent) const -> Bounds {
	const math::vec2 cornerMin = iParent.position + scaled(iParent.size, anchorMin);
	const math::vec2 cornerMax = iParent.position + scaled(iParent.size, anchorMax);
	const math::vec2 extent = cornerMax - cornerMin + size;
	const math::vec2 pivotPoint = cornerMin + scaled(cornerMax - cornerMin, pivot) + anchorOffset;
	return {.position = pivotPoint - scaled(extent, pivot), .size = extent};
}

void UiRect::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;
	emitVec2(emitter, "anchorMin", anchorMin);
	emitVec2(emitter, "anchorMax", anchorMax);
	emitVec2(emitter, "pivot", pivot);
	emitVec2(emitter, "size", size);
	emitVec2(emitter, "offset", anchorOffset);
	emitter << YAML::EndMap;
}

void UiRect::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	readVec2(node, "anchorMin", anchorMin);
	readVec2(node, "anchorMax", anchorMax);
	readVec2(node, "pivot", pivot);
	readVec2(node, "size", size);
	readVec2(node, "offset", anchorOffset);
}

}// namespace owl::scene::component
