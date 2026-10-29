// One simulation step of the particles of an emitter: one thread per slot. The slots of the ring from `spawn.x`
// (`spawn.y` of them) take new particles, the others age, fall under the gravity, slow down with the drag and record
// their trail. Same arithmetic as `renderer::simulateParticle`.
//
// Layout contract (must match `renderer::RendererParticles`):
//   binding 0 : SSBO read-only  - the step (ParticleStep)
//   binding 1 : SSBO read-write - particles[capacity]
//   binding 2 : SSBO read-write - trails[capacity * trail length] (one point when there is no trail)

#include "particles.slangh"

[[vk::binding(0)]]
StructuredBuffer<ParticleStep> gStep;

[[vk::binding(1)]]
RWStructuredBuffer<Particle> gParticles;

[[vk::binding(2)]]
RWStructuredBuffer<TrailPoint> gTrails;

void spawnParticle(uint index, uint rank, ParticleStep step) {
	uint serial = step.spawn.z + rank;
	uint seed = step.config.z;
	float lifetime = max(lerp(step.life.x, step.life.y, particleRandom(seed, serial, 0u)), 1e-3f);
	float speed = lerp(step.life.z, step.life.w, particleRandom(seed, serial, 1u));
	float r2 = particleRandom(seed, serial, 2u);
	float r3 = particleRandom(seed, serial, 3u);
	float r4 = particleRandom(seed, serial, 4u);
	float3 position = float3(0.0f, 0.0f, 0.0f);
	float3 direction = float3(0.0f, 1.0f, 0.0f);
	uint shape = step.config.x;
	if (shape == 0u) {
		direction = randomUnit(r2, r3);
	} else if (shape == 1u) {
		direction = randomUnit(r2, r3);
		position = direction * (step.shape.x * pow(r4, 1.0f / 3.0f));
	} else if (shape == 2u) {
		position = (float3(r2, r3, r4) * 2.0f - 1.0f) * step.shape.xyz;
	} else if (shape == 3u) {
		float angle = step.shape.w * sqrt(r2);
		float phi = PARTICLE_TWO_PI * r3;
		float radius = step.shape.x * sqrt(r4);
		position = float3(radius * cos(phi), 0.0f, radius * sin(phi));
		direction = float3(sin(angle) * cos(phi), cos(angle), sin(angle) * sin(phi));
	} else {
		float phi = PARTICLE_TWO_PI * r2;
		direction = float3(cos(phi), sin(phi), 0.0f);
		position = direction * (step.shape.x * sqrt(r3));
	}
	float3 velocity = direction * speed + step.velocity.xyz;
	if (step.config.y == 0u) {
		float ratio = (float(rank) + 0.5f) / float(max(step.spawn.y, 1u));
		float3 from = transformPoint(step.previous, position);
		float3 to = transformPoint(step.emitter, position);
		position = from + (to - from) * ratio;
		velocity = transformDirection(step.emitter, velocity);
	}
	Particle p;
	p.position = float4(position, 0.0f);
	p.velocity = float4(velocity, lifetime);
	p.extra = float4(lerp(step.sizes.x, step.sizes.y, particleRandom(seed, serial, 5u)),
	                 PARTICLE_TWO_PI * particleRandom(seed, serial, 6u),
	                 lerp(step.sizes.z, step.sizes.w, particleRandom(seed, serial, 7u)),
	                 particleRandom(seed, serial, 8u));
	gParticles[index] = p;
	uint length = step.config.w;
	for (uint point = 0u; point < length; ++point)
		gTrails[index * length + point].position = p.position;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 id : SV_DispatchThreadID) {
	ParticleStep step = gStep[0];
	uint capacity = max(step.spawn.w, 1u);
	uint index = id.x;
	if (index >= capacity)
		return;
	Particle p = gParticles[index];
	if (step.flags.x != 0u) {
		p.position.w = 1.0f;
		p.velocity.w = 0.0f;
	}
	uint rank = (index + capacity - step.spawn.x % capacity) % capacity;
	if (rank < step.spawn.y) {
		spawnParticle(index, rank, step);
		return;
	}
	if (!isAlive(p)) {
		gParticles[index] = p;
		return;
	}
	uint length = step.config.w;
	float seconds = step.velocity.w;
	float3 position = p.position.xyz;
	if (step.config.y == 0u) {
		position -= step.shift.xyz;
		for (uint point = 0u; point < length; ++point)
			gTrails[index * length + point].position.xyz -= step.shift.xyz;
	}
	float previousAge = p.position.w;
	float age = previousAge + seconds;
	if (age >= p.velocity.w) {
		p.position = float4(position, age);
		gParticles[index] = p;
		return;
	}
	float3 velocity = p.velocity.xyz + step.gravity.xyz * seconds;
	velocity = velocity * (1.0f / (1.0f + step.gravity.w * seconds));
	position = position + velocity * seconds;
	p.position = float4(position, age);
	p.velocity = float4(velocity, p.velocity.w);
	p.extra.y += p.extra.z * seconds;
	gParticles[index] = p;
	float interval = step.shift.w;
	if (length > 0u && interval > 0.0f && floor(age / interval) > floor(previousAge / interval)) {
		for (uint point = length - 1u; point > 0u; --point)
			gTrails[index * length + point].position = gTrails[index * length + point - 1u].position;
		gTrails[index * length].position = p.position;
	}
}
