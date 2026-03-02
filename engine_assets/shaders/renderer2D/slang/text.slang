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
	[[vk::location(0)]] float3 position : POSITION;
	[[vk::location(1)]] float4 color    : COLOR;
	[[vk::location(2)]] float2 texCoord : TEXCOORD0;
	[[vk::location(3)]] float  texIndex : TEXCOORD1;
	[[vk::location(4)]] int    entityID : TEXCOORD2;
};

struct FragmentInput {
	[[vk::location(0)]] float4 color    : COLOR;
	[[vk::location(1)]] float2 texCoord : TEXCOORD0;
	[[vk::location(2)]] nointerpolation float texIndex : TEXCOORD1;
	[[vk::location(3)]] nointerpolation int   entityID : TEXCOORD2;
};

struct FragmentOutput {
	float4 color    : SV_Target0;
	int    entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

float median(float r, float g, float b) {
	return max(min(r, g), min(max(r, g), b));
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
	output.frag.color    = input.color;
	output.frag.texCoord = input.texCoord;
	output.frag.texIndex = input.texIndex;
	output.frag.entityID = input.entityID;
	output.svPosition    = mul(gCamera.viewProjection, float4(input.position, 1.0));
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	int idx = int(input.texIndex);
	float4 texColor = input.color;

	// MSDF text rendering
	float3 msd = gTextures[NonUniformResourceIndex(idx)].Sample(input.texCoord).rgb;
	float sd = median(msd.r, msd.g, msd.b);

	// Screen-space pixel range for anti-aliasing
	const float pxRange = 2.0;
	uint texW, texH;
	gTextures[NonUniformResourceIndex(idx)].GetDimensions(texW, texH);
	float2 texDimensions = float2(texW, texH);
	float2 unitRange = float2(pxRange) / texDimensions;
	float2 screenTexSize = float2(1.0) / fwidth(input.texCoord);
	float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);

	float screenPxDistance = screenPxRange * (sd - 0.5);
	float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
	if (opacity < 0.001)
		discard;

	output.color = float4(texColor.rgb, texColor.a * opacity);
	if (output.color.a < 0.001)
		discard;

	output.entityID = input.entityID;
	return output;
}
