// RendererVoxel mesh shader. Samples a single atlas texture: each vertex carries
// a tile sub-rect (uMin, vMin, uSize, vSize) and a UV in tile-units (0..w / 0..h
// across a greedy-merged quad). The fragment maps tileRect.xy + frac(uv)*tileRect.zw
// so the tile repeats across the merged quad instead of stretching, while still
// selecting the correct cell of the atlas. The repeat is done here. Pairs with Nearest filtering on the atlas
// (pixel-art), which keeps tile cells pixel-exact and avoids seam/derivative issues at the frac() wrap, or with
// PixelArt filtering, whose mipmaps are read inside the cell (atlas.slangh). Shares the Renderer3D descriptor layout (scene UBO at binding 0,
// texture array at binding 1); the atlas is the draw's first texture (slot 1) and
// texIndex carries the block surface packed by data::voxel::packSurface. A tileset with relief and roughness maps
// gives them as the normal (slot 3) and roughness (slot 6) maps of the draw's material, on the grid of the atlas.

#include "pbr.slangh"
#include "atlas.slangh"

static const int VOXEL_ATLAS_SLOT = 1;
static const float VOXEL_MAX_LUMINANCE = 100000.0f;// data::voxel::g_MaxBlockLuminance.

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
	[[vk::location(2)]] nointerpolation int surface : TEXCOORD1;
	[[vk::location(3)]] nointerpolation float4 tileRect : TEXCOORD2;
	[[vk::location(4)]] float ao : TEXCOORD3;
	[[vk::location(5)]] float3 position : TEXCOORD4;// camera-relative.
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

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
	float3 position = relativePosition(gScene.model, input.position);
	output.svPosition = mul(gScene.viewProjection, float4(position, 1.0f));
	output.frag.position = position;
	output.frag.clipDepth = output.svPosition.zw;
	output.frag.normal = normalize(mul((float3x3) gScene.model, input.normal));
	output.frag.uv = input.uv;
	output.frag.surface = input.texIndex;
	output.frag.tileRect = input.tileRect;
	output.frag.ao = input.ao;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float2 uvDdx = ddx(input.uv) * input.tileRect.zw;
	float2 uvDdy = ddy(input.uv) * input.tileRect.zw;
	float2 atlasUv = clampToAtlasCell(gTextures[VOXEL_ATLAS_SLOT], input.tileRect.xy + frac(input.uv) * input.tileRect.zw,
									  input.tileRect.xy, input.tileRect.xy + input.tileRect.zw, uvDdx, uvDdy);
	float4 texColor = gTextures[VOXEL_ATLAS_SLOT].SampleGrad(atlasUv, uvDdx, uvDdy);
	texColor.rgb = authoredColor(texColor.rgb);
	if (texColor.a < 0.001f)
		discard;
	if (gScene.ids.z != 0) {
		output.color = float4(input.clipDepth.x / input.clipDepth.y, 0.0f, 0.0f, 1.0f);
		output.entityID = -1;
		return output;
	}
	float3 normal = normalize(input.normal);
	float3 helper = abs(normal.y) < 0.99f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 tangent = normalize(cross(helper, normal));
	// The relief map of the tileset, on the frame of the tile UV (its v runs up the atlas, as OpenGL normals expect).
	if (hasMap(2)) {
		float3x3 frame = cotangentFrame(normal, input.position, input.uv);
		float3 mapped = gTextures[MAP_NORMAL].SampleGrad(atlasUv, uvDdx, uvDdy).xyz * 2.0f - 1.0f;
		normal = normalize(mul(mapped, frame));
		tangent = normalize(frame[0] - normal * dot(frame[0], normal));
	}
	Surface s;
	s.position = input.position;
	s.normal = normal;
	s.tangent = tangent;
	s.bitangent = cross(normal, tangent);
	s.view = normalize(-input.position);
	s.albedo = texColor.rgb * authoredColor(gMaterial.baseColor.rgb);
	s.alpha = texColor.a;
	uint packed = asuint(input.surface);
	float4 block = float4(packed & 0xFFu, (packed >> 8) & 0xFFu, (packed >> 16) & 0xFFu, packed >> 24) / 255.0f;
	s.metallic = block.x;
	float roughness = block.y;
	// The roughness map of the tileset modulates the roughness of the block: 50 % grey keeps it.
	if (hasMap(5))
		roughness *= 2.0f * gTextures[MAP_ROUGHNESS].SampleGrad(atlasUv, uvDdx, uvDdy).r;
	s.roughness = clamp(roughness, MIN_ROUGHNESS, 1.0f);
	s.occlusion = input.ao;
	// The emissive byte is logarithmic: exp2(byte * log2(1 + max)) - 1 nits.
	float luminance = exp2(block.z * log2(1.0f + VOXEL_MAX_LUMINANCE)) - 1.0f;
	s.emissive = s.albedo * luminance * gScene.ambient.w;
	s.clearcoat = block.w;
	s.clearcoatRoughness = 0.05f;
	s.sheenColor = float3(0.0f);
	s.sheenRoughness = 1.0f;
	s.anisotropy = 0.0f;
	output.color = float4(shadeSurface(s), texColor.a);
	output.entityID = gScene.ids.x;
	return output;
}
