/**
 * @file Bench.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

/**
 * @brief
 *  Minimal in-house micro-benchmark harness (no Google Benchmark package in DepManager).
 */
namespace owl::bench {

/**
 * @brief
 *  Prevent the optimizer from discarding a value computed in a timed loop.
 * @tparam T Type of the value.
 * @param[in] iValue The value to keep alive.
 */
template<typename T>
inline void doNotOptimize(const T& iValue) {
	asm volatile("" : : "r,m"(iValue) : "memory");// NOLINT(hicpp-no-assembler)
}

/**
 * @brief
 *  Force the compiler to assume memory was clobbered.
 */
inline void clobberMemory() {
	asm volatile("" : : : "memory");// NOLINT(hicpp-no-assembler)
}

/**
 * @brief
 *  Runner configuration, filled from the command line.
 */
struct Options {
	/// Number of timed samples per benchmark.
	uint32_t samples = 15;
	/// Number of untimed warm-up samples per benchmark.
	uint32_t warmup = 2;
	/// Target duration of one sample, used to calibrate the batch size of fast bodies.
	double minSampleMs = 10.0;
	/// Upper bound of the calibrated batch size.
	uint64_t maxBatch = 1ULL << 22U;
	/// Only run benchmarks whose name contains this string (empty = all).
	std::string filter;
	/// Skip benchmarks whose name contains this string (empty = none).
	std::string exclude;
	/// Optional CSV output path.
	std::filesystem::path csvPath;
	/// Optional JSON output path.
	std::filesystem::path jsonPath;
	/// Keep the engine log enabled (warnings and errors) instead of silencing it.
	bool verbose = false;
};

/**
 * @brief
 *  Statistics of one benchmark, in nanoseconds per body call.
 */
struct Result {
	/// Full benchmark name (`group/case/variant`).
	std::string name;
	/// Logical items processed by one body call (entities, quads, chunks...).
	uint64_t items = 1;
	/// Body calls per timed sample.
	uint64_t batch = 1;
	/// Number of timed samples.
	uint32_t samples = 0;
	/// Median time of one body call.
	double medianNs = 0.0;
	/// First quartile of one body call.
	double p25Ns = 0.0;
	/// Third quartile of one body call.
	double p75Ns = 0.0;
	/// Fastest sample.
	double minNs = 0.0;
	/// Slowest sample.
	double maxNs = 0.0;
};

/**
 * @brief
 *  A non-timing measurement attached to the run (sizes, counts, memory).
 */
struct Metric {
	/// Metric name.
	std::string name;
	/// Metric value.
	double value = 0.0;
	/// Unit of the value.
	std::string unit;
};

/**
 * @brief
 *  Collects and runs timed bodies, then reports their statistics.
 */
class Runner final {
public:
	Runner(const Runner&) = delete;

	Runner(Runner&&) = delete;

	auto operator=(const Runner&) -> Runner& = delete;

	auto operator=(Runner&&) -> Runner& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iOptions The runner options.
	 */
	explicit Runner(Options iOptions);

	~Runner() = default;

	/**
	 * @brief
	 *  Whether a benchmark name passes the filters (lets a case skip its setup).
	 * @param[in] iName The benchmark name, or a prefix of it.
	 * @return True when the benchmark must run.
	 */
	[[nodiscard]] auto wants(const std::string& iName) const -> bool;

	/**
	 * @brief
	 *  Time a cheap, repeatable body; the batch size is calibrated to reach the minimum sample duration.
	 * @tparam Fn Callable type.
	 * @param[in] iName Benchmark name.
	 * @param[in] iItems Logical items processed by one call.
	 * @param[in] iBody The timed body.
	 */
	template<typename Fn>
	void measure(const std::string& iName, const uint64_t iItems, Fn&& iBody) {
		if (!wants(iName))
			return;
		const uint64_t batch = calibrate(iBody);
		std::vector<double> samples;
		samples.reserve(m_options.samples);
		for (uint32_t s = 0; s < m_options.warmup + m_options.samples; ++s) {
			const auto start = Clock::now();
			for (uint64_t i = 0; i < batch; ++i) iBody();
			const auto stop = Clock::now();
			if (s >= m_options.warmup)
				samples.push_back(nanoseconds(stop - start) / static_cast<double>(batch));
		}
		record(iName, iItems, batch, samples);
	}

	/**
	 * @brief
	 *  Time a body that needs a fresh, untimed setup before every call (batch size 1).
	 * @tparam Setup Setup callable type.
	 * @tparam Fn Body callable type.
	 * @param[in] iName Benchmark name.
	 * @param[in] iItems Logical items processed by one call.
	 * @param[in] iSetup Untimed setup run before every call.
	 * @param[in] iBody The timed body.
	 */
	template<typename Setup, typename Fn>
	void measureWithSetup(const std::string& iName, const uint64_t iItems, Setup&& iSetup, Fn&& iBody) {
		if (!wants(iName))
			return;
		std::vector<double> samples;
		samples.reserve(m_options.samples);
		for (uint32_t s = 0; s < m_options.warmup + m_options.samples; ++s) {
			iSetup();
			const auto start = Clock::now();
			iBody();
			const auto stop = Clock::now();
			if (s >= m_options.warmup)
				samples.push_back(nanoseconds(stop - start));
		}
		record(iName, iItems, 1, samples);
	}

	/**
	 * @brief
	 *  Time a body exactly once (cold paths such as the first shader compilation).
	 * @tparam Fn Callable type.
	 * @param[in] iName Benchmark name.
	 * @param[in] iBody The timed body.
	 */
	template<typename Fn>
	void measureOnce(const std::string& iName, Fn&& iBody) {
		if (!wants(iName))
			return;
		const auto start = Clock::now();
		iBody();
		const auto stop = Clock::now();
		record(iName, 1, 1, {nanoseconds(stop - start)});
	}

	/**
	 * @brief
	 *  Record a non-timing metric.
	 * @param[in] iName Metric name.
	 * @param[in] iValue Metric value.
	 * @param[in] iUnit Unit of the value.
	 */
	void metric(const std::string& iName, double iValue, const std::string& iUnit);

	/**
	 * @brief
	 *  Write the CSV / JSON reports requested in the options.
	 */
	void writeReports() const;

	/**
	 * @brief
	 *  Access the options.
	 * @return The runner options.
	 */
	[[nodiscard]] auto getOptions() const -> const Options& { return m_options; }

private:
	/// Clock used for every measurement.
	using Clock = std::chrono::steady_clock;

	/**
	 * @brief
	 *  Convert a clock duration to nanoseconds.
	 * @param[in] iDuration The duration.
	 * @return The duration in nanoseconds.
	 */
	static auto nanoseconds(const Clock::duration iDuration) -> double {
		return std::chrono::duration<double, std::nano>(iDuration).count();
	}

	/**
	 * @brief
	 *  Find the batch size that makes one sample last at least the minimum sample duration.
	 * @tparam Fn Callable type.
	 * @param[in] iBody The body to calibrate.
	 * @return The batch size.
	 */
	template<typename Fn>
	auto calibrate(Fn& iBody) const -> uint64_t {
		uint64_t batch = 1;
		const double target = m_options.minSampleMs * 1.0e6;
		while (batch < m_options.maxBatch) {
			const auto start = Clock::now();
			for (uint64_t i = 0; i < batch; ++i) iBody();
			const double elapsed = nanoseconds(Clock::now() - start);
			if (elapsed >= target)
				break;
			const double factor = elapsed > 0.0 ? std::min(10.0, 1.2 * target / elapsed) : 10.0;
			batch = std::max(batch + 1, static_cast<uint64_t>(static_cast<double>(batch) * factor));
		}
		return std::min(batch, m_options.maxBatch);
	}

	/**
	 * @brief
	 *  Compute the statistics of a benchmark, print them and store them.
	 * @param[in] iName Benchmark name.
	 * @param[in] iItems Logical items processed by one call.
	 * @param[in] iBatch Body calls per sample.
	 * @param[in] iSamples Per-call duration of every sample.
	 */
	void record(const std::string& iName, uint64_t iItems, uint64_t iBatch, std::vector<double> iSamples);

	/// The runner options.
	Options m_options;
	/// Every recorded benchmark.
	std::vector<Result> m_results;
	/// Every recorded metric.
	std::vector<Metric> m_metrics;
	/// System load average when the runner was created.
	std::string m_loadAtStart;
};

/**
 * @brief
 *  Format a duration in nanoseconds with an adapted unit.
 * @param[in] iNs The duration in nanoseconds.
 * @return The formatted duration.
 */
auto formatDuration(double iNs) -> std::string;

/**
 * @brief
 *  Bytes currently allocated through malloc (glibc `mallinfo2`), 0 when unavailable.
 * @return The allocated byte count.
 */
auto allocatedBytes() -> size_t;

/**
 * @brief
 *  Read the system load average.
 * @return The content of `/proc/loadavg`, or an empty string.
 */
auto loadAverage() -> std::string;

}// namespace owl::bench
