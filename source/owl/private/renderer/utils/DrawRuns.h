/**
 * @file DrawRuns.h
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace owl::renderer::utils {

/**
 * @brief
 *  Kind of a 2D primitive; also the index of its instance buffer.
 */
enum struct RunKind : uint8_t {
	/// Textured or plain quad.
	Quad,
	/// Circle.
	Circle,
	/// Line segment.
	Line,
	/// Glyph of a string.
	Text
};

/**
 * @brief
 *  Consecutive primitives of one kind and one sort key: a contiguous range of that kind's instances.
 */
struct DrawRun {
	/// Sort key of the primitives of the run.
	float depth = 0.f;
	/// Index of the first instance in the buffer of its kind.
	uint32_t begin = 0;
	/// Amount of instances.
	uint32_t count = 0;
	/// Submission rank of the run, the stable tie-break of the sort.
	uint32_t seq = 0;
	/// Kind of the instances.
	RunKind kind = RunKind::Quad;
};

/**
 * @brief
 *  Draw order of the 2D primitives of a batch: depth first, then type, then submission order.
 *
 * Every primitive is noted as it is submitted. While all of them share one depth (the common case) nothing but the
 * count per type is kept and each type draws as one range, quads, circles, lines then text. Once a second depth
 * appears, consecutive primitives of one type and one depth form a run; the runs are drawn by increasing depth, then
 * by type, then in submission order, and runs of one type that touch in their buffer merge into one draw.
 */
class DrawRuns {
public:
	/**
	 * @brief
	 *  Reserve room for runs, so noting them allocates nothing up to that count.
	 * @param[in] iCount Amount of runs.
	 */
	void reserve(const size_t iCount) { m_runs.reserve(iCount); }

	/**
	 * @brief
	 *  Forget every primitive.
	 */
	void clear() {
		m_runs.clear();
		m_totals.fill(0);
		m_uniform = true;
		m_inOrder = true;
	}

	/**
	 * @brief
	 *  Amount of runs (none while every primitive shares one depth).
	 * @return The run count.
	 */
	[[nodiscard]] auto size() const -> size_t { return m_runs.size(); }

	/**
	 * @brief
	 *  Note the instance just appended to the buffer of its kind.
	 * @param[in] iKind Kind of the instance.
	 * @param[in] iDepth Sort key.
	 */
	void note(const RunKind iKind, const float iDepth) {
		const auto kind = static_cast<size_t>(iKind);
		const uint32_t position = m_totals[kind]++;
		if (m_uniform) {
			if (total() == 1) {
				m_depth = iDepth;
				return;
			}
			if (!(iDepth < m_depth) && !(m_depth < iDepth))
				return;
			split(kind);
		}
		if (!m_runs.empty() && m_runs.back().kind == iKind && !(m_runs.back().depth < iDepth) &&
			!(iDepth < m_runs.back().depth)) {
			++m_runs.back().count;
			return;
		}
		open(iKind, iDepth, position);
	}

	/**
	 * @brief
	 *  Visit the draws in order. Sorts the runs when they are not in order.
	 * @tparam DrawT Callable taking the kind, the first instance and the instance count.
	 * @param[in] iDraw Called once per draw.
	 */
	template<typename DrawT>
	void forEachDraw(DrawT&& iDraw) {
		if (m_uniform) {
			for (size_t kind = 0; kind < m_totals.size(); ++kind)
				if (m_totals[kind] != 0)
					iDraw(static_cast<RunKind>(kind), uint32_t{0}, m_totals[kind]);
			return;
		}
		if (!m_inOrder) {
			std::ranges::sort(m_runs, [](const DrawRun& iA, const DrawRun& iB) -> bool {
				if (iA.depth < iB.depth || iB.depth < iA.depth)
					return iA.depth < iB.depth;
				return iA.kind != iB.kind ? iA.kind < iB.kind : iA.seq < iB.seq;
			});
			m_inOrder = true;
		}
		DrawRun pending{};
		for (const auto& run: m_runs) {
			if (pending.count != 0 && pending.kind == run.kind && pending.begin + pending.count == run.begin) {
				pending.count += run.count;
				continue;
			}
			if (pending.count != 0)
				iDraw(pending.kind, pending.begin, pending.count);
			pending = run;
		}
		if (pending.count != 0)
			iDraw(pending.kind, pending.begin, pending.count);
	}

private:
	/**
	 * @brief
	 *  Amount of instances noted, every kind together.
	 * @return The total.
	 */
	[[nodiscard]] auto total() const -> uint32_t { return m_totals[0] + m_totals[1] + m_totals[2] + m_totals[3]; }

	/**
	 * @brief
	 *  Leave the single-depth mode: the primitives noted so far become one run per kind, at that depth.
	 * @param[in] iNewKind Kind of the instance being noted, already counted in the totals.
	 */
	void split(const size_t iNewKind) {
		m_uniform = false;
		for (size_t kind = 0; kind < m_totals.size(); ++kind) {
			const uint32_t count = m_totals[kind] - (kind == iNewKind ? 1u : 0u);
			if (count != 0)
				m_runs.push_back({.depth = m_depth,
								  .begin = 0,
								  .count = count,
								  .seq = static_cast<uint32_t>(m_runs.size()),
								  .kind = static_cast<RunKind>(kind)});
		}
	}

	/**
	 * @brief
	 *  Open a run for the instance just appended (out of line: the common case extends the last run).
	 * @param[in] iKind Kind of the instance.
	 * @param[in] iDepth Sort key.
	 * @param[in] iPosition Index of the instance in the buffer of its kind.
	 */
	[[gnu::noinline]] void open(const RunKind iKind, const float iDepth, const uint32_t iPosition) {
		if (!m_runs.empty()) {
			const auto& last = m_runs.back();
			if (iDepth < last.depth || (!(last.depth < iDepth) && iKind < last.kind))
				m_inOrder = false;
		}
		m_runs.push_back({.depth = iDepth,
						  .begin = iPosition,
						  .count = 1,
						  .seq = static_cast<uint32_t>(m_runs.size()),
						  .kind = iKind});
	}

	/// Runs in submission order (empty in single-depth mode).
	std::vector<DrawRun> m_runs;
	/// Instances noted per kind.
	std::array<uint32_t, 4> m_totals{};
	/// Depth shared by every primitive in single-depth mode.
	float m_depth = 0.f;
	/// True while every primitive shares one depth.
	bool m_uniform = true;
	/// True while the runs are already in draw order.
	bool m_inOrder = true;
};

}// namespace owl::renderer::utils
