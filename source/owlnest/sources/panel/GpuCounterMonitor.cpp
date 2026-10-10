/**
 * @file GpuCounterMonitor.cpp
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "GpuCounterMonitor.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace owl::nest::panel {

namespace {

void keepMax(GpuCounterDeltas& ioMax, const GpuCounterDeltas& iSample) {
	ioMax.submits = std::max(ioMax.submits, iSample.submits);
	ioMax.queueWaitIdles = std::max(ioMax.queueWaitIdles, iSample.queueWaitIdles);
	ioMax.deviceWaitIdles = std::max(ioMax.deviceWaitIdles, iSample.deviceWaitIdles);
	ioMax.fenceWaits = std::max(ioMax.fenceWaits, iSample.fenceWaits);
	ioMax.paceWaitMs = std::max(ioMax.paceWaitMs, iSample.paceWaitMs);
}

auto isBehind(const renderer::gpu::RenderCounters& iNow, const renderer::gpu::RenderCounters& iBefore) -> bool {
	return iNow.submits < iBefore.submits || iNow.queueWaitIdles < iBefore.queueWaitIdles ||
		   iNow.deviceWaitIdles < iBefore.deviceWaitIdles || iNow.fenceWaits < iBefore.fenceWaits ||
		   iNow.paceWaitMs < iBefore.paceWaitMs;
}

}// namespace

void GpuCounterMonitor::update(const renderer::gpu::RenderCounters& iCounters, const clock::time_point iNow) {
	const int64_t bucket = iNow.time_since_epoch() / bucketDuration;
	if (m_bucketIndex >= 0 && bucket > m_bucketIndex) {
		const int64_t elapsed = std::min<int64_t>(bucket - m_bucketIndex, static_cast<int64_t>(bucketCount));
		for (int64_t i = 1; i <= elapsed; ++i) m_buckets[static_cast<size_t>(m_bucketIndex + i) % bucketCount] = {};
	}
	m_bucketIndex = bucket;
	if (!m_hasPrevious || isBehind(iCounters, m_previous)) {
		m_last = {};
	} else {
		m_last = {.submits = iCounters.submits - m_previous.submits,
				  .queueWaitIdles = iCounters.queueWaitIdles - m_previous.queueWaitIdles,
				  .deviceWaitIdles = iCounters.deviceWaitIdles - m_previous.deviceWaitIdles,
				  .fenceWaits = iCounters.fenceWaits - m_previous.fenceWaits,
				  .paceWaitMs = iCounters.paceWaitMs - m_previous.paceWaitMs};
		keepMax(m_buckets[static_cast<size_t>(bucket) % bucketCount], m_last);
	}
	m_previous = iCounters;
	m_hasPrevious = true;
}

void GpuCounterMonitor::reset() {
	m_previous = {};
	m_hasPrevious = false;
	m_last = {};
	m_buckets = {};
	m_bucketIndex = -1;
}

auto GpuCounterMonitor::getWindowMax() const -> GpuCounterDeltas {
	GpuCounterDeltas result;
	for (const auto& bucket: m_buckets) keepMax(result, bucket);
	return result;
}

}// namespace owl::nest::panel
