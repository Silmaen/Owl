// Isometric billboards — instanced, SSBO-driven, one draw call per pass.
// Each instance is a pre-rendered sprite anchored at its world foot point:
// only the foot point goes through the dimetric view, the sprite keeps its
// pixels (scaled by the zoom in the projection). The depth of each corner
// follows a plane through the foot: a vertical plane facing the viewer for an
// upright sprite (higher pixels are nearer), the ground plane for a flat
// terrain tile (higher pixels are farther); the slope is per instance.

struct IsometricScene {
	column_major float4x4 view;
	column_major float4x4 projection;
	// x: alpha below which a texel is discarded (cutout); yzw unused.
	float4 params;
};

[[vk::binding(0)]]
ConstantBuffer<IsometricScene> gScene;

#ifdef BACKEND_VULKAN
[[vk::binding(1)]]
#else
[[vk::binding(0)]]
#endif
Sampler2D gTextures[32];

struct BillboardInstance {
	// xyz: world foot point; w: depth bias towards the viewer, in view pixels.
	float4 foot;
	// xy: sprite size in pixels; z: view depth per pixel up the sprite; w unused.
	float4 size;
	float2 uv[4];
	float4 color;
	uint texIndex;
	int entityId;
	uint pad0;
	uint pad1;
};

[[vk::binding(2)]]
StructuredBuffer<BillboardInstance> gInstances;

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

// Corners anchored at the bottom centre — BL, BR, TR, TL, matching the index
// buffer's {0,1,2, 2,3,0} winding used by the CPU side.
static const float2 kCorners[4] = {
	float2(-0.5f, 0.0f),
	float2( 0.5f, 0.0f),
	float2( 0.5f, 1.0f),
	float2(-0.5f, 1.0f),
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint iid : SV_InstanceID) {
	VertexOutput output;
	BillboardInstance inst = gInstances[iid];
	float4 viewPos = mul(gScene.view, float4(inst.foot.xyz, 1.0f));
	const float2 offset = kCorners[input.cornerIndex] * inst.size.xy;
	viewPos.xy += offset;
	viewPos.z += offset.y * inst.size.z + inst.foot.w;
	output.svPosition = mul(gScene.projection, viewPos);
	output.frag.color = inst.color;
	output.frag.texCoord = inst.uv[input.cornerIndex];
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
	output.color = texColor;
	output.entityID = input.entityID;
	return output;
}
