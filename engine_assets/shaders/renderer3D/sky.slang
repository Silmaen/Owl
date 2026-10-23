// The environment drawn as the sky: a full-screen quad looking up the equirectangular image (level 0 of the
// prefiltered environment) along the view ray of each pixel. The model matrix of the scene block holds the inverse
// camera-relative view-projection: the point of the pixel on the near plane (reversed-Z depth 1) gives the ray from
// the eye at the origin. Writes the linear radiance times the environment intensity and no entity. A procedural sky
// adds its sun, moon and stars here, at the resolution of the screen (`skySun`, `skyBodies` of the lighting block).

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

float skyHash(float3 p) {
	p = frac(p * float3(0.1031f, 0.1030f, 0.0973f));
	p += dot(p, p.yxz + 33.33f);
	return frac((p.x + p.y) * p.z);
}

float skyNoise(float2 p) {
	float2 i = floor(p);
	float2 f = frac(p);
	float2 u = f * f * (3.0f - 2.0f * f);
	float a = skyHash(float3(i, 0.0f));
	float b = skyHash(float3(i + float2(1.0f, 0.0f), 0.0f));
	float c = skyHash(float3(i + float2(0.0f, 1.0f), 0.0f));
	float d = skyHash(float3(i + float2(1.0f, 1.0f), 0.0f));
	return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// Stars: one candidate per cell of a cube-face grid of directions, a point about one pixel wide (its size follows the
// screen footprint of the direction), of a random brightness and tint.
float3 starLight(float3 d, float pixelAngle) {
	float3 a = abs(d);
	float2 face;
	float faceId;
	if (a.x >= a.y && a.x >= a.z) {
		face = d.yz / a.x;
		faceId = d.x > 0.0f ? 0.0f : 1.0f;
	} else if (a.y >= a.z) {
		face = d.xz / a.y;
		faceId = d.y > 0.0f ? 2.0f : 3.0f;
	} else {
		face = d.xy / a.z;
		faceId = d.z > 0.0f ? 4.0f : 5.0f;
	}
	const float cells = 260.0f;
	float2 cell = floor((face * 0.5f + 0.5f) * cells);
	float3 key = float3(cell, faceId);
	if (skyHash(key) < 0.97f)
		return float3(0.0f);
	float2 jitter = 0.2f + 0.6f * float2(skyHash(key + 11.0f), skyHash(key + 23.0f));
	float2 centre = ((cell + jitter) / cells) * 2.0f - 1.0f;
	float3 star;
	if (faceId < 2.0f)
		star = float3(faceId < 1.0f ? 1.0f : -1.0f, centre.x, centre.y);
	else if (faceId < 4.0f)
		star = float3(centre.x, faceId < 3.0f ? 1.0f : -1.0f, centre.y);
	else
		star = float3(centre.x, centre.y, faceId < 5.0f ? 1.0f : -1.0f);
	float angle = acos(clamp(dot(d, normalize(star)), -1.0f, 1.0f));
	float size = max(pixelAngle * 0.9f, 1e-5f);
	float brightness = pow(skyHash(key + 37.0f), 6.0f) * 0.9f + 0.1f;
	float3 tint = lerp(float3(0.75f, 0.85f, 1.0f), float3(1.0f, 0.88f, 0.7f), skyHash(key + 51.0f));
	return tint * brightness * exp(-(angle * angle) / (size * size));
}

// The moon: a full disc opposite the sun, darker seas of low-frequency noise, small craters, a darker limb.
float3 moonLight(float3 d, float3 moon, float radius) {
	float3 helper = abs(moon.y) < 0.99f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 t = normalize(cross(helper, moon));
	float3 b = cross(moon, t);
	float2 p = float2(dot(d, t), dot(d, b)) / sin(radius);
	float r2 = dot(p, p);
	if (dot(d, moon) <= 0.0f || r2 >= 1.0f)
		return float3(0.0f);
	float limb = sqrt(1.0f - r2);
	float seas = smoothstep(0.42f, 0.62f, skyNoise(p * 2.2f + 3.1f) * 0.65f + skyNoise(p * 5.3f) * 0.35f);
	float craters = 0.0f;
	float2 grid = p * 6.0f;
	float2 cell = floor(grid);
	float2 local = frac(grid) - 0.5f;
	float pick = skyHash(float3(cell, 7.0f));
	if (pick > 0.55f) {
		float rim = length(local - (float2(skyHash(float3(cell, 9.0f)), skyHash(float3(cell, 13.0f))) - 0.5f) * 0.4f);
		float size = 0.12f + 0.2f * skyHash(float3(cell, 17.0f));
		craters = smoothstep(size, size * 0.6f, rim) * 0.35f - smoothstep(size * 1.2f, size, rim) * 0.12f;
	}
	float albedo = clamp(0.9f - 0.5f * seas - craters, 0.2f, 1.0f);
	float edge = smoothstep(1.0f, 0.97f, r2);
	return float3(0.92f, 0.94f, 1.0f) * albedo * (0.55f + 0.45f * limb) * edge;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 near = mul(gScene.model, float4(input.clip, 1.0f, 1.0f));
	float3 direction = normalize(near.xyz / near.w);
	float3 looked = rotateEnvironment(direction);
	float2 uv = equirectangularUv(looked);
	float3 radiance = gTextures[ENVIRONMENT_SLOT].SampleLevel(uv, 0.0f).rgb * gLighting.environment.x;
	float radius = gLighting.skySun.w;
	if (radius > 0.0f) {
		float3 sun = normalize(gLighting.skySun.xyz);
		float above = smoothstep(-0.01f, 0.01f, looked.y);
		float pixelAngle = length(fwidth(looked));
		float sunCos = dot(looked, sun);
		float disc = smoothstep(cos(radius) - pixelAngle * 1.5f, cos(radius), sunCos);
		float halo = pow(saturate(sunCos), 900.0f) * 0.08f + pow(saturate(sunCos), 90.0f) * 0.01f;
		float3 sunColour = lerp(float3(1.0f, 0.5f, 0.22f), float3(1.0f, 0.96f, 0.88f), smoothstep(0.0f, 0.3f, sun.y));
		radiance += sunColour * (disc + halo) * gLighting.skyBodies.x * above;
		radiance += moonLight(looked, -sun, radius * 1.5f) * gLighting.skyBodies.y * above;
		if (gLighting.skyBodies.z > 0.0f)
			radiance += starLight(looked, pixelAngle) * gLighting.skyBodies.z * smoothstep(0.0f, 0.12f, looked.y);
	}
	output.color = float4(radiance, 1.0f);
	output.entityID = -1;
	return output;
}
