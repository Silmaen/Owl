// Renderer3D generic forward mesh shader. Non-instanced: a real per-vertex VBO
// (position / normal / uv / texIndex / tileRect / ao) drawn with one model matrix per draw via
// the per-frame/per-draw scene UBO. Fragment does a single directional-light
// Lambert term plus ambient, sampling a per-vertex slot of the texture array tinted by the draw colour.
// Outputs colour + the draw's entity id to match the scene framebuffer attachments.

struct Scene3D {
	column_major float4x4 viewProjection;
	column_major float4x4 model;
	float4 sunDirection;// xyz: direction the light travels (world space); w unused.
	float4 ambient;     // rgb: ambient term; w unused.
	float4 tint;        // colour multiplied with the sampled texture.
	int4 ids;           // x: entity id written to the picking attachment (-1 for none); y: 1 when the target stores linear colour (texels and tint are decoded from sRGB); zw unused.
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
	[[vk::location(4)]] float4 tileRect : TEXCOORD2;// (uMin, vMin, uSize, vSize): the uv maps into this sub-rect.
	[[vk::location(5)]] float ao : TEXCOORD3;
};

struct FragmentInput {
	[[vk::location(0)]] float3 normal : NORMAL;
	[[vk::location(1)]] float2 uv : TEXCOORD0;
	[[vk::location(2)]] nointerpolation int texIndex : TEXCOORD1;
	[[vk::location(3)]] float ao : TEXCOORD3;
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

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
	float4 worldPos = mul(gScene.model, float4(input.position, 1.0f));
	output.svPosition = mul(gScene.viewProjection, worldPos);
	output.frag.normal = normalize(mul((float3x3) gScene.model, input.normal));
	output.frag.uv = input.tileRect.xy + input.uv * input.tileRect.zw;
	output.frag.texIndex = input.texIndex;
	output.frag.ao = input.ao;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 texel = gTextures[NonUniformResourceIndex(input.texIndex)].Sample(input.uv);
	float4 tint = gScene.tint;
	if (gScene.ids.y != 0) {
		texel.rgb = srgbToLinear(texel.rgb);
		tint.rgb = srgbToLinear(tint.rgb);
	}
	float4 texColor = texel * tint;
	if (texColor.a < 0.001f)
		discard;
	float ndl = max(dot(normalize(input.normal), normalize(-gScene.sunDirection.xyz)), 0.0f);
	float3 lit = texColor.rgb * (gScene.ambient.rgb + ndl) * input.ao;
	output.color = float4(lit, texColor.a);
	output.entityID = gScene.ids.x;
	return output;
}
