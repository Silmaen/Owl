// RendererVoxel mesh shader. Identical to mesh3d but samples frac(uv) so a
// greedy-merged quad (whose UV spans 0..w / 0..h) tiles its per-face texture
// instead of stretching it — the engine sampler is clamp-only, so the tiling is
// done here in the fragment shader. Pairs with Nearest filtering on block
// textures (pixel-art), which avoids the seam/derivative issues frac() would
// otherwise cause with mipmaps. Shares the Renderer3D descriptor layout
// (scene UBO at binding 0, texture array at binding 1).

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
};

struct FragmentInput {
	[[vk::location(0)]] float3 normal : NORMAL;
	[[vk::location(1)]] float2 uv : TEXCOORD0;
	[[vk::location(2)]] nointerpolation int texIndex : TEXCOORD1;
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
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 texColor = gTextures[NonUniformResourceIndex(input.texIndex)].Sample(frac(input.uv));
	if (texColor.a < 0.001f)
		discard;
	float ndl = max(dot(normalize(input.normal), normalize(-gScene.sunDirection.xyz)), 0.0f);
	float3 lit = texColor.rgb * (gScene.ambient.rgb + ndl);
	output.color = float4(lit, texColor.a);
	output.entityID = -1;
	return output;
}
