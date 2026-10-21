// End of the world chain: reads the linear scene colour pixel for pixel, applies the camera tone mapping and
// encodes to sRGB into the display target. The entity id is copied afterwards, the id output only fills the slot.

struct EncodeParams {
	int4 mode;// x: tone mapping (0 None, 1 Khronos PBR Neutral, 2 ACES); yzw unused.
};

[[vk::binding(0)]]
ConstantBuffer<EncodeParams> gParams;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gScene;

struct VertexInput {
	[[vk::location(0)]] float2 position : POSITION;
};

struct VertexOutput {
	float4 svPosition : SV_Position;
};

struct FragmentOutput {
	float4 color    : SV_Target0;
	int    entityID : SV_Target1;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
	output.svPosition = float4(input.position, 0.0, 1.0);
	return output;
}

float3 pbrNeutral(float3 color) {
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;
	float x = min(color.r, min(color.g, color.b));
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;
	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression)
		return color;
	const float d = 1.0 - startCompression;
	float newPeak = 1.0 - d * d / (peak + d - startCompression);
	color *= newPeak / peak;
	float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
	return lerp(color, float3(newPeak), g);
}

float3 acesFilmic(float3 x) {
	return (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
}

float3 linearToSrgb(float3 c) {
	float3 low = c * 12.92;
	float3 high = 1.055 * pow(c, float3(1.0 / 2.4)) - 0.055;
	return float3(c.r <= 0.0031308 ? low.r : high.r, c.g <= 0.0031308 ? low.g : high.g,
				  c.b <= 0.0031308 ? low.b : high.b);
}

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input) {
	float4 scene = gScene.Load(int3(int2(input.svPosition.xy), 0));
	float3 color = max(scene.rgb, float3(0.0));
	if (gParams.mode.x == 1)
		color = pbrNeutral(color);
	else if (gParams.mode.x == 2)
		color = acesFilmic(color);
	FragmentOutput output;
	output.color = float4(linearToSrgb(saturate(color)), saturate(scene.a));
	output.entityID = -1;
	return output;
}
