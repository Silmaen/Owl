// Hierarchical world-transform compute pass. One thread per entity slot.
// Each thread walks its parent chain up to the root, accumulating
// local-space transforms into a world matrix.
//
// Race-free design — only reads `locals[]` (frozen for the dispatch) and
// writes a single slot in `worlds[]` per thread, so a single dispatch
// covers any hierarchy depth without intermediate barriers.
//
// Trade-off: the compute is O(depth × N) instead of O(N). For typical
// scene graphs (depth ≤ 5 in Owl projects) the constant is small enough
// to win against multi-dispatch + per-level barriers. Caller pre-sorts
// entries so each parent index points *upward* in the array, which keeps
// the parent-chain walk cache-friendly.
//
// Size handling: the caller pads `locals` / `parents` / `worlds` up to a
// multiple of the workgroup size (64) and fills tail slots with
// `parents[i] = -1` + `locals[i] = identity`. Padded threads write
// identity into their `worlds[i]` slot, which is harmless because no
// graphics pass indexes those slots.
//
// Layout contract (must match `renderer::utils::WorldTransformPass`):
//   binding 0 : SSBO read-only  — locals[i]   (column-major float4x4)
//   binding 1 : SSBO read-only  — parents[i]  (int, -1 for root or tail)
//   binding 2 : SSBO read-write — worlds[i]   (column-major float4x4)
//
// Dispatch: workgroups = paddedEntryCount / 64.

[[vk::binding(0)]]
StructuredBuffer<column_major float4x4> locals;

[[vk::binding(1)]]
StructuredBuffer<int> parents;

[[vk::binding(2)]]
RWStructuredBuffer<column_major float4x4> worlds;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 dispatchID : SV_DispatchThreadID) {
	const uint i = dispatchID.x;
	column_major float4x4 acc = locals[i];
	int parent = parents[i];
	// Cap the climb to guard against malformed input (cycle or out-of-range
	// parent index). A real scene graph never reaches this bound; the engine
	// rejects cycles at edit time and the deserializer drops orphan parents.
	for (int hops = 0; parent >= 0 && hops < 64; ++hops) {
		acc = mul(locals[parent], acc);
		parent = parents[parent];
	}
	worlds[i] = acc;
}
