
#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[32];

static const float PI = 3.14159265359;

struct VertexInput {
	[[vk::location(0)]]  float3 position : POSITION;
	[[vk::location(1)]]  float2 texCoord : TEXCOORD0;
	[[vk::location(2)]]  float  texIndex : TEXCOORD1;
	[[vk::location(3)]]  float4 color    : COLOR0;
	[[vk::location(4)]]  float4 topColor : COLOR1;
	[[vk::location(5)]]  int    mode     : TEXCOORD2;
	[[vk::location(6)]]  int    entityID : TEXCOORD3;
	[[vk::location(7)]]  float4 invVR0   : TEXCOORD4;
	[[vk::location(8)]]  float4 invVR1   : TEXCOORD5;
	[[vk::location(9)]]  float4 invVR2   : TEXCOORD6;
	[[vk::location(10)]] float4 invVR3   : TEXCOORD7;
};

struct FragmentInput {
	[[vk::location(0)]] float2 texCoord  : TEXCOORD0;
	[[vk::location(1)]] float4 color     : COLOR0;
	[[vk::location(2)]] float4 topColor  : COLOR1;
	[[vk::location(3)]] nointerpolation float texIndex : TEXCOORD1;
	[[vk::location(4)]] nointerpolation int   mode     : TEXCOORD2;
	[[vk::location(5)]] nointerpolation int   entityID : TEXCOORD3;
	[[vk::location(6)]] nointerpolation float4 invVR0  : TEXCOORD4;
	[[vk::location(7)]] nointerpolation float4 invVR1  : TEXCOORD5;
	[[vk::location(8)]] nointerpolation float4 invVR2  : TEXCOORD6;
	[[vk::location(9)]] nointerpolation float4 invVR3  : TEXCOORD7;
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
	output.frag.texCoord = input.texCoord;
	output.frag.color    = input.color;
	output.frag.topColor = input.topColor;
	output.frag.texIndex = input.texIndex;
	output.frag.mode     = input.mode;
	output.frag.entityID = input.entityID;
	output.frag.invVR0   = input.invVR0;
	output.frag.invVR1   = input.invVR1;
	output.frag.invVR2   = input.invVR2;
	output.frag.invVR3   = input.invVR3;
	output.svPosition    = float4(input.position.xy, 0.0, 1.0);
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	output.entityID = input.entityID;
	int idx = int(input.texIndex);

	if (input.mode == 0) {
		// Solid color
		output.color = input.color;
	} else if (input.mode == 1) {
		// Gradient (bottom to top)
		output.color = lerp(input.color, input.topColor, input.texCoord.y);
	} else if (input.mode == 2) {
		// Texture background
		output.color = gTextures[NonUniformResourceIndex(idx)].Sample(input.texCoord);
	} else {
		// Skybox equirectangular
		// GLSL mat4(v0,v1,v2,v3) takes columns, Slang float4x4(v0,v1,v2,v3) takes rows → transpose
		float4x4 invVR = transpose(float4x4(input.invVR0, input.invVR1, input.invVR2, input.invVR3));
		float2 ndc = input.texCoord * 2.0 - 1.0;
		float4 worldDir = mul(invVR, float4(ndc, -1.0, 1.0));
		float3 dir = normalize(worldDir.xyz);
		float2 uv = float2(atan2(dir.z, dir.x) / (2.0 * PI) + 0.5,
							asin(clamp(dir.y, -1.0, 1.0)) / PI + 0.5);
		output.color = gTextures[NonUniformResourceIndex(idx)].Sample(uv);
	}
	return output;
}
