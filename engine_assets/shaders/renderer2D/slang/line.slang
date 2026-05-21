// Renderer2D line — instanced, SSBO-driven. One drawcall per batch via
// `glDrawArraysInstanced(GL_LINES, 0, 2, instanceCount)`. No per-vertex
// VBO at all: the vertex shader picks `point1` for `SV_VertexID == 0`
// and `point2` for `SV_VertexID == 1`. Per-line data (endpoints, colour,
// entity id) lives in `gInstances[gl_InstanceID]`.

struct Camera {
	column_major float4x4 viewProjection;
};

[[vk::binding(0)]]
ConstantBuffer<Camera> gCamera;

struct LineInstance {
	float3 point1;
	float _pad0;
	float3 point2;
	float _pad1;
	float4 color;
	int entityId;
	float _pad2[3];
};

[[vk::binding(2)]]
StructuredBuffer<LineInstance> gInstances;

struct VertexInput {
	[[vk::location(0)]] int endpointIndex : POSITION;
};

struct FragmentInput {
	[[vk::location(0)]] float4 color : COLOR;
	[[vk::location(1)]] nointerpolation int entityID : TEXCOORD0;
};

struct FragmentOutput {
	float4 color : SV_Target0;
	int entityID : SV_Target1;
};

struct VertexOutput {
	FragmentInput frag;
	float4 svPosition : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint iid : SV_InstanceID) {
	VertexOutput output;
	LineInstance inst = gInstances[iid];
	float3 pos = (input.endpointIndex == 0) ? inst.point1 : inst.point2;
	output.svPosition = mul(gCamera.viewProjection, float4(pos, 1.0));
	output.frag.color = inst.color;
	output.frag.entityID = inst.entityId;
	return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
	FragmentOutput output;
	output.color = input.color;
	output.entityID = input.entityID;
	return output;
}
