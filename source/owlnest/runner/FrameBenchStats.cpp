/**
 * @file FrameBenchStats.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "FrameBenchStats.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace owl::nest::runner {

auto percentile(const std::vector<double>& iSorted, const double iFraction) -> double {
	if (iSorted.empty())
		return 0.0;
	const double rank = std::clamp(iFraction, 0.0, 1.0) * static_cast<double>(iSorted.size() - 1);
	const auto low = static_cast<size_t>(std::floor(rank));
	const size_t high = std::min(low + 1, iSorted.size() - 1);
	const double weight = rank - static_cast<double>(low);
	return iSorted[low] + (iSorted[high] - iSorted[low]) * weight;
}

auto summarize(std::vector<double> iSamples) -> SeriesSummary {
	SeriesSummary summary;
	if (iSamples.empty())
		return summary;
	std::ranges::sort(iSamples);
	summary.count = iSamples.size();
	summary.median = percentile(iSamples, 0.5);
	summary.p95 = percentile(iSamples, 0.95);
	summary.p99 = percentile(iSamples, 0.99);
	summary.iqr = percentile(iSamples, 0.75) - percentile(iSamples, 0.25);
	summary.mean = std::accumulate(iSamples.begin(), iSamples.end(), 0.0) / static_cast<double>(iSamples.size());
	summary.min = iSamples.front();
	summary.max = iSamples.back();
	return summary;
}

}// namespace owl::nest::runner
