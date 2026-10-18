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
};

[[vk::binding(0)]]
ConstantBuffer<Scene3D> gScene;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[32];

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
	float4 worldPos = mul(gScene.model, float4(input.position, 1.0f));
	output.svPosition = mul(gScene.viewProjection, worldPos);
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
	if (texColor.a < 0.001f)
		discard;
	float ndl = max(dot(normalize(input.normal), normalize(-gScene.sunDirection.xyz)), 0.0f);
	float3 lit = texColor.rgb * (gScene.ambient.rgb + ndl) * input.ao;
	output.color = float4(lit, texColor.a);
	output.entityID = -1;
	return output;
}
