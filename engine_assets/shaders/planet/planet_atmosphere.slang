// Atmosphere shell of a planet: single scattering of the sun (Rayleigh and Mie) along the view ray through the shell,
// stopped by the sea sphere. Seen from outside the front faces are drawn, over the planet disc and its limb; from
// inside the back faces, the sky dome. Everything is relative to the eye, at the drawn scale.
//
// Layout contract (must match `renderer::RendererPlanet`):
//   binding 0 : UBO - AtmosphereParams

struct AtmosphereParams {
	column_major float4x4 viewProjection; // projection * camera-relative view.
	float4 center;     // xyz planet centre from the eye, w radius of the sea surface.
	float4 shell;      // x radius of the shell, y Rayleigh scale height, z Mie scale height, w 1 on a linear target.
	float4 scattering; // xyz Rayleigh coefficients at the sea surface, w the Mie one.
	float4 toSun;      // xyz unit direction toward the sun.
	float4 sunColor;   // xyz pre-exposed illuminance of the sun.
	float4 heights;    // x eye height above the shell, y eye height above the sea surface (from the CPU in double).
	int4 ids;          // x entity id, y 1 when the eye is inside the shell.
};

[[vk::binding(0)]]
ConstantBuffer<AtmosphereParams> gParams;

struct VertexInput {
	[[vk::location(0)]] float3 position : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float3 relative : TEXCOORD0;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

static const int kViewSamples = 12;
static const int kLightSamples = 4;
static const float kPi = 3.14159265f;
static const float kMieG = 0.76f;

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
	float3 relative = gParams.center.xyz + input.position * gParams.shell.x;
	output.frag.relative = relative;
	output.svPosition = mul(gParams.viewProjection, float4(relative, 1.0f));
	return output;
}

// Distances along a ray from `origin` to a sphere around the planet centre, `c` being |origin - centre|² - radius²
// computed without cancellation; x > y when there is no hit.
float2 sphereHits(float3 origin, float3 direction, float c) {
	float b = dot(origin - gParams.center.xyz, direction);
	float discriminant = b * b - c;
	if (discriminant < 0.0f)
		return float2(1.0f, -1.0f);
	float root = sqrt(discriminant);
	return float2(-b - root, -b + root);
}

float sphereC(float3 origin, float radius) {
	float3 offset = origin - gParams.center.xyz;
	return dot(offset, offset) - radius * radius;
}

// Optical depths (Rayleigh, Mie) from a point toward the sun, to the edge of the shell; -1 when the planet shadows it.
float2 sunDepths(float3 point) {
	float3 toSun = gParams.toSun.xyz;
	float2 planet = sphereHits(point, toSun, sphereC(point, gParams.center.w));
	if (planet.x <= planet.y && planet.x > 0.0f)
		return float2(-1.0f, -1.0f);
	float2 shell = sphereHits(point, toSun, sphereC(point, gParams.shell.x));
	float reach = max(shell.y, 0.0f);
	float stride = reach / float(kLightSamples);
	float2 depths = float2(0.0f, 0.0f);
	for (int i = 0; i < kLightSamples; ++i) {
		float3 at = point + toSun * (stride * (float(i) + 0.5f));
		float height = max(distance(at, gParams.center.xyz) - gParams.center.w, 0.0f);
		depths += float2(exp(-height / gParams.shell.y), exp(-height / gParams.shell.z)) * stride;
	}
	return depths;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	output.entityID = gParams.ids.x;
	float3 direction = normalize(input.relative);
	float3 eye = float3(0.0f, 0.0f, 0.0f);
	// Heights from the CPU: |eye - centre|² - r² = h (h + 2r) keeps its precision near the surface of a large planet.
	float shellC = gParams.heights.x * (gParams.heights.x + 2.0f * gParams.shell.x);
	float seaC = gParams.heights.y * (gParams.heights.y + 2.0f * gParams.center.w);
	float2 shell = sphereHits(eye, direction, shellC);
	if (shell.x > shell.y || shell.y <= 0.0f)
		discard;
	float start = max(shell.x, 0.0f);
	float end = shell.y;
	float2 sea = sphereHits(eye, direction, seaC);
	if (sea.x <= sea.y && sea.x > 0.0f)
		end = min(end, sea.x);
	if (end <= start)
		discard;
	float stride = (end - start) / float(kViewSamples);
	float3 betaR = gParams.scattering.xyz;
	float betaM = gParams.scattering.w;
	float2 viewDepths = float2(0.0f, 0.0f);
	float3 sumR = float3(0.0f, 0.0f, 0.0f);
	float3 sumM = float3(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < kViewSamples; ++i) {
		float3 at = direction * (start + stride * (float(i) + 0.5f));
		float height = max(distance(at, gParams.center.xyz) - gParams.center.w, 0.0f);
		float2 densities = float2(exp(-height / gParams.shell.y), exp(-height / gParams.shell.z)) * stride;
		viewDepths += densities;
		float2 light = sunDepths(at);
		if (light.x < 0.0f)
			continue;
		float3 tau = betaR * (viewDepths.x + light.x) + betaM * 1.1f * (viewDepths.y + light.y);
		float3 attenuation = exp(-tau);
		sumR += attenuation * densities.x;
		sumM += attenuation * densities.y;
	}
	float mu = dot(direction, gParams.toSun.xyz);
	float phaseR = 3.0f / (16.0f * kPi) * (1.0f + mu * mu);
	float g2 = kMieG * kMieG;
	float phaseM = 3.0f / (8.0f * kPi) * ((1.0f - g2) * (1.0f + mu * mu)) /
				   ((2.0f + g2) * pow(max(1.0f + g2 - 2.0f * kMieG * mu, 1e-4f), 1.5f));
	float3 inscatter = gParams.sunColor.xyz * (sumR * betaR * phaseR + sumM * betaM * phaseM);
	float3 transmittance = exp(-(betaR * viewDepths.x + betaM * 1.1f * viewDepths.y));
	float alpha = clamp(1.0f - (transmittance.x + transmittance.y + transmittance.z) / 3.0f, 0.0f, 1.0f);
	// Over blending keeps `src * a + dst * (1 - a)`: the in-scattered light is added whole, the background dimmed.
	float3 color = inscatter / max(alpha, 1e-3f);
	if (gParams.shell.w == 0.0f)
		color = saturate(color);
	output.color = float4(color, max(alpha, 1e-3f));
	return output;
}
