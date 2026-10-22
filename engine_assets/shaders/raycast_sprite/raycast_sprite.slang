// Raycast billboard sprites, one instance per sprite in back-to-front order: instance i draws
// `gSprites[gOrder[i].value]`, the order sorted on the GPU by `BitonicSortPass` (key = -distance). The quad spans
// the whole ray columns the sprite touches; each fragment samples the sprite at the centre of its column and is
// dropped where the column's wall is nearer than the sprite.
//
// Layout contract (must match `renderer::RendererRaycast`'s `RendererRaycastSprites` block):
//   binding 0 : UBO  - SpriteParams
//   binding 1 : Sampler2D[16] - the sprite textures
//   binding 2 : SSBO - sprites[i]       (RaycastSprite)
//   binding 3 : SSBO - order[i]         (sorted key / sprite index)
//   binding 4 : SSBO - columnDepths[c]  (perpendicular distance of the opaque wall of column c)

struct SpriteParams {
	column_major float4x4 viewProjection;
	// (stripePxWidth, numRays, unused, unused)
	float4 floats;
	uint spriteCount;
	// 1 when the target stores linear colour (the fragment colour is decoded from sRGB).
	int linearOutput;
	int pad0;
	int pad1;
};

[[vk::binding(0)]]
ConstantBuffer<SpriteParams> gParams;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[16];

struct RaycastSprite {
	float4 rect;// Pixel space: left, centre y, right, height.
	float4 uv;// Left, right, bottom, top.
	float4 color;// Tint with the fog of its distance, sRGB as authored.
	float depth;// Perpendicular distance to the camera.
	int textureSlot;
	int entityId;
	int pad;
};

struct SortItem {
	float key;
	uint value;
};

struct ColumnDepth {
	float depth;
};

[[vk::binding(2)]]
StructuredBuffer<RaycastSprite> gSprites;

[[vk::binding(3)]]
StructuredBuffer<SortItem> gOrder;

[[vk::binding(4)]]
StructuredBuffer<ColumnDepth> gColumnDepths;

struct VertexInput {
	[[vk::location(0)]] int cornerIndex : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float4 color : COLOR;
	[[vk::location(1)]] float pixelX : TEXCOORD0;
	[[vk::location(2)]] float v : TEXCOORD1;
	[[vk::location(3)]] nointerpolation float4 spriteSpan : TEXCOORD2;// left, right, uv left, uv right.
	[[vk::location(4)]] nointerpolation float depth : TEXCOORD3;
	[[vk::location(5)]] nointerpolation int textureSlot : TEXCOORD4;
	[[vk::location(6)]] nointerpolation int entityID : TEXCOORD5;
};

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

// Unit-quad corners: BL, BR, TR, TL, matching the {0,1,2, 2,3,0} index buffer.
static const float2 kCorners[4] = {
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
};

float3 srgbToLinear(float3 c) {
	return select(c <= 0.04045f, c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f));
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instanceId : SV_InstanceID) {
	VertexOutput output;
	const RaycastSprite sprite = gSprites[gOrder[instanceId].value];
	const float stripe = gParams.floats.x;
	// Whole columns: the first one the sprite touches to the last, as the per-column stripes of the CPU path.
	const float left = floor(sprite.rect.x / stripe) * stripe;
	const float right = ceil(sprite.rect.z / stripe) * stripe;
	const float2 corner = kCorners[input.cornerIndex];
	const float x = lerp(left, right, corner.x);
	const float y = sprite.rect.y + (corner.y - 0.5f) * sprite.rect.w;
	output.svPosition = mul(gParams.viewProjection, float4(x, y, 0.0f, 1.0f));
	output.frag.color = sprite.color;
	output.frag.pixelX = x;
	output.frag.v = lerp(sprite.uv.z, sprite.uv.w, corner.y);
	output.frag.spriteSpan = float4(sprite.rect.x, sprite.rect.z, sprite.uv.x, sprite.uv.y);
	output.frag.depth = sprite.depth;
	output.frag.textureSlot = sprite.textureSlot;
	output.frag.entityID = sprite.entityId;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	const float stripe = gParams.floats.x;
	const uint numRays = uint(gParams.floats.y);
	const uint column = min(uint(max(input.pixelX, 0.0f) / stripe), numRays - 1);
	if (input.depth >= gColumnDepths[column].depth)
		discard;
	const float columnCentre = (float(column) + 0.5f) * stripe;
	const float width = max(input.spriteSpan.y - input.spriteSpan.x, 1e-3f);
	const float u = lerp(input.spriteSpan.z, input.spriteSpan.w, saturate((columnCentre - input.spriteSpan.x) / width));
	// u is constant along a column (as the CPU stripes): no horizontal derivative, so the same mip level is sampled.
	const float2 uv = float2(u, input.v);
	float4 texColor = input.color * gTextures[NonUniformResourceIndex(input.textureSlot)].SampleGrad(
										uv, float2(0.0f, ddx(input.v)), float2(0.0f, ddy(input.v)));
	if (texColor.a < 0.001f)
		discard;
	if (gParams.linearOutput != 0)
		texColor.rgb = srgbToLinear(texColor.rgb);
	output.color = texColor;
	output.entityID = input.entityID;
	return output;
}
