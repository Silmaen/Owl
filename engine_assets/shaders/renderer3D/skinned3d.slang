// Renderer3D skinned mesh shader: the vertices of `mesh3d` moved by up to four joints, the surface of
// common/mesh_surface.slangh. The skin matrices of the draw start at `gScene.ids.w` in `gSkin` (one palette per
// skinned draw in the storage buffer of the pass); a vertex whose weights sum to zero stays in bind space.

#include "mesh_surface.slangh"

struct VertexInput {
	[[vk::location(0)]] float3 position : POSITION;
	[[vk::location(1)]] float3 normal : NORMAL;
	[[vk::location(2)]] float2 uv : TEXCOORD0;
	[[vk::location(3)]] int texIndex : TEXCOORD1;
	[[vk::location(4)]] float4 tileRect : TEXCOORD2;// (uMin, vMin, uSize, vSize): the uv maps into this sub-rect.
	[[vk::location(5)]] float ao : TEXCOORD3;
	[[vk::location(6)]] int4 joints : TEXCOORD4;
	[[vk::location(7)]] float4 weights : TEXCOORD5;
};

// One skin matrix (bone model matrix times inverse bind), wrapped in a struct of its own.
struct SkinMatrix {
	column_major float4x4 matrix;
};

[[vk::binding(5)]]
StructuredBuffer<SkinMatrix> gSkin;

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
	VertexOutput output;
	float4 skinned = float4(input.position, 1.0f);
	float3 normal = input.normal;
	float total = input.weights.x + input.weights.y + input.weights.z + input.weights.w;
	if (total > 0.0001f) {
		uint base = uint(max(gScene.ids.w, 0));
		float4x4 skin = gSkin[base + uint(input.joints.x)].matrix * input.weights.x;
		skin += gSkin[base + uint(input.joints.y)].matrix * input.weights.y;
		skin += gSkin[base + uint(input.joints.z)].matrix * input.weights.z;
		skin += gSkin[base + uint(input.joints.w)].matrix * input.weights.w;
		skinned = mul(skin, float4(input.position, 1.0f));
		normal = mul((float3x3) skin, input.normal);
	}
	float4x4 model = gScene.model;
	output.frag.tint = gScene.tint;
	output.frag.entityId = gScene.ids.x;
	float3 position = relativePosition(model, skinned.xyz);
	output.svPosition = mul(gScene.viewProjection, float4(position, 1.0f));
	output.frag.position = position;
	output.frag.clipDepth = output.svPosition.zw;
	output.frag.normal = normalize(mul((float3x3) model, normal));
	output.frag.uv = input.tileRect.xy + input.uv * input.tileRect.zw;
	output.frag.texIndex = input.texIndex;
	output.frag.ao = input.ao;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input, bool frontFace : SV_IsFrontFace) {
	return shadeMeshSurface(input, frontFace);
}
