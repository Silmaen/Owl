/**
 * @file FrameBenchStats.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstddef>
#include <vector>

namespace owl::nest::runner {

/**
 * @brief
 *  Order statistics of one per-frame series.
 */
struct SeriesSummary {
	/// Number of samples.
	size_t count{0};
	/// Median (p50).
	double median{0.0};
	/// 95th percentile.
	double p95{0.0};
	/// 99th percentile.
	double p99{0.0};
	/// Interquartile range (p75 - p25).
	double iqr{0.0};
	/// Arithmetic mean.
	double mean{0.0};
	/// Smallest sample.
	double min{0.0};
	/// Largest sample.
	double max{0.0};
};

/**
 * @brief
 *  Percentile of a sorted series, linear interpolation between the closest ranks.
 * @param[in] iSorted The samples, sorted in ascending order.
 * @param[in] iFraction The percentile as a fraction in [0, 1].
 * @return The percentile, 0 for an empty series.
 */
[[nodiscard]] auto percentile(const std::vector<double>& iSorted, double iFraction) -> double;

/**
 * @brief
 *  Summarise a series.
 * @param[in] iSamples The samples, in any order.
 * @return The order statistics, all zero for an empty series.
 */
[[nodiscard]] auto summarize(std::vector<double> iSamples) -> SeriesSummary;

}// namespace owl::nest::runner
