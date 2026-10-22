// The environment drawn as the sky: a full-screen quad looking up the equirectangular image (level 0 of the
// prefiltered environment) along the view ray of each pixel. The model matrix of the scene block holds the inverse
// camera-relative view-projection: the point of the pixel on the near plane (reversed-Z depth 1) gives the ray from
// the eye at the origin. Writes the linear radiance times the environment intensity and no entity.

#include "pbr.slangh"

struct VertexInput {
	[[vk::location(0)]] float3 position : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float2 clip : TEXCOORD0;
};

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
#ifdef BACKEND_VULKAN
	// Vertices are given in OpenGL NDC (y up); the flipped projections expect clip y down.
	float2 clip = float2(input.position.x, -input.position.y);
#else
	float2 clip = input.position.xy;
#endif
	output.frag.clip = clip;
	output.svPosition = float4(clip, 0.0f, 1.0f);
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 near = mul(gScene.model, float4(input.clip, 1.0f, 1.0f));
	float3 direction = normalize(near.xyz / near.w);
	float2 uv = equirectangularUv(rotateEnvironment(direction));
	float3 radiance = gTextures[ENVIRONMENT_SLOT].SampleLevel(uv, 0.0f).rgb * gLighting.environment.x;
	output.color = float4(radiance, 1.0f);
	output.entityID = -1;
	return output;
}
