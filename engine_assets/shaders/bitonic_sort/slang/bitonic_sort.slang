// Single-workgroup bitonic sort for sprite Z-sort (#32). Sorts an SSBO of
// `{key, value}` pairs ascending by `key` using groupshared memory. Caller
// pads up to `kSortSize` (1024) — tail slots filled with `+INF` keys so they
// land at the end and downstream consumers can ignore them via the
// `WorldTransformPass`-style entry count.
//
// Painter's-order callers (farthest first) negate their input distance so
// the ascending sort lays elements out back-to-front naturally.
//
// Layout contract (must match `renderer::utils::BitonicSortPass`):
//   binding 0 : SSBO read-write — items[i] = { float key; uint value; }
//
// Dispatch: exactly 1 workgroup of `kSortSize` threads.

struct SortItem {
	float key;
	uint value;
};

static const uint kSortSize = 1024;

[[vk::binding(0)]]
RWStructuredBuffer<SortItem> items;

groupshared SortItem g_shared[kSortSize];

[shader("compute")]
[numthreads(kSortSize, 1, 1)]
void computeMain(uint3 gid : SV_GroupThreadID) {
	const uint tid = gid.x;
	g_shared[tid] = items[tid];
	GroupMemoryBarrierWithGroupSync();

	// Standard bitonic sort: outer k = comparator block size, inner j = stride
	// within the block. Each thread acts as the lower endpoint of one pair per
	// stage; the upper endpoint does nothing, so only `ixj > tid` swaps.
	for (uint k = 2; k <= kSortSize; k <<= 1) {
		for (uint j = k >> 1; j > 0; j >>= 1) {
			const uint ixj = tid ^ j;
			if (ixj > tid) {
				const bool ascending = ((tid & k) == 0);
				if ((ascending && g_shared[tid].key > g_shared[ixj].key) ||
				    (!ascending && g_shared[tid].key < g_shared[ixj].key)) {
					const SortItem tmp = g_shared[tid];
					g_shared[tid] = g_shared[ixj];
					g_shared[ixj] = tmp;
				}
			}
			GroupMemoryBarrierWithGroupSync();
		}
	}

	items[tid] = g_shared[tid];
}
