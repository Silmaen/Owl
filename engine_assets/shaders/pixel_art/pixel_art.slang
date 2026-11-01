// Pixel-art composite of a 3D layer drawn at a low resolution: texture 0 holds its colour, texture 1 its depth
// (reversed-Z, 0 where nothing was drawn), both `frame.zw` texels. Each pixel of the frame shows the texel it falls in
// (nearest), its brightness rounded to `params.x` levels; an empty texel beside a covered one takes the outline
// colour. The depth of the texel is written, so the layers drawn next hide it or not as they would hide the meshes.

struct PixelArtParams {
	float4 outline;// rgb: linear colour of the outline, a: 1 to draw it.
	float4 frame;  // xy: frame size in pixels, zw: low-resolution size in texels.
	float4 params; // x: brightness levels (below 2 keeps the colours).
};

[[vk::binding(0)]]
ConstantBuffer<PixelArtParams> gParams;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[2];// 0 colour, 1 depth, of the low-resolution layer.

struct VertexInput {
	[[vk::location(0)]] float2 position : POSITION;
};

struct VertexOutput {
	float4 svPosition : SV_Position;
};

struct FragmentOutput {
	float4 color    : SV_Target0;
	int    entityID : SV_Target1;
	float  depth    : SV_Depth;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
	output.svPosition = float4(input.position, 0.0, 1.0);
	return output;
}

static const float EMPTY_DEPTH = 1e-7f;

float2 texelUv(int2 texel) {
	return (float2(texel) + 0.5f) / gParams.frame.zw;
}

float depthAt(int2 texel) {
	if (any(texel < int2(0, 0)) || any(texel >= int2(gParams.frame.zw)))
		return 0.0f;
	return gTextures[1].SampleLevel(texelUv(texel), 0.0f).r;
}

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input) {
	FragmentOutput output;
	output.entityID = -1;
	int2 texel = int2(floor(input.svPosition.xy / gParams.frame.xy * gParams.frame.zw));
	float depth = depthAt(texel);
	if (depth > EMPTY_DEPTH) {
		float3 color = max(gTextures[0].SampleLevel(texelUv(texel), 0.0f).rgb, float3(0.0f, 0.0f, 0.0f));
		float levels = gParams.params.x;
		float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
		if (levels >= 2.0f && luminance > 1e-6f) {
			// The brightness is rounded, not each channel, so the hues stay; on a square-root scale, so the dark
			// tones keep as many steps as the light ones.
			float steps = round(sqrt(luminance) * (levels - 1.0f)) / (levels - 1.0f);
			color *= steps * steps / luminance;
		}
		output.color = float4(color, 1.0f);
		output.depth = depth;
		return output;
	}
	if (gParams.outline.a <= 0.0f)
		discard;
	float nearest = max(max(depthAt(texel + int2(1, 0)), depthAt(texel - int2(1, 0))),
						max(depthAt(texel + int2(0, 1)), depthAt(texel - int2(0, 1))));
	if (nearest <= EMPTY_DEPTH)
		discard;
	output.color = float4(gParams.outline.rgb, 1.0f);
	output.depth = nearest;
	return output;
}
