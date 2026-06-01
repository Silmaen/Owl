// Renderer2D text — instanced, SSBO-driven. Same instance layout as
// `quad.slang` minus the tiling factor (text never tiles); the fragment
// shader runs the MSDF screenPxRange path instead of the plain sample.
// One drawcall per batch via `glDrawElementsInstanced(GL_TRIANGLES, 6,
// instanceCount)`. World matrix resolution follows the same scene /
// transient split as `quad.slang`.

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

struct TextInstance {
	int worldIndex;
	float4 color;
	float2 uv[4];
	uint texIndex;
	int entityId;
};

[[vk::binding(2)]]
StructuredBuffer<TextInstance> gInstances;

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
	[[vk::location(2)]] nointerpolation int texIndex : TEXCOORD1;
	[[vk::location(3)]] nointerpolation int entityID : TEXCOORD2;
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

float median(float r, float g, float b) {
	return max(min(r, g), min(max(r, g), b));
}

float4x4 resolveWorldMatrix(int worldIndex) {
	if (worldIndex < 0) {
		return gTransientWorlds[-(worldIndex + 1)];
	}
	return gSceneWorlds[worldIndex];
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint iid : SV_InstanceID) {
	VertexOutput output;
	TextInstance inst = gInstances[iid];
	float4x4 world = resolveWorldMatrix(inst.worldIndex);
	float4 worldPos = mul(world, float4(kCorners[input.cornerIndex], 0.0f, 1.0f));
	output.svPosition = mul(gCamera.viewProjection, worldPos);
	output.frag.color = inst.color;
	output.frag.texCoord = inst.uv[input.cornerIndex];
	output.frag.texIndex = int(inst.texIndex);
	output.frag.entityID = inst.entityId;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 texColor = input.color;

	float3 msd = gTextures[NonUniformResourceIndex(input.texIndex)].Sample(input.texCoord).rgb;
	float sd = median(msd.r, msd.g, msd.b);

	const float pxRange = 2.0;
	uint texW, texH;
	gTextures[NonUniformResourceIndex(input.texIndex)].GetDimensions(texW, texH);
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
