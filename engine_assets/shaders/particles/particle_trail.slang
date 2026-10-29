// Ribbon trails: behind each particle of an emitter (one instance per slot, its head at the particle, then the points
// it recorded), or behind an entity (one instance, the points of `gTrails`). The ribbon faces the camera; it tapers and
// fades along its length.

#include "particle_draw.slangh"

struct VertexInput {
	[[vk::location(0)]] int vertexIndex : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float4 color : COLOR;
	[[vk::location(1)]] float across : TEXCOORD0;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

float3 pointOf(uint instance, uint j) {
	if (gParams.trail.y != 0u)
		return gTrails[j].position.xyz;
	if (j == 0u)
		return worldOf(gParticles[instance].position.xyz);
	return worldOf(gTrails[instance * gParams.trail.x + j - 1u].position.xyz);
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instanceId : SV_InstanceID) {
	VertexOutput output;
	output.frag.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
	output.frag.across = 0.0f;
	output.svPosition = OUTSIDE;
	bool entity = gParams.trail.y != 0u;
	uint count = entity ? gParams.trail.z : gParams.trail.x + 1u;
	uint j = uint(input.vertexIndex) / 2u;
	float sideSign = (uint(input.vertexIndex) % 2u) == 0u ? -1.0f : 1.0f;
	if (count < 2u || j >= count)
		return output;
	float width = gParams.trailParams.x;
	float along = float(j) / float(count - 1u);
	float4 color;
	if (entity) {
		width *= sizeAt(along);
		color = colorAt(along);
	} else {
		if (instanceId >= gParams.config.x)
			return output;
		Particle p = gParticles[instanceId];
		if (!isAlive(p))
			return output;
		float life = p.position.w / p.velocity.w;
		width *= p.extra.x * sizeAt(life) * (1.0f - along);
		color = colorAt(life);
		color.a *= 1.0f - along;
	}
	float3 position = pointOf(instanceId, j);
	float3 tangent = pointOf(instanceId, min(j + 1u, count - 1u)) - pointOf(instanceId, j == 0u ? 0u : j - 1u);
	float3 toEye = gParams.eye.w != 0.0f ? gParams.eye.xyz - position : -gParams.forward.xyz;
	float3 side = cross(tangent, toEye);
	float sideLength = length(side);
	side = sideLength > 1e-6f ? side / sideLength : gParams.right.xyz;
	output.svPosition = mul(gParams.viewProjection, float4(position + side * (0.5f * width * sideSign), 1.0f));
	output.frag.color = color;
	output.frag.across = sideSign;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 color = input.color;
	color.a *= saturate(1.0f - input.across * input.across);
	if (color.a < 0.02f)
		discard;
	output.color = shade(color);
	output.entityID = int(gParams.trail.w);
	return output;
}
