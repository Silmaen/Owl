// Particles of an emitter as camera-facing quads, one instance per slot (dead ones collapse), in the order of the GPU
// sort for an alpha-blended emitter. A soft disc, or the texture of the emitter, tinted by the colour over life.

#include "particle_draw.slangh"

struct VertexInput {
	[[vk::location(0)]] int cornerIndex : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float4 color : COLOR;
	[[vk::location(1)]] float2 uv : TEXCOORD0;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

static const float2 kCorners[4] = {
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instanceId : SV_InstanceID) {
	VertexOutput output;
	uint index = gParams.config.y != 0u ? gOrder[instanceId].value : instanceId;
	float2 corner = kCorners[input.cornerIndex];
	output.frag.uv = float2(corner.x, 1.0f - corner.y);
	output.frag.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
	output.svPosition = OUTSIDE;
	if (index >= gParams.config.x)
		return output;
	Particle p = gParticles[index];
	if (!isAlive(p))
		return output;
	float life = p.position.w / p.velocity.w;
	float size = p.extra.x * sizeAt(life);
	float2 c = corner * 2.0f - 1.0f;
	float s = sin(p.extra.y);
	float co = cos(p.extra.y);
	float2 r = float2(c.x * co - c.y * s, c.x * s + c.y * co) * (0.5f * size);
	float3 world = worldOf(p.position.xyz) + gParams.right.xyz * r.x + gParams.up.xyz * r.y;
	output.svPosition = mul(gParams.viewProjection, float4(world, 1.0f));
	output.frag.color = colorAt(life);
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 color = input.color;
	if (gParams.config.z != 0u) {
		color *= gTextures[0].Sample(input.uv);
	} else {
		float2 d = input.uv * 2.0f - 1.0f;
		float falloff = saturate(1.0f - dot(d, d));
		color.a *= falloff * falloff;
	}
	if (color.a < 0.02f)
		discard;
	output.color = shade(color);
	output.entityID = int(gParams.trail.w);
	return output;
}
