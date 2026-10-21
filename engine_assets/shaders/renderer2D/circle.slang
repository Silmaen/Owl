// Renderer2D circle — instanced, SSBO-driven. The per-vertex VBO is the
// same shared 4-corner unit quad as `quad.slang`; per-instance data
// (worldIndex, colour, thickness, fade, entity id) lives in `gInstances`.
// World matrix is fetched from `gSceneWorlds` (worldIndex >= 0) or
// `gTransientWorlds` (worldIndex < 0). See `quad.slang` for the resolution
// scheme. The fragment shader signed-distance test runs on local-space
// coordinates derived directly from `SV_VertexID` — no per-vertex
// localPosition stream needed.

struct Camera {
	column_major float4x4 viewProjection;
	// x: 1 when the target stores linear colour (the fragment colour is decoded from sRGB); yzw unused.
	int4 flags;
};

[[vk::binding(0)]]
ConstantBuffer<Camera> gCamera;

struct CircleInstance {
	int worldIndex;
	float4 color;
	float thickness;
	float fade;
	int entityId;
};

[[vk::binding(2)]]
StructuredBuffer<CircleInstance> gInstances;

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

struct VertexInput {
	[[vk::location(0)]] int cornerIndex : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float3 localPosition : POSITION1;
	[[vk::location(1)]] float4 color : COLOR;
	[[vk::location(2)]] float thickness : TEXCOORD0;
	[[vk::location(3)]] float fade : TEXCOORD1;
	[[vk::location(4)]] nointerpolation int entityID : TEXCOORD2;
};

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

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
	CircleInstance inst = gInstances[iid];
	float4x4 world = resolveWorldMatrix(inst.worldIndex);
	float4 worldPos = mul(world, float4(kCorners[input.cornerIndex], 0.0f, 1.0f));
	output.svPosition = mul(gCamera.viewProjection, worldPos);
	output.frag.localPosition = float3(kCorners[input.cornerIndex] * 2.0f, 0.0f);
	output.frag.color = inst.color;
	output.frag.thickness = inst.thickness;
	output.frag.fade = inst.fade;
	output.frag.entityID = inst.entityId;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;

	float dist = 1.0 - length(input.localPosition);
	float circle = smoothstep(0.0, input.fade, dist);
	circle *= smoothstep(input.thickness + input.fade, input.thickness, dist);

	if (circle < 0.001)
		discard;

	output.color = input.color;
	output.color.a *= circle;
	if (output.color.a < 0.001)
		discard;
	if (gCamera.flags.x != 0)
		output.color.rgb = srgbToLinear(output.color.rgb);

	output.entityID = input.entityID;
	return output;
}
