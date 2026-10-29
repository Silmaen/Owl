// Effects of the linear scene colour that read its depth (texture 1: reversed-Z depth of the 3D layers, 0 where
// empty). Mode 0, depth of field: the blur radius of a pixel grows with its distance to the focus; a disc of taps
// gathers the neighbours whose own blur covers the pixel, the ones behind a sharper pixel only within its radius, so
// a sharp object keeps its edges over a blurred background. Mode 1, motion blur: the point of each pixel is moved
// back to where the previous frame saw it through the reprojection matrix, and the colour is averaged along that
// motion.

struct EffectParams {
	column_major float4x4 reprojection; // motion blur: clip space of this frame to the clip space of the previous one.
	float4 params; // depth of field: xy depth of a distance d (x + y / d), z focus distance, w focus range;
	               // motion blur: x shutter fraction, y largest motion in pixels.
	float4 frame;  // xy: size of the frame in pixels, z: largest blur radius in pixels, w: mode.
};

[[vk::binding(0)]]
ConstantBuffer<EffectParams> gParams;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[2];// 0 scene colour, 1 scene depth.

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

static const int BOKEH_TAPS = 32;
static const int MOTION_TAPS = 12;
static const float GOLDEN_ANGLE = 2.39996323f;

// A point of the frame kept half a pixel inside it: the samplers may wrap around.
float2 inside(float2 uv) {
	float2 inset = 0.5f / gParams.frame.xy;
	return clamp(uv, inset, 1.0f - inset);
}

float viewDistance(float2 uv) {
	float depth = gTextures[1].SampleLevel(uv, 0.0f).r;
	return gParams.params.y / max(depth - gParams.params.x, 1e-7f);
}

float blurRadius(float distance) {
	return gParams.frame.z * saturate(abs(distance - gParams.params.z) / max(gParams.params.w, 1e-4f));
}

float4 depthOfField(float2 uv) {
	float centreDistance = viewDistance(uv);
	float centreRadius = blurRadius(centreDistance);
	float4 centre = gTextures[0].SampleLevel(uv, 0.0f);
	float3 sum = centre.rgb;
	float weight = 1.0f;
	for (int tap = 0; tap < BOKEH_TAPS; ++tap) {
		float radius = gParams.frame.z * sqrt((float(tap) + 0.5f) / float(BOKEH_TAPS));
		float angle = float(tap) * GOLDEN_ANGLE;
		float2 tapUv = inside(uv + float2(cos(angle), sin(angle)) * radius / gParams.frame.xy);
		float tapDistance = viewDistance(tapUv);
		float covers = saturate(blurRadius(tapDistance) - radius + 1.0f);
		if (tapDistance > centreDistance)
			covers = min(covers, saturate(centreRadius - radius + 1.0f));
		sum += gTextures[0].SampleLevel(tapUv, 0.0f).rgb * covers;
		weight += covers;
	}
	return float4(sum / weight, centre.a);
}

float4 motionBlur(float2 uv) {
	float depth = max(gTextures[1].SampleLevel(uv, 0.0f).r, 1e-7f);
	float4 previous = mul(gParams.reprojection, float4(uv * 2.0f - 1.0f, depth, 1.0f));
	float2 previousUv = previous.xy / max(previous.w, 1e-7f) * 0.5f + 0.5f;
	float2 motion = (uv - previousUv) * gParams.params.x;
	float2 pixels = motion * gParams.frame.xy;
	float length2 = dot(pixels, pixels);
	if (length2 > gParams.params.y * gParams.params.y)
		motion *= gParams.params.y / sqrt(length2);
	float4 centre = gTextures[0].SampleLevel(uv, 0.0f);
	float3 sum = float3(0.0f);
	for (int tap = 0; tap < MOTION_TAPS; ++tap) {
		float t = (float(tap) + 0.5f) / float(MOTION_TAPS) - 0.5f;
		sum += gTextures[0].SampleLevel(inside(uv + motion * t), 0.0f).rgb;
	}
	return float4(sum / float(MOTION_TAPS), centre.a);
}

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input) {
	float2 uv = input.svPosition.xy / gParams.frame.xy;
	FragmentOutput output;
	output.color = gParams.frame.w < 0.5f ? depthOfField(uv) : motionBlur(uv);
	output.entityID = -1;
	return output;
}
