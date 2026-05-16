// Tilemap instanced renderer — one drawcall per (tilemap, layer) pair via
// `vkCmdDrawIndexed(6, cellCount, …)` / `glDrawElementsInstanced(GL_TRIANGLES,
// 6, …, cellCount)`. The per-vertex VBO holds a single 4-vertex quad reused
// for every cell; the per-instance VBO carries one entry per non-empty cell
// (world position, tile index, entity id). The fragment shader samples the
// tileset atlas at the UV computed from the tile index and the atlas grid
// metadata uploaded once per drawcall through the per-draw UBO.

struct Camera {
	column_major float4x4 viewProjection;
};

[[vk::binding(0)]]
ConstantBuffer<Camera> gCamera;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[32];

// Per-draw configuration. Updated by the CPU before every instanced draw —
// one upload per (tilemap, layer) pair. Carries the atlas grid + cell size
// so the vertex shader can synthesise UVs and world positions on the fly.
struct DrawData {
	// Atlas grid metadata (cells inside the tileset atlas).
	uint atlasColumns;
	uint atlasRows;
	// Cell size in world units (`TilemapAsset::cellSize`).
	float cellSize;
	// Painter's-order Z offset (layers within the same tilemap share an X/Y
	// origin but stack along Z so back layers stay behind front ones even
	// with depth-test disabled).
	float layerZ;
	// Per-draw tint multiplied into the atlas sample (white = identity).
	float4 tint;
	// Slot index inside `gTextures[32]` where the tileset atlas is bound.
	uint textureSlot;
	uint _pad0;
	// Half-texel inset for the atlas sample — eliminates the seam flicker
	// when the atlas filter is set to Linear (same trick as the raycast
	// sprite path).
	float2 halfTexel;
};

[[vk::binding(2)]]
ConstantBuffer<DrawData> gDraw;

struct VertexInput {
	// Per-vertex attributes (binding 0, advance per vertex).
	// `cornerIndex` ∈ {0..3} picks the corner of the unit quad — the per-vertex
	// VBO contains exactly 4 vertices reused for every instance.
	[[vk::location(0)]] int cornerIndex : POSITION;

	// Per-instance attributes (binding 1, advance per instance). One entry
	// per non-empty cell.
	[[vk::location(1)]] float2 cellWorld : TEXCOORD0;
	[[vk::location(2)]] int tileIndex : TEXCOORD1;
	[[vk::location(3)]] int entityID : TEXCOORD2;
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

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;

	const float2 cornerLocal = kCornerOffsets[input.cornerIndex] * gDraw.cellSize;
	const float3 worldPos = float3(input.cellWorld + cornerLocal, gDraw.layerZ);
	output.svPosition = mul(gCamera.viewProjection, float4(worldPos, 1.0f));

	// Resolve the tile's UV rectangle inside the atlas. The CPU-side
	// `Tileset::getTileUv` performs the same math; doing it here saves the
	// per-cell function call + the per-vertex bandwidth that would otherwise
	// carry 4 vec2 UVs per cell.
	const uint tileIdx = uint(max(input.tileIndex, 0));
	const uint tileCol = tileIdx % gDraw.atlasColumns;
	const uint tileRow = tileIdx / gDraw.atlasColumns;
	const float2 atlasStep = float2(1.0f / float(gDraw.atlasColumns), 1.0f / float(gDraw.atlasRows));
	const float2 atlasMin =
			float2(float(tileCol) * atlasStep.x, 1.0f - float(tileRow + 1) * atlasStep.y);
	const float2 cornerUv = kCornerUv[input.cornerIndex];
	// Half-texel inset: pull the UV inward by half a texel on each side so
	// linear filtering never reaches into neighbour tiles.
	output.frag.texCoord =
			atlasMin + cornerUv * atlasStep - (cornerUv * 2.0f - 1.0f) * gDraw.halfTexel;

	output.frag.entityID = input.entityID;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 texColor = gTextures[NonUniformResourceIndex(int(gDraw.textureSlot))].Sample(input.texCoord);
	texColor *= gDraw.tint;
	if (texColor.a < 0.001f)
		discard;
	output.color = texColor;
	output.entityID = input.entityID;
	return output;
}
