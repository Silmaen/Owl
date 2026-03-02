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

struct VertexInput {
	[[vk::location(0)]] float3 position     : POSITION;
	[[vk::location(1)]] float4 color        : COLOR;
	[[vk::location(2)]] float2 texCoord     : TEXCOORD0;
	[[vk::location(3)]] float  texIndex     : TEXCOORD1;
	[[vk::location(4)]] float  tilingFactor : TEXCOORD2;
	[[vk::location(5)]] int    entityID     : TEXCOORD3;
};

struct FragmentInput {
	[[vk::location(0)]] float4 color        : COLOR;
	[[vk::location(1)]] float2 texCoord     : TEXCOORD0;
	[[vk::location(2)]] float  tilingFactor : TEXCOORD1;
	[[vk::location(3)]] nointerpolation float texIndex : TEXCOORD2;
	[[vk::location(4)]] nointerpolation int   entityID : TEXCOORD3;
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
	output.frag.color       = input.color;
	output.frag.texCoord    = input.texCoord;
	output.frag.tilingFactor = input.tilingFactor;
	output.frag.texIndex    = input.texIndex;
	output.frag.entityID    = input.entityID;
	output.svPosition       = mul(gCamera.viewProjection, float4(input.position, 1.0));
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 texColor = input.color;
	int idx = int(input.texIndex);
	texColor *= gTextures[NonUniformResourceIndex(idx)].Sample(
		input.texCoord * input.tilingFactor);
	if (texColor.a < 0.001)
		discard;
	output.color    = texColor;
	output.entityID = input.entityID;
	return output;
}
