// Renderer2D quad — instanced, SSBO-driven. One drawcall per batch via
// `vkCmdDrawIndexed(6, instanceCount, …)` / `glDrawElementsInstanced(…)`.
// The per-vertex VBO holds a single 4-vertex unit quad reused for every
// instance; per-quad data (worldIndex, colour, UVs, atlas slot, entity id,
// tiling factor) lives in `gInstances[gl_InstanceID]`.
//
// World matrix lookup: instances reference a slot in one of two SSBOs
// instead of carrying a baked mat4. Positive `worldIndex` reads
// `gSceneWorlds[worldIndex]` (computed by `WorldTransformPass`); negative
// `worldIndex` reads `gTransientWorlds[-(worldIndex + 1)]` (the per-batch
// scratch buffer Renderer2D uploads for non-entity callers like UI builders
// or gizmos). The split lets entity draws skip the per-instance mat4 upload
// while keeping the transient path drop-in compatible with legacy calls.

struct Camera {
	column_major float4x4 viewProjection;
};

[[vk::binding(0)]]
ConstantBuffer<Camera> gCamera;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[32];

struct QuadInstance {
	int worldIndex;
	float4 color;
	float2 uv[4];
	uint texIndex;
	int entityId;
	float2 tilingFactor;
};

[[vk::binding(2)]]
StructuredBuffer<QuadInstance> gInstances;

[[vk::binding(3)]]
StructuredBuffer<column_major float4x4> gSceneWorlds;

[[vk::binding(4)]]
StructuredBuffer<column_major float4x4> gTransientWorlds;

struct VertexInput {
	[[vk::location(0)]] int cornerIndex : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float4 color : COLOR;
	[[vk::location(1)]] float2 texCoord : TEXCOORD0;
	[[vk::location(2)]] float2 tilingFactor : TEXCOORD1;
	[[vk::location(3)]] nointerpolation int texIndex : TEXCOORD2;
	[[vk::location(4)]] nointerpolation int entityID : TEXCOORD3;
};

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

// Unit-quad corners — BL, BR, TR, TL, matching the index buffer's
// {0,1,2, 2,3,0} winding used by the CPU side.
static const float2 kCorners[4] = {
	float2(-0.5f, -0.5f),
	float2( 0.5f, -0.5f),
	float2( 0.5f,  0.5f),
	float2(-0.5f,  0.5f),
};

float4x4 resolveWorldMatrix(int worldIndex) {
	if (worldIndex < 0) {
		return gTransientWorlds[-(worldIndex + 1)];
	}
	return gSceneWorlds[worldIndex];
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint iid : SV_InstanceID) {
	VertexOutput output;
	QuadInstance inst = gInstances[iid];
	float4x4 world = resolveWorldMatrix(inst.worldIndex);
	float4 worldPos = mul(world, float4(kCorners[input.cornerIndex], 0.0f, 1.0f));
	output.svPosition = mul(gCamera.viewProjection, worldPos);
	output.frag.color = inst.color;
	output.frag.texCoord = inst.uv[input.cornerIndex];
	output.frag.tilingFactor = inst.tilingFactor;
	output.frag.texIndex = int(inst.texIndex);
	output.frag.entityID = inst.entityId;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 texColor = input.color;
	texColor *= gTextures[NonUniformResourceIndex(input.texIndex)].Sample(input.texCoord * input.tilingFactor);
	if (texColor.a < 0.001f)
		discard;
	output.color = texColor;
	output.entityID = input.entityID;
	return output;
}
