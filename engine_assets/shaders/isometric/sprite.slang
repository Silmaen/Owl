// Isometric sprites — the compact instance of vehicles, props and building slices.
// Same projection as billboard.slang (only the foot point goes through the dimetric view, the sprite keeps its
// pixels), but a 64 byte instance: foot, size, anchor, uv rectangle and a packed colour. The depth plane of the
// upright sprite is derived from the view matrix, so no per-instance slope is stored.

struct IsometricScene {
	column_major float4x4 view;
	column_major float4x4 projection;
	// x: alpha below which a texel is discarded (cutout).
	// y: zoom to snap the foot to whole screen pixels at (0 = no snapping); zw: fractional pixel offset of the
	// viewport centre (0 or 0.5 on an odd size).
	float4 params;
	// xyz: world position of the back corner of the map (tile only).
	float4 origin;
	// xy: tile sprite size in pixels; z: view depth per pixel up the sprite (tile only).
	float4 tile;
	// x: entity id (tile only); y: 1 when the target stores linear colour (the fragment colour is decoded from sRGB).
	int4 ids;
};

[[vk::binding(0)]]
ConstantBuffer<IsometricScene> gScene;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[32];

struct SpriteInstance {
	// xyz: world foot point; w: depth bias towards the viewer, in view pixels.
	float4 foot;
	// xy: sprite size in pixels; zw: anchor as a fraction of the size (0.5, 0 = bottom centre).
	float4 sizeAnchor;
	// Texture rectangle: u of the left, v of the bottom, u of the right, v of the top.
	float4 uvRect;
	// Tint, RGBA8 with red in the low byte.
	uint color;
	uint texIndex;
	int entityId;
	uint pad;
};

[[vk::binding(2)]]
StructuredBuffer<SpriteInstance> gSprites;

struct VertexInput {
	[[vk::location(0)]] int cornerIndex : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float4 color : COLOR;
	[[vk::location(1)]] float2 texCoord : TEXCOORD0;
	[[vk::location(2)]] nointerpolation int texIndex : TEXCOORD1;
	[[vk::location(3)]] nointerpolation int entityID : TEXCOORD2;
};

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

// Corners BL, BR, TR, TL as fractions of the sprite, matching the index buffer {0,1,2, 2,3,0}.
static const float2 kCorners[4] = {
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
};

float4 unpackColor(uint iColor) {
	return float4(float(iColor & 0xFFu), float((iColor >> 8) & 0xFFu), float((iColor >> 16) & 0xFFu),
				  float((iColor >> 24) & 0xFFu)) * (1.0f / 255.0f);
}

// Exact piecewise sRGB decode, used when the target stores linear colour.
float3 srgbToLinear(float3 c) {
	return select(c <= 0.04045f, c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f));
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint iid : SV_InstanceID) {
	VertexOutput output;
	SpriteInstance inst = gSprites[iid];
	float4 viewPos = mul(gScene.view, float4(inst.foot.xyz, 1.0f));
	if (gScene.params.y > 0.0f) {
		const float zoom = gScene.params.y;
		const float2 offset = gScene.params.zw;
		viewPos.xy = (round(viewPos.xy * zoom - offset) + offset) / zoom;
	}
	const float3 up = mul(gScene.view, float4(0.0f, 0.0f, 1.0f, 0.0f)).xyz;
	const float2 corner = (kCorners[input.cornerIndex] - inst.sizeAnchor.zw) * inst.sizeAnchor.xy;
	viewPos.xy += corner;
	viewPos.z += corner.y * (up.z / up.y) + inst.foot.w;
	output.svPosition = mul(gScene.projection, viewPos);
	output.frag.color = unpackColor(inst.color);
	const float2 uvLow = inst.uvRect.xy;
	const float2 uvHigh = inst.uvRect.zw;
	output.frag.texCoord = lerp(uvLow, uvHigh, kCorners[input.cornerIndex]);
	output.frag.texIndex = int(inst.texIndex);
	output.frag.entityID = inst.entityId;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 texColor = input.color;
	texColor *= gTextures[NonUniformResourceIndex(input.texIndex)].Sample(input.texCoord);
	if (texColor.a < gScene.params.x)
		discard;
	if (gScene.ids.y != 0)
		texColor.rgb = srgbToLinear(texColor.rgb);
	output.color = texColor;
	output.entityID = input.entityID;
	return output;
}
