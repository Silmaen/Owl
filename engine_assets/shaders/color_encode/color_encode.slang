// End of the world chain: reads the linear scene colour pixel for pixel (its red and blue channels apart towards the
// edges with a chromatic aberration), applies the exposure and the bloom, the camera tone mapping, the vignette, the
// film grain and the colour-grading LUT, and encodes to sRGB into the display target. The entity id is copied
// afterwards, the id output only fills the slot.

struct EncodeParams {
	int4 mode;          // x: tone mapping (0 None, 1 Khronos PBR Neutral, 2 ACES), y: 1 with bloom, z: 1 with a LUT, w: LUT size.
	float4 post;        // x exposure multiplier, y bloom intensity, z vignette intensity (0 off), w vignette smoothness.
	float4 vignetteColor;// rgb: colour of the vignette (sRGB), w: LUT contribution.
	float4 frame;       // xy: size of the frame in pixels; z: EV100 of the pre-exposure; w: 1 to apply the adapted EV100 of texture 3.
	float4 effects;     // x: chromatic aberration (0 off), y: film grain (0 off), z: grain size in pixels, w: grain seed.
};

[[vk::binding(0)]]
ConstantBuffer<EncodeParams> gParams;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[4];// 0 scene colour, 1 bloom, 2 LUT strip, 3 adapted EV100 (one texel).

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

float3 srgbToLinear(float3 c) {
	return select(c <= 0.04045f, c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f));
}

// The strip holds the N slices of blue side by side: two bilinear lookups blended along blue.
float3 applyLut(float3 c) {
	float n = float(gParams.mode.w);
	float3 scaled = saturate(c) * (n - 1.0f);
	float slice = floor(scaled.b);
	float t = scaled.b - slice;
	float2 size = float2(n * n, n);
	float2 uv0 = (float2(scaled.r + slice * n, scaled.g) + 0.5f) / size;
	float2 uv1 = uv0 + float2(n / size.x, 0.0f);
	float3 a = gTextures[2].SampleLevel(uv0, 0.0f).rgb;
	float3 b = gTextures[2].SampleLevel(slice + 1.0f < n ? uv1 : uv0, 0.0f).rgb;
	return lerp(a, b, t);
}

float3 linearToSrgb(float3 c) {
	float3 low = c * 12.92;
	float3 high = 1.055 * pow(c, float3(1.0 / 2.4)) - 0.055;
	return float3(c.r <= 0.0031308 ? low.r : high.r, c.g <= 0.0031308 ? low.g : high.g,
				  c.b <= 0.0031308 ? low.b : high.b);
}

float grainHash(float3 p) {
	p = frac(p * float3(0.1031f, 0.1030f, 0.0973f));
	p += dot(p, p.yzx + 33.33f);
	return frac((p.x + p.y) * p.z);
}

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input) {
	float2 uv = input.svPosition.xy / gParams.frame.xy;
	float4 scene = gTextures[0].SampleLevel(uv, 0.0);
	if (gParams.effects.x > 0.0) {
		float2 offset = (uv - 0.5f) * (0.02f * gParams.effects.x);
		float2 inset = 0.5f / gParams.frame.xy;
		scene.r = gTextures[0].SampleLevel(clamp(uv - offset, inset, 1.0f - inset), 0.0).r;
		scene.b = gTextures[0].SampleLevel(clamp(uv + offset, inset, 1.0f - inset), 0.0).b;
	}
	float3 color = max(scene.rgb, float3(0.0));
	if (gParams.mode.y != 0)
		color += gTextures[1].SampleLevel(input.svPosition.xy / gParams.frame.xy, 0.0).rgb * gParams.post.y;
	float exposure = gParams.post.x;
	if (gParams.frame.w > 0.0)
		exposure *= exp2(gParams.frame.z - gTextures[3].SampleLevel(float2(0.5, 0.5), 0.0).r);
	color *= exposure;
	if (gParams.mode.x == 1)
		color = pbrNeutral(color);
	else if (gParams.mode.x == 2)
		color = acesFilmic(color);
	color = saturate(color);
	if (gParams.post.z > 0.0) {
		float2 centred = input.svPosition.xy / gParams.frame.xy - 0.5f;
		float edge = smoothstep(0.75f - gParams.post.w, 0.75f, length(centred) * 1.41421f);
		color = lerp(color, srgbToLinear(gParams.vignetteColor.rgb), edge * gParams.post.z);
	}
	if (gParams.effects.y > 0.0) {
		float3 cell = float3(floor(input.svPosition.xy / gParams.effects.z), gParams.effects.w);
		float noise = grainHash(cell) + grainHash(cell + 17.0f) - 1.0f;
		float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
		float response = 0.25f + 3.0f * luminance * (1.0f - luminance);
		color = saturate(color + noise * gParams.effects.y * 0.5f * response);
	}
	float3 encoded = linearToSrgb(color);
	if (gParams.mode.z != 0)
		encoded = lerp(encoded, applyLut(encoded), gParams.vignetteColor.w);
	FragmentOutput output;
	output.color = float4(encoded, saturate(scene.a));
	output.entityID = -1;
	return output;
}
