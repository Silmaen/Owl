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

// Written only when a number leaves the pixels, so a file without ratios stays as it was.
void emitUnits(YAML::Emitter& ioEmitter, const char* iKey, const UiRect::Units& iUnits) {
	if (iUnits.isPixels())
		return;
	ioEmitter << YAML::Key << iKey << YAML::Value << YAML::Flow << YAML::BeginSeq
			  << std::string{UiRect::getUnitName(iUnits.x)} << std::string{UiRect::getUnitName(iUnits.y)}
			  << YAML::EndSeq;
}

// An absent key, a wrong shape or an unknown name leaves pixels.
void readUnits(const core::YamlNode& iNode, const char* iKey, UiRect::Units& oUnits) {
	oUnits = {};
	const auto seq = iNode[iKey];
	if (!seq.isSequence() || seq.size() < 2)
		return;
	const auto unitOf = [](const core::YamlNode& iItem) -> UiRect::Unit {
		return magic_enum::enum_cast<UiRect::Unit>(iItem.as<std::string>(std::string{})).value_or(UiRect::Unit::Pixels);
	};
	oUnits.x = unitOf(seq[0]);
	oUnits.y = unitOf(seq[1]);
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

auto UiRect::getUnitName(const Unit iUnit) -> std::string_view { return magic_enum::enum_name(iUnit); }

auto UiRect::parseUnit(const std::string_view iName) -> std::optional<Unit> {
	return magic_enum::enum_cast<Unit>(iName);
}

auto UiRect::getUnitExtent(const Unit iUnit, const Frame& iFrame) -> float {
	switch (iUnit) {
		case Unit::Pixels:
			return 1.f;
		case Unit::ParentWidth:
			return iFrame.parent.size.x();
		case Unit::ParentHeight:
			return iFrame.parent.size.y();
		case Unit::ScreenWidth:
			return iFrame.screen.x();
		case Unit::ScreenHeight:
			return iFrame.screen.y();
	}
	return 1.f;
}

namespace {

auto toPixels(const math::vec2& iValue, const UiRect::Units& iUnits, const UiRect::Frame& iFrame) -> math::vec2 {
	return {iValue.x() * UiRect::getUnitExtent(iUnits.x, iFrame), iValue.y() * UiRect::getUnitExtent(iUnits.y, iFrame)};
}

// An empty extent (a collapsed parent) cannot hold a ratio: the value falls back to 0.
auto fromPixels(const float iPixels, const UiRect::Unit iUnit, const UiRect::Frame& iFrame) -> float {
	const float extent = UiRect::getUnitExtent(iUnit, iFrame);
	return extent > 0.f ? iPixels / extent : 0.f;
}

void convertNumber(float& ioValue, UiRect::Units& ioUnits, const UiRect::Axis iAxis, const UiRect::Unit iUnit,
				   const UiRect::Frame& iFrame) {
	const float pixels = ioValue * UiRect::getUnitExtent(ioUnits.get(iAxis), iFrame);
	ioUnits.set(iAxis, iUnit);
	ioValue = fromPixels(pixels, iUnit, iFrame);
}

}// namespace

auto UiRect::getPixelSize(const Frame& iFrame) const -> math::vec2 { return toPixels(size, sizeUnit, iFrame); }

auto UiRect::getPixelOffset(const Frame& iFrame) const -> math::vec2 {
	return toPixels(anchorOffset, offsetUnit, iFrame);
}

void UiRect::setPixelSize(const math::vec2& iPixels, const Frame& iFrame) {
	size = {fromPixels(iPixels.x(), sizeUnit.x, iFrame), fromPixels(iPixels.y(), sizeUnit.y, iFrame)};
}

void UiRect::setPixelOffset(const math::vec2& iPixels, const Frame& iFrame) {
	anchorOffset = {fromPixels(iPixels.x(), offsetUnit.x, iFrame), fromPixels(iPixels.y(), offsetUnit.y, iFrame)};
}

void UiRect::convertSizeUnit(const Axis iAxis, const Unit iUnit, const Frame& iFrame) {
	convertNumber(iAxis == Axis::X ? size.x() : size.y(), sizeUnit, iAxis, iUnit, iFrame);
}

void UiRect::convertOffsetUnit(const Axis iAxis, const Unit iUnit, const Frame& iFrame) {
	convertNumber(iAxis == Axis::X ? anchorOffset.x() : anchorOffset.y(), offsetUnit, iAxis, iUnit, iFrame);
}

void UiRect::moveBy(const math::vec2& iDelta, const Frame& iFrame) {
	setPixelOffset(getPixelOffset(iFrame) + iDelta, iFrame);
}

auto UiRect::computeBounds(const Bounds& iParent) const -> Bounds { return computeBounds(iParent, iParent.size); }

auto UiRect::computeBounds(const Bounds& iParent, const math::vec2& iScreen) const -> Bounds {
	const Frame frame{.parent = iParent, .screen = iScreen};
	const math::vec2 pixelSize = getPixelSize(frame);
	const math::vec2 pixelOffset = getPixelOffset(frame);
	const math::vec2 cornerMin = iParent.position + scaled(iParent.size, anchorMin);
	const math::vec2 cornerMax = iParent.position + scaled(iParent.size, anchorMax);
	const math::vec2 extent = cornerMax - cornerMin + pixelSize;
	const math::vec2 pivotPoint = cornerMin + scaled(cornerMax - cornerMin, pivot) + pixelOffset;
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
	emitUnits(emitter, "sizeUnit", sizeUnit);
	emitUnits(emitter, "offsetUnit", offsetUnit);
	emitter << YAML::EndMap;
}

void UiRect::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	readVec2(node, "anchorMin", anchorMin);
	readVec2(node, "anchorMax", anchorMax);
	readVec2(node, "pivot", pivot);
	readVec2(node, "size", size);
	readVec2(node, "offset", anchorOffset);
	readUnits(node, "sizeUnit", sizeUnit);
	readUnits(node, "offsetUnit", offsetUnit);
}

}// namespace owl::scene::component
