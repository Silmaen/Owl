// One face of one level of a reflection probe cube, drawn from the captured cube. Mode 0 halves the level above (the
// mip chain of the capture); mode 1 filters the whole chain by GGX importance sampling, each sample read from the
// level whose texels match the solid angle it stands for, so a few samples give a smooth lobe. The face is drawn as a
// full-screen quad without the Vulkan flip, like the faces the probe was captured with, so a pixel and its direction
// in the cube agree on both backends.

struct FilterParams {
	int4 face;     // x: face (+X, -X, +Y, -Y, +Z, -Z), y: mode (0 downsample, 1 GGX), z: level read by a downsample.
	float4 params; // x: GGX alpha of the level, y: face size of level 0 of the source, z: last level of the source.
};

[[vk::binding(0)]]
ConstantBuffer<FilterParams> gParams;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
SamplerCube gSource;

struct VertexInput {
	[[vk::location(0)]] float2 position : POSITION;
};

struct VertexOutput {
	[[vk::location(0)]] float2 ndc : TEXCOORD0;
	float4 svPosition : SV_Position;
};

struct FragmentOutput {
	float4 color    : SV_Target0;
	int    entityID : SV_Target1;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
	output.ndc = input.position;
	output.svPosition = float4(input.position, 0.0, 1.0);
	return output;
}

static const float PI = 3.14159265359f;
static const uint SAMPLE_COUNT = 48;

// Direction of a point of a face, with the look and up vectors of the face cameras.
float3 faceDirection(int face, float2 ndc) {
	float3 forward;
	float3 up;
	switch (face) {
		case 0:
			forward = float3(1.0f, 0.0f, 0.0f);
			up = float3(0.0f, -1.0f, 0.0f);
			break;
		case 1:
			forward = float3(-1.0f, 0.0f, 0.0f);
			up = float3(0.0f, -1.0f, 0.0f);
			break;
		case 2:
			forward = float3(0.0f, 1.0f, 0.0f);
			up = float3(0.0f, 0.0f, 1.0f);
			break;
		case 3:
			forward = float3(0.0f, -1.0f, 0.0f);
			up = float3(0.0f, 0.0f, -1.0f);
			break;
		case 4:
			forward = float3(0.0f, 0.0f, 1.0f);
			up = float3(0.0f, -1.0f, 0.0f);
			break;
		default:
			forward = float3(0.0f, 0.0f, -1.0f);
			up = float3(0.0f, -1.0f, 0.0f);
			break;
	}
	float3 right = normalize(cross(forward, up));
	return normalize(forward + ndc.x * right + ndc.y * cross(right, forward));
}

float2 hammersley(uint i) {
	uint bits = reversebits(i);
	return float2((float(i) + 0.5f) / float(SAMPLE_COUNT), float(bits) * 2.3283064365386963e-10f);
}

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input) {
	FragmentOutput output;
	output.entityID = -1;
	float3 n = faceDirection(gParams.face.x, input.ndc);
	if (gParams.face.y == 0) {
		output.color = float4(gSource.SampleLevel(n, float(gParams.face.z)).rgb, 1.0f);
		return output;
	}
	float3 tangent = normalize(cross(abs(n.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f), n));
	float3 bitangent = cross(n, tangent);
	float alpha2 = gParams.params.x * gParams.params.x;
	float texelSolidAngle = 4.0f * PI / (6.0f * gParams.params.y * gParams.params.y);
	float3 sum = float3(0.0f);
	float weight = 0.0f;
	// A bound the compiler cannot fold (mode is 1 here) keeps the loop rolled: unrolled, llvmpipe compiles it for seconds.
	uint samples = SAMPLE_COUNT + uint(max(1 - gParams.face.y, 0));
	for (uint i = 0; i < samples; ++i) {
		float2 xi = hammersley(i);
		float cosTheta = sqrt((1.0f - xi.y) / (1.0f + (alpha2 - 1.0f) * xi.y));
		float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));
		float phi = 2.0f * PI * xi.x;
		float3 h = tangent * (sinTheta * cos(phi)) + bitangent * (sinTheta * sin(phi)) + n * cosTheta;
		float3 l = 2.0f * dot(n, h) * h - n;
		float nol = dot(n, l);
		if (nol <= 0.0f)
			continue;
		float d = cosTheta * cosTheta * (alpha2 - 1.0f) + 1.0f;
		float pdf = alpha2 / (4.0f * PI * d * d);
		float sampleSolidAngle = 1.0f / (float(SAMPLE_COUNT) * pdf + 1e-6f);
		float lod = clamp(0.5f * log2(sampleSolidAngle / texelSolidAngle) + 1.0f, 0.0f, gParams.params.z);
		sum += gSource.SampleLevel(l, lod).rgb * nol;
		weight += nol;
	}
	output.color = float4(weight > 0.0f ? sum / weight : gSource.SampleLevel(n, 0.0f).rgb, 1.0f);
	return output;
}
