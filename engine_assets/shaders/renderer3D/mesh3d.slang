// Renderer3D forward mesh shader: one material per draw, the shading of common/pbr.slangh (Standard, Toon or Unlit),
// the lights, cascaded shadows and image-based lighting of the scene. In a shadow pass it writes the normalized device
// depth instead. Outputs colour + the entity id. The model, tint and entity id come from the draw (`gScene`) or, when
// `gScene.ids.w >= 0`, from the instance list: instance i of the draw reads `gInstances[gVisible[ids.w + i].index]`.

#include "pbr.slangh"

struct VertexInput {
	[[vk::location(0)]] float3 position : POSITION;
	[[vk::location(1)]] float3 normal : NORMAL;
	[[vk::location(2)]] float2 uv : TEXCOORD0;
	[[vk::location(3)]] int texIndex : TEXCOORD1;
	[[vk::location(4)]] float4 tileRect : TEXCOORD2;// (uMin, vMin, uSize, vSize): the uv maps into this sub-rect.
	[[vk::location(5)]] float ao : TEXCOORD3;
};

struct FragmentInput {
	[[vk::location(0)]] float3 normal : NORMAL;
	[[vk::location(1)]] float2 uv : TEXCOORD0;
	[[vk::location(2)]] nointerpolation int texIndex : TEXCOORD1;
	[[vk::location(3)]] float ao : TEXCOORD3;
	[[vk::location(4)]] float3 worldPos : TEXCOORD4;
	[[vk::location(5)]] float2 clipDepth : TEXCOORD5;// clip z and w, for the shadow-map depth.
	[[vk::location(6)]] nointerpolation float4 tint : TEXCOORD6;
	[[vk::location(7)]] nointerpolation int entityId : TEXCOORD7;
};

// One instance of an instanced draw (must match `renderer::MeshInstance`).
struct MeshInstance {
	column_major float4x4 model;
	float4 tint;
	int4 ids;// x: entity id; yzw unused.
};

// Index of a visible instance, wrapped in a struct of its own (one element type per storage buffer).
struct VisibleInstance {
	uint index;
};

[[vk::binding(5)]]
StructuredBuffer<MeshInstance> gInstances;

[[vk::binding(6)]]
StructuredBuffer<VisibleInstance> gVisible;

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instanceId : SV_InstanceID) {
	VertexOutput output;
	float4x4 model = gScene.model;
	output.frag.tint = gScene.tint;
	output.frag.entityId = gScene.ids.x;
	if (gScene.ids.w >= 0) {
		const MeshInstance instance = gInstances[gVisible[uint(gScene.ids.w) + instanceId].index];
		model = instance.model;
		output.frag.tint = instance.tint;
		output.frag.entityId = instance.ids.x;
	}
	float4 worldPos = mul(model, float4(input.position, 1.0f));
	output.svPosition = mul(gScene.viewProjection, worldPos);
	output.frag.worldPos = worldPos.xyz;
	output.frag.clipDepth = output.svPosition.zw;
	output.frag.normal = normalize(mul((float3x3) model, input.normal));
	output.frag.uv = input.tileRect.xy + input.uv * input.tileRect.zw;
	output.frag.texIndex = input.texIndex;
	output.frag.ao = input.ao;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input, bool frontFace : SV_IsFrontFace) {
	FragmentOutput output;
	float4 base = float4(authoredColor(gMaterial.baseColor.rgb), gMaterial.baseColor.a);
	if (hasMap(0)) {
		// The vertices name the base-colour slot (MAP_BASE_COLOR for the meshes of the mesh cache).
		float4 texel = gTextures[NonUniformResourceIndex(input.texIndex)].Sample(input.uv);
		base *= float4(authoredColor(texel.rgb), texel.a);
	}
	base *= float4(authoredColor(input.tint.rgb), input.tint.a);
	int alphaMode = gMaterial.model.y;
	if (alphaMode == 1 && base.a < gMaterial.emissive.w)
		discard;
	if (alphaMode == 0)
		base.a = 1.0f;
	if (base.a < 0.001f)
		discard;
	if (gScene.ids.z != 0) {
		output.color = float4(input.clipDepth.x / input.clipDepth.y, 0.0f, 0.0f, 1.0f);
		output.entityID = -1;
		return output;
	}
	float3 geometricNormal = normalize(input.normal);
	if (!frontFace)
		geometricNormal = -geometricNormal;
	float3x3 frame = cotangentFrame(geometricNormal, input.worldPos, input.uv);
	float3 normal = geometricNormal;
	if (hasMap(2)) {
		float3 mapped = gTextures[MAP_NORMAL].Sample(input.uv).xyz * 2.0f - 1.0f;
		if (gMaterial.channels.w == 1)
			mapped.y = -mapped.y;
		mapped.xy *= gMaterial.factors.z;
		normal = normalize(mul(mapped, frame));
	}
	float metallic = gMaterial.factors.x;
	float roughness = gMaterial.factors.y;
	if (hasMap(1))
		metallic *= sampleChannel(MAP_METALLIC, input.uv, gMaterial.channels.x);
	if (hasMap(5))
		roughness *= sampleChannel(MAP_ROUGHNESS, input.uv, gMaterial.channels.y);
	float occlusion = input.ao;
	if (hasMap(3))
		occlusion *= lerp(1.0f, sampleChannel(MAP_OCCLUSION, input.uv, gMaterial.channels.z), gMaterial.factors.w);
	float3 emissive = authoredColor(gMaterial.emissive.rgb) * gMaterial.toon.y * gScene.ambient.w;
	if (hasMap(4))
		emissive *= authoredColor(gTextures[MAP_EMISSIVE].Sample(input.uv).rgb);
	float rotation = gMaterial.layers.w;
	float3 tangent = normalize(frame[0] * cos(rotation) + frame[1] * sin(rotation));
	tangent = normalize(tangent - normal * dot(normal, tangent));
	Surface s;
	s.position = input.worldPos;
	s.normal = normal;
	s.tangent = tangent;
	s.bitangent = cross(normal, tangent);
	s.view = normalize(gLighting.cameraPosition.xyz - input.worldPos);
	s.albedo = base.rgb;
	s.alpha = base.a;
	s.metallic = saturate(metallic);
	s.roughness = clamp(roughness, MIN_ROUGHNESS, 1.0f);
	s.occlusion = occlusion;
	s.emissive = emissive;
	s.clearcoat = saturate(gMaterial.layers.x);
	s.clearcoatRoughness = clamp(gMaterial.layers.y, MIN_ROUGHNESS, 1.0f);
	s.sheenColor = authoredColor(gMaterial.sheen.rgb);
	s.sheenRoughness = clamp(gMaterial.sheen.w, MIN_ROUGHNESS, 1.0f);
	s.anisotropy = clamp(gMaterial.layers.z, -1.0f, 1.0f);
	output.color = float4(shadeSurface(s), base.a);
	output.entityID = input.entityId;
	return output;
}
