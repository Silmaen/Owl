struct Camera {
	column_major float4x4 viewProjection;
};

[[vk::binding(0)]]
ConstantBuffer<Camera> gCamera;

struct VertexInput {
	[[vk::location(0)]] float3 position : POSITION;
	[[vk::location(1)]] float4 color    : COLOR;
	[[vk::location(2)]] int    entityID : TEXCOORD0;
};

struct FragmentInput {
	[[vk::location(0)]] float4 color : COLOR;
	[[vk::location(1)]] nointerpolation int entityID : TEXCOORD0;
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
	output.frag.color    = input.color;
	output.frag.entityID = input.entityID;
	output.svPosition    = mul(gCamera.viewProjection, float4(input.position, 1.0));
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	output.color = input.color;
	output.entityID = input.entityID;
	return output;
}
