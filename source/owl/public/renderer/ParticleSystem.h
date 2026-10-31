/**
 * @file ParticleSystem.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "core/Core.h"
#include "gpu/Texture.h"
#include "math/Curve.h"
#include "math/matrices.h"
#include "math/matrixCreation.h"
#include "math/vectors.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

/**
 * @brief
 *  Particles: the settings of an emitter, its simulation (on the GPU, or on the CPU without one) and its tables.
 */
namespace owl::renderer {

/**
 * @brief
 *  One stop of a colour gradient.
 */
struct OWL_API ColorKey {
	/// Position of the stop, 0 to 1.
	float time = 0.f;
	/// Colour at the stop, sRGB as authored, alpha in `w`.
	math::vec4 color{1.f, 1.f, 1.f, 1.f};
};

/**
 * @brief
 *  A colour over a normalised time: stops interpolated linearly in the authored (sRGB) space.
 */
struct OWL_API ColorGradient {
	/// Stops, sorted by time; empty means white all along.
	std::vector<ColorKey> keys;
	/**
	 * @brief
	 *  Add a stop, kept sorted by time.
	 * @param[in] iKey The stop.
	 * @return The index the stop landed at.
	 */
	auto addKey(const ColorKey& iKey) -> size_t;
	/**
	 * @brief
	 *  Colour at a time, the first and last stops held beyond them.
	 * @param[in] iTime The time, 0 to 1.
	 * @return The sRGB colour and alpha.
	 */
	[[nodiscard]] auto evaluate(float iTime) const -> math::vec4;
};

/// Shape particles are spawned in.
enum struct ParticleShape : uint8_t {
	Point,///< At the emitter, in every direction.
	Sphere,///< Inside a sphere of radius `shapeSize.x`, moving outwards.
	Box,///< Inside a box of half extents `shapeSize`, moving along +Y.
	Cone,///< On a disc of radius `shapeSize.x`, moving along +Y within `coneAngle`.
	Circle,///< On a disc of radius `shapeSize.x` in the XY plane, moving outwards (2D).
};

/// Space the particles live in.
enum struct ParticleSpace : uint8_t {
	World,///< Left behind where they were spawned when the emitter moves.
	Local,///< Carried along by the emitter.
};

/// How particles cover what is behind them.
enum struct ParticleBlend : uint8_t {
	Additive,///< Added to the scene, in any order: fire, sparks, magic.
	Alpha,///< Blended over the scene, sorted back to front on the GPU: smoke, dust.
};

/// Largest number of particles of an emitter.
constexpr uint32_t g_maxParticles = 65536;
/// Largest number of particles of an emitter blended with `ParticleBlend::Alpha` (sorted in one GPU workgroup).
constexpr uint32_t g_maxSortedParticles = 1024;
/// Largest number of points of the trail of a particle.
constexpr uint32_t g_maxParticleTrail = 16;
/// Number of samples of the colour and size tables a GPU reads.
constexpr uint32_t g_particleTableSize = 16;

/**
 * @brief
 *  Everything an emitter is authored with.
 */
struct OWL_API ParticleEmitterSettings {
	/// Largest number of living particles (the oldest is replaced past it).
	uint32_t maxParticles = 256;
	/// Particles spawned per second while emitting.
	float rate = 20.f;
	/// Particles spawned at once each time the emission starts (or loops).
	uint32_t burst = 0;
	/// Seconds the emission lasts; 0 emits forever.
	float duration = 0.f;
	/// Whether the emission starts again at the end of its duration.
	bool loop = true;
	/// Shortest life of a particle, in seconds.
	float lifetimeMin = 1.f;
	/// Longest life of a particle, in seconds.
	float lifetimeMax = 2.f;
	/// Shape particles are spawned in.
	ParticleShape shape = ParticleShape::Cone;
	/// Size of the shape: the radius in `x` (sphere, cone, circle) or the half extents (box).
	math::vec3 shapeSize{0.1f, 0.1f, 0.1f};
	/// Half angle of the cone, in radians.
	float coneAngle = 0.3f;
	/// Slowest start speed, in units per second.
	float speedMin = 1.f;
	/// Fastest start speed, in units per second.
	float speedMax = 2.f;
	/// Velocity added to every spawned particle, in the space of the emitter.
	math::vec3 velocity{0.f, 0.f, 0.f};
	/// Acceleration of every particle, in world units per second squared.
	math::vec3 gravity{0.f, 0.f, 0.f};
	/// Share of the velocity lost per second (air resistance).
	float drag = 0.f;
	/// Smallest start size, in world units.
	float sizeMin = 0.1f;
	/// Largest start size, in world units.
	float sizeMax = 0.2f;
	/// Size multiplier over the life of a particle (empty: 1).
	math::Curve sizeOverLife;
	/// Slowest spin, in radians per second.
	float spinMin = 0.f;
	/// Fastest spin, in radians per second.
	float spinMax = 0.f;
	/// Tint of every particle, sRGB as authored.
	math::vec4 color{1.f, 1.f, 1.f, 1.f};
	/// Colour over the life of a particle, multiplying the tint (empty: white).
	ColorGradient colorOverLife;
	/// Luminance added to the colour, in nits (pre-exposed like the other lights): the bloom makes it glow.
	float emissive = 0.f;
	/// How particles cover what is behind them.
	ParticleBlend blend = ParticleBlend::Additive;
	/// Space the particles live in.
	ParticleSpace space = ParticleSpace::World;
	/// Texture of a particle; none draws a soft disc.
	shared<gpu::Texture2D> texture;
	/// Points of the ribbon trail each particle leaves behind; 0 draws no trail.
	uint32_t trailLength = 0;
	/// Seconds between two points of a trail.
	float trailInterval = 0.05f;
	/// Width of a trail at its head, relative to the size of the particle.
	float trailWidth = 0.5f;
	/// Seed of the random draws: two emitters with different seeds never look the same.
	uint32_t seed = 1;
};

/// Starting points of an emitter, tuned for a 3D scene in metres.
enum struct ParticlePreset : uint8_t {
	Fire,///< Additive flames rising and shrinking, orange to dark red, emissive.
	Smoke,///< Grey puffs blended over the scene, rising, growing and fading, sorted.
	Sparks,///< Fast additive sparks falling under gravity, with trails, emissive.
	Magic,///< A slow swirl of blue and violet motes around the emitter, emissive.
	Snow,///< White flakes drifting down from a wide box.
};

/**
 * @brief
 *  The settings of a preset.
 * @param[in] iPreset The preset.
 * @return The settings.
 */
[[nodiscard]] OWL_API auto makeParticlePreset(ParticlePreset iPreset) -> ParticleEmitterSettings;

/**
 * @brief
 *  The colour and size tables of an emitter, sampled over the life of a particle.
 */
struct OWL_API ParticleTables {
	/// Tint times colour over life, sRGB, alpha in `w`.
	std::array<math::vec4, g_particleTableSize> color{};
	/// Size over life.
	std::array<float, g_particleTableSize> size{};
	/**
	 * @brief
	 *  Sample the curves of an emitter.
	 * @param[in] iSettings The emitter.
	 * @return The tables.
	 */
	[[nodiscard]] static auto bake(const ParticleEmitterSettings& iSettings) -> ParticleTables;
	/**
	 * @brief
	 *  Colour at a point of the life, linearly between the samples (as the GPU reads it).
	 * @param[in] iLife The age over the lifetime, 0 to 1.
	 * @return The sRGB colour and alpha.
	 */
	[[nodiscard]] auto colorAt(float iLife) const -> math::vec4;
	/**
	 * @brief
	 *  Size at a point of the life, linearly between the samples.
	 * @param[in] iLife The age over the lifetime, 0 to 1.
	 * @return The size multiplier.
	 */
	[[nodiscard]] auto sizeAt(float iLife) const -> float;
};

/**
 * @brief
 *  One particle as the GPU stores it (std430, 48 bytes).
 */
struct OWL_API GpuParticle {
	/// Position (in the space of the emitter for `ParticleSpace::Local`), age in seconds in `w`.
	math::vec4 position{0.f, 0.f, 0.f, 1.f};
	/// Velocity, lifetime in seconds in `w` (a particle whose age reached its lifetime is dead).
	math::vec4 velocity{0.f, 0.f, 0.f, 0.f};
	/// x start size, y rotation (radians), z spin (radians per second), w a random value of the particle.
	math::vec4 extra{0.f, 0.f, 0.f, 0.f};
	/**
	 * @brief
	 *  Whether the particle lives.
	 * @return True while its age is below its lifetime.
	 */
	[[nodiscard]] auto isAlive() const -> bool { return position.w() < velocity.w(); }
};

/**
 * @brief
 *  What one simulation step does, laid out as the compute shader reads it (std430).
 */
struct OWL_API ParticleStepParams {
	/// Columns of the emitter's world matrix at the end of the step.
	std::array<math::vec4, 4> emitter{};
	/// Columns of the emitter's world matrix at the start of the step (spawns are spread between both).
	std::array<math::vec4, 4> previous{};
	/// xyz acceleration in the simulation space, w drag.
	math::vec4 gravity{0.f, 0.f, 0.f, 0.f};
	/// xyz shape size, w cone half angle.
	math::vec4 shape{0.f, 0.f, 0.f, 0.f};
	/// x shortest life, y longest life, z slowest speed, w fastest speed.
	math::vec4 life{1.f, 1.f, 0.f, 0.f};
	/// x smallest size, y largest size, z slowest spin, w fastest spin.
	math::vec4 sizes{0.f, 0.f, 0.f, 0.f};
	/// xyz velocity added at spawn (emitter space), w duration of the step in seconds.
	math::vec4 velocity{0.f, 0.f, 0.f, 0.f};
	/// xyz origin shift taken off the world-space particles, w seconds between two trail points.
	math::vec4 shift{0.f, 0.f, 0.f, 0.f};
	/// x first spawned slot, y spawned count, z serial of the first spawn, w capacity.
	std::array<uint32_t, 4> spawn{0, 0, 0, 0};
	/// x shape, y space (0 world, 1 local), z seed, w trail length.
	std::array<uint32_t, 4> config{0, 0, 0, 0};
	/// x 1 to kill every particle first, yzw unused.
	std::array<uint32_t, 4> flags{0, 0, 0, 0};
};

/**
 * @brief
 *  Simulate one particle for one step, as the compute shader `particle_simulate` does.
 * @param[in,out] ioParticle The particle.
 * @param[in,out] ioTrail The points of its trail (`config.w` of them), may be empty.
 * @param[in] iIndex Slot of the particle.
 * @param[in] iParams The step.
 */
OWL_API void simulateParticle(GpuParticle& ioParticle, std::span<math::vec4> ioTrail, uint32_t iIndex,
							  const ParticleStepParams& iParams);

/**
 * @brief
 *  Random value of the simulation, the same in C++ and in the shaders.
 * @param[in] iSeed The seed of the emitter.
 * @param[in] iSerial Serial number of the spawn.
 * @param[in] iChannel Which draw of the spawn.
 * @return A value in [0, 1).
 */
[[nodiscard]] OWL_API auto particleRandom(uint32_t iSeed, uint32_t iSerial, uint32_t iChannel) -> float;

/**
 * @brief
 *  GPU resources of a particle system, owned by `RendererParticles`.
 */
struct ParticleGpuState;

/**
 * @brief
 *  The particles of one emitter: what it emits, and their simulation.
 *
 * `advance` does the emission bookkeeping on the CPU each frame (spawn counts, the ring of slots, bursts, loops). With a
 * GPU the step is recorded and run by a compute shader when `RendererParticles` draws the system; on the Null backend,
 * or with `Path::Cpu`, `advance` simulates the particles on the CPU with the same arithmetic (`simulateParticle`).
 * Spawned particles take the slots of a ring: past `maxParticles`, the oldest one is replaced. No allocation happens
 * per frame: the buffers change only with the capacity or the trail length.
 */
class OWL_API ParticleSystem final {
public:
	/// Where the particles are simulated.
	enum struct Path : uint8_t {
		Cpu,///< On the CPU, the particles readable through `getParticles`.
		Gpu,///< In a compute shader, drawn by `RendererParticles`.
	};
	/**
	 * @brief
	 *  Create a stopped system.
	 * @param[in] iPath Where the particles are simulated.
	 */
	explicit ParticleSystem(Path iPath = Path::Cpu);
	/**
	 * @brief
	 *  Destructor.
	 */
	~ParticleSystem();
	/**
	 * @brief
	 *  Copy constructor (deleted: the GPU buffers are not shared).
	 */
	ParticleSystem(const ParticleSystem&) = delete;
	/**
	 * @brief
	 *  Move constructor (deleted).
	 */
	ParticleSystem(ParticleSystem&&) = delete;
	/**
	 * @brief
	 *  Copy assignment (deleted).
	 * @return This system.
	 */
	auto operator=(const ParticleSystem&) -> ParticleSystem& = delete;
	/**
	 * @brief
	 *  Move assignment (deleted).
	 * @return This system.
	 */
	auto operator=(ParticleSystem&&) -> ParticleSystem& = delete;

	/**
	 * @brief
	 *  Start or resume: the emission runs and the particles move.
	 */
	void play();
	/**
	 * @brief
	 *  Freeze the particles and the emission where they are.
	 */
	void pause();
	/**
	 * @brief
	 *  Stop the emission; the living particles finish their life.
	 */
	void stop();
	/**
	 * @brief
	 *  Kill every particle and start the emission from its beginning.
	 */
	void restart();
	/**
	 * @brief
	 *  Spawn particles at the next step, whether the emission runs or not.
	 * @param[in] iCount Number of particles.
	 */
	void emit(uint32_t iCount);
	/**
	 * @brief
	 *  Whether the system runs (not paused).
	 * @return True while playing.
	 */
	[[nodiscard]] auto isPlaying() const -> bool { return m_playing; }
	/**
	 * @brief
	 *  Whether the emission runs.
	 * @return True while emitting.
	 */
	[[nodiscard]] auto isEmitting() const -> bool { return m_emitting; }

	/**
	 * @brief
	 *  Advance the system by a frame.
	 * @param[in] iSettings The emitter.
	 * @param[in] iSeconds Duration of the frame (nothing happens while paused).
	 * @param[in] iEmitter World matrix of the emitter.
	 */
	void advance(const ParticleEmitterSettings& iSettings, float iSeconds, const math::mat4& iEmitter);
	/**
	 * @brief
	 *  Follow a shift of the floating origin: the world-space particles and their trails move back by it.
	 * @param[in] iShift The shift of the origin.
	 */
	void shiftOrigin(const math::vec3& iShift);

	/**
	 * @brief
	 *  Where the particles are simulated.
	 * @return The path.
	 */
	[[nodiscard]] auto getPath() const -> Path { return m_path; }
	/**
	 * @brief
	 *  Number of particle slots.
	 * @return The capacity.
	 */
	[[nodiscard]] auto getCapacity() const -> uint32_t { return m_capacity; }
	/**
	 * @brief
	 *  Number of points of each trail.
	 * @return The trail length.
	 */
	[[nodiscard]] auto getTrailLength() const -> uint32_t { return m_trailLength; }
	/**
	 * @brief
	 *  Number of particles spawned since the last restart.
	 * @return The count.
	 */
	[[nodiscard]] auto getSpawnedCount() const -> uint64_t { return m_spawned; }
	/**
	 * @brief
	 *  Number of living particles (CPU path only, 0 on the GPU path).
	 * @return The count.
	 */
	[[nodiscard]] auto getAliveCount() const -> uint32_t;
	/**
	 * @brief
	 *  The particle slots (CPU path only, empty on the GPU path).
	 * @return The particles.
	 */
	[[nodiscard]] auto getParticles() const -> std::span<const GpuParticle> { return m_particles; }
	/**
	 * @brief
	 *  The trail points of every slot, `getTrailLength()` per slot (CPU path only).
	 * @return The trail points: xyz position.
	 */
	[[nodiscard]] auto getTrails() const -> std::span<const math::vec4> { return m_trails; }
	/**
	 * @brief
	 *  Whether a step is waiting for the GPU.
	 * @return True when `takeStep` has a step to run.
	 */
	[[nodiscard]] auto hasPendingStep() const -> bool { return m_pending; }
	/**
	 * @brief
	 *  Take the step recorded since the last one, for the compute shader.
	 * @return The step parameters.
	 */
	auto takeStep() -> const ParticleStepParams&;
	/**
	 * @brief
	 *  The emitter's world matrix of the last frame.
	 * @return The matrix.
	 */
	[[nodiscard]] auto getEmitterMatrix() const -> const math::mat4& { return m_emitter; }
	/**
	 * @brief
	 *  The GPU resources of the system, made by `RendererParticles`.
	 * @return The state, null before the first draw.
	 */
	[[nodiscard]] auto getGpuState() const -> const shared<ParticleGpuState>& { return m_gpu; }
	/**
	 * @brief
	 *  Hand the GPU resources to the system.
	 * @param[in] iState The state.
	 */
	void setGpuState(const shared<ParticleGpuState>& iState) { m_gpu = iState; }

private:
	/**
	 * @brief
	 *  Size the slots for an emitter; a new size kills every particle.
	 * @param[in] iSettings The emitter.
	 */
	void resize(const ParticleEmitterSettings& iSettings);
	/**
	 * @brief
	 *  Count the particles to spawn in a frame.
	 * @param[in] iSettings The emitter.
	 * @param[in] iSeconds Duration of the frame.
	 * @return The count.
	 */
	auto countSpawns(const ParticleEmitterSettings& iSettings, float iSeconds) -> uint32_t;
	/**
	 * @brief
	 *  Fill the step of a frame.
	 * @param[in] iSettings The emitter.
	 * @param[in] iSeconds Duration of the frame.
	 * @param[in] iEmitter World matrix of the emitter.
	 * @param[in] iSpawns Particles to spawn.
	 */
	void recordStep(const ParticleEmitterSettings& iSettings, float iSeconds, const math::mat4& iEmitter,
					uint32_t iSpawns);

	/// Where the particles are simulated.
	Path m_path;
	/// Whether the system runs.
	bool m_playing = false;
	/// Whether the emission runs.
	bool m_emitting = false;
	/// Whether the next step starts from no particle (a restart, a new capacity).
	bool m_reset = true;
	/// Whether the burst of the emission start is due.
	bool m_burstDue = true;
	/// Whether a step waits for the GPU.
	bool m_pending = false;
	/// Whether the emitter matrix of the previous frame is known.
	bool m_hasEmitter = false;
	/// Number of slots.
	uint32_t m_capacity = 0;
	/// Points per trail.
	uint32_t m_trailLength = 0;
	/// Next slot to spawn in.
	uint32_t m_head = 0;
	/// Serial number of the next spawn (feeds the random draws).
	uint32_t m_serial = 0;
	/// Particles asked by `emit`.
	uint32_t m_requested = 0;
	/// Particles spawned since the restart.
	uint64_t m_spawned = 0;
	/// Fraction of a particle carried over to the next frame.
	float m_emitCarry = 0.f;
	/// Seconds since the emission started (or looped).
	float m_emitTime = 0.f;
	/// Origin shift waiting for the next step.
	math::vec3 m_shift{0.f, 0.f, 0.f};
	/// World matrix of the emitter, last frame.
	math::mat4 m_emitter = math::identity<float, 4>();
	/// The step recorded for the GPU, or run on the CPU.
	ParticleStepParams m_step;
	/// Particle slots (CPU path).
	std::vector<GpuParticle> m_particles;
	/// Trail points (CPU path).
	std::vector<math::vec4> m_trails;
	/// GPU resources (GPU path).
	shared<ParticleGpuState> m_gpu;
};

}// namespace owl::renderer
