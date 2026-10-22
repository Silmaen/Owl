// Next level of the occlusion pyramid: each texel keeps the farthest depth of the 2x2 texels below it (clamped at the
// border of an odd level), so a box nearer than a texel is never hidden by it.
// Layout contract (must match `renderer::utils::MeshCullingPass`):
//   binding 1 : params[0]   source and destination sizes and offsets
//   binding 2 : pyramid[]   every level, one float per texel

struct HizParams {
	uint srcWidth;
	uint srcHeight;
	uint srcOffset;
	uint dstOffset;
	uint dstWidth;
	uint dstHeight;
	uint pad0;
	uint pad1;
};

struct HizTexel {
	float depth;
};

[[vk::binding(1)]]
StructuredBuffer<HizParams> params;

[[vk::binding(2)]]
RWStructuredBuffer<HizTexel> pyramid;

float source(HizParams p, uint x, uint y) {
	return pyramid[p.srcOffset + min(y, p.srcHeight - 1) * p.srcWidth + min(x, p.srcWidth - 1)].depth;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchId : SV_DispatchThreadID) {
	const HizParams p = params[0];
	if (dispatchId.x >= p.dstWidth || dispatchId.y >= p.dstHeight)
		return;
	const uint x = dispatchId.x * 2;
	const uint y = dispatchId.y * 2;
	// Reversed-Z: the farthest depth is the smallest.
	const float farthest = min(min(source(p, x, y), source(p, x + 1, y)), min(source(p, x, y + 1), source(p, x + 1, y + 1)));
	pyramid[p.dstOffset + dispatchId.y * p.dstWidth + dispatchId.x].depth = farthest;
}
