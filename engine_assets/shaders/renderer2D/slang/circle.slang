// Renderer2D circle — instanced, SSBO-driven. The per-vertex VBO is the
// same shared 4-corner unit quad as `quad.slang`; per-instance data
// (transform, colour, thickness, fade, entity id) lives in `gInstances`.
// The fragment shader signed-distance test runs on local-space coordinates
// derived directly from `SV_VertexID` — no per-vertex localPosition stream
// needed.

struct Camera {
	column_major float4x4 viewProjection;
};

[[vk::binding(0)]]
ConstantBuffer<Camera> gCamera;

struct CircleInstance {
	column_major float4x4 transform;
	float4 color;
	float thickness;
	float fade;
	int entityId;
	uint _pad0;
};

[[vk::binding(2)]]
StructuredBuffer<CircleInstance> gInstances;

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

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint iid : SV_InstanceID) {
	VertexOutput output;
	CircleInstance inst = gInstances[iid];
	float4 worldPos = mul(inst.transform, float4(kCorners[input.cornerIndex], 0.0f, 1.0f));
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

	output.entityID = input.entityID;
	return output;
}
