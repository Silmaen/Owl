// Bloom chain and automatic exposure of the post-process stack, one full-screen pass per step: mode 0 thresholds and
// downsamples the scene colour (13 taps, soft knee on the brightness the adapted exposure of texture 1 shows when target.w is 1),
// mode 1 downsamples a level (13 taps), mode 2 upsamples the level below with a 9-tap tent and adds the level of the
// same size; mode 3 measures the log2 luminance of the scene, averaged per texel, mode 4 halves it (one bilinear tap
// is the mean of four), mode 5 adapts the EV100 of texture 1 towards the measured one. Linear HDR values in and out.

struct BloomParams {
	float4 texel;    // xy: texel size of the source, zw: texel size of the added level (mode 2).
	float4 threshold;// x threshold, y knee, z scatter (weight of the upsampled level); mode 5: x pre-exposure EV100, y darkest and z brightest EV100, w largest change.
	int4 mode;       // x: the mode; y: 1 when mode 5 starts from the measure; zw unused.
	float4 target;   // xy: size of the target in pixels; z: pre-exposure EV100, w: 1 when mode 0 applies the adapted exposure.
};

[[vk::binding(0)]]
ConstantBuffer<BloomParams> gParams;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[2];

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

float3 sampleSource(float2 uv) {
	return gTextures[0].SampleLevel(uv, 0.0).rgb;
}

// 13-tap downsample (Jimenez, Call of Duty: Advanced Warfare).
float3 downsample(float2 uv) {
	float2 t = gParams.texel.xy;
	float3 a = sampleSource(uv + t * float2(-2, -2));
	float3 b = sampleSource(uv + t * float2(0, -2));
	float3 c = sampleSource(uv + t * float2(2, -2));
	float3 d = sampleSource(uv + t * float2(-2, 0));
	float3 e = sampleSource(uv);
	float3 f = sampleSource(uv + t * float2(2, 0));
	float3 g = sampleSource(uv + t * float2(-2, 2));
	float3 h = sampleSource(uv + t * float2(0, 2));
	float3 i = sampleSource(uv + t * float2(2, 2));
	float3 j = sampleSource(uv + t * float2(-1, -1));
	float3 k = sampleSource(uv + t * float2(1, -1));
	float3 l = sampleSource(uv + t * float2(-1, 1));
	float3 m = sampleSource(uv + t * float2(1, 1));
	return e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
}

float3 upsampleTent(float2 uv) {
	float2 t = gParams.texel.xy;
	float3 sum = sampleSource(uv) * 4.0;
	sum += (sampleSource(uv + t * float2(-1, 0)) + sampleSource(uv + t * float2(1, 0)) +
			sampleSource(uv + t * float2(0, -1)) + sampleSource(uv + t * float2(0, 1))) * 2.0;
	sum += sampleSource(uv + t * float2(-1, -1)) + sampleSource(uv + t * float2(1, -1)) +
		   sampleSource(uv + t * float2(-1, 1)) + sampleSource(uv + t * float2(1, 1));
	return sum / 16.0;
}

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input) {
	FragmentOutput output;
	float2 uv = input.svPosition.xy / gParams.target.xy;
	float3 color;
	if (gParams.mode.x == 3) {
		// Mean log2 luminance of a 4x4 grid inside the tile of the scene this texel covers.
		float2 tile = 1.0 / gParams.target.xy;
		float sum = 0.0;
		for (int y = 0; y < 4; ++y)
			for (int x = 0; x < 4; ++x) {
				float2 at = uv + tile * (float2(x, y) + 0.5 - 2.0) / 4.0;
				float luminance = dot(sampleSource(at), float3(0.2126, 0.7152, 0.0722));
				sum += log2(max(luminance, 1e-9));
			}
		output.color = float4(sum / 16.0, 0.0, 0.0, 1.0);
		output.entityID = -1;
		return output;
	}
	if (gParams.mode.x == 4) {
		output.color = float4(sampleSource(uv).r, 0.0, 0.0, 1.0);
		output.entityID = -1;
		return output;
	}
	if (gParams.mode.x == 5) {
		// EV100 = log2(L * 100 / 12.5) with L the scene luminance, the pre-exposed one divided by 1 / (1.2 * 2^EV100).
		float measured = sampleSource(float2(0.5, 0.5)).r + log2(1.2) + gParams.threshold.x + 3.0;
		float wanted = clamp(measured, gParams.threshold.y, gParams.threshold.z);
		float previous = gTextures[1].SampleLevel(float2(0.5, 0.5), 0.0).r;
		float adapted = gParams.mode.y != 0 ? wanted
							: previous + clamp(wanted - previous, -gParams.threshold.w, gParams.threshold.w);
		output.color = float4(adapted, 0.0, 0.0, 1.0);
		output.entityID = -1;
		return output;
	}
	if (gParams.mode.x == 0) {
		color = downsample(uv);
		// Soft-knee threshold on the brightness as displayed; the colour stays pre-exposed (the encode exposes it).
		float displayed = gParams.target.w > 0.0
								  ? exp2(gParams.target.z - gTextures[1].SampleLevel(float2(0.5, 0.5), 0.0).r)
								  : 1.0;
		float brightness = max(color.r, max(color.g, color.b)) * displayed;
		float knee = gParams.threshold.x * gParams.threshold.y + 1e-5;
		float soft = clamp(brightness - gParams.threshold.x + knee, 0.0, 2.0 * knee);
		soft = soft * soft / (4.0 * knee);
		float contribution = max(soft, brightness - gParams.threshold.x) / max(brightness, 1e-9);
		color *= contribution;
	} else if (gParams.mode.x == 1) {
		color = downsample(uv);
	} else {
		color = upsampleTent(uv) * gParams.threshold.z + gTextures[1].SampleLevel(uv, 0.0).rgb;
	}
	output.color = float4(max(color, 0.0), 1.0);
	output.entityID = -1;
	return output;
}
