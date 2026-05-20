/**
 * @file CurveEditor.cpp
 * @author Silmaen
 * @date 26/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/widgets/CurveEditor.h"

#include <ImCurveEdit.h>

namespace owl::gui::widgets {

namespace {
class CurveDelegate final : public ImCurveEdit::Delegate {
public:
	explicit CurveDelegate(math::Curve& iCurve) {
		m_buffer.reserve(iCurve.keyCount());
		for (const auto& key: iCurve.keys()) m_buffer.emplace_back(key.time, key.value);
		fitRangeToBuffer();
	}

	[[nodiscard]] auto buffer() const -> const std::vector<ImVec2>& { return m_buffer; }

	[[nodiscard]] auto dirty() const -> bool { return m_dirty; }

	void setCurveType(const ImCurveEdit::CurveType iType) { m_type = iType; }

	auto GetCurveCount() -> size_t override { return 1; }

	[[nodiscard]] auto GetCurveType(size_t /*iCurveIndex*/) const -> ImCurveEdit::CurveType override { return m_type; }

	auto GetMin() -> ImVec2& override { return m_min; }

	auto GetMax() -> ImVec2& override { return m_max; }

	auto GetPointCount(size_t /*iCurveIndex*/) -> size_t override { return m_buffer.size(); }

	auto GetCurveColor(size_t /*iCurveIndex*/) -> uint32_t override { return IM_COL32(255, 199, 38, 255); }

	auto GetPoints(size_t /*iCurveIndex*/) -> ImVec2* override { return m_buffer.data(); }

	auto EditPoint(size_t /*iCurveIndex*/, const int iPointIndex, const ImVec2 iValue) -> int override {
		if (iPointIndex < 0 || static_cast<size_t>(iPointIndex) >= m_buffer.size())
			return -1;
		m_buffer[static_cast<size_t>(iPointIndex)] = iValue;
		m_dirty = true;
		return iPointIndex;
	}

	void AddPoint(size_t /*iCurveIndex*/, const ImVec2 iValue) override {
		m_buffer.push_back(iValue);
		m_dirty = true;
	}

private:
	void fitRangeToBuffer() {
		if (m_buffer.empty()) {
			m_min = {0.f, -1.f};
			m_max = {1.f, 1.f};
			return;
		}
		float xMin = m_buffer.front().x;
		float xMax = m_buffer.front().x;
		float yMin = m_buffer.front().y;
		float yMax = m_buffer.front().y;
		for (const auto& point: m_buffer) {
			xMin = std::min(xMin, point.x);
			xMax = std::max(xMax, point.x);
			yMin = std::min(yMin, point.y);
			yMax = std::max(yMax, point.y);
		}
		// Always show the full normalized progress range.
		xMin = std::min(xMin, 0.f);
		xMax = std::max(xMax, 1.f);
		// Y margin: 20% of the spread, with a 0.5 absolute floor.
		const float yMargin = std::max(0.2f * (yMax - yMin), 0.5f);
		yMin -= yMargin;
		yMax += yMargin;
		// Always include the zero baseline so positive-only curves still show it.
		yMin = std::min(yMin, 0.f);
		yMax = std::max(yMax, 0.f);
		m_min = {xMin, yMin};
		m_max = {xMax, yMax};
	}

	std::vector<ImVec2> m_buffer;
	ImVec2 m_min{0.f, -1.f};
	ImVec2 m_max{1.f, 1.f};
	ImCurveEdit::CurveType m_type{ImCurveEdit::CurveLinear};
	bool m_dirty{false};
};

auto toImCurveType(const math::CurveInterpolation iMode) -> ImCurveEdit::CurveType {
	switch (iMode) {
		case math::CurveInterpolation::Constant:
			return ImCurveEdit::CurveDiscrete;
		case math::CurveInterpolation::Linear:
			return ImCurveEdit::CurveLinear;
		case math::CurveInterpolation::Smooth:
			return ImCurveEdit::CurveSmooth;
	}
	return ImCurveEdit::CurveLinear;
}

}// namespace

auto curveEditor(const char* iLabel, math::Curve& ioCurve, const ImVec2 iSize) -> bool {
	bool changed = false;
	ImGui::PushID(iLabel);
	CurveDelegate delegate(ioCurve);
	delegate.setCurveType(toImCurveType(ioCurve.getInterpolation()));
	ImCurveEdit::Edit(delegate, iSize, ImGui::GetID("##canvas"));
	if (delegate.dirty()) {
		ioCurve.clear();
		for (const auto& point: delegate.buffer()) ioCurve.addKey({point.x, point.y});
		changed = true;
	}
	constexpr std::array<const char*, 3> modeNames{"Constant", "Linear", "Smooth"};
	int modeIndex = static_cast<int>(ioCurve.getInterpolation());
	if (ImGui::Combo("Interpolation", &modeIndex, modeNames.data(), static_cast<int>(modeNames.size()))) {
		ioCurve.setInterpolation(static_cast<math::CurveInterpolation>(modeIndex));
		changed = true;
	}
	if (!ioCurve.empty() && ImGui::SmallButton("Clear##curve")) {
		ioCurve.clear();
		changed = true;
	}
	ImGui::PopID();
	return changed;
}

}// namespace owl::gui::widgets
