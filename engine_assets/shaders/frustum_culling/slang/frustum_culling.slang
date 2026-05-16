// Frustum culling compute pre-pass (#34). One thread per entity AABB.
// Visible AABBs append themselves (via atomic counter) to a draw-command
// SSBO that the graphics pass consumes via
// `vkCmdDrawIndexedIndirect` / `glMultiDrawElementsIndirect`.
//
// Layout contract (must match `renderer::utils::FrustumCullingPass`):
//   binding 0 : SSBO read-only  — aabbs[i]   (Aabb struct, w of minPos = 0
//                                              for padding / culled-out entries)
//   binding 1 : SSBO read-only  — frustum    (Frustum struct = 6 vec4 planes
//                                              + template draw command)
//   binding 2 : SSBO read-write — counter[0] (atomic counter, caller resets
//                                              to 0 before each dispatch)
//   binding 3 : SSBO read-write — commands[] (DrawIndexedIndirectCommand)
//
// Caller pads `aabbs` up to a multiple of 64 with `minPos.w = 0` sentinels
// so the workgroup count is `paddedCount / 64`. Padding threads exit early
// thanks to the validity flag and never increment the counter.

struct Aabb {
	float4 minPos;
	float4 maxPos;
};

struct Frustum {
	float4 planes[6];
	uint indexCount;
	uint firstIndex;
	uint baseVertex;
	uint _pad;
};

struct DrawCommand {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	uint baseVertex;
	uint baseInstance;
};

[[vk::binding(0)]]
StructuredBuffer<Aabb> aabbs;

[[vk::binding(1)]]
StructuredBuffer<Frustum> frustum;

[[vk::binding(2)]]
RWStructuredBuffer<uint> counter;

[[vk::binding(3)]]
RWStructuredBuffer<DrawCommand> commands;

bool insideFrustum(float3 mn, float3 mx, float4 plane) {
	// Choose the corner of the AABB that's furthest along the plane normal.
	// If even that corner is on the negative side, the box is outside.
	const float3 positive = float3(
		plane.x >= 0.0f ? mx.x : mn.x,
		plane.y >= 0.0f ? mx.y : mn.y,
		plane.z >= 0.0f ? mx.z : mn.z);
	return dot(plane.xyz, positive) + plane.w >= 0.0f;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 dispatchID : SV_DispatchThreadID) {
	const uint i = dispatchID.x;
	const Aabb box = aabbs[i];
	// `minPos.w` is the validity flag — 1.0 for real entries, 0.0 for the
	// padding tail. Skip culled-out / padding entries silently.
	if (box.minPos.w < 0.5f)
		return;
	const Frustum f = frustum[0];
	for (int p = 0; p < 6; ++p) {
		if (!insideFrustum(box.minPos.xyz, box.maxPos.xyz, f.planes[p]))
			return;
	}
	uint slot;
	InterlockedAdd(counter[0], 1u, slot);
	DrawCommand cmd;
	cmd.indexCount = f.indexCount;
	cmd.instanceCount = 1u;
	cmd.firstIndex = f.firstIndex;
	cmd.baseVertex = f.baseVertex;
	cmd.baseInstance = i;
	commands[slot] = cmd;
}
