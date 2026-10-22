// First level of the occlusion pyramid: one thread per texel of the occluder depth (`RedFloat`, clip z / w, 1 where
// nothing was drawn), copied into the pyramid buffer, rows in the order of the texture.
// Layout contract (must match `renderer::utils::MeshCullingPass`):
//   binding 0 : depth       occluder depth texture, nearest sampler
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

[[vk::binding(0)]]
Sampler2D depth;

[[vk::binding(1)]]
StructuredBuffer<HizParams> params;

[[vk::binding(2)]]
RWStructuredBuffer<HizTexel> pyramid;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchId : SV_DispatchThreadID) {
	const HizParams p = params[0];
	if (dispatchId.x >= p.dstWidth || dispatchId.y >= p.dstHeight)
		return;
	// Sampled at the texel centre: a texel fetch from a combined sampler is not portable.
	const float2 uv = (float2(dispatchId.xy) + 0.5f) / float2(p.dstWidth, p.dstHeight);
	pyramid[p.dstOffset + dispatchId.y * p.dstWidth + dispatchId.x].depth = depth.SampleLevel(uv, 0.0f).r;
}
