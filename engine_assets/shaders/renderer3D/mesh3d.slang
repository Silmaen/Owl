// Renderer3D forward mesh shader: the surface of common/mesh_surface.slangh (one material per draw, PBR, Toon or
// Unlit, the shadow-pass depth). The model, tint and entity id come from the draw (`gScene`) or, when
// `gScene.ids.w >= 0`, from the instance list: instance i of the draw reads `gInstances[gVisible[ids.w + i].index]`.

#include "mesh_surface.slangh"

struct VertexInput {
	[[vk::location(0)]] float3 position : POSITION;
	[[vk::location(1)]] float3 normal : NORMAL;
	[[vk::location(2)]] float2 uv : TEXCOORD0;
	[[vk::location(3)]] int texIndex : TEXCOORD1;
	[[vk::location(4)]] float4 tileRect : TEXCOORD2;// (uMin, vMin, uSize, vSize): the uv maps into this sub-rect.
	[[vk::location(5)]] float ao : TEXCOORD3;
};

// One instance of an instanced draw (must match `renderer::MeshInstance`).
struct MeshInstance {
	column_major float4x4 model;
	float4 tint;
	int4 ids;// x: entity id; yzw unused.
};

// Index of a visible instance, wrapped in a struct of its own (one element type per storage buffer).
struct VisibleInstance {
	uint index;
};

[[vk::binding(5)]]
StructuredBuffer<MeshInstance> gInstances;

[[vk::binding(6)]]
StructuredBuffer<VisibleInstance> gVisible;

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instanceId : SV_InstanceID) {
	VertexOutput output;
	float4x4 model = gScene.model;
	output.frag.tint = gScene.tint;
	output.frag.entityId = gScene.ids.x;
	if (gScene.ids.w >= 0) {
		const MeshInstance instance = gInstances[gVisible[uint(gScene.ids.w) + instanceId].index];
		model = instance.model;
		output.frag.tint = instance.tint;
		output.frag.entityId = instance.ids.x;
	}
	float3 position = relativePosition(model, input.position);
	output.svPosition = mul(gScene.viewProjection, float4(position, 1.0f));
	output.frag.position = position;
	output.frag.clipDepth = output.svPosition.zw;
	output.frag.normal = normalize(mul((float3x3) model, input.normal));
	output.frag.uv = input.tileRect.xy + input.uv * input.tileRect.zw;
	output.frag.texIndex = input.texIndex;
	output.frag.ao = input.ao;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input, bool frontFace : SV_IsFrontFace) {
	return shadeMeshSurface(input, frontFace);
}
