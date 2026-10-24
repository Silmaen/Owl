// Renderer2D quad — instanced, SSBO-driven. One drawcall per batch via
// `vkCmdDrawIndexed(6, instanceCount, …)` / `glDrawElementsInstanced(…)`.
// The per-vertex VBO holds a single 4-vertex unit quad reused for every
// instance; per-quad data (worldIndex, colour, UVs, atlas slot, entity id,
// tiling factor) lives in `gInstances[gl_InstanceID]`.
//
// World matrix lookup: instances reference a slot in one of two SSBOs
// instead of carrying a baked mat4. Positive `worldIndex` reads
// `gSceneWorlds[worldIndex]` (composed on the CPU by `Scene`); negative
// `worldIndex` reads `gTransientWorlds[-(worldIndex + 1)]` (the per-batch
// scratch buffer Renderer2D uploads for non-entity callers like UI builders
// or gizmos). The split lets entity draws skip the per-instance mat4 upload
// while non-entity callers keep a plain per-instance matrix.

struct Camera {
	column_major float4x4 viewProjection;
	// x: 1 when the target stores linear colour (the fragment colour is decoded from sRGB); y: 2D light count;
	// z: 1 when the 2D lights are on the ground plane (x, z) of a Y-up world (billboards); w unused.
	int4 flags;
	float4 ambient;// rgb: linear ambient of the 2D lights; w: 1 when the 2D lighting is on.
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
	int normalIndex;// texture slot of the normal map, -1 for none.
	uint flags;     // bit 0: lit by the 2D lights; bit 1: shadow caster (not shadowed itself).
	float4 color;
	float2 uv[4];
	uint texIndex;
	int entityId;
	float2 tilingFactor;
};

[[vk::binding(2)]]
StructuredBuffer<QuadInstance> gInstances;

// One element type per buffer: the OpenGL SPIR-V path (NVIDIA) aliases storage buffers sharing a block type.
struct SceneWorld {
	column_major float4x4 world;
};

struct TransientWorld {
	column_major float4x4 world;
};

[[vk::binding(3)]]
StructuredBuffer<SceneWorld> gSceneWorlds;

[[vk::binding(4)]]
StructuredBuffer<TransientWorld> gTransientWorlds;

#define LIGHT2D_LIGHT_BINDING 5
#define LIGHT2D_SHADOW_BINDING 6
#include "light2d.slangh"

struct VertexInput {
	[[vk::location(0)]] int cornerIndex : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float4 color : COLOR;
	[[vk::location(1)]] float2 texCoord : TEXCOORD0;
	[[vk::location(2)]] float2 tilingFactor : TEXCOORD1;
	[[vk::location(3)]] nointerpolation int texIndex : TEXCOORD2;
	[[vk::location(4)]] nointerpolation int entityID : TEXCOORD3;
	[[vk::location(5)]] float2 worldPos : TEXCOORD4;// in the plane of the 2D lights: xy, or xz on the ground plane.
	[[vk::location(6)]] nointerpolation int normalIndex : TEXCOORD5;
	[[vk::location(7)]] nointerpolation uint flags : TEXCOORD6;
	[[vk::location(8)]] nointerpolation float4 axes : TEXCOORD7;// xy: world X axis of the sprite, zw: its Y axis.
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
		return gTransientWorlds[-(worldIndex + 1)].world;
	}
	return gSceneWorlds[worldIndex].world;
}

// Exact piecewise sRGB decode, used when the target stores linear colour.
float3 srgbToLinear(float3 c) {
	return select(c <= 0.04045f, c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f));
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
	output.frag.worldPos = gCamera.flags.z != 0 ? worldPos.xz : worldPos.xy;
	output.frag.normalIndex = inst.normalIndex;
	output.frag.flags = inst.flags;
	float2 axisX = float2(world[0][0], world[1][0]);
	float2 axisY = float2(world[0][1], world[1][1]);
	output.frag.axes = float4(axisX / max(length(axisX), 1e-6f), axisY / max(length(axisY), 1e-6f));
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 texColor = input.color;
	texColor *= gTextures[NonUniformResourceIndex(input.texIndex)].Sample(input.texCoord * input.tilingFactor);
	if (texColor.a < 0.001f)
		discard;
	if (gCamera.flags.x != 0)
		texColor.rgb = srgbToLinear(texColor.rgb);
	if (gCamera.ambient.w != 0.0f && (input.flags & 1u) != 0u) {
		float3 normal = float3(0.0f, 0.0f, 1.0f);
		bool groundPlane = gCamera.flags.z != 0;
		bool hasNormal = input.normalIndex >= 0 && !groundPlane;
		if (hasNormal) {
			float3 local = gTextures[NonUniformResourceIndex(input.normalIndex)]
								   .Sample(input.texCoord * input.tilingFactor)
								   .xyz * 2.0f - 1.0f;
			normal = normalize(float3(local.x * input.axes.xy + local.y * input.axes.zw, local.z));
		}
		bool shadowed = (input.flags & 2u) == 0u && !groundPlane;
		texColor.rgb *= lighting2D(input.worldPos, 0.0f, normal, hasNormal, shadowed, gCamera.flags.y,
								   gCamera.ambient.rgb);
	}
	output.color = texColor;
	output.entityID = input.entityID;
	return output;
}
