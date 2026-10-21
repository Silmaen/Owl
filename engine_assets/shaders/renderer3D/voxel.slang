// RendererVoxel mesh shader. Samples a single atlas texture: each vertex carries
// a tile sub-rect (uMin, vMin, uSize, vSize) and a UV in tile-units (0..w / 0..h
// across a greedy-merged quad). The fragment maps tileRect.xy + frac(uv)*tileRect.zw
// so the tile repeats across the merged quad instead of stretching, while still
// selecting the correct cell of the atlas. The engine sampler is clamp-only, so
// the repeat is done here. Pairs with Nearest filtering on the atlas (pixel-art),
// which keeps tile cells pixel-exact and avoids seam/derivative issues at the
// frac() wrap. Shares the Renderer3D descriptor layout (scene UBO at binding 0,
// texture array at binding 1).

struct Scene3D {
	column_major float4x4 viewProjection;
	column_major float4x4 model;
	float4 sunDirection;
	float4 ambient;
	float4 tint;// unused: the atlas colours are drawn as they are.
	int4 ids;   // x: entity id written to the picking attachment (-1 for none); y: 1 when the target stores linear colour (texels are decoded from sRGB); z: 1 in a shadow pass (the depth is written); w unused.
};

[[vk::binding(0)]]
ConstantBuffer<Scene3D> gScene;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[32];

struct LightData {
	float4 positionRange;// xyz: world position (point, spot); w: range.
	float4 directionType;// xyz: direction the light travels; w: 0 directional, 1 point, 2 spot.
	float4 color;        // rgb: linear colour times intensity.
	float4 spotCosines;  // x: cosine of the inner angle, y: of the outer angle.
};

// A matrix array ignores `column_major`: each cascade matrix sits in a struct of its own.
struct CascadeMatrix {
	column_major float4x4 viewProjection;
};

struct Lighting {
	int4 counts;         // x: light count (0: the sun of the layer), y: shadow cascades, z: index of the shadowed light.
	float4 splitDepths;  // view depth where each cascade ends.
	float4 texelSizes;   // world size of a shadow texel, per cascade.
	float4 viewDepthRow; // view depth of a point p: -(dot(xyz, p) + w).
	float4 shadowParams; // x: normal-offset bias in texels, y: shadow-map size in texels.
	CascadeMatrix cascades[4];
};

[[vk::binding(2)]]
StructuredBuffer<LightData> gLights;

[[vk::binding(3)]]
ConstantBuffer<Lighting> gLighting;

static const int SHADOW_MAP_SLOT = 28;

struct VertexInput {
	[[vk::location(0)]] float3 position : POSITION;
	[[vk::location(1)]] float3 normal : NORMAL;
	[[vk::location(2)]] float2 uv : TEXCOORD0;
	[[vk::location(3)]] int texIndex : TEXCOORD1;
	[[vk::location(4)]] float4 tileRect : TEXCOORD2;
	[[vk::location(5)]] float ao : TEXCOORD3;
};

struct FragmentInput {
	[[vk::location(0)]] float3 normal : NORMAL;
	[[vk::location(1)]] float2 uv : TEXCOORD0;
	[[vk::location(2)]] nointerpolation int texIndex : TEXCOORD1;
	[[vk::location(3)]] nointerpolation float4 tileRect : TEXCOORD2;
	[[vk::location(4)]] float ao : TEXCOORD3;
	[[vk::location(5)]] float3 worldPos : TEXCOORD4;
	[[vk::location(6)]] float2 clipDepth : TEXCOORD5;// clip z and w, for the shadow-map depth.
};

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

// Exact piecewise sRGB decode, used when the target stores linear colour.
float3 srgbToLinear(float3 c) {
	return select(c <= 0.04045f, c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f));
}


// Depth stored in a texel of a cascade's shadow map, sampled at its centre (nearest sampler): a texel fetch from
// the texture array crashes lavapipe.
float loadShadowDepth(int cascade, int2 texel, float size) {
	return gTextures[NonUniformResourceIndex(SHADOW_MAP_SLOT + cascade)].SampleLevel((float2(texel) + 0.5f) / size, 0.0f).r;
}

// 1 when lit, 0 in shadow: 3x3 comparisons in the cascade covering the point.
float shadowFactor(float3 worldPos, float3 normal) {
	float depth = -(dot(gLighting.viewDepthRow.xyz, worldPos) + gLighting.viewDepthRow.w);
	int cascade = 0;
	while (cascade < gLighting.counts.y && depth > gLighting.splitDepths[cascade])
		cascade++;
	if (cascade >= gLighting.counts.y)
		return 1.0f;
	float3 offsetPos = worldPos + normal * (gLighting.texelSizes[cascade] * gLighting.shadowParams.x);
	float4 clip = mul(gLighting.cascades[cascade].viewProjection, float4(offsetPos, 1.0f));
	float3 ndc = clip.xyz / clip.w;
	float2 uv = ndc.xy * 0.5f + 0.5f;
	if (any(uv < 0.0f) || any(uv > 1.0f) || ndc.z > 1.0f)
		return 1.0f;
	int size = int(gLighting.shadowParams.y);
	int2 texel = int2(uv * float(size));
	float lit = 0.0f;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			int2 tap = clamp(texel + int2(dx, dy), int2(0), int2(size - 1));
			float stored = loadShadowDepth(cascade, tap, float(size));
			lit += ndc.z <= stored + 0.0005f ? 1.0f : 0.0f;
		}
	}
	return lit / 9.0f;
}

// Light reaching a surface (ambient included), the lights of the scene or, without any, the sun of the layer.
float3 lightAt(float3 worldPos, float3 normal) {
	if (gLighting.counts.x == 0)
		return gScene.ambient.rgb + max(dot(normal, normalize(-gScene.sunDirection.xyz)), 0.0f);
	float3 total = gScene.ambient.rgb;
	for (int index = 0; index < gLighting.counts.x; ++index) {
		LightData light = gLights[index];
		int type = int(light.directionType.w);
		float3 toLight;
		float attenuation = 1.0f;
		if (type == 0) {
			toLight = -light.directionType.xyz;
			if (index == gLighting.counts.z)
				attenuation = shadowFactor(worldPos, normal);
		} else {
			float3 delta = light.positionRange.xyz - worldPos;
			float dist = length(delta);
			toLight = delta / max(dist, 1e-4f);
			float ratio = dist / max(light.positionRange.w, 1e-4f);
			float window = saturate(1.0f - ratio * ratio * ratio * ratio);
			attenuation = window * window;
			if (type == 2)
				attenuation *= smoothstep(light.spotCosines.y, light.spotCosines.x, dot(-toLight, light.directionType.xyz));
		}
		total += light.color.rgb * max(dot(normal, toLight), 0.0f) * attenuation;
	}
	return total;
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
	float4 worldPos = mul(gScene.model, float4(input.position, 1.0f));
	output.svPosition = mul(gScene.viewProjection, worldPos);
	output.frag.worldPos = worldPos.xyz;
	output.frag.clipDepth = output.svPosition.zw;
	output.frag.normal = normalize(mul((float3x3) gScene.model, input.normal));
	output.frag.uv = input.uv;
	output.frag.texIndex = input.texIndex;
	output.frag.tileRect = input.tileRect;
	output.frag.ao = input.ao;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float2 atlasUv = input.tileRect.xy + frac(input.uv) * input.tileRect.zw;
	// frac() makes the auto-derivative spike at every block boundary, so anisotropic/linear sampling would smear
	// across the tile edge (a seam line). Feed the continuous uv's gradient to SampleGrad so the footprint stays
	// inside the tile.
	float2 uvDdx = ddx(input.uv) * input.tileRect.zw;
	float2 uvDdy = ddy(input.uv) * input.tileRect.zw;
	float4 texColor = gTextures[NonUniformResourceIndex(input.texIndex)].SampleGrad(atlasUv, uvDdx, uvDdy);
	if (gScene.ids.y != 0)
		texColor.rgb = srgbToLinear(texColor.rgb);
	if (texColor.a < 0.001f)
		discard;
	if (gScene.ids.z != 0) {
		output.color = float4(input.clipDepth.x / input.clipDepth.y, 0.0f, 0.0f, 1.0f);
		output.entityID = -1;
		return output;
	}
	float3 lit = texColor.rgb * lightAt(input.worldPos, normalize(input.normal)) * input.ao;
	output.color = float4(lit, texColor.a);
	output.entityID = gScene.ids.x;
	return output;
}
