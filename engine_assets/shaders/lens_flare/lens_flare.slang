// Lens flares of the bright lights and the sun. Mode 1 copies the scene colour into the flare target (one full-screen
// quad); mode 0 adds the flare elements of every source over it, one instance per element: a glow with four streaks at
// the source, a halo ring around the centre of the frame and `ghosts` discs along the line from the source through the
// centre. The vertex stage measures how much of the source the depth of the frame lets through (16 taps around it,
// reversed-Z: a nearer occluder holds a larger depth, an empty pixel 0) and scales its elements by it.
//
// Layout contract (must match `renderer::LensFlare`):
//   binding 0 : UBO  - FlareParams
//   binding 1 : Sampler2D[2] - 0 scene colour, 1 scene depth

struct FlareParams {
	column_major float4x4 viewProjection;
	float4 sources[8];// xyz world position (w 1) or direction towards the light (w 0).
	float4 colors[8]; // rgb pre-exposed colour of the flare of each source.
	float4 params;    // x ghost count, y ghost spacing, z halo radius, w glow size (fractions of the frame height).
	float4 frame;     // xy frame size in pixels, z aspect ratio, w mode (0 elements, 1 copy).
	uint4 counts;     // x number of sources.
};

[[vk::binding(0)]]
ConstantBuffer<FlareParams> gParams;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[2];

struct VertexInput {
	[[vk::location(0)]] int cornerIndex : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float2 local : TEXCOORD0;
	[[vk::location(1)]] nointerpolation float4 color : COLOR;// rgb colour, w element kind (0 glow, 1 halo, 2 ghost).
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

static const float2 kCorners[4] = {
	float2(-1.0f, -1.0f),
	float2(1.0f, -1.0f),
	float2(1.0f, 1.0f),
	float2(-1.0f, 1.0f),
};

static const float3 kGhostTints[3] = {
	float3(1.0f, 0.55f, 0.3f),
	float3(0.4f, 0.8f, 1.0f),
	float3(0.7f, 1.0f, 0.5f),
};

static const float4 OUTSIDE = float4(2.0f, 2.0f, 2.0f, 1.0f);

// Share of the source the depth lets through.
float visibility(float2 ndc, float sourceDepth) {
	float2 uv = ndc * 0.5f + 0.5f;
	float2 texel = 2.0f / gParams.frame.xy;
	float seen = 0.0f;
	for (int y = 0; y < 4; ++y) {
		for (int x = 0; x < 4; ++x) {
			float2 tap = saturate(uv + (float2(x, y) - 1.5f) * texel);
			float depth = gTextures[1].SampleLevel(tap, 0.0f).r;
			seen += depth <= sourceDepth + 1e-6f ? 1.0f : 0.0f;
		}
	}
	return seen / 16.0f;
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instanceId : SV_InstanceID) {
	VertexOutput output;
	float2 corner = kCorners[input.cornerIndex];
	output.frag.local = corner;
	output.frag.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
	if (gParams.frame.w > 0.5f) {
		output.svPosition = float4(corner, 0.0f, 1.0f);
		return output;
	}
	output.svPosition = OUTSIDE;
	uint ghosts = uint(gParams.params.x);
	uint perSource = ghosts + 2u;
	uint source = instanceId / perSource;
	uint element = instanceId % perSource;
	if (source >= gParams.counts.x)
		return output;
	float4 position = gParams.sources[source];
	float4 clip = mul(gParams.viewProjection, position);
	if (clip.w <= 1e-5f)
		return output;
	float2 ndc = clip.xy / clip.w;
	if (any(abs(ndc) > 1.2f))
		return output;
	float sourceDepth = position.w > 0.5f ? clip.z / clip.w : 0.0f;
	float seen = visibility(ndc, sourceDepth);
	if (seen <= 0.0f)
		return output;
	float3 color = gParams.colors[source].rgb * seen;
	float2 centre = ndc;
	float size = gParams.params.w;
	float kind = 0.0f;
	if (element == 1u) {
		centre = float2(0.0f, 0.0f);
		size = gParams.params.z;
		kind = 1.0f;
		color *= 0.06f * saturate(1.0f - length(ndc) * 0.5f);
	} else if (element >= 2u) {
		uint ghost = element - 2u;
		float offset = 1.0f - gParams.params.y * float(ghost + 1u);
		centre = ndc * offset;
		size = gParams.params.w * (0.25f + 0.15f * float(ghost % 3u));
		kind = 2.0f;
		color *= kGhostTints[ghost % 3u] * 0.12f;
	}
	output.svPosition = float4(centre + corner * float2(size / gParams.frame.z, size), 0.0f, 1.0f);
	output.frag.color = float4(color, kind);
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input, float4 svPosition : SV_Position) {
	FragmentOutput output;
	output.entityID = -1;
	if (gParams.frame.w > 0.5f) {
		output.color = gTextures[0].SampleLevel(svPosition.xy / gParams.frame.xy, 0.0f);
		return output;
	}
	float2 p = input.local;
	float r = length(p);
	float shape;
	if (input.color.w < 0.5f) {
		float core = exp(-r * r * 24.0f);
		float streaks = exp(-abs(p.y) * 60.0f) * saturate(1.0f - abs(p.x)) + exp(-abs(p.x) * 60.0f) * saturate(1.0f - abs(p.y));
		shape = core + 0.35f * streaks + 0.2f * saturate(1.0f - r);
	} else if (input.color.w < 1.5f) {
		shape = exp(-pow((r - 0.85f) * 12.0f, 2.0f));
	} else {
		shape = smoothstep(1.0f, 0.8f, r) * (0.6f + 0.4f * smoothstep(0.5f, 0.95f, r));
	}
	if (shape <= 1e-3f)
		discard;
	output.color = float4(input.color.rgb * shape, 1.0f);
	return output;
}
