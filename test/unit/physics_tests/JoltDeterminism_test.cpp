/**
 * @file JoltDeterminism_test.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Core.h>
#include <core/external/jolt.h>

#include <bit>
#include <cstdint>
#include <format>
#include <vector>

using namespace owl;

namespace {

// Final state of the reference scene, the same value on every target, instruction set and worker count.
constexpr uint64_t g_GoldenHash = 0x36d15cc0c444dc80ull;

constexpr JPH::ObjectLayer g_StaticLayer = 0;
constexpr JPH::ObjectLayer g_MovingLayer = 1;
constexpr uint32_t g_StepCount = 300;
constexpr float g_TimeStep = 1.0f / 60.0f;

class Fnv1a final {
public:
	void add(const float iValue) {
		auto bits = std::bit_cast<uint32_t>(iValue);
		for (uint32_t byte = 0; byte < 4; ++byte, bits >>= 8u) {
			m_hash ^= bits & 0xffu;
			m_hash *= 0x100000001b3ull;
		}
	}

	void add(const JPH::Vec3 iValue) {
		add(iValue.GetX());
		add(iValue.GetY());
		add(iValue.GetZ());
	}

	void add(const JPH::Quat iValue) {
		add(iValue.GetX());
		add(iValue.GetY());
		add(iValue.GetZ());
		add(iValue.GetW());
	}

	[[nodiscard]] auto get() const -> uint64_t { return m_hash; }

private:
	uint64_t m_hash = 0xcbf29ce484222325ull;
};

// A box pyramid hit by spheres and capsules thrown with spin, all on the moving layer.
class ReferenceScene final {
public:
	ReferenceScene(const ReferenceScene&) = delete;

	ReferenceScene(ReferenceScene&&) = delete;

	auto operator=(const ReferenceScene&) -> ReferenceScene& = delete;

	auto operator=(ReferenceScene&&) -> ReferenceScene& = delete;

	ReferenceScene() {
		m_objectFilter.EnableCollision(g_StaticLayer, g_MovingLayer);
		m_objectFilter.EnableCollision(g_MovingLayer, g_MovingLayer);
		m_broadPhaseLayers.MapObjectToBroadPhaseLayer(g_StaticLayer, JPH::BroadPhaseLayer(0));
		m_broadPhaseLayers.MapObjectToBroadPhaseLayer(g_MovingLayer, JPH::BroadPhaseLayer(1));
		m_system.Init(1024, 0, 4096, 4096, m_broadPhaseLayers, m_broadPhaseFilter, m_objectFilter);
		auto& bodies = m_system.GetBodyInterface();
		bodies.CreateAndAddBody(JPH::BodyCreationSettings(new JPH::BoxShape(JPH::Vec3(40.0f, 1.0f, 40.0f)),
														  JPH::RVec3(0.0f, -1.0f, 0.0f), JPH::Quat::sIdentity(),
														  JPH::EMotionType::Static, g_StaticLayer),
								JPH::EActivation::DontActivate);
		const JPH::RefConst<JPH::Shape> box = new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
		constexpr int levels = 8;
		for (int level = 0; level < levels; ++level) {
			for (int x = level; x < levels; ++x) {
				for (int z = level; z < levels; ++z) {
					const auto px = static_cast<float>(2 * x - levels - level) * 0.51f;
					const auto pz = static_cast<float>(2 * z - levels - level) * 0.51f;
					addMoving(JPH::BodyCreationSettings(box, JPH::RVec3(px, 0.5f + static_cast<float>(level), pz),
														JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
														g_MovingLayer));
				}
			}
		}
		const JPH::RefConst<JPH::Shape> sphere = new JPH::SphereShape(0.6f);
		const JPH::RefConst<JPH::Shape> capsule = new JPH::CapsuleShape(0.5f, 0.3f);
		for (int i = 0; i < 24; ++i) {
			const auto angle = static_cast<float>(i) * 0.2617994f;
			JPH::BodyCreationSettings settings(
					i % 2 == 0 ? sphere : capsule, JPH::RVec3(12.0f, 3.0f + static_cast<float>(i % 4), 0.0f),
					JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), angle), JPH::EMotionType::Dynamic, g_MovingLayer);
			settings.mLinearVelocity = JPH::Vec3(-14.0f, 2.0f, static_cast<float>(i % 5) - 2.0f);
			settings.mAngularVelocity = JPH::Vec3(angle, 3.0f, -angle);
			addMoving(settings);
		}
		m_system.OptimizeBroadPhase();
	}

	~ReferenceScene() {
		auto& bodies = m_system.GetBodyInterface();
		bodies.RemoveBodies(m_moving.data(), static_cast<int>(m_moving.size()));
		bodies.DestroyBodies(m_moving.data(), static_cast<int>(m_moving.size()));
	}

	auto run(JPH::JobSystem& ioJobs) -> uint64_t {
		JPH::TempAllocatorImpl temp(16u * 1024u * 1024u);
		for (uint32_t step = 0; step < g_StepCount; ++step)
			EXPECT_EQ(m_system.Update(g_TimeStep, 1, &temp, &ioJobs), JPH::EPhysicsUpdateError::None);
		const auto& bodies = m_system.GetBodyInterfaceNoLock();
		Fnv1a hash;
		for (const auto id: m_moving) {
			hash.add(JPH::Vec3(bodies.GetPosition(id)));
			hash.add(bodies.GetRotation(id));
			hash.add(bodies.GetLinearVelocity(id));
			hash.add(bodies.GetAngularVelocity(id));
		}
		return hash.get();
	}

private:
	void addMoving(const JPH::BodyCreationSettings& iSettings) {
		m_moving.push_back(m_system.GetBodyInterface().CreateAndAddBody(iSettings, JPH::EActivation::Activate));
	}

	/// Broad-phase layer of each object layer.
	JPH::BroadPhaseLayerInterfaceTable m_broadPhaseLayers{2, 2};
	/// Which object layers collide.
	JPH::ObjectLayerPairFilterTable m_objectFilter{2};
	/// Which object layer meets which broad-phase layer, derived from the two tables above.
	JPH::ObjectVsBroadPhaseLayerFilterTable m_broadPhaseFilter{m_broadPhaseLayers, 2, m_objectFilter, 2};
	/// The simulated world.
	JPH::PhysicsSystem m_system;
	/// Dynamic bodies, in creation order, which is the hash order.
	std::vector<JPH::BodyID> m_moving;
};

class JoltDeterminismTest : public testing::Test {
public:
	static void SetUpTestSuite() {
		JPH::RegisterDefaultAllocator();
		s_factory = mkUniq<JPH::Factory>();
		JPH::Factory::sInstance = s_factory.get();
		JPH::RegisterTypes();
	}

	static void TearDownTestSuite() {
		JPH::UnregisterTypes();
		JPH::Factory::sInstance = nullptr;
		s_factory.reset();
	}

protected:
	static auto runWith(const int iThreadCount) -> uint64_t {
		ReferenceScene scene;
		if (iThreadCount == 0) {
			JPH::JobSystemSingleThreaded jobs(JPH::cMaxPhysicsJobs);
			return scene.run(jobs);
		}
		JPH::JobSystemThreadPool jobs(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, iThreadCount);
		return scene.run(jobs);
	}

private:
	/// Jolt's type factory, alive for the whole suite.
	static uniq<JPH::Factory> s_factory;
};

uniq<JPH::Factory> JoltDeterminismTest::s_factory;

}// namespace

TEST_F(JoltDeterminismTest, SameHashRunToRun) { EXPECT_EQ(runWith(0), runWith(0)); }

TEST_F(JoltDeterminismTest, SameHashWhateverTheWorkerCount) {
	const auto reference = runWith(0);
	EXPECT_EQ(runWith(1), reference);
	EXPECT_EQ(runWith(3), reference);
	EXPECT_EQ(runWith(7), reference);
}

TEST_F(JoltDeterminismTest, FrozenHashOnEveryTarget) {
	const auto hash = runWith(0);
	EXPECT_EQ(hash, g_GoldenHash) << std::format("{:#x}", hash);
}
