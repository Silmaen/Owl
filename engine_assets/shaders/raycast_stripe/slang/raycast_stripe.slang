// Raycast wall stripe — one instance per (column, hit) pair, fed by
// `RaycastDDAPass` output. Each instance reads `gColumnHits[col*maxHits +
// hitIdx]` and emits a textured wall stripe at the right screen X / Y.
//
// Layout contract (must match `renderer::RendererRaycast`'s descriptor
// block):
//   binding 0 : UBO  — StripeParams (viewProjection + counts + horizon + ids)
//   binding 1 : Sampler2D — tileset atlas (single texture, no array)
//   binding 2 : SSBO — hitCount[col]            (uint per column)
//   binding 3 : SSBO — columnHits[col*maxHits + k] (ColumnHit per slot)
//   binding 4 : SSBO — tileUvRects[tileIndex]   (float4: uvLeft, uvBottom, uvRight, uvTop)
//
// Painter's order: instance `localK = iid % maxHits` indexes the hit list
// back-to-front, so `localK=0` picks the FARTHEST hit (rendered first) and
// the closest hit ends up on top. Slots past `hitCount[col]` emit a
// zero-area quad off-screen so the rasteriser drops them cheaply.

struct StripeParams {
	column_major float4x4 viewProjection;
	// (viewportY, horizonY, stripePxWidth, yDarken)
	float4 floats;
	uint numRays;
	uint maxHits;
	int entityId;
	uint _pad;
};

[[vk::binding(0)]]
ConstantBuffer<StripeParams> gParams;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTileset;

struct ColumnHit {
	int tileIndex;
	float perpDist;
	float wallX;
	float wallHeight;
	uint side;
	uint transparent;
	uint _pad0;
	uint _pad1;
};

[[vk::binding(2)]]
StructuredBuffer<uint> gHitCount;

[[vk::binding(3)]]
StructuredBuffer<ColumnHit> gColumnHits;

[[vk::binding(4)]]
StructuredBuffer<float4> gTileUvRects;

struct VertexInput {
	[[vk::location(0)]] int cornerIndex : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float4 color : COLOR;
	[[vk::location(1)]] float2 texCoord : TEXCOORD0;
	[[vk::location(2)]] nointerpolation int entityID : TEXCOORD1;
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
	float2(-0.5f, -0.5f),
	float2( 0.5f, -0.5f),
	float2( 0.5f,  0.5f),
	float2(-0.5f,  0.5f),
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint iid : SV_InstanceID) {
	VertexOutput output;
	const uint col = iid / gParams.maxHits;
	const uint localK = iid % gParams.maxHits;

	bool degenerate = (col >= gParams.numRays);
	uint hits = 0u;
	if (!degenerate) {
		hits = gHitCount[col];
		degenerate = (localK >= hits);
	}
	if (degenerate) {
		output.svPosition = float4(2.0f, 2.0f, 0.0f, 1.0f);
		output.frag.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
		output.frag.texCoord = float2(0.0f, 0.0f);
		output.frag.entityID = 0;
		return output;
	}

	const uint hitIdx = hits - 1u - localK;
	const ColumnHit hit = gColumnHits[col * gParams.maxHits + hitIdx];

	const float viewportY = gParams.floats.x;
	const float horizonY = gParams.floats.y;
	const float stripePxWidth = gParams.floats.z;
	const float yDarken = gParams.floats.w;

	const float stripeX = (float(col) + 0.5f) * stripePxWidth;
	const float lineHeightUnit = floor((viewportY / hit.perpDist) * 0.5f) * 2.0f;
	const float lineHeight = lineHeightUnit * hit.wallHeight;
	const float screenCenterY = horizonY + lineHeightUnit * (hit.wallHeight - 1.0f) * 0.5f;

	const float4 uvRect = gTileUvRects[uint(hit.tileIndex)];
	const float uHit = uvRect.x + hit.wallX * (uvRect.z - uvRect.x);

	const float2 corner = kCorners[input.cornerIndex];
	const float2 screenPos = float2(stripeX + corner.x * stripePxWidth,
									screenCenterY + corner.y * lineHeight);
	output.svPosition = mul(gParams.viewProjection, float4(screenPos, 0.0f, 1.0f));

	const float v = (corner.y < 0.0f) ? uvRect.y : uvRect.w;
	output.frag.texCoord = float2(uHit, v);
	const float darken = (hit.side == 1u) ? yDarken : 1.0f;
	output.frag.color = float4(darken, darken, darken, 1.0f);
	output.frag.entityID = gParams.entityId;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	const float4 texColor = input.color * gTileset.Sample(input.texCoord);
	if (texColor.a < 0.001f)
		discard;
	output.color = texColor;
	output.entityID = input.entityID;
	return output;
}
