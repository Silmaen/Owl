// Per-column DDA raycast compute pass (#35). One thread per screen column.
// Each thread walks the 2D tilemap grid from the camera in the direction of
// its column's ray, recording every wall hit (opaque or transparent) into a
// per-column slot range of `columnHits[]`. Transparent hits keep the ray
// going up to `kMaxHits` total; the first opaque hit terminates the walk
// and is mirrored into `zBuffer[col]` for the sprite-occlusion pass.
//
// Layout contract (must match `renderer::utils::RaycastDDAPass`):
//   binding 0 : SSBO read-only  — params[0]    (RaycastParams)
//   binding 1 : SSBO read-only  — tileGrid[]   (int32_t, row-major, -1 = empty)
//   binding 2 : SSBO read-only  — tileMeta[]   (TileMeta — wallHeight + flags)
//   binding 3 : SSBO read-write — hitCount[col] (uint per column)
//   binding 4 : SSBO read-write — zBuffer[col] (float, +∞ for misses)
//   binding 5 : SSBO read-write — columnHits[col * kMaxHits + k] (ColumnHit)
//
// Dispatch: ceil(numRays / 64) workgroups.

struct RaycastParams {
	float2 camCellPos;
	float2 camCellDir;
	float2 camCellPlane;
	uint numRays;
	uint maxSteps;
	uint gridWidth;
	uint gridHeight;
	float minPerpDist;
	uint _pad0;
	uint _pad1;
	uint _pad2;
};

struct TileMeta {
	float wallHeight;
	uint transparent;// 0 = opaque, 1 = transparent
	uint _pad0;
	uint _pad1;
};

struct ColumnHit {
	int tileIndex;
	float perpDist;
	float wallX;
	float wallHeight;
	uint side;// 0 = X-edge, 1 = Y-edge
	uint transparent;
	uint _pad0;
	uint _pad1;
};

static const uint kMaxHits = 8;

[[vk::binding(0)]]
StructuredBuffer<RaycastParams> params;

[[vk::binding(1)]]
StructuredBuffer<int> tileGrid;

[[vk::binding(2)]]
StructuredBuffer<TileMeta> tileMeta;

[[vk::binding(3)]]
RWStructuredBuffer<uint> hitCount;

[[vk::binding(4)]]
RWStructuredBuffer<float> zBuffer;

[[vk::binding(5)]]
RWStructuredBuffer<ColumnHit> columnHits;

int fetchTile(uint mapX, uint mapY, RaycastParams p) {
	if (mapX >= p.gridWidth || mapY >= p.gridHeight)
		return -1;
	return tileGrid[mapY * p.gridWidth + mapX];
}

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 dispatchID : SV_DispatchThreadID) {
	const uint col = dispatchID.x;
	const RaycastParams p = params[0];
	if (col >= p.numRays)
		return;

	// Reset outputs for this column up front so the consumer doesn't need to
	// pre-clear them every frame.
	hitCount[col] = 0u;
	zBuffer[col] = 1e30f;

	const float cameraX = 2.0f * (float(col) + 0.5f) / float(p.numRays) - 1.0f;
	const float2 rayDir = float2(
		p.camCellDir.x + p.camCellPlane.x * cameraX,
		p.camCellDir.y + p.camCellPlane.y * cameraX);
	const float deltaX = (abs(rayDir.x) < 1e-6f) ? 1e30f : abs(1.0f / rayDir.x);
	const float deltaY = (abs(rayDir.y) < 1e-6f) ? 1e30f : abs(1.0f / rayDir.y);
	int mapX = int(floor(p.camCellPos.x));
	int mapY = int(floor(p.camCellPos.y));
	const int stepX = (rayDir.x < 0.0f) ? -1 : 1;
	const int stepY = (rayDir.y < 0.0f) ? -1 : 1;
	float sideDistX = (rayDir.x < 0.0f) ? (p.camCellPos.x - float(mapX)) * deltaX
										: (float(mapX + 1) - p.camCellPos.x) * deltaX;
	float sideDistY = (rayDir.y < 0.0f) ? (p.camCellPos.y - float(mapY)) * deltaY
										: (float(mapY + 1) - p.camCellPos.y) * deltaY;

	uint localHitCount = 0u;
	uint side = 0u;
	for (uint step = 0u; step < p.maxSteps; ++step) {
		if (sideDistX < sideDistY) {
			sideDistX += deltaX;
			mapX += stepX;
			side = 0u;
		} else {
			sideDistY += deltaY;
			mapY += stepY;
			side = 1u;
		}
		const int cell = fetchTile(uint(mapX), uint(mapY), p);
		if (cell < 0)
			continue;
		const float perpDistCells = (side == 0u) ? (sideDistX - deltaX) : (sideDistY - deltaY);
		const float perpDistSafe = max(perpDistCells, p.minPerpDist);
		float wallX = (side == 0u) ? (p.camCellPos.y + perpDistSafe * rayDir.y)
								   : (p.camCellPos.x + perpDistSafe * rayDir.x);
		wallX -= floor(wallX);
		// Match the CPU walk's flip convention so the GPU output indexes into
		// the same texture U strip.
		if (side == 0u && rayDir.x > 0.0f)
			wallX = 1.0f - wallX;
		if (side == 1u && rayDir.y < 0.0f)
			wallX = 1.0f - wallX;
		const TileMeta meta = tileMeta[uint(cell)];

		ColumnHit hit;
		hit.tileIndex = cell;
		hit.perpDist = perpDistSafe;
		hit.wallX = wallX;
		hit.wallHeight = max(0.0f, meta.wallHeight);
		hit.side = side;
		hit.transparent = meta.transparent;
		columnHits[col * kMaxHits + localHitCount] = hit;
		++localHitCount;

		if (meta.transparent == 0u) {
			zBuffer[col] = perpDistSafe;
			break;
		}
		if (localHitCount >= kMaxHits)
			break;
	}
	hitCount[col] = localHitCount;
}
