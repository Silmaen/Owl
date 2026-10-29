// Sort keys of the particles of an alpha-blended emitter, for `bitonic_sort`: the distance along the view, negated so
// the ascending sort lays the particles out far to near. Dead particles and the slots past the capacity get +inf and
// land at the end. One thread per item of the 1024 the sort takes.
//
// Layout contract (must match `renderer::RendererParticles`):
//   binding 0 : SSBO read-only  - the camera (SortParams)
//   binding 1 : SSBO read-only  - particles[capacity]
//   binding 2 : SSBO write      - items[1024]

#include "particles.slangh"

struct SortParams {
	float4 eye;       // xyz camera position.
	float4 forward;   // xyz view direction.
	float4 emitter[4];// columns of the emitter's world matrix (local emitters).
	uint4 config;     // x capacity, y 1 for a local emitter.
};

static const uint SORT_SIZE = 1024u;

[[vk::binding(0)]]
StructuredBuffer<SortParams> gSort;

[[vk::binding(1)]]
StructuredBuffer<Particle> gParticles;

[[vk::binding(2)]]
RWStructuredBuffer<SortItem> gItems;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 id : SV_DispatchThreadID) {
	uint index = id.x;
	if (index >= SORT_SIZE)
		return;
	SortParams params = gSort[0];
	SortItem item;
	item.key = 3.0e38f;
	item.value = index;
	if (index < params.config.x) {
		Particle p = gParticles[index];
		if (isAlive(p)) {
			float3 position = params.config.y != 0u ? transformPoint(params.emitter, p.position.xyz) : p.position.xyz;
			item.key = -dot(position - params.eye.xyz, params.forward.xyz);
		}
	}
	gItems[index] = item;
}
