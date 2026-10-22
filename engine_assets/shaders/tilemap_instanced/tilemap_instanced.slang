// Tilemap chunk renderer — one `vkCmdDrawIndexed(6, cellCount, …)` /
// `glDrawElementsInstanced(GL_TRIANGLES, 6, …, cellCount)` per visible chunk
// of a tilemap layer. The per-vertex VBO holds a single 4-vertex quad reused
// for every cell; the per-instance VBO of the chunk is persistent and carries
// one entry per non-empty cell (layer cell coordinates and tile index). The
// per-draw uniform block holds everything shared by the cells of one layer:
// camera, entity world transform, grid origin, cell size, layer depth,
// parallax offset, atlas grid, half-texel inset, entity id and texture slot.

struct TilemapDraw {
	column_major float4x4 viewProjection;
	column_major float4x4 model;
	// Local centre of cell (0, 0) in x, y; cell size; layer depth (painter's order).
	float4 grid;
	// Half-texel inset in x, y; parallax offset in world x, y.
	float4 texel;
	// Atlas columns, rows; entity id; slot inside `gTextures[32]`.
	int4 atlas;
	// x: 1 when the target stores linear colour (the fragment colour is decoded from sRGB); y: 2D light count;
	// zw unused.
	int4 flags;
	float4 ambient;// rgb: linear ambient of the 2D lights; w: 1 when the 2D lighting is on.
};

[[vk::binding(0)]]
ConstantBuffer<TilemapDraw> gDraw;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[32];

// The bindings of Renderer2D, which owns the buffers: on OpenGL the storage binding points are global.
#define LIGHT2D_LIGHT_BINDING 5
#define LIGHT2D_SHADOW_BINDING 6
#include "light2d.slangh"

struct VertexInput {
	// Per-vertex attribute (binding 0): `cornerIndex` ∈ {0..3} picks the corner of the unit quad.
	[[vk::location(0)]] int cornerIndex : POSITION;
	// Per-instance attributes (binding 1): layer cell coordinates packed as `x | y << 16`, then the tile index.
	[[vk::location(1)]] int cell : TEXCOORD0;
	[[vk::location(2)]] int tileIndex : TEXCOORD1;
};

struct FragmentInput {
	[[vk::location(0)]] float2 texCoord : TEXCOORD0;
	[[vk::location(1)]] nointerpolation int entityID : TEXCOORD1;
	[[vk::location(2)]] nointerpolation int textureSlot : TEXCOORD2;
	[[vk::location(3)]] float2 worldPos : TEXCOORD3;
};

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

// Corner layout: 0 = BL, 1 = BR, 2 = TR, 3 = TL — matches the index buffer's
// {0,1,2, 2,3,0} winding used by the CPU side.
static const float2 kCornerOffsets[4] = {
	float2(-0.5f, -0.5f),
	float2( 0.5f, -0.5f),
	float2( 0.5f,  0.5f),
	float2(-0.5f,  0.5f),
};

static const float2 kCornerUv[4] = {
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
};

// Exact piecewise sRGB decode, used when the target stores linear colour.
float3 srgbToLinear(float3 c) {
	return select(c <= 0.04045f, c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f));
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;

	const uint packedCell = uint(input.cell);
	const float2 cellCoord = float2(float(packedCell & 0xffffu), float(packedCell >> 16u));
	const float cellSize = gDraw.grid.z;
	// Rows grow downwards in the grid, upwards in the world.
	const float2 cellCentre = gDraw.grid.xy + float2(cellCoord.x, -cellCoord.y) * cellSize;
	const float2 cornerLocal = cellCentre + kCornerOffsets[input.cornerIndex] * cellSize;
	float4 worldPos = mul(gDraw.model, float4(cornerLocal, gDraw.grid.w, 1.0f));
	worldPos.xy += gDraw.texel.zw;
	output.svPosition = mul(gDraw.viewProjection, worldPos);
	output.frag.worldPos = worldPos.xy;

	// Resolve the tile's UV rectangle inside the atlas (same math as `Tileset::getTileUv`).
	const uint atlasColumns = uint(max(gDraw.atlas.x, 1));
	const uint atlasRows = uint(max(gDraw.atlas.y, 1));
	const uint tileIdx = uint(max(input.tileIndex, 0));
	const uint tileCol = tileIdx % atlasColumns;
	const uint tileRow = tileIdx / atlasColumns;
	const float2 atlasStep = float2(1.0f / float(atlasColumns), 1.0f / float(atlasRows));
	const float2 atlasMin =
			float2(float(tileCol) * atlasStep.x, 1.0f - float(tileRow + 1) * atlasStep.y);
	const float2 cornerUv = kCornerUv[input.cornerIndex];
	// Half-texel inset: pull the UV inward by half a texel on each side so
	// linear filtering never reaches into neighbour tiles.
	output.frag.texCoord =
			atlasMin + cornerUv * atlasStep - (cornerUv * 2.0f - 1.0f) * gDraw.texel.xy;

	output.frag.entityID = gDraw.atlas.z;
	output.frag.textureSlot = gDraw.atlas.w;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 texColor = gTextures[NonUniformResourceIndex(input.textureSlot)].Sample(input.texCoord);
	if (texColor.a < 0.001f)
		discard;
	if (gDraw.flags.x != 0)
		texColor.rgb = srgbToLinear(texColor.rgb);
	if (gDraw.ambient.w != 0.0f)
		texColor.rgb *= lighting2D(input.worldPos, 0.0f, float3(0.0f, 0.0f, 1.0f), false, true, gDraw.flags.y,
								   gDraw.ambient.rgb);
	output.color = texColor;
	output.entityID = input.entityID;
	return output;
}
