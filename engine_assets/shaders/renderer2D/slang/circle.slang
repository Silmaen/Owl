struct Camera {
	column_major float4x4 viewProjection;
};

[[vk::binding(0)]]
ConstantBuffer<Camera> gCamera;

struct VertexInput {
	[[vk::location(0)]] float3 worldPosition : POSITION0;
	[[vk::location(1)]] float3 localPosition : POSITION1;
	[[vk::location(2)]] float4 color         : COLOR;
	[[vk::location(3)]] float  thickness     : TEXCOORD0;
	[[vk::location(4)]] float  fade          : TEXCOORD1;
	[[vk::location(5)]] int    entityID      : TEXCOORD2;
};

struct FragmentInput {
	[[vk::location(0)]] float3 localPosition : POSITION1;
	[[vk::location(1)]] float4 color         : COLOR;
	[[vk::location(2)]] float  thickness     : TEXCOORD0;
	[[vk::location(3)]] float  fade          : TEXCOORD1;
	[[vk::location(4)]] nointerpolation int entityID : TEXCOORD2;
};

struct FragmentOutput {
	float4 color    : SV_Target0;
	int    entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
	output.frag.localPosition = input.localPosition;
	output.frag.color         = input.color;
	output.frag.thickness     = input.thickness;
	output.frag.fade          = input.fade;
	output.frag.entityID      = input.entityID;
	output.svPosition         = mul(gCamera.viewProjection, float4(input.worldPosition, 1.0));
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

	output.color    = input.color;
	output.color.a *= circle;
	if (output.color.a < 0.001)
		discard;

	output.entityID = input.entityID;
	return output;
}
