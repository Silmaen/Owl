// Isometric tilemap chunks — one instanced draw per chunk of a layer, from a persistent per-chunk instance buffer.
// Same projection as billboard.slang (only the foot point goes through the dimetric view, the tile keeps its pixels,
// each pixel writes the depth of the ground plane through the foot), but a 32 byte instance holding only what changes
// with the cells: the foot in map space and the texture rectangle. What the cells of a tilemap share (map origin,
// sprite size, depth slope, entity id) is in the per-draw uniform block, so moving the map or changing the
// presentation never rebuilds a chunk.

struct IsometricScene {
	column_major float4x4 view;
	column_major float4x4 projection;
	// x: alpha below which a texel is discarded (cutout).
	// y: zoom to snap the foot to whole screen pixels at (0 = no snapping); zw: fractional pixel offset of the
	// viewport centre (0 or 0.5 on an odd size).
	float4 params;
	// xyz: world position of the back corner of the map.
	float4 origin;
	// xy: tile sprite size in pixels; z: view depth per pixel up the sprite (negative: flat on the ground).
	float4 tile;
	// x: entity id written to the picking attachment.
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

struct VertexInput {
	// Per-vertex attribute (binding 0): the corner of the sprite, 0 = BL, 1 = BR, 2 = TR, 3 = TL.
	[[vk::location(0)]] int cornerIndex : POSITION;
	// Per-instance attributes (binding 1): xy foot in map space, z height, w depth bias.
	[[vk::location(1)]] float4 foot : TEXCOORD0;
	// Texture rectangle: u of the left, v of the bottom, u of the right, v of the top.
	[[vk::location(2)]] float4 uvRect : TEXCOORD1;
};

struct FragmentInput {
	[[vk::location(0)]] float2 texCoord : TEXCOORD0;
	[[vk::location(1)]] nointerpolation int entityID : TEXCOORD1;
};

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

static const float2 kCorners[4] = {
	float2(-0.5f, 0.0f),
	float2( 0.5f, 0.0f),
	float2( 0.5f, 1.0f),
	float2(-0.5f, 1.0f),
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
	float4 viewPos = mul(gScene.view, float4(gScene.origin.xyz + input.foot.xyz, 1.0f));
	if (gScene.params.y > 0.0f) {
		const float zoom = gScene.params.y;
		const float2 centre = gScene.params.zw;
		viewPos.xy = (round(viewPos.xy * zoom - centre) + centre) / zoom;
	}
	const float2 offset = kCorners[input.cornerIndex] * gScene.tile.xy;
	viewPos.xy += offset;
	viewPos.z += offset.y * gScene.tile.z + input.foot.w;
	output.svPosition = mul(gScene.projection, viewPos);
	const bool right = input.cornerIndex == 1 || input.cornerIndex == 2;
	const bool top = input.cornerIndex >= 2;
	output.frag.texCoord = float2(right ? input.uvRect.z : input.uvRect.x, top ? input.uvRect.w : input.uvRect.y);
	output.frag.entityID = gScene.ids.x;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	const float4 texColor = gTextures[0].Sample(input.texCoord);
	if (texColor.a < gScene.params.x)
		discard;
	output.color = texColor;
	output.entityID = input.entityID;
	return output;
}
