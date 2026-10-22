// Instance culling of the mesh layer: one thread per instance. A visible instance takes a slot in the indirect
// command of its group (atomic instance count) and writes its index in the visible list at the group's base.
// Layout contract (must match `renderer::utils::MeshCullingPass`):
//   binding 0 : items[i]    instance i: world box and group
//   binding 1 : groups[g]   first entry of group g in the visible list
//   binding 2 : params[0]   frustum planes and item count
//   binding 3 : commands[g] DrawIndexedIndirectCommand of group g, instance count zeroed by the caller
//   binding 4 : visible[]   indices of the visible instances, grouped
//   binding 5 : pyramid[]   occlusion pyramid (`hiz_build`), read when params.hizLevels > 0
// An instance in the frustum is then hidden when its box, projected, is farther than the farthest occluder depth of
// the pyramid texels it covers (a level where it spans at most 2x2 texels).
// Padding is spelled as scalars: a `uint3` aligns on 16 bytes in std430 and would grow the structs.

struct CullItem {
	float4 boxMin;// xyz: world min corner.
	float4 boxMax;// xyz: world max corner.
	uint group;
	uint pad0;
	uint pad1;
	uint pad2;
};

struct CullGroup {
	uint visibleBase;
	uint pad0;
	uint pad1;
	uint pad2;
};

struct CullParams {
	float4 planes[6];
	float4 viewProjectionRows[4];// Rows of the view-projection the pyramid was drawn with.
	uint itemCount;
	uint hizLevels;// 0 without occlusion.
	uint hizWidth;
	uint hizHeight;
	uint4 levelOffsets[4];// First texel of each level, 16 levels at most.
};

struct HizTexel {
	float depth;
};

struct DrawCommand {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	int baseVertex;
	uint baseInstance;
};

struct VisibleInstance {
	uint index;
};

[[vk::binding(0)]]
StructuredBuffer<CullItem> items;

[[vk::binding(1)]]
StructuredBuffer<CullGroup> groups;

[[vk::binding(2)]]
StructuredBuffer<CullParams> params;

[[vk::binding(3)]]
RWStructuredBuffer<DrawCommand> commands;

[[vk::binding(4)]]
RWStructuredBuffer<VisibleInstance> visible;

[[vk::binding(5)]]
StructuredBuffer<HizTexel> pyramid;

bool insidePlane(float3 mn, float3 mx, float4 plane) {
	// The corner furthest along the normal: when even it is behind the plane, the whole box is.
	const float3 far = float3(plane.x >= 0.0f ? mx.x : mn.x, plane.y >= 0.0f ? mx.y : mn.y,
							  plane.z >= 0.0f ? mx.z : mn.z);
	return dot(plane.xyz, far) + plane.w >= 0.0f;
}

uint levelOffset(CullParams p, uint level) { return p.levelOffsets[level / 4][level % 4]; }

uint2 levelSize(CullParams p, uint level) {
	uint2 size = uint2(p.hizWidth, p.hizHeight);
	for (uint index = 0; index < level; ++index)
		size = max(uint2(1, 1), (size + 1) / 2);
	return size;
}

bool occluded(CullParams p, float3 mn, float3 mx) {
	float2 lo = float2(1e30f, 1e30f);
	float2 hi = float2(-1e30f, -1e30f);
	float nearest = 1e30f;
	for (uint corner = 0; corner < 8; ++corner) {
		const float4 position = float4((corner & 1) != 0 ? mx.x : mn.x, (corner & 2) != 0 ? mx.y : mn.y,
									   (corner & 4) != 0 ? mx.z : mn.z, 1.0f);
		const float4 clip = float4(dot(p.viewProjectionRows[0], position), dot(p.viewProjectionRows[1], position),
								   dot(p.viewProjectionRows[2], position), dot(p.viewProjectionRows[3], position));
		// A box crossing the camera plane cannot be projected: kept.
		if (clip.w <= 1e-5f)
			return false;
		const float3 ndc = clip.xyz / clip.w;
		lo = min(lo, ndc.xy);
		hi = max(hi, ndc.xy);
		nearest = min(nearest, ndc.z);
	}
	const float2 uv0 = saturate(lo * 0.5f + 0.5f);
	const float2 uv1 = saturate(hi * 0.5f + 0.5f);
	const float2 extent = (uv1 - uv0) * float2(p.hizWidth, p.hizHeight);
	uint level = min(uint(ceil(log2(max(max(extent.x, extent.y), 1.0f)))), p.hizLevels - 1);
	uint2 size = levelSize(p, level);
	uint2 first = min(uint2(uv0 * float2(size)), size - 1);
	uint2 last = min(uint2(uv1 * float2(size)), size - 1);
	if (last.x - first.x > 1 || last.y - first.y > 1) {
		if (level + 1 >= p.hizLevels)
			return false;
		++level;
		size = levelSize(p, level);
		first = min(uint2(uv0 * float2(size)), size - 1);
		last = min(uint2(uv1 * float2(size)), size - 1);
	}
	const uint offset = levelOffset(p, level);
	float farthest = -1e30f;
	for (uint y = first.y; y <= last.y; ++y) {
		for (uint x = first.x; x <= last.x; ++x)
			farthest = max(farthest, pyramid[offset + y * size.x + x].depth);
	}
	return nearest > farthest;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 dispatchId : SV_DispatchThreadID) {
	const uint index = dispatchId.x;
	const CullParams p = params[0];
	if (index >= p.itemCount)
		return;
	const CullItem item = items[index];
	for (int plane = 0; plane < 6; ++plane) {
		if (!insidePlane(item.boxMin.xyz, item.boxMax.xyz, p.planes[plane]))
			return;
	}
	if (p.hizLevels > 0 && occluded(p, item.boxMin.xyz, item.boxMax.xyz))
		return;
	uint slot;
	InterlockedAdd(commands[item.group].instanceCount, 1u, slot);
	visible[groups[item.group].visibleBase + slot].index = index;
}
