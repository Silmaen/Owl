#include "testHelper.h"

#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneBody.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

using namespace owl::core;
using namespace owl::physics;
using namespace owl::scene;

TEST(PhysicCommand, Creation) {
	Log::init(Log::Level::Off);
	Scene scene;
	// create a scene with physic

	auto b1 = scene.createEntity("body1");
	auto& phy1 = b1.addComponent<component::PhysicBody>();
	phy1.body.type = SceneBody::BodyType::Dynamic;
	phy1.body.restitution = 1.0f;
	auto& [transform1] = b1.getComponent<component::Transform>();
	transform1.translation().x() = 15.0f;

	auto b2 = scene.createEntity("body2");
	auto& phy2 = b2.addComponent<component::PhysicBody>();
	phy2.body.type = SceneBody::BodyType::Static;
	phy2.body.restitution = 1.0f;
	auto& [transform2] = b2.getComponent<component::Transform>();
	transform2.translation().x() = -15.0f;

	auto b3 = scene.createEntity("body3");
	auto& phy3 = b3.addComponent<component::PhysicBody>();
	phy3.body.type = SceneBody::BodyType::Kinematic;
	auto& [transform3] = b3.getComponent<component::Transform>();
	transform3.translation().x() = 0.0f;

	Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(100));
	EXPECT_FALSE(PhysicCommand::isInitialized());
	PhysicCommand::frame(ts);
	PhysicCommand::init(nullptr);
	EXPECT_FALSE(PhysicCommand::isInitialized());
	PhysicCommand::init(&scene);
	EXPECT_TRUE(PhysicCommand::isInitialized());
	PhysicCommand::init(&scene);
	EXPECT_TRUE(PhysicCommand::isInitialized());
	PhysicCommand::frame(ts);
	// test the new positions...
	// 100 ms = 6 fixed steps of 1/60 s; the transform shows the blend between steps 5 and 6.
	EXPECT_EQ(PhysicCommand::getLastFrameStepCount(), 6u);
	EXPECT_NEAR(transform1.translation().y(), -0.0357656f, 1e-5f);
	EXPECT_EQ(transform2.translation().y(), 0.0f);
	EXPECT_EQ(transform3.translation().y(), 0.0f);

	PhysicCommand::destroy();
	EXPECT_FALSE(PhysicCommand::isInitialized());
	Log::invalidate();
}


TEST(PhysicCommand, badImpulse) {
	Log::init(Log::Level::Off);
	// Single body scene...
	Scene scene;
	auto b1 = scene.createEntity("body1");
	// uninitialized.
	PhysicCommand::impulse(b1, {0, 15});
	EXPECT_EQ(PhysicCommand::getVelocity(b1), owl::math::vec2f(0, 0));

	// initialization
	PhysicCommand::init(&scene);

	// void entity
	PhysicCommand::impulse({}, {0, 15});
	EXPECT_EQ(PhysicCommand::getVelocity({}), owl::math::vec2f(0, 0));

	// entity without physics
	PhysicCommand::impulse(b1, {0, 15});
	EXPECT_EQ(PhysicCommand::getVelocity(b1), owl::math::vec2f(0, 0));

	{
		auto& [body] = b1.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Static;
		body.restitution = 1.0f;
		auto& [transform1] = b1.getComponent<component::Transform>();
		transform1.translation().x() = 15.0f;
	}

	// static body!
	PhysicCommand::impulse(b1, {0, 15});
	EXPECT_EQ(PhysicCommand::getVelocity(b1), owl::math::vec2f(0, 0));

	PhysicCommand::destroy();
	Log::invalidate();
}


TEST(PhysicCommand, Impulse) {
	Log::init(Log::Level::Off);
	// Single body scene...
	Scene scene;
	auto b1 = scene.createEntity("body1");
	{
		auto& [body] = b1.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Dynamic;
		body.restitution = 1.0f;
		auto& [transform1] = b1.getComponent<component::Transform>();
		transform1.translation().x() = 15.0f;
	}

	PhysicCommand::init(&scene);
	PhysicCommand::impulse(b1, {0, 15});
	Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(100));
	PhysicCommand::frame(ts);
	EXPECT_NEAR(PhysicCommand::getVelocity(b1).y(), 14.019001, 0.001);

	PhysicCommand::destroy();
	Log::invalidate();
}

// A dynamic body with `gravityScale = 0` must not accumulate any velocity from world gravity.
// This is the API the top-down `world_player.lua` uses to opt out of gravity entirely.
TEST(PhysicCommand, GravityScaleZeroCancelsFalling) {
	Log::init(Log::Level::Off);
	Scene scene;
	auto b1 = scene.createEntity("body1");
	{
		auto& [body] = b1.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Dynamic;
	}
	PhysicCommand::init(&scene);
	PhysicCommand::setGravityScale(b1, 0.f);
	Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(500));
	for (int i = 0; i < 10; ++i) PhysicCommand::frame(ts);
	// Vertical velocity must stay at 0 — Box2D world gravity has no effect on this body.
	EXPECT_NEAR(PhysicCommand::getVelocity(b1).y(), 0.0, 1e-3);
	PhysicCommand::destroy();
	Log::invalidate();
}

// `setGravityScale` must be a no-op (no crash) for the cases we silently swallow elsewhere:
// uninitialized engine, null entity, no physic body, or a static body. Dynamic bodies
// must be registered in the physics world (i.e. created BEFORE `init`) before we can
// dereference their `bodyId` — that path is covered by `GravityScaleZeroCancelsFalling`.
TEST(PhysicCommand, GravityScaleNoOpEdgeCases) {
	Log::init(Log::Level::Off);
	Scene scene;
	auto noBody = scene.createEntity("nobody");
	auto staticBody = scene.createEntity("static");
	{
		auto& [body] = staticBody.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Static;
	}

	// Uninitialized: warns but does not crash.
	PhysicCommand::setGravityScale(noBody, 0.f);
	PhysicCommand::init(&scene);
	// Null entity.
	PhysicCommand::setGravityScale({}, 0.f);
	// Entity without PhysicBody.
	PhysicCommand::setGravityScale(noBody, 0.f);
	// Static body — silently ignored (only Dynamic is meaningful).
	PhysicCommand::setGravityScale(staticBody, 0.f);
	PhysicCommand::destroy();
	Log::invalidate();
}

// All "command" entry points must safely no-op before init / on null entity /
// on entity without a PhysicBody. Covers the symmetrical guards in
// setVelocity / setTransform.
TEST(PhysicCommand, SetVelocityAndTransformGuards) {
	Log::init(Log::Level::Off);
	Scene scene;
	auto noBody = scene.createEntity("nobody");
	auto staticBody = scene.createEntity("static");
	{
		auto& [body] = staticBody.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Static;
	}

	// Uninitialized: ignored, no crash.
	PhysicCommand::setVelocity(noBody, {1.f, 1.f});
	PhysicCommand::setTransform(noBody, {0.f, 0.f}, 0.f);

	PhysicCommand::init(&scene);
	// Null entity.
	PhysicCommand::setVelocity({}, {1.f, 1.f});
	PhysicCommand::setTransform({}, {0.f, 0.f}, 0.f);
	// Entity without PhysicBody.
	PhysicCommand::setVelocity(noBody, {1.f, 1.f});
	PhysicCommand::setTransform(noBody, {0.f, 0.f}, 0.f);
	// setVelocity on Static body: no-op (only Dynamic/Kinematic move).
	PhysicCommand::setVelocity(staticBody, {1.f, 1.f});
	EXPECT_EQ(PhysicCommand::getVelocity(staticBody), owl::math::vec2f(0, 0));
	// setTransform on Static body still teleports the body (no type filter).
	PhysicCommand::setTransform(staticBody, {5.f, 6.f}, 0.f);
	PhysicCommand::destroy();
	Log::invalidate();
}

TEST(PhysicCommand, SetVelocityOnDynamicBodyApplied) {
	Log::init(Log::Level::Off);
	Scene scene;
	auto b1 = scene.createEntity("body1");
	{
		auto& [body] = b1.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Dynamic;
	}
	PhysicCommand::init(&scene);
	PhysicCommand::setVelocity(b1, {3.f, -2.f});
	const auto vel = PhysicCommand::getVelocity(b1);
	EXPECT_NEAR(vel.x(), 3.f, 0.001f);
	EXPECT_NEAR(vel.y(), -2.f, 0.001f);
	PhysicCommand::destroy();
	Log::invalidate();
}

// getSnapshot/applySnapshot must round-trip linear/angular velocity for dynamic
// bodies, and silently produce a zero-snapshot for static / missing / null cases.
TEST(PhysicCommand, SnapshotRoundTrip) {
	Log::init(Log::Level::Off);
	Scene scene;
	auto noBody = scene.createEntity("nobody");
	auto staticBody = scene.createEntity("static");
	{
		auto& [body] = staticBody.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Static;
	}
	auto dyn = scene.createEntity("dyn");
	{
		auto& [body] = dyn.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Dynamic;
	}

	// Before init: default snapshot (awake was only false when an earlier test left a dangling world).
	ASSERT_FALSE(PhysicCommand::isInitialized());
	auto snap0 = PhysicCommand::getSnapshot(dyn);
	EXPECT_EQ(snap0.linearVelocity, owl::math::vec2f(0, 0));
	EXPECT_FLOAT_EQ(snap0.angularVelocity, 0.f);
	EXPECT_TRUE(snap0.awake);

	PhysicCommand::init(&scene);

	// Static body: zero snapshot, applySnapshot is no-op.
	auto snapStatic = PhysicCommand::getSnapshot(staticBody);
	EXPECT_EQ(snapStatic.linearVelocity, owl::math::vec2f(0, 0));
	PhysicCommand::applySnapshot(staticBody, snapStatic);

	// No-body / null entity: zero snapshot.
	auto snapNoBody = PhysicCommand::getSnapshot(noBody);
	EXPECT_EQ(snapNoBody.linearVelocity, owl::math::vec2f(0, 0));
	PhysicCommand::applySnapshot(noBody, snapNoBody);
	auto snapNull = PhysicCommand::getSnapshot({});
	EXPECT_EQ(snapNull.linearVelocity, owl::math::vec2f(0, 0));
	PhysicCommand::applySnapshot({}, snapNull);

	// Dynamic body: round-trip a velocity.
	PhysicCommand::setVelocity(dyn, {4.f, 5.f});
	auto snap = PhysicCommand::getSnapshot(dyn);
	EXPECT_NEAR(snap.linearVelocity.x(), 4.f, 0.001f);
	EXPECT_NEAR(snap.linearVelocity.y(), 5.f, 0.001f);

	PhysicCommand::setVelocity(dyn, {0.f, 0.f});
	PhysicCommand::applySnapshot(dyn, snap);
	const auto vel = PhysicCommand::getVelocity(dyn);
	EXPECT_NEAR(vel.x(), 4.f, 0.001f);
	EXPECT_NEAR(vel.y(), 5.f, 0.001f);
	PhysicCommand::destroy();
	Log::invalidate();
}

// getVelocity guards: uninitialized + null entity + no body + static body all
// return zero without crashing.
TEST(PhysicCommand, GetVelocityGuards) {
	Log::init(Log::Level::Off);
	Scene scene;
	auto noBody = scene.createEntity("nobody");
	// Uninitialized.
	EXPECT_EQ(PhysicCommand::getVelocity(noBody), owl::math::vec2f(0, 0));

	PhysicCommand::init(&scene);
	// Null entity.
	EXPECT_EQ(PhysicCommand::getVelocity({}), owl::math::vec2f(0, 0));
	// Entity without PhysicBody.
	EXPECT_EQ(PhysicCommand::getVelocity(noBody), owl::math::vec2f(0, 0));
	PhysicCommand::destroy();
	Log::invalidate();
}

// frame() must update child entity transforms relative to a moving parent
// — this exercises the hierarchy branch that converts world-space Box2D
// positions back into local space.
TEST(PhysicCommand, FrameUpdatesChildBodiesInLocalSpace) {
	Log::init(Log::Level::Off);
	Scene scene;
	auto parent = scene.createEntity("parent");
	auto child = scene.createEntity("child");
	scene.setParent(child, parent);
	{
		auto& [body] = parent.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Static;
		auto& [t] = parent.getComponent<component::Transform>();
		t.translation().x() = 10.f;
	}
	{
		auto& [body] = child.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Dynamic;
	}

	PhysicCommand::init(&scene);
	Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(100));
	PhysicCommand::frame(ts);

	// Child is parented to parent at (10,0). Its world-pos is around (10, gravity*dt^2/2)
	// but the LOCAL translation must reflect "world − parent" so the local x stays near 0.
	const auto& [childTransform] = child.getComponent<component::Transform>();
	EXPECT_NEAR(childTransform.translation().x(), 0.f, 0.5f);

	PhysicCommand::destroy();
	Log::invalidate();
}

// `frame()` before init must warn and return without crashing.
TEST(PhysicCommand, FrameBeforeInitIsNoOp) {
	Log::init(Log::Level::Off);
	Scene scene;
	scene.createEntity("e1");
	Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(16));
	PhysicCommand::frame(ts);// must not crash, must not init.
	EXPECT_FALSE(PhysicCommand::isInitialized());
	Log::invalidate();
}

// Destroying the bound scene releases the static world, so no later test sees a dangling scene (audit F-05).
TEST(PhysicCommand, SceneDestructionReleasesWorld) {
	Log::init(Log::Level::Off);
	{
		Scene scene;
		auto b1 = scene.createEntity("body1");
		b1.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
		PhysicCommand::init(&scene);
		EXPECT_TRUE(PhysicCommand::isInitialized());
	}
	EXPECT_FALSE(PhysicCommand::isInitialized());
	Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(16));
	PhysicCommand::frame(ts);
	EXPECT_FALSE(PhysicCommand::isInitialized());
	Log::invalidate();
}

// Releasing a scene that is not the bound one keeps the world alive; destroy() is idempotent.
TEST(PhysicCommand, ReleaseSceneIgnoresUnboundScene) {
	Log::init(Log::Level::Off);
	Scene bound;
	PhysicCommand::init(&bound);
	{
		const Scene other;
		PhysicCommand::releaseScene(&other);
	}
	PhysicCommand::releaseScene(nullptr);
	EXPECT_TRUE(PhysicCommand::isInitialized());
	PhysicCommand::releaseScene(&bound);
	EXPECT_FALSE(PhysicCommand::isInitialized());
	PhysicCommand::destroy();
	EXPECT_FALSE(PhysicCommand::isInitialized());
	Log::invalidate();
}

namespace {
auto makeFallingPair(Scene& ioScene) -> std::pair<Entity, Entity> {
	auto ground = ioScene.createEntity("ground");
	ground.getComponent<component::Transform>().transform.scale() = {10.f, 1.f, 1.f};
	ground.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;
	auto box = ioScene.createEntity("box");
	box.getComponent<component::Transform>().transform.translation() = {0.f, 1.5f, 0.f};
	box.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
	return {ground, box};
}
}// namespace

TEST(PhysicCommand, CollisionReportedOncePerPair) {
	Log::init(Log::Level::Off);
	Scene scene;
	const auto [ground, box] = makeFallingPair(scene);
	EXPECT_TRUE(PhysicCommand::takeCollisionEvents().empty());
	PhysicCommand::init(&scene);
	Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(16));
	std::vector<PhysicCommand::CollisionEvent> events;
	for (int i = 0; i < 120; ++i) {
		PhysicCommand::frame(ts);
		for (const auto& event: PhysicCommand::takeCollisionEvents()) events.push_back(event);
	}
	ASSERT_EQ(events.size(), 1u);
	const std::set<uint64_t> pair{events.front().entityA, events.front().entityB};
	EXPECT_EQ(pair, (std::set<uint64_t>{ground.getUUID(), box.getUUID()}));
	EXPECT_TRUE(PhysicCommand::takeCollisionEvents().empty());
	PhysicCommand::destroy();
	Log::invalidate();
}

TEST(PhysicCommand, DestroyedBodyStopsReportingCollisions) {
	Log::init(Log::Level::Off);
	Scene scene;
	const auto [ground, box] = makeFallingPair(scene);
	PhysicCommand::init(&scene);
	Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(16));
	for (int i = 0; i < 60; ++i) PhysicCommand::frame(ts);
	EXPECT_EQ(PhysicCommand::takeCollisionEvents().size(), 1u);
	PhysicCommand::destroyBody(ground);
	for (int i = 0; i < 60; ++i) PhysicCommand::frame(ts);
	EXPECT_TRUE(PhysicCommand::takeCollisionEvents().empty());
	EXPECT_LT(box.getComponent<component::Transform>().transform.translation().y(), 0.f);
	PhysicCommand::destroy();
	Log::invalidate();
}
