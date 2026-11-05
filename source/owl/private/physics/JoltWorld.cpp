/**
 * @file JoltWorld.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "JoltWorld.h"
#include "SolverTaskPool.h"
#include "core/external/jolt.h"
#include "core/external/taskflow.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace owl::physics {

namespace {

constexpr uint32_t g_MaxBodies = 65536;
constexpr uint32_t g_MaxBodyPairs = 65536;
constexpr uint32_t g_MaxContactConstraints = 32768;
constexpr uint32_t g_TempAllocatorBytes = 32u * 1024u * 1024u;
// A 2D collider is a slab of unit depth: its mass is the density times its area, as in a plane.
constexpr float g_HalfDepth = 0.5f;
constexpr float g_MinHalfExtent = 0.005f;
constexpr float g_MinDensity = 1e-3f;
constexpr float g_WakeMargin = 0.05f;

// An object layer packs the collision layer of a body (bits 0-2), whether it moves (bit 3) and its mask (bits 4-11).
constexpr uint32_t g_LayerBits = 3;
constexpr JPH::ObjectLayer g_MovingBit = 1u << g_LayerBits;
constexpr uint32_t g_MaskShift = g_LayerBits + 1;

constexpr auto toObjectLayer(const uint8_t iLayer, const bool iMoving, const uint8_t iMask) -> JPH::ObjectLayer {
	return static_cast<JPH::ObjectLayer>((iLayer & (scene::SceneBody::layerCount - 1)) | (iMoving ? g_MovingBit : 0u) |
										 (static_cast<uint32_t>(iMask) << g_MaskShift));
}

constexpr auto layerOf(const JPH::ObjectLayer iLayer) -> uint32_t { return iLayer & (g_MovingBit - 1); }

constexpr auto isMoving(const JPH::ObjectLayer iLayer) -> bool { return (iLayer & g_MovingBit) != 0; }

constexpr auto maskOf(const JPH::ObjectLayer iLayer) -> uint32_t { return (iLayer >> g_MaskShift) & 0xffu; }

constexpr auto hasLayer(const uint32_t iMask, const JPH::ObjectLayer iLayer) -> bool {
	return ((iMask >> layerOf(iLayer)) & 1u) != 0;
}

// Two broad-phase trees: the bodies that never move, and the others.
class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
	[[nodiscard]] auto GetNumBroadPhaseLayers() const -> JPH::uint override { return 2; }

	[[nodiscard]] auto GetBroadPhaseLayer(const JPH::ObjectLayer iLayer) const -> JPH::BroadPhaseLayer override {
		return JPH::BroadPhaseLayer(isMoving(iLayer) ? 1 : 0);
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	[[nodiscard]] auto GetBroadPhaseLayerName(const JPH::BroadPhaseLayer iLayer) const -> const char* override {
		return iLayer.GetValue() == 0 ? "Static" : "Moving";
	}
#endif
};

// A body that never moves only looks for the moving ones.
class BroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
	[[nodiscard]] auto ShouldCollide(const JPH::ObjectLayer iLayer, const JPH::BroadPhaseLayer iTree) const
			-> bool override {
		return isMoving(iLayer) || iTree.GetValue() == 1;
	}
};

// Two bodies collide when one moves and each one's mask holds the other's layer.
class LayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
	[[nodiscard]] auto ShouldCollide(const JPH::ObjectLayer iFirst, const JPH::ObjectLayer iSecond) const
			-> bool override {
		return (isMoving(iFirst) || isMoving(iSecond)) && hasLayer(maskOf(iFirst), iSecond) &&
			   hasLayer(maskOf(iSecond), iFirst);
	}
};

// A query sees the bodies whose layer is in its mask.
class QueryLayerFilter final : public JPH::ObjectLayerFilter {
public:
	explicit QueryLayerFilter(const uint8_t iMask) : m_mask{iMask} {}

	[[nodiscard]] auto ShouldCollide(const JPH::ObjectLayer iLayer) const -> bool override {
		return hasLayer(m_mask, iLayer);
	}

private:
	// Layers looked at.
	uint32_t m_mask;
};

// A query goes through the sensors.
class SolidBodyFilter final : public JPH::BodyFilter {
public:
	[[nodiscard]] auto ShouldCollideLocked(const JPH::Body& iBody) const -> bool override { return !iBody.IsSensor(); }
};

// Jolt's type registry and allocator are process-wide: the first world sets them up, the last one tears them down.
class JoltRuntime final {
public:
	JoltRuntime(const JoltRuntime&) = delete;

	JoltRuntime(JoltRuntime&&) = delete;

	auto operator=(const JoltRuntime&) -> JoltRuntime& = delete;

	auto operator=(JoltRuntime&&) -> JoltRuntime& = delete;

	JoltRuntime() { acquire(); }

	~JoltRuntime() { release(); }

private:
	static void acquire() {
		const std::scoped_lock<std::mutex> lock(s_mutex);
		if (s_users++ > 0 || JPH::Factory::sInstance != nullptr)
			return;
		JPH::RegisterDefaultAllocator();
		s_factory = mkUniq<JPH::Factory>();
		JPH::Factory::sInstance = s_factory.get();
		JPH::RegisterTypes();
	}

	static void release() {
		const std::scoped_lock<std::mutex> lock(s_mutex);
		if (--s_users > 0 || !s_factory)
			return;
		JPH::UnregisterTypes();
		JPH::Factory::sInstance = nullptr;
		s_factory.reset();
	}

	static inline std::mutex s_mutex;
	static inline uint32_t s_users = 0;
	static inline uniq<JPH::Factory> s_factory;
};

// Jolt's jobs run on the world's Taskflow executor; the stepping thread runs jobs too while it waits on a barrier.
class TaskflowJobSystem final : public JPH::JobSystemWithBarrier {
public:
	TaskflowJobSystem(const TaskflowJobSystem&) = delete;

	TaskflowJobSystem(TaskflowJobSystem&&) = delete;

	auto operator=(const TaskflowJobSystem&) -> TaskflowJobSystem& = delete;

	auto operator=(TaskflowJobSystem&&) -> TaskflowJobSystem& = delete;

	explicit TaskflowJobSystem(SolverTaskPool& ioPool)
		: JobSystemWithBarrier(JPH::cMaxPhysicsBarriers), m_executor{ioPool.getExecutor()},
		  m_concurrency{static_cast<int>(ioPool.getWorkerCount()) + 1} {
		m_jobs.Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsJobs);
	}

	// A job run by the stepping thread can leave its Taskflow task behind, still to release it: wait for those tasks.
	~TaskflowJobSystem() override { m_executor.wait_for_all(); }

	[[nodiscard]] auto GetMaxConcurrency() const -> int override { return m_concurrency; }

	auto CreateJob(const char* iName, const JPH::ColorArg iColor, const JobFunction& iFunction,
				   const JPH::uint32 iDependencies) -> JobHandle override {
		uint32_t index = Jobs::cInvalidObjectIndex;
		while (index == Jobs::cInvalidObjectIndex)
			index = m_jobs.ConstructObject(iName, iColor, this, iFunction, iDependencies);
		Job* job = &m_jobs.Get(index);
		JobHandle handle(job);
		if (iDependencies == 0)
			QueueJob(job);
		return handle;
	}

protected:
	void QueueJob(Job* iJob) override {
		iJob->AddRef();
		m_executor.silent_async([iJob]() -> void {
			iJob->Execute();
			iJob->Release();
		});
	}

	void QueueJobs(Job** iJobs, const JPH::uint iCount) override {
		for (JPH::uint i = 0; i < iCount; ++i) QueueJob(iJobs[i]);
	}

	void FreeJob(Job* iJob) override { m_jobs.DestructObject(iJob); }

private:
	using Jobs = JPH::FixedSizeFreeList<Job>;
	// Executor of the world's pool.
	tf::Executor& m_executor;
	// Workers plus the stepping thread.
	int m_concurrency;
	// Storage of the jobs.
	Jobs m_jobs;
};

// Contacts arrive from Jolt's jobs in any order; they are sorted after the step.
class ContactCollector final : public JPH::ContactListener {
public:
	struct Entry {
		JPH::uint32 bodyA;
		JPH::uint32 shapeA;
		JPH::uint32 bodyB;
		JPH::uint32 shapeB;

		auto operator<=>(const Entry&) const = default;
	};

	void OnContactAdded(const JPH::Body& iBodyA, const JPH::Body& iBodyB, const JPH::ContactManifold& iManifold,
						JPH::ContactSettings& /*ioSettings*/) override {
		const std::scoped_lock<std::mutex> lock(m_mutex);
		m_begins.push_back({.bodyA = iBodyA.GetID().GetIndexAndSequenceNumber(),
							.shapeA = iManifold.mSubShapeID1.GetValue(),
							.bodyB = iBodyB.GetID().GetIndexAndSequenceNumber(),
							.shapeB = iManifold.mSubShapeID2.GetValue()});
	}

	void OnContactRemoved(const JPH::SubShapeIDPair& iPair) override {
		const std::scoped_lock<std::mutex> lock(m_mutex);
		m_ends.push_back({.bodyA = iPair.GetBody1ID().GetIndexAndSequenceNumber(),
						  .shapeA = iPair.GetSubShapeID1().GetValue(),
						  .bodyB = iPair.GetBody2ID().GetIndexAndSequenceNumber(),
						  .shapeB = iPair.GetSubShapeID2().GetValue()});
	}

	[[nodiscard]] auto take() -> BodyContacts {
		const auto convert = [](std::vector<Entry>& ioEntries) -> std::vector<BodyContact> {
			std::ranges::sort(ioEntries);
			std::vector<BodyContact> contacts;
			contacts.reserve(ioEntries.size());
			for (const auto& entry: ioEntries)
				contacts.push_back(
						{.bodyA = entry.bodyA, .shapeA = entry.shapeA, .bodyB = entry.bodyB, .shapeB = entry.shapeB});
			ioEntries.clear();
			return contacts;
		};
		return {.begins = convert(m_begins), .ends = convert(m_ends)};
	}

private:
	// Guards the lists against concurrent jobs.
	std::mutex m_mutex;
	// Begun contacts.
	std::vector<Entry> m_begins;
	// Ended contacts.
	std::vector<Entry> m_ends;
};

// Bodies woken up by Jolt's jobs, in any order; sorted when taken.
class ActivationCollector final : public JPH::BodyActivationListener {
public:
	void OnBodyActivated(const JPH::BodyID& iBody, const JPH::uint64 /*iUserData*/) override {
		const std::scoped_lock<std::mutex> lock(m_mutex);
		m_bodies.push_back(iBody.GetIndexAndSequenceNumber());
	}

	void OnBodyDeactivated(const JPH::BodyID& /*iBody*/, const JPH::uint64 /*iUserData*/) override {}

	[[nodiscard]] auto take() -> std::vector<uint32_t> {
		const std::scoped_lock<std::mutex> lock(m_mutex);
		auto bodies = std::exchange(m_bodies, {});
		std::ranges::sort(bodies);
		const auto [first, last] = std::ranges::unique(bodies);
		bodies.erase(first, last);
		return bodies;
	}

private:
	// Guards the list against concurrent jobs.
	std::mutex m_mutex;
	// Woken bodies.
	std::vector<uint32_t> m_bodies;
};

// A Jolt state as plain bytes: written by SaveState, read back by RestoreState.
class ByteRecorder final : public JPH::StateRecorder {
public:
	ByteRecorder() = default;

	explicit ByteRecorder(const std::span<const uint8_t> iBytes) : m_bytes{iBytes.begin(), iBytes.end()} {}

	void WriteBytes(const void* iData, const size_t iSize) override {
		const auto* bytes = static_cast<const uint8_t*>(iData);
		m_bytes.insert(m_bytes.end(), bytes, std::next(bytes, static_cast<std::ptrdiff_t>(iSize)));
	}

	void ReadBytes(void* oData, const size_t iSize) override {
		if (m_failed || iSize > m_bytes.size() - m_read) {
			m_failed = true;
			std::memset(oData, 0, iSize);
			return;
		}
		std::memcpy(oData, std::next(m_bytes.data(), static_cast<std::ptrdiff_t>(m_read)), iSize);
		m_read += iSize;
	}

	[[nodiscard]] auto IsEOF() const -> bool override { return m_read >= m_bytes.size(); }

	[[nodiscard]] auto IsFailed() const -> bool override { return m_failed; }

	[[nodiscard]] auto take() -> std::vector<uint8_t> { return std::move(m_bytes); }

private:
	// The state.
	std::vector<uint8_t> m_bytes;
	// Bytes read so far.
	size_t m_read = 0;
	// A read ran past the end.
	bool m_failed = false;
};

auto toBodyId(const uint32_t iBody) -> JPH::BodyID { return JPH::BodyID(iBody); }

auto toVec3(const math::vec2f& iVector) -> JPH::Vec3 { return {iVector.x(), iVector.y(), 0.f}; }

auto planeRotation(const float iAngle) -> JPH::Quat { return JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), iAngle); }

auto toAngle(const JPH::Quat& iRotation) -> float { return 2.f * JPH::ATan2(iRotation.GetZ(), iRotation.GetW()); }

auto toQuat(const math::vec4f& iRotation) -> JPH::Quat {
	return JPH::Quat(iRotation.x(), iRotation.y(), iRotation.z(), iRotation.w()).Normalized();
}

auto fromQuat(const JPH::Quat& iRotation) -> math::vec4f {
	return {iRotation.GetX(), iRotation.GetY(), iRotation.GetZ(), iRotation.GetW()};
}

auto wrapAngle(const float iAngle) -> float {
	if (iAngle > JPH::JPH_PI)
		return iAngle - 2.f * JPH::JPH_PI;
	if (iAngle < -JPH::JPH_PI)
		return iAngle + 2.f * JPH::JPH_PI;
	return iAngle;
}

auto makeBox(const BodyBox& iBox, const float iDensity) -> JPH::Ref<JPH::BoxShapeSettings> {
	const JPH::Vec3 half{std::max(iBox.halfExtents.x(), g_MinHalfExtent),
						 std::max(iBox.halfExtents.y(), g_MinHalfExtent), g_HalfDepth};
	auto settings = JPH::Ref<JPH::BoxShapeSettings>(
			new JPH::BoxShapeSettings(half, std::min(JPH::cDefaultConvexRadius, half.ReduceMin() * 0.5f)));
	settings->mDensity = iDensity;
	return settings.GetPtr();
}

// Jolt settings are reference counted: the Ref takes what `new` returns.
template<typename T, typename... Args>
auto makeConvex(Args&&... iArgs) -> JPH::Ref<JPH::ConvexShapeSettings> {
	return JPH::Ref<JPH::ConvexShapeSettings>(new T(std::forward<Args>(iArgs)...));
}

// Radius and half height of the cylinder part of a capsule; a capsule no taller than wide is a sphere (0).
auto capsuleOf(const math::vec3f& iSize) -> std::pair<float, float> {
	const float radius = std::max(iSize.x() * 0.5f, g_MinHalfExtent);
	return {radius, std::max(iSize.y() * 0.5f - radius, 0.f)};
}

auto makeMeshShape(const BodyDescription& iDescription) -> JPH::Ref<JPH::ShapeSettings> {
	JPH::VertexList vertices;
	vertices.reserve(iDescription.meshVertices.size());
	for (const auto& vertex: iDescription.meshVertices) vertices.push_back({vertex.x(), vertex.y(), vertex.z()});
	JPH::IndexedTriangleList triangles;
	triangles.reserve(iDescription.meshIndices.size() / 3);
	for (size_t i = 0; i + 2 < iDescription.meshIndices.size(); i += 3)
		triangles.push_back(JPH::IndexedTriangle(iDescription.meshIndices[i], iDescription.meshIndices[i + 1],
												 iDescription.meshIndices[i + 2]));
	auto settings =
			JPH::Ref<JPH::MeshShapeSettings>(new JPH::MeshShapeSettings(std::move(vertices), std::move(triangles)));
	return settings.GetPtr();
}

auto makeHull(const BodyDescription& iDescription) -> JPH::Ref<JPH::ConvexShapeSettings> {
	JPH::Array<JPH::Vec3> points;
	points.reserve(iDescription.meshVertices.size());
	for (const auto& vertex: iDescription.meshVertices) points.push_back({vertex.x(), vertex.y(), vertex.z()});
	return makeConvex<JPH::ConvexHullShapeSettings>(points);
}

auto makeSingleShape(const BodyDescription& iDescription, const float iDensity) -> JPH::Ref<JPH::ShapeSettings> {
	const bool space = iDescription.dimension == scene::SceneBody::Dimension::Space;
	// Jolt has no mass for a triangle mesh: a dynamic body takes the hull of its mesh.
	if (iDescription.shape == scene::SceneBody::Shape::Mesh && iDescription.type != scene::SceneBody::BodyType::Dynamic)
		return makeMeshShape(iDescription);
	JPH::Ref<JPH::ConvexShapeSettings> settings;
	switch (iDescription.shape) {
		case scene::SceneBody::Shape::ConvexHull:
		case scene::SceneBody::Shape::Mesh:
			settings = makeHull(iDescription);
			break;
		case scene::SceneBody::Shape::Box:
			{
				const JPH::Vec3 half{std::max(iDescription.size.x() * 0.5f, g_MinHalfExtent),
									 std::max(iDescription.size.y() * 0.5f, g_MinHalfExtent),
									 space ? std::max(iDescription.size.z() * 0.5f, g_MinHalfExtent) : g_HalfDepth};
				settings = makeConvex<JPH::BoxShapeSettings>(
						half, std::min(JPH::cDefaultConvexRadius, half.ReduceMin() * 0.5f));
				break;
			}
		case scene::SceneBody::Shape::Sphere:
			settings = makeConvex<JPH::SphereShapeSettings>(std::max(iDescription.size.x() * 0.5f, g_MinHalfExtent));
			break;
		case scene::SceneBody::Shape::Capsule:
			{
				const auto [radius, halfHeight] = capsuleOf(iDescription.size);
				if (halfHeight > 0.f)
					settings = makeConvex<JPH::CapsuleShapeSettings>(halfHeight, radius);
				else
					settings = makeConvex<JPH::SphereShapeSettings>(radius);
				break;
			}
		case scene::SceneBody::Shape::Cylinder:
			{
				const float radius = std::max(iDescription.size.x() * 0.5f, g_MinHalfExtent);
				const float halfHeight = std::max(iDescription.size.y() * 0.5f, g_MinHalfExtent);
				settings = makeConvex<JPH::CylinderShapeSettings>(
						halfHeight, radius, std::min(JPH::cDefaultConvexRadius, std::min(radius, halfHeight) * 0.5f));
				break;
			}
	}
	settings->mDensity = iDensity;
	return settings.GetPtr();
}

auto makeShape(const BodyDescription& iDescription) -> JPH::ShapeSettings::ShapeResult {
	const float density = std::max(iDescription.density, g_MinDensity);
	if (iDescription.boxes.empty())
		return makeSingleShape(iDescription, density)->Create();
	if (iDescription.boxes.size() == 1 && iDescription.boxes.front().offset == math::vec2f{0.f, 0.f})
		return makeBox(iDescription.boxes.front(), density)->Create();
	JPH::StaticCompoundShapeSettings compound;
	for (const auto& box: iDescription.boxes)
		compound.AddShape(toVec3(box.offset), JPH::Quat::sIdentity(), makeBox(box, density));
	return compound.Create();
}

// A round 2D body weighs its density times its area, like a box of unit depth, not times its volume.
auto planeMass(const BodyDescription& iDescription) -> std::optional<float> {
	if (iDescription.dimension != scene::SceneBody::Dimension::Plane || !iDescription.boxes.empty() ||
		iDescription.shape == scene::SceneBody::Shape::Box ||
		iDescription.shape == scene::SceneBody::Shape::ConvexHull ||
		iDescription.shape == scene::SceneBody::Shape::Mesh)
		return std::nullopt;
	if (iDescription.shape == scene::SceneBody::Shape::Cylinder)
		return std::max(iDescription.density, g_MinDensity) * std::max(iDescription.size.x(), 2.f * g_MinHalfExtent) *
			   std::max(iDescription.size.y(), 2.f * g_MinHalfExtent);
	const auto [radius, halfHeight] = iDescription.shape == scene::SceneBody::Shape::Capsule
											  ? capsuleOf(iDescription.size)
											  : std::pair{std::max(iDescription.size.x() * 0.5f, g_MinHalfExtent), 0.f};
	return std::max(iDescription.density, g_MinDensity) * (JPH::JPH_PI * radius * radius + 4.f * radius * halfHeight);
}

auto toMotionType(const scene::SceneBody::BodyType iType) -> JPH::EMotionType {
	switch (iType) {
		case scene::SceneBody::BodyType::Static:
			return JPH::EMotionType::Static;
		case scene::SceneBody::BodyType::Dynamic:
			return JPH::EMotionType::Dynamic;
		case scene::SceneBody::BodyType::Kinematic:
			return JPH::EMotionType::Kinematic;
	}
	return JPH::EMotionType::Static;
}

}// namespace

struct JoltWorld::Impl {
	Impl(const Impl&) = delete;

	Impl(Impl&&) = delete;

	auto operator=(const Impl&) -> Impl& = delete;

	auto operator=(Impl&&) -> Impl& = delete;

	explicit Impl(SolverTaskPool* ipPool) {
		system = mkUniq<JPH::PhysicsSystem>();
		system->Init(g_MaxBodies, 0, g_MaxBodyPairs, g_MaxContactConstraints, broadPhaseLayers, broadPhaseFilter,
					 pairFilter);
		system->SetGravity({0.f, -9.81f, 0.f});
		system->SetContactListener(&contacts);
		system->SetBodyActivationListener(&activations);
		if (ipPool != nullptr)
			jobs = mkUniq<TaskflowJobSystem>(*ipPool);
		else
			jobs = mkUniq<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
	}

	~Impl() {
		for (const auto& joint: joints | std::views::values) system->RemoveConstraint(joint);
		auto& bodyInterface = system->GetBodyInterfaceNoLock();
		JPH::BodyIDVector ids;
		system->GetBodies(ids);
		for (const auto id: ids) {
			if (bodyInterface.IsAdded(id))
				bodyInterface.RemoveBody(id);
			bodyInterface.DestroyBody(id);
		}
	}

	[[nodiscard]] auto bodies() const -> JPH::BodyInterface& { return system->GetBodyInterfaceNoLock(); }

	// Jolt registered for the whole life of the world: built first, destroyed last.
	JoltRuntime runtime;
	// Broad-phase tree of each object layer.
	BroadPhaseLayers broadPhaseLayers;
	// Which object layer meets which tree.
	BroadPhaseFilter broadPhaseFilter;
	// Which object layers collide.
	LayerPairFilter pairFilter;
	// Contacts reported during the steps.
	ContactCollector contacts;
	// Bodies woken up since the last step.
	ActivationCollector activations;
	// Scratch memory of a step.
	JPH::TempAllocatorImpl temp{g_TempAllocatorBytes};
	// Jobs of a step.
	uniq<JPH::JobSystem> jobs;
	// The simulated world, destroyed before the jobs and the listener it uses.
	uniq<JPH::PhysicsSystem> system;
	// The broad phase is built once, before the first step, over the bodies of the loaded scene.
	bool isFirstStep = true;
	// Joints by id, removed before the world.
	std::map<uint32_t, JPH::Ref<JPH::TwoBodyConstraint>> joints;
	// Id of the next joint.
	uint32_t nextJoint = 1;
};

JoltWorld::JoltWorld(SolverTaskPool* ipPool) : mp_impl{mkUniq<Impl>(ipPool)} {}

JoltWorld::~JoltWorld() = default;

auto JoltWorld::createBody(const BodyDescription& iDescription) -> std::optional<uint32_t> {
	const auto shape = makeShape(iDescription);
	if (shape.HasError()) {
		OWL_CORE_WARN("Physics: Invalid collider ({}), body not created.", shape.GetError().c_str())
		return std::nullopt;
	}
	const auto motion = toMotionType(iDescription.type);
	const bool space = iDescription.dimension == scene::SceneBody::Dimension::Space;
	const auto& [spacePosition, spaceRotation] = iDescription.spacePose;
	JPH::BodyCreationSettings settings(
			shape.Get(),
			space ? JPH::RVec3(spacePosition.x(), spacePosition.y(), spacePosition.z())
				  : JPH::RVec3(toVec3(iDescription.pose.position)),
			space ? toQuat(spaceRotation) : planeRotation(iDescription.pose.angle), motion,
			toObjectLayer(iDescription.layer, motion != JPH::EMotionType::Static, iDescription.mask));
	if (motion != JPH::EMotionType::Static) {
		constexpr auto planeTranslation = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY;
		if (space)
			settings.mAllowedDOFs = iDescription.fixedRotation ? planeTranslation | JPH::EAllowedDOFs::TranslationZ
															   : JPH::EAllowedDOFs::All;
		else
			settings.mAllowedDOFs = iDescription.fixedRotation ? planeTranslation : JPH::EAllowedDOFs::Plane2D;
	}
	if (const auto mass = planeMass(iDescription); mass) {
		settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		settings.mMassPropertiesOverride.mMass = *mass;
	}
	if (motion == JPH::EMotionType::Dynamic) {
		settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
		// A body sliding over a row of tilemap cells must not catch on the edges between two cells.
		settings.mEnhancedInternalEdgeRemoval = true;
	}
	settings.mIsSensor = iDescription.sensor;
	settings.mFriction = iDescription.friction;
	settings.mRestitution = iDescription.restitution;
	settings.mLinearDamping = 0.f;
	settings.mAngularDamping = 0.f;
	const JPH::BodyID id = mp_impl->bodies().CreateAndAddBody(
			settings, motion == JPH::EMotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
	if (id.IsInvalid()) {
		OWL_CORE_WARN("Physics: World full ({} bodies), body not created.", g_MaxBodies)
		return std::nullopt;
	}
	return id.GetIndexAndSequenceNumber();
}

void JoltWorld::destroyBody(const uint32_t iBody) {
	auto& bodies = mp_impl->bodies();
	const auto id = toBodyId(iBody);
	if (!bodies.IsAdded(id))
		return;
	std::erase_if(mp_impl->joints, [this, id](const auto& iEntry) -> bool {
		if (iEntry.second->GetBody1()->GetID() != id && iEntry.second->GetBody2()->GetID() != id)
			return false;
		mp_impl->system->RemoveConstraint(iEntry.second);
		return true;
	});
	// What rested on the body must fall once it is gone, as it would if its support moved.
	auto bounds = bodies.GetTransformedShape(id).GetWorldSpaceBounds();
	bounds.ExpandBy(JPH::Vec3::sReplicate(g_WakeMargin));
	bodies.ActivateBodiesInAABox(bounds, {}, {});
	bodies.RemoveBody(id);
	bodies.DestroyBody(id);
}

void JoltWorld::step(const float iSeconds, const uint32_t iCollisionSteps) {
	if (std::exchange(mp_impl->isFirstStep, false))
		mp_impl->system->OptimizeBroadPhase();
	const auto error = mp_impl->system->Update(iSeconds, static_cast<int>(std::max(iCollisionSteps, 1u)),
											   &mp_impl->temp, mp_impl->jobs.get());
	if (error != JPH::EPhysicsUpdateError::None)
		OWL_CORE_WARN_ONCE("Physics: Step overflowed the world limits (error {:#x}).", static_cast<uint32_t>(error))
}

auto JoltWorld::saveState() const -> std::vector<uint8_t> {
	ByteRecorder recorder;
	mp_impl->system->SaveState(recorder);
	return recorder.take();
}

auto JoltWorld::restoreState(const std::span<const uint8_t> iState) -> bool {
	ByteRecorder recorder{iState};
	return mp_impl->system->RestoreState(recorder) && !recorder.IsFailed() && recorder.IsEOF();
}

auto JoltWorld::takeContacts() -> BodyContacts { return mp_impl->contacts.take(); }

auto JoltWorld::takeActivatedBodies() -> std::vector<uint32_t> { return mp_impl->activations.take(); }

auto JoltWorld::getPose(const uint32_t iBody) const -> BodyPose {
	JPH::RVec3 position;
	JPH::Quat rotation;
	mp_impl->bodies().GetPositionAndRotation(toBodyId(iBody), position, rotation);
	return {.position = {position.GetX(), position.GetY()}, .angle = wrapAngle(toAngle(rotation))};
}

void JoltWorld::setPose(const uint32_t iBody, const BodyPose& iPose) {
	mp_impl->bodies().SetPositionAndRotation(toBodyId(iBody), JPH::RVec3(toVec3(iPose.position)),
											 planeRotation(iPose.angle), JPH::EActivation::Activate);
}

void JoltWorld::setPositionQuietly(const uint32_t iBody, const math::vec2f& iPosition) {
	auto& bodies = mp_impl->bodies();
	const auto id = toBodyId(iBody);
	const auto z = bodies.GetPosition(id).GetZ();
	bodies.SetPosition(id, JPH::RVec3(iPosition.x(), iPosition.y(), z), JPH::EActivation::DontActivate);
}

auto JoltWorld::getLinearVelocity(const uint32_t iBody) const -> math::vec2f {
	const auto velocity = mp_impl->bodies().GetLinearVelocity(toBodyId(iBody));
	return {velocity.GetX(), velocity.GetY()};
}

void JoltWorld::setLinearVelocity(const uint32_t iBody, const math::vec2f& iVelocity) {
	mp_impl->bodies().SetLinearVelocity(toBodyId(iBody), toVec3(iVelocity));
}

auto JoltWorld::getAngularVelocity(const uint32_t iBody) const -> float {
	return mp_impl->bodies().GetAngularVelocity(toBodyId(iBody)).GetZ();
}

void JoltWorld::setAngularVelocity(const uint32_t iBody, const float iVelocity) {
	mp_impl->bodies().SetAngularVelocity(toBodyId(iBody), {0.f, 0.f, iVelocity});
}

void JoltWorld::addImpulse(const uint32_t iBody, const math::vec2f& iImpulse) {
	mp_impl->bodies().AddImpulse(toBodyId(iBody), toVec3(iImpulse));
}

void JoltWorld::setGravityFactor(const uint32_t iBody, const float iFactor) {
	auto& bodies = mp_impl->bodies();
	bodies.SetGravityFactor(toBodyId(iBody), iFactor);
	bodies.ActivateBody(toBodyId(iBody));
}

auto JoltWorld::isAwake(const uint32_t iBody) const -> bool { return mp_impl->bodies().IsActive(toBodyId(iBody)); }

void JoltWorld::wake(const uint32_t iBody) { mp_impl->bodies().ActivateBody(toBodyId(iBody)); }

auto JoltWorld::getSpacePose(const uint32_t iBody) const -> SpacePose {
	JPH::RVec3 position;
	JPH::Quat rotation;
	mp_impl->bodies().GetPositionAndRotation(toBodyId(iBody), position, rotation);
	return {.position = {position.GetX(), position.GetY(), position.GetZ()}, .rotation = fromQuat(rotation)};
}

void JoltWorld::setSpacePose(const uint32_t iBody, const SpacePose& iPose) {
	mp_impl->bodies().SetPositionAndRotation(toBodyId(iBody),
											 JPH::RVec3(iPose.position.x(), iPose.position.y(), iPose.position.z()),
											 toQuat(iPose.rotation), JPH::EActivation::Activate);
}

auto JoltWorld::getSpaceVelocity(const uint32_t iBody) const -> math::vec3f {
	const auto velocity = mp_impl->bodies().GetLinearVelocity(toBodyId(iBody));
	return {velocity.GetX(), velocity.GetY(), velocity.GetZ()};
}

void JoltWorld::setSpaceVelocity(const uint32_t iBody, const math::vec3f& iVelocity) {
	mp_impl->bodies().SetLinearVelocity(toBodyId(iBody), {iVelocity.x(), iVelocity.y(), iVelocity.z()});
}

void JoltWorld::addSpaceImpulse(const uint32_t iBody, const math::vec3f& iImpulse) {
	mp_impl->bodies().AddImpulse(toBodyId(iBody), {iImpulse.x(), iImpulse.y(), iImpulse.z()});
}

auto JoltWorld::getSpaceAngularVelocity(const uint32_t iBody) const -> math::vec3f {
	const auto velocity = mp_impl->bodies().GetAngularVelocity(toBodyId(iBody));
	return {velocity.GetX(), velocity.GetY(), velocity.GetZ()};
}

void JoltWorld::setSpaceAngularVelocity(const uint32_t iBody, const math::vec3f& iVelocity) {
	mp_impl->bodies().SetAngularVelocity(toBodyId(iBody), {iVelocity.x(), iVelocity.y(), iVelocity.z()});
}

auto JoltWorld::castRay(const math::vec3f& iOrigin, const math::vec3f& iDirection, const float iDistance,
						const uint8_t iMask) const -> std::optional<QueryHit> {
	const JPH::RRayCast ray{JPH::RVec3(iOrigin.x(), iOrigin.y(), iOrigin.z()),
							JPH::Vec3(iDirection.x(), iDirection.y(), iDirection.z()) * iDistance};
	JPH::RayCastResult result;
	const QueryLayerFilter layers{iMask};
	const SolidBodyFilter solid;
	if (!mp_impl->system->GetNarrowPhaseQuery().CastRay(ray, result, {}, layers, solid))
		return std::nullopt;
	const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
	JPH::Vec3 normal = -ray.mDirection.Normalized();
	if (const JPH::BodyLockRead lock(mp_impl->system->GetBodyLockInterfaceNoLock(), result.mBodyID); lock.Succeeded())
		normal = lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point);
	return QueryHit{.body = result.mBodyID.GetIndexAndSequenceNumber(),
					.point = {point.GetX(), point.GetY(), point.GetZ()},
					.normal = {normal.GetX(), normal.GetY(), normal.GetZ()},
					.distance = result.mFraction * iDistance};
}

namespace {

auto makeQueryShape(const QueryVolume& iVolume) -> JPH::RefConst<JPH::Shape> {
	const auto settings =
			iVolume.sphere
					? makeConvex<JPH::SphereShapeSettings>(std::max(iVolume.halfExtents.x(), g_MinHalfExtent))
					: makeConvex<JPH::BoxShapeSettings>(JPH::Vec3(std::max(iVolume.halfExtents.x(), g_MinHalfExtent),
																  std::max(iVolume.halfExtents.y(), g_MinHalfExtent),
																  std::max(iVolume.halfExtents.z(), g_MinHalfExtent)),
														0.f);
	const auto shape = settings->Create();
	return shape.HasError() ? nullptr : shape.Get();
}

}// namespace

auto JoltWorld::castVolume(const QueryVolume& iVolume, const math::vec3f& iOrigin, const math::vec3f& iDirection,
						   const float iDistance, const uint8_t iMask) const -> std::optional<QueryHit> {
	const auto shape = makeQueryShape(iVolume);
	if (shape == nullptr)
		return std::nullopt;
	const JPH::Vec3 motion = JPH::Vec3(iDirection.x(), iDirection.y(), iDirection.z()) * iDistance;
	const JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(
			shape, JPH::Vec3::sReplicate(1.f),
			JPH::RMat44::sRotationTranslation(toQuat(iVolume.rotation), {iOrigin.x(), iOrigin.y(), iOrigin.z()}),
			motion);
	JPH::ShapeCastSettings settings;
	settings.mReturnDeepestPoint = true;
	JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
	const QueryLayerFilter layers{iMask};
	const SolidBodyFilter solid;
	mp_impl->system->GetNarrowPhaseQuery().CastShape(cast, settings, JPH::RVec3::sZero(), collector, {}, layers, solid);
	if (!collector.HadHit())
		return std::nullopt;
	const auto& hit = collector.mHit;
	const JPH::Vec3 normal = hit.mPenetrationAxis.LengthSq() > 0.f
									 ? -hit.mPenetrationAxis.Normalized()
									 : -JPH::Vec3(iDirection.x(), iDirection.y(), iDirection.z());
	return QueryHit{.body = hit.mBodyID2.GetIndexAndSequenceNumber(),
					.point = {hit.mContactPointOn2.GetX(), hit.mContactPointOn2.GetY(), hit.mContactPointOn2.GetZ()},
					.normal = {normal.GetX(), normal.GetY(), normal.GetZ()},
					.distance = hit.mFraction * iDistance};
}

auto JoltWorld::overlap(const QueryVolume& iVolume, const math::vec3f& iCentre, const uint8_t iMask) const
		-> std::vector<uint32_t> {
	const auto shape = makeQueryShape(iVolume);
	if (shape == nullptr)
		return {};
	JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
	const QueryLayerFilter layers{iMask};
	const SolidBodyFilter solid;
	mp_impl->system->GetNarrowPhaseQuery().CollideShape(
			shape, JPH::Vec3::sReplicate(1.f),
			JPH::RMat44::sRotationTranslation(toQuat(iVolume.rotation), {iCentre.x(), iCentre.y(), iCentre.z()}), {},
			JPH::RVec3::sZero(), collector, {}, layers, solid);
	std::vector<uint32_t> found;
	found.reserve(collector.mHits.size());
	for (const auto& hit: collector.mHits) found.push_back(hit.mBodyID2.GetIndexAndSequenceNumber());
	std::ranges::sort(found);
	const auto [first, last] = std::ranges::unique(found);
	found.erase(first, last);
	return found;
}

namespace {

auto toJolt(const math::vec3f& iVector) -> JPH::Vec3 { return {iVector.x(), iVector.y(), iVector.z()}; }

template<typename T>
auto makeSettings() -> JPH::Ref<T> {
	return JPH::Ref<T>(new T);
}

auto motorOf(const JointDescription& iDescription) -> JPH::MotorSettings {
	JPH::MotorSettings motor;
	motor.SetForceLimit(iDescription.motorForce);
	motor.SetTorqueLimit(iDescription.motorForce);
	return motor;
}

auto makeJointSettings(const JointDescription& iDescription) -> JPH::Ref<JPH::TwoBodyConstraintSettings> {
	const JPH::RVec3 point(iDescription.point.x(), iDescription.point.y(), iDescription.point.z());
	const JPH::Vec3 axis = toJolt(iDescription.axis).NormalizedOr(JPH::Vec3::sAxisZ());
	switch (iDescription.type) {
		case scene::SceneJoint::Type::Fixed:
			{
				const auto settings = makeSettings<JPH::FixedConstraintSettings>();
				settings->mAutoDetectPoint = true;
				return settings.GetPtr();
			}
		case scene::SceneJoint::Type::Distance:
			{
				const auto settings = makeSettings<JPH::DistanceConstraintSettings>();
				settings->mPoint1 = JPH::RVec3(iDescription.otherPoint.x(), iDescription.otherPoint.y(),
											   iDescription.otherPoint.z());
				settings->mPoint2 = point;
				if (iDescription.limited) {
					settings->mMinDistance = std::max(iDescription.lower, 0.f);
					settings->mMaxDistance = std::max(iDescription.upper, settings->mMinDistance);
				}
				settings->mLimitsSpringSettings.mFrequency = std::max(iDescription.springFrequency, 0.f);
				settings->mLimitsSpringSettings.mDamping = std::max(iDescription.springDamping, 0.f);
				return settings.GetPtr();
			}
		case scene::SceneJoint::Type::Hinge:
			{
				const auto settings = makeSettings<JPH::HingeConstraintSettings>();
				settings->mPoint1 = settings->mPoint2 = point;
				settings->mHingeAxis1 = settings->mHingeAxis2 = axis;
				settings->mNormalAxis1 = settings->mNormalAxis2 = axis.GetNormalizedPerpendicular();
				if (iDescription.limited) {
					settings->mLimitsMin = std::clamp(iDescription.lower, -JPH::JPH_PI, 0.f);
					settings->mLimitsMax = std::clamp(iDescription.upper, 0.f, JPH::JPH_PI);
				}
				settings->mMotorSettings = motorOf(iDescription);
				return settings.GetPtr();
			}
		case scene::SceneJoint::Type::Slider:
			{
				const auto settings = makeSettings<JPH::SliderConstraintSettings>();
				settings->mPoint1 = settings->mPoint2 = point;
				settings->SetSliderAxis(axis);
				if (iDescription.limited) {
					settings->mLimitsMin = std::min(iDescription.lower, 0.f);
					settings->mLimitsMax = std::max(iDescription.upper, 0.f);
				}
				settings->mMotorSettings = motorOf(iDescription);
				return settings.GetPtr();
			}
	}
	return nullptr;
}

}// namespace

auto JoltWorld::createJoint(const JointDescription& iDescription) -> std::optional<uint32_t> {
	auto& bodies = mp_impl->bodies();
	const JPH::BodyID first = toBodyId(iDescription.body);
	const JPH::BodyID second = iDescription.other ? toBodyId(*iDescription.other) : JPH::BodyID{};
	if (!bodies.IsAdded(first) || (iDescription.other && !bodies.IsAdded(second)) || first == second) {
		OWL_CORE_WARN("Physics: Joint between missing or identical bodies, not created.")
		return std::nullopt;
	}
	const auto settings = makeJointSettings(iDescription);
	// The connected body (or the world) comes first: a motor and the position measure the motion of the joint's own body.
	const JPH::Ref<JPH::TwoBodyConstraint> joint = bodies.CreateConstraint(settings.GetPtr(), second, first);
	if (joint == nullptr) {
		OWL_CORE_WARN("Physics: Joint refused by Jolt, not created.")
		return std::nullopt;
	}
	mp_impl->system->AddConstraint(joint);
	const uint32_t id = mp_impl->nextJoint++;
	mp_impl->joints[id] = joint;
	setJointMotor(id, iDescription.motor, iDescription.motorSpeed);
	bodies.ActivateConstraint(joint);
	return id;
}

void JoltWorld::destroyJoint(const uint32_t iJoint) {
	const auto it = mp_impl->joints.find(iJoint);
	if (it == mp_impl->joints.end())
		return;
	mp_impl->bodies().ActivateConstraint(it->second);
	mp_impl->system->RemoveConstraint(it->second);
	mp_impl->joints.erase(it);
}

void JoltWorld::setJointMotor(const uint32_t iJoint, const bool iEnabled, const float iSpeed) {
	const auto it = mp_impl->joints.find(iJoint);
	if (it == mp_impl->joints.end())
		return;
	const auto state = iEnabled ? JPH::EMotorState::Velocity : JPH::EMotorState::Off;
	auto* joint = it->second.GetPtr();
	if (joint->GetSubType() == JPH::EConstraintSubType::Hinge) {
		auto* hinge = static_cast<JPH::HingeConstraint*>(joint);
		hinge->SetMotorState(state);
		hinge->SetTargetAngularVelocity(iSpeed);
	} else if (joint->GetSubType() == JPH::EConstraintSubType::Slider) {
		auto* slider = static_cast<JPH::SliderConstraint*>(joint);
		slider->SetMotorState(state);
		slider->SetTargetVelocity(iSpeed);
	} else {
		return;
	}
	mp_impl->bodies().ActivateConstraint(it->second);
}

auto JoltWorld::getJointPosition(const uint32_t iJoint) const -> float {
	const auto it = mp_impl->joints.find(iJoint);
	if (it == mp_impl->joints.end())
		return 0.f;
	const auto* joint = it->second.GetPtr();
	if (joint->GetSubType() == JPH::EConstraintSubType::Hinge)
		return static_cast<const JPH::HingeConstraint*>(joint)->GetCurrentAngle();
	if (joint->GetSubType() == JPH::EConstraintSubType::Slider)
		return static_cast<const JPH::SliderConstraint*>(joint)->GetCurrentPosition();
	return 0.f;
}

auto JoltWorld::toRotation(const math::vec3f& iEuler) -> math::vec4f {
	return fromQuat(JPH::Quat::sEulerAngles({iEuler.x(), iEuler.y(), iEuler.z()}));
}

auto JoltWorld::toEuler(const math::vec4f& iRotation) -> math::vec3f {
	const auto angles = toQuat(iRotation).GetEulerAngles();
	return {angles.GetX(), angles.GetY(), angles.GetZ()};
}

auto JoltWorld::toLocalEuler(const math::vec3f& iParentEuler, const math::vec4f& iRotation) -> math::vec3f {
	const auto parent = JPH::Quat::sEulerAngles({iParentEuler.x(), iParentEuler.y(), iParentEuler.z()});
	const auto angles = (parent.Conjugated() * toQuat(iRotation)).Normalized().GetEulerAngles();
	return {angles.GetX(), angles.GetY(), angles.GetZ()};
}

auto JoltWorld::blend(const SpacePose& iFrom, const SpacePose& iTo, const float iAlpha) -> SpacePose {
	const auto from = toQuat(iFrom.rotation);
	auto to = toQuat(iTo.rotation);
	// q and -q are the same rotation: blend towards the one on the short way.
	if (from.Dot(to) < 0.f)
		to = -to;
	return {.position = iFrom.position + (iTo.position - iFrom.position) * iAlpha,
			.rotation = fromQuat(from.LERP(to, iAlpha).Normalized())};
}

auto JoltWorld::carryPrevious(const BodyPose& iPrevious, const BodyPose& iCurrent, const BodyPose& iMoved) -> BodyPose {
	const float turn = iMoved.angle - iCurrent.angle;
	const float cosTurn = JPH::Cos(turn);
	const float sinTurn = JPH::Sin(turn);
	const math::vec2f offset = iPrevious.position - iCurrent.position;
	return {.position = {iMoved.position.x() + cosTurn * offset.x() - sinTurn * offset.y(),
						 iMoved.position.y() + sinTurn * offset.x() + cosTurn * offset.y()},
			.angle = wrapAngle(iPrevious.angle + turn)};
}

}// namespace owl::physics
