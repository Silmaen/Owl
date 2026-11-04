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
#include <cstdint>
#include <mutex>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace owl::physics {

namespace {

constexpr JPH::ObjectLayer g_StaticLayer = 0;
constexpr JPH::ObjectLayer g_MovingLayer = 1;
constexpr uint32_t g_MaxBodies = 65536;
constexpr uint32_t g_MaxBodyPairs = 65536;
constexpr uint32_t g_MaxContactConstraints = 32768;
constexpr uint32_t g_TempAllocatorBytes = 32u * 1024u * 1024u;
// A 2D collider is a slab of unit depth: its mass is the density times its area, as in a plane.
constexpr float g_HalfDepth = 0.5f;
constexpr float g_MinHalfExtent = 0.005f;
constexpr float g_MinDensity = 1e-3f;
constexpr float g_WakeMargin = 0.05f;

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
			for (const auto& entry: ioEntries) contacts.push_back({.bodyA = entry.bodyA, .bodyB = entry.bodyB});
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

auto makeSingleShape(const BodyDescription& iDescription, const float iDensity) -> JPH::Ref<JPH::ShapeSettings> {
	const bool space = iDescription.dimension == scene::SceneBody::Dimension::Space;
	JPH::Ref<JPH::ConvexShapeSettings> settings;
	switch (iDescription.shape) {
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
		iDescription.shape == scene::SceneBody::Shape::Box)
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
		objectFilter.EnableCollision(g_StaticLayer, g_MovingLayer);
		objectFilter.EnableCollision(g_MovingLayer, g_MovingLayer);
		broadPhaseLayers.MapObjectToBroadPhaseLayer(g_StaticLayer, JPH::BroadPhaseLayer(0));
		broadPhaseLayers.MapObjectToBroadPhaseLayer(g_MovingLayer, JPH::BroadPhaseLayer(1));
		broadPhaseFilter = mkUniq<JPH::ObjectVsBroadPhaseLayerFilterTable>(broadPhaseLayers, 2, objectFilter, 2);
		system = mkUniq<JPH::PhysicsSystem>();
		system->Init(g_MaxBodies, 0, g_MaxBodyPairs, g_MaxContactConstraints, broadPhaseLayers, *broadPhaseFilter,
					 objectFilter);
		system->SetGravity({0.f, -9.81f, 0.f});
		system->SetContactListener(&contacts);
		if (ipPool != nullptr)
			jobs = mkUniq<TaskflowJobSystem>(*ipPool);
		else
			jobs = mkUniq<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
	}

	~Impl() {
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
	// Broad-phase layer of each object layer.
	JPH::BroadPhaseLayerInterfaceTable broadPhaseLayers{2, 2};
	// Which object layers collide.
	JPH::ObjectLayerPairFilterTable objectFilter{2};
	// Which object layer meets which broad-phase layer, built from the two tables once they are filled.
	uniq<JPH::ObjectVsBroadPhaseLayerFilterTable> broadPhaseFilter;
	// Contacts reported during the steps.
	ContactCollector contacts;
	// Scratch memory of a step.
	JPH::TempAllocatorImpl temp{g_TempAllocatorBytes};
	// Jobs of a step.
	uniq<JPH::JobSystem> jobs;
	// The simulated world, destroyed before the jobs and the listener it uses.
	uniq<JPH::PhysicsSystem> system;
	// The broad phase is built once, before the first step, over the bodies of the loaded scene.
	bool isFirstStep = true;
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
	JPH::BodyCreationSettings settings(shape.Get(),
									   space ? JPH::RVec3(spacePosition.x(), spacePosition.y(), spacePosition.z())
											 : JPH::RVec3(toVec3(iDescription.pose.position)),
									   space ? toQuat(spaceRotation) : planeRotation(iDescription.pose.angle), motion,
									   motion == JPH::EMotionType::Static ? g_StaticLayer : g_MovingLayer);
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

auto JoltWorld::takeContacts() -> BodyContacts { return mp_impl->contacts.take(); }

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
