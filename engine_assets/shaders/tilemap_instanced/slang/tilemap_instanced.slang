// Tilemap instanced renderer — every tilemap of a scene (all entities, all
// layers) renders in a single `vkCmdDrawIndexed(6, cellCount, …)` /
// `glDrawElementsInstanced(GL_TRIANGLES, 6, …, cellCount)`. The per-vertex VBO
// holds a single 4-vertex quad reused for every cell; the per-instance VBO
// carries one entry per non-empty cell with everything the cell needs (world
// position, tile index, entity id, layer Z, atlas grid + cell size, half-texel
// inset, and the slot inside `gTextures[32]` holding its tileset atlas). No
// per-draw UBO: distinct tilesets are bound to distinct texture slots and each
// cell indexes its own slot, so one drawcall spans any mix of tilemaps.

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
	// Painter's-order Z for this cell's layer. Back layers carry a smaller Z
	// than front layers so they stack correctly within the single drawcall.
	[[vk::location(4)]] float layerZ : TEXCOORD3;
	// Cell size in world units (`TilemapAsset::cellSize`).
	[[vk::location(5)]] float cellSize : TEXCOORD4;
	// Atlas grid metadata (cells inside this cell's tileset atlas).
	[[vk::location(6)]] int atlasColumns : TEXCOORD5;
	[[vk::location(7)]] int atlasRows : TEXCOORD6;
	// Half-texel inset for the atlas sample — kills the seam flicker under
	// Linear filtering (same trick as the raycast sprite path).
	[[vk::location(8)]] float2 halfTexel : TEXCOORD7;
	// Slot inside `gTextures[32]` holding this cell's tileset atlas.
	[[vk::location(9)]] int textureSlot : TEXCOORD8;
};

struct FragmentInput {
	[[vk::location(0)]] float2 texCoord : TEXCOORD0;
	[[vk::location(1)]] nointerpolation int entityID : TEXCOORD1;
	[[vk::location(2)]] nointerpolation int textureSlot : TEXCOORD2;
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

	const float2 cornerLocal = kCornerOffsets[input.cornerIndex] * input.cellSize;
	const float3 worldPos = float3(input.cellWorld + cornerLocal, input.layerZ);
	output.svPosition = mul(gCamera.viewProjection, float4(worldPos, 1.0f));

	// Resolve the tile's UV rectangle inside the atlas. The CPU-side
	// `Tileset::getTileUv` performs the same math; doing it here saves the
	// per-cell function call + the per-vertex bandwidth that would otherwise
	// carry 4 vec2 UVs per cell.
	const uint atlasColumns = uint(max(input.atlasColumns, 1));
	const uint atlasRows = uint(max(input.atlasRows, 1));
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
			atlasMin + cornerUv * atlasStep - (cornerUv * 2.0f - 1.0f) * input.halfTexel;

	output.frag.entityID = input.entityID;
	output.frag.textureSlot = input.textureSlot;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	float4 texColor = gTextures[NonUniformResourceIndex(input.textureSlot)].Sample(input.texCoord);
	if (texColor.a < 0.001f)
		discard;
	output.color = texColor;
	output.entityID = input.entityID;
	return output;
}
