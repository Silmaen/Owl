/**
 * @file Bench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "harness/Bench.h"

#include <cmath>
#include <cstdio>
#include <format>
#include <fstream>
#include <print>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace owl::bench {

namespace {

auto quantile(const std::vector<double>& iSorted, const double iQ) -> double {
	if (iSorted.empty())
		return 0.0;
	const double pos = iQ * static_cast<double>(iSorted.size() - 1);
	const auto low = static_cast<size_t>(std::floor(pos));
	const size_t high = std::min(low + 1, iSorted.size() - 1);
	const double frac = pos - static_cast<double>(low);
	return iSorted[low] + (iSorted[high] - iSorted[low]) * frac;
}

auto jsonEscape(const std::string& iText) -> std::string {
	std::string out;
	out.reserve(iText.size());
	for (const char c: iText) {
		if (c == '"' || c == '\\')
			out.push_back('\\');
		if (c == '\n') {
			out += "\\n";
			continue;
		}
		out.push_back(c);
	}
	return out;
}

auto iqrPercent(const Result& iResult) -> double {
	return iResult.medianNs > 0.0 ? 100.0 * (iResult.p75Ns - iResult.p25Ns) / iResult.medianNs : 0.0;
}

}// namespace

Runner::Runner(Options iOptions) : m_options{std::move(iOptions)}, m_loadAtStart{loadAverage()} {
	std::println("{:<58} {:>12} {:>12} {:>8} {:>12} {:>10}", "benchmark", "median", "min", "IQR%", "per item", "batch");
}

auto Runner::wants(const std::string& iName) const -> bool {
	if (!m_options.exclude.empty() && iName.find(m_options.exclude) != std::string::npos)
		return false;
	if (m_options.filter.empty())
		return true;
	return iName.find(m_options.filter) != std::string::npos || m_options.filter.find(iName) == 0;
}

void Runner::record(const std::string& iName, const uint64_t iItems, const uint64_t iBatch,
					std::vector<double> iSamples) {
	std::ranges::sort(iSamples);
	Result res{.name = iName,
			   .items = iItems,
			   .batch = iBatch,
			   .samples = static_cast<uint32_t>(iSamples.size()),
			   .medianNs = quantile(iSamples, 0.5),
			   .p25Ns = quantile(iSamples, 0.25),
			   .p75Ns = quantile(iSamples, 0.75),
			   .minNs = iSamples.empty() ? 0.0 : iSamples.front(),
			   .maxNs = iSamples.empty() ? 0.0 : iSamples.back()};
	const double perItem = res.medianNs / static_cast<double>(std::max<uint64_t>(iItems, 1));
	std::println("{:<58} {:>12} {:>12} {:>7.1f}% {:>12} {:>10}", iName, formatDuration(res.medianNs),
				 formatDuration(res.minNs), iqrPercent(res), formatDuration(perItem), iBatch);
	std::fflush(stdout);
	m_results.push_back(std::move(res));
}

void Runner::metric(const std::string& iName, const double iValue, const std::string& iUnit) {
	if (!wants(iName))
		return;
	std::println("{:<58} {:>12.6g} {}", iName, iValue, iUnit);
	std::fflush(stdout);
	m_metrics.push_back({.name = iName, .value = iValue, .unit = iUnit});
}

void Runner::writeReports() const {
	if (!m_options.csvPath.empty()) {
		std::ofstream csv(m_options.csvPath);
		csv << "name,items,batch,samples,median_ns,p25_ns,p75_ns,min_ns,max_ns,iqr_pct,ns_per_item\n";
		for (const auto& res: m_results) {
			csv << std::format("{},{},{},{},{:.1f},{:.1f},{:.1f},{:.1f},{:.1f},{:.2f},{:.3f}\n", res.name, res.items,
							   res.batch, res.samples, res.medianNs, res.p25Ns, res.p75Ns, res.minNs, res.maxNs,
							   iqrPercent(res), res.medianNs / static_cast<double>(std::max<uint64_t>(res.items, 1)));
		}
		for (const auto& met: m_metrics) csv << std::format("{},metric,,,{},,,,,,{}\n", met.name, met.value, met.unit);
	}
	if (!m_options.jsonPath.empty()) {
		std::ofstream json(m_options.jsonPath);
		json << "{\n  \"context\": {";
		json << std::format("\"load_start\": \"{}\", \"load_end\": \"{}\", \"samples\": {}, \"warmup\": {}, "
							"\"min_sample_ms\": {}",
							jsonEscape(m_loadAtStart), jsonEscape(loadAverage()), m_options.samples, m_options.warmup,
							m_options.minSampleMs);
		json << "},\n  \"results\": [\n";
		for (size_t i = 0; i < m_results.size(); ++i) {
			const auto& res = m_results[i];
			json << std::format("    {{\"name\": \"{}\", \"items\": {}, \"batch\": {}, \"samples\": {}, "
								"\"median_ns\": {:.1f}, \"p25_ns\": {:.1f}, \"p75_ns\": {:.1f}, \"min_ns\": {:.1f}, "
								"\"max_ns\": {:.1f}}}{}\n",
								jsonEscape(res.name), res.items, res.batch, res.samples, res.medianNs, res.p25Ns,
								res.p75Ns, res.minNs, res.maxNs, i + 1 < m_results.size() ? "," : "");
		}
		json << "  ],\n  \"metrics\": [\n";
		for (size_t i = 0; i < m_metrics.size(); ++i) {
			const auto& met = m_metrics[i];
			json << std::format("    {{\"name\": \"{}\", \"value\": {}, \"unit\": \"{}\"}}{}\n", jsonEscape(met.name),
								met.value, jsonEscape(met.unit), i + 1 < m_metrics.size() ? "," : "");
		}
		json << "  ]\n}\n";
	}
	std::println("load average: start [{}] end [{}]", m_loadAtStart, loadAverage());
}

auto formatDuration(const double iNs) -> std::string {
	if (iNs < 1.0e3)
		return std::format("{:.1f} ns", iNs);
	if (iNs < 1.0e6)
		return std::format("{:.2f} us", iNs / 1.0e3);
	if (iNs < 1.0e9)
		return std::format("{:.2f} ms", iNs / 1.0e6);
	return std::format("{:.2f} s", iNs / 1.0e9);
}

auto allocatedBytes() -> size_t {
#if defined(__GLIBC__)
	const auto info = mallinfo2();
	return info.uordblks + info.hblkhd;
#else
	return 0;
#endif
}

auto loadAverage() -> std::string {
	std::ifstream file("/proc/loadavg");
	std::string line;
	if (!file.good() || !std::getline(file, line))
		return {};
	return line;
}

}// namespace owl::bench
