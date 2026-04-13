/**
 * @file CoreCoverage_test.cpp
 * @author Silmaen
 * @date 14/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/IFactory.h>
#include <core/Log.h>
#include <debug/LogSink.h>
#include <debug/Tracker.h>
#include <event/AppEvent.h>
#include <scene/SceneCamera.h>

using namespace owl;

// ============================================================================
// Target 1: Factory.cpp — register, unregister, lookup, create, edge cases
// ============================================================================

namespace {

/// Minimal concrete FactoryProduct for testing.
class TestProduct final : public core::FactoryProduct {
public:
	TestProduct() = default;
	[[nodiscard]] auto getPid() const -> core::FactoryPid override { return s_pid; }
	static auto getStaticType() -> std::string { return "TestProduct"; }
	static core::FactoryPid s_pid;
};
core::FactoryPid TestProduct::s_pid = core::INVALID_FACTORY_PID;

/// Second product type to test multiple registrations.
class TestProduct2 final : public core::FactoryProduct {
public:
	TestProduct2() = default;
	[[nodiscard]] auto getPid() const -> core::FactoryPid override { return s_pid; }
	static auto getStaticType() -> std::string { return "TestProduct2"; }
	static core::FactoryPid s_pid;
};
core::FactoryPid TestProduct2::s_pid = core::INVALID_FACTORY_PID;

}// namespace

TEST(CoreCoverage, FactoryRegisterAndLookup) {
	auto& factory = core::IFactory::getInstance();

	// Register TestProduct via the template helper.
	const core::FactoryPid pid = core::factoryRegisterType<TestProduct>();
	TestProduct::s_pid = pid;
	EXPECT_NE(pid, core::INVALID_FACTORY_PID);

	// isRegistered by key.
	EXPECT_TRUE(factory.isRegistered("TestProduct"));
	EXPECT_FALSE(factory.isRegistered("NonExistent"));

	// isRegistered by PID.
	EXPECT_TRUE(factory.isRegistered(pid));
	EXPECT_FALSE(factory.isRegistered(core::INVALID_FACTORY_PID));

	// getKey from PID.
	std::string key;
	EXPECT_TRUE(factory.getKey(pid, key));
	EXPECT_EQ(key, "TestProduct");
	// Invalid PID returns false.
	std::string badKey;
	EXPECT_FALSE(factory.getKey(core::INVALID_FACTORY_PID, badKey));

	// getPid by type_index.
	const core::FactoryPid pidByType = factory.getPid(typeid(TestProduct));
	EXPECT_EQ(pidByType, pid);
	// Unknown type returns INVALID.
	const core::FactoryPid pidUnknown = factory.getPid(typeid(int));
	EXPECT_EQ(pidUnknown, core::INVALID_FACTORY_PID);

	// getPid by key.
	const core::FactoryPid pidByKey = factory.getPid(std::string("TestProduct"));
	EXPECT_EQ(pidByKey, pid);
	const core::FactoryPid pidBadKey = factory.getPid(std::string("NotRegistered"));
	EXPECT_EQ(pidBadKey, core::INVALID_FACTORY_PID);

	// getFactoryPid<T> template.
	EXPECT_EQ(core::getFactoryPid<TestProduct>(), pid);

	// hasFactoryPid.
	EXPECT_TRUE(core::hasFactoryPid(pid));
	EXPECT_FALSE(core::hasFactoryPid(core::INVALID_FACTORY_PID));
}

TEST(CoreCoverage, FactoryCreateProduct) {
	auto& factory = core::IFactory::getInstance();

	// Ensure TestProduct is registered (may already be from previous test).
	if (!factory.isRegistered("TestProduct")) {
		core::factoryRegisterType<TestProduct>();
	}

	// Single creation.
	core::FactoryProduct* product = factory.createProduct("TestProduct");
	ASSERT_NE(product, nullptr);
	EXPECT_NE(product->getPid(), core::INVALID_FACTORY_PID);
	// NOLINTBEGIN(cppcoreguidelines-owning-memory)
	delete product;
	// NOLINTEND(cppcoreguidelines-owning-memory)

	// Creation with unknown key returns nullptr.
	EXPECT_EQ(factory.createProduct("UnknownProduct"), nullptr);

	// Multiple creation.
	std::vector<core::FactoryProduct*> elements;
	const bool ok = factory.createProducts("TestProduct", 3, elements);
	EXPECT_TRUE(ok);
	EXPECT_EQ(elements.size(), 3u);
	// NOLINTBEGIN(cppcoreguidelines-owning-memory)
	for (auto* elem: elements) { delete elem; }
	// NOLINTEND(cppcoreguidelines-owning-memory)

	// Multiple creation with unknown key.
	std::vector<core::FactoryProduct*> badElements;
	EXPECT_FALSE(factory.createProducts("UnknownProduct", 2, badElements));
}

TEST(CoreCoverage, FactoryDuplicateRegistration) {
	// Register TestProduct2.
	const core::FactoryPid pid2 = core::factoryRegisterType<TestProduct2>();
	TestProduct2::s_pid = pid2;
	EXPECT_NE(pid2, core::INVALID_FACTORY_PID);

	// Register it again — should be a no-op (key already present).
	const core::FactoryPid pidAgain = core::factoryRegisterType<TestProduct2>();
	EXPECT_EQ(pidAgain, pid2);
}

TEST(CoreCoverage, FactoryProductAllocatorDefaults) {
	// Default-constructed allocator has null function pointers.
	const core::ProductAllocator alloc;
	EXPECT_EQ(alloc.singleAllocator, nullptr);
	EXPECT_EQ(alloc.multipleAllocator, nullptr);
}

// ============================================================================
// Target 2: Event.h + AppEvent.h — FileDropEvent, EventDispatcher, handled flag
// ============================================================================

TEST(CoreCoverage, FileDropEventBasic) {
	const std::vector<std::filesystem::path> paths = {"/tmp/a.png", "/tmp/b.obj"};
	const event::FileDropEvent event(paths);

	EXPECT_EQ(event.getType(), event::Type::FileDrop);
	EXPECT_EQ(event::FileDropEvent::getStaticType(), event::Type::FileDrop);
	EXPECT_EQ(event.getCategoryFlags(), event::Category::Application);
	EXPECT_STREQ(event.getName().c_str(), "FileDropEvent");
	EXPECT_EQ(event.toString(), "FileDropEvent: 2 file(s)");
	EXPECT_EQ(event.getPaths().size(), 2u);
	EXPECT_EQ(event.getPaths()[0], std::filesystem::path("/tmp/a.png"));

	EXPECT_TRUE(event.isInCategory(event::Category::Application));
	EXPECT_FALSE(event.isInCategory(event::Category::Input));
	EXPECT_FALSE(event.isInCategory(event::Category::Mouse));
	EXPECT_FALSE(event.isInCategory(event::Category::Keyboard));
	EXPECT_FALSE(event.isInCategory(event::Category::MouseButton));
}

TEST(CoreCoverage, FileDropEventEmpty) {
	const std::vector<std::filesystem::path> empty;
	const event::FileDropEvent event(empty);
	EXPECT_EQ(event.getPaths().size(), 0u);
	EXPECT_EQ(event.toString(), "FileDropEvent: 0 file(s)");
}

TEST(CoreCoverage, EventHandledFlagPropagation) {
	event::AppTickEvent tick;
	EXPECT_FALSE(tick.handled);

	// Dispatcher sets handled to true via OR.
	event::EventDispatcher dispatcher(tick);
	dispatcher.dispatch<event::AppTickEvent>([](event::Event&) { return true; });
	EXPECT_TRUE(tick.handled);

	// Once handled, it stays handled even if a new dispatch returns false.
	dispatcher.dispatch<event::AppTickEvent>([](event::Event&) { return false; });
	EXPECT_TRUE(tick.handled);
}

TEST(CoreCoverage, EventDispatcherTypeMismatch) {
	event::AppRenderEvent render;
	event::EventDispatcher dispatcher(render);

	// Dispatch for a different type should return false and not call the callback.
	bool called = false;
	const bool dispatched = dispatcher.dispatch<event::AppTickEvent>([&called](event::Event&) {
		called = true;
		return true;
	});
	EXPECT_FALSE(dispatched);
	EXPECT_FALSE(called);
}

TEST(CoreCoverage, WindowResizeEventGetSize) {
	const event::WindowResizeEvent event({800, 600});
	const auto& size = event.getSize();
	EXPECT_EQ(size.x(), 800u);
	EXPECT_EQ(size.y(), 600u);
}

// ============================================================================
// Target 3: Log.h / Log.cpp — macros at all levels, frame logging, log buffer
// ============================================================================

TEST(CoreCoverage, LogMacrosAllLevels) {
	core::Log::init(core::Log::Level::Trace);

	// Exercise every core macro — this covers the log dispatch paths.
	OWL_CORE_TRACE("trace message {}", 1)
	OWL_CORE_INFO("info message {}", 2)
	OWL_CORE_WARN("warn message {}", 3)
	OWL_CORE_ERROR("error message {}", 4)
	OWL_CORE_CRITICAL("critical message {}", 5)

	// Exercise every client macro.
	OWL_TRACE("client trace {}", 10)
	OWL_INFO("client info {}", 20)
	OWL_WARN("client warn {}", 30)
	OWL_ERROR("client error {}", 40)
	OWL_CRITICAL("client critical {}", 50)

	core::Log::invalidate();
}

TEST(CoreCoverage, LogFrameTraceAndAdvance) {
	core::Log::init(core::Log::Level::Trace, 2);

	// Frame 0: counter=0 mod 2 == 0 => frameLog() is true.
	EXPECT_TRUE(core::Log::frameLog());
	OWL_CORE_FRAME_TRACE("frame 0 trace")

	// Advance to frame 1: counter=1 mod 2 != 0.
	OWL_CORE_FRAME_ADVANCE
	EXPECT_FALSE(core::Log::frameLog());

	// Advance to frame 2: counter=2 mod 2 == 0.
	OWL_CORE_FRAME_ADVANCE
	EXPECT_TRUE(core::Log::frameLog());

	// With frequency 0, frameLog returns false.
	core::Log::setFrameFrequency(0);
	EXPECT_FALSE(core::Log::frameLog());

	core::Log::invalidate();
}

TEST(CoreCoverage, LogVerbosityLevel) {
	core::Log::init(core::Log::Level::Error);
	EXPECT_TRUE(core::Log::initiated());

	// Change verbosity after init.
	core::Log::setVerbosityLevel(core::Log::Level::Warning);

	// Log at various levels — this should not crash even if level is filtered.
	OWL_CORE_TRACE("filtered trace")
	OWL_CORE_INFO("filtered info")
	OWL_CORE_WARN("visible warn")
	OWL_CORE_ERROR("visible error")

	core::Log::invalidate();
	EXPECT_FALSE(core::Log::initiated());
}

TEST(CoreCoverage, LogSetVerbosityBeforeInit) {
	// setVerbosityLevel when loggers are null should not crash.
	core::Log::setVerbosityLevel(core::Log::Level::Off);
	core::Log::setVerbosityLevel(core::Log::Level::Trace);
	// Re-init to confirm normal operation after.
	core::Log::init(core::Log::Level::Off);
	core::Log::invalidate();
}

TEST(CoreCoverage, LogBufferCapture) {
	core::Log::init(core::Log::Level::Trace);

	auto& buffer = core::Log::getLogBuffer();
	buffer.clear();

	OWL_CORE_INFO("buffer test message")

	// Give spdlog a moment to flush through sinks.
	const auto entries = buffer.getEntries();
	// The buffer should have captured at least one entry.
	EXPECT_FALSE(entries.empty());

	bool found = false;
	for (const auto& entry: entries) {
		if (entry.message.find("buffer test message") != std::string::npos) {
			found = true;
			EXPECT_EQ(entry.level, core::Log::Level::Info);
			break;
		}
	}
	EXPECT_TRUE(found);

	buffer.clear();
	EXPECT_TRUE(buffer.getEntries().empty());

	core::Log::invalidate();
}

// ============================================================================
// Target 4: Tracker.cpp — scopes, MemorySize::str(), allocation state
// ============================================================================

TEST(CoreCoverage, MemorySizeStr) {
	// Exercise all branches of MemorySize::str().
	debug::MemorySize ms;

	ms.size = 500;
	EXPECT_EQ(ms.str(), "500 bytes");

	ms.size = 2048;
	const auto kbStr = ms.str();
	EXPECT_TRUE(kbStr.find("kB") != std::string::npos);

	ms.size = 2 * 1024 * 1024;
	const auto mbStr = ms.str();
	EXPECT_TRUE(mbStr.find("MB") != std::string::npos);

	ms.size = static_cast<size_t>(3) * 1024 * 1024 * 1024;
	const auto gbStr = ms.str();
	EXPECT_TRUE(gbStr.find("GB") != std::string::npos);

	ms.size = static_cast<size_t>(2) * 1024 * 1024 * 1024 * 1024;
	const auto tbStr = ms.str();
	EXPECT_TRUE(tbStr.find("TB") != std::string::npos);
}

TEST(CoreCoverage, ScopeUntrackDisablesTracking) {
#ifndef OWL_SANITIZER_CUSTOM_ALLOCATOR
	// Check if the tracker is actually intercepting allocations on this platform.
	// On Windows with shared library builds, operator new/delete overrides may not
	// apply to the test executable's allocations.
	debug::TrackerAPI::checkState();// reset
	auto probe = mkShared<int>(1);
	const auto& probeState = debug::TrackerAPI::checkState();
	static_cast<void>(probe);
	if (probeState.allocationCalls == 0) {
		ASSERT_TRUE(true) << "Tracker not intercepting allocations on this platform";
		return;
	}

	size_t allocsBefore = 0;
	{
		OWL_SCOPE_UNTRACK
		auto ptr = mkShared<int>(42);
		const auto& state = debug::TrackerAPI::checkState();
		allocsBefore = state.allocationCalls;
		EXPECT_EQ(allocsBefore, 0u);
		static_cast<void>(ptr);
	}
	auto ptr2 = mkShared<int>(99);
	const auto& stateAfter = debug::TrackerAPI::checkState();
	EXPECT_GT(stateAfter.allocationCalls, 0u);
	static_cast<void>(ptr2);
#else
	ASSERT_TRUE(true) << "Custom allocator sanitizer active — tracker is not functional";
#endif
}

TEST(CoreCoverage, ScopeTrackReenablesTracking) {
#ifndef OWL_SANITIZER_CUSTOM_ALLOCATOR
	// Check if the tracker is actually intercepting allocations.
	debug::TrackerAPI::checkState();
	auto probe = mkShared<int>(1);
	const auto& probeState = debug::TrackerAPI::checkState();
	static_cast<void>(probe);
	if (probeState.allocationCalls == 0) {
		ASSERT_TRUE(true) << "Tracker not intercepting allocations on this platform";
		return;
	}

	debug::TrackerAPI::checkState();
	{
		OWL_SCOPE_UNTRACK {
			OWL_SCOPE_FORCE_TRACK
			auto ptr = mkShared<int>(7);
			const auto& state = debug::TrackerAPI::checkState();
			EXPECT_GT(state.allocationCalls, 0u);
			static_cast<void>(ptr);
		}
	}
#else
	ASSERT_TRUE(true) << "Custom allocator sanitizer active — tracker is not functional";
#endif
}

TEST(CoreCoverage, TrackerGlobalsNonEmpty) {
#ifndef OWL_SANITIZER_CUSTOM_ALLOCATOR
	const auto& globals = debug::TrackerAPI::globals();
	// After many allocations in the process, globals should be non-zero.
	EXPECT_GT(globals.allocationCalls, 0u);
	EXPECT_GT(globals.allocatedMemory, 0u);
	EXPECT_GT(globals.memoryPeek, 0u);
#else
	ASSERT_TRUE(true) << "Custom allocator sanitizer active — tracker is not functional";
#endif
}

TEST(CoreCoverage, AllocationStateResetState) {
	debug::AllocationState state;
	state.allocationCalls = 10;
	state.deallocationCalls = 5;
	state.allocatedMemory = 1024;
	state.memoryPeek = 2048;

	state.resetState();

	EXPECT_EQ(state.allocationCalls, 0u);
	EXPECT_EQ(state.deallocationCalls, 0u);
	EXPECT_EQ(state.allocatedMemory, 0u);
	EXPECT_EQ(state.memoryPeek, 0u);
	EXPECT_TRUE(state.allocs.empty());
}

TEST(CoreCoverage, AllocationInfoToStrWithoutStacktrace) {
	// In non-stacktrace builds, toStr should still produce a meaningful string.
	int dummy = 0;
	debug::AllocationInfo info(&dummy, 64);
	const auto str = info.toStr(false, false);
	EXPECT_FALSE(str.empty());
	// Should mention the size.
	EXPECT_TRUE(str.find("64") != std::string::npos);
}

// ============================================================================
// Target 5: SceneCamera.cpp — perspective/orthographic switching, setViewportSize
// ============================================================================

TEST(CoreCoverage, SceneCameraDefaultIsOrthographic) {
	scene::SceneCamera cam;
	EXPECT_EQ(cam.getProjectionType(), scene::SceneCamera::ProjectionType::Orthographic);
	EXPECT_FLOAT_EQ(cam.getOrthographicSize(), 10.0f);
	EXPECT_FLOAT_EQ(cam.getOrthographicNearClip(), -1.0f);
	EXPECT_FLOAT_EQ(cam.getOrthographicFarClip(), 1.0f);
}

TEST(CoreCoverage, SceneCameraPerspectiveSettings) {
	scene::SceneCamera cam;
	cam.setViewportSize({800, 600});

	cam.setPerspective(math::radians(60.0f), 0.1f, 500.0f);
	EXPECT_EQ(cam.getProjectionType(), scene::SceneCamera::ProjectionType::Perspective);
	EXPECT_FLOAT_EQ(cam.getPerspectiveVerticalFov(), math::radians(60.0f));
	EXPECT_FLOAT_EQ(cam.getPerspectiveNearClip(), 0.1f);
	EXPECT_FLOAT_EQ(cam.getPerspectiveFarClip(), 500.0f);

	// Projection matrix should not be identity after setting perspective.
	const auto& proj = cam.getProjection();
	EXPECT_NE(proj(0, 0), 1.0f);
}

TEST(CoreCoverage, SceneCameraOrthographicSettings) {
	scene::SceneCamera cam;
	cam.setViewportSize({400, 200});

	cam.setOrthographic(5.0f, -2.0f, 10.0f);
	EXPECT_EQ(cam.getProjectionType(), scene::SceneCamera::ProjectionType::Orthographic);
	EXPECT_FLOAT_EQ(cam.getOrthographicSize(), 5.0f);
	EXPECT_FLOAT_EQ(cam.getOrthographicNearClip(), -2.0f);
	EXPECT_FLOAT_EQ(cam.getOrthographicFarClip(), 10.0f);
}

TEST(CoreCoverage, SceneCameraSwitchProjectionType) {
	scene::SceneCamera cam;
	cam.setViewportSize({640, 480});

	// Switch to perspective.
	cam.setProjectionType(scene::SceneCamera::ProjectionType::Perspective);
	EXPECT_EQ(cam.getProjectionType(), scene::SceneCamera::ProjectionType::Perspective);
	const auto projPersp = cam.getProjection();

	// Switch back to orthographic.
	cam.setProjectionType(scene::SceneCamera::ProjectionType::Orthographic);
	EXPECT_EQ(cam.getProjectionType(), scene::SceneCamera::ProjectionType::Orthographic);
	const auto projOrtho = cam.getProjection();

	// The two projections should differ.
	EXPECT_NE(projPersp(0, 0), projOrtho(0, 0));
}

TEST(CoreCoverage, SceneCameraIndividualSetters) {
	scene::SceneCamera cam;
	cam.setViewportSize({100, 100});

	// Set individual perspective parameters.
	cam.setProjectionType(scene::SceneCamera::ProjectionType::Perspective);
	cam.setPerspectiveVerticalFov(math::radians(90.0f));
	EXPECT_FLOAT_EQ(cam.getPerspectiveVerticalFov(), math::radians(90.0f));
	cam.setPerspectiveNearClip(0.5f);
	EXPECT_FLOAT_EQ(cam.getPerspectiveNearClip(), 0.5f);
	cam.setPerspectiveFarClip(100.0f);
	EXPECT_FLOAT_EQ(cam.getPerspectiveFarClip(), 100.0f);

	// Set individual orthographic parameters.
	cam.setProjectionType(scene::SceneCamera::ProjectionType::Orthographic);
	cam.setOrthographicSize(20.0f);
	EXPECT_FLOAT_EQ(cam.getOrthographicSize(), 20.0f);
	cam.setOrthographicNearClip(-5.0f);
	EXPECT_FLOAT_EQ(cam.getOrthographicNearClip(), -5.0f);
	cam.setOrthographicFarClip(5.0f);
	EXPECT_FLOAT_EQ(cam.getOrthographicFarClip(), 5.0f);
}

TEST(CoreCoverage, SceneCameraCopyAndMove) {
	scene::SceneCamera cam;
	cam.setViewportSize({320, 240});
	cam.setPerspective(math::radians(45.0f), 0.01f, 1000.0f);

	// Copy constructor.
	const scene::SceneCamera camCopy(cam);
	EXPECT_EQ(camCopy.getProjectionType(), scene::SceneCamera::ProjectionType::Perspective);
	EXPECT_FLOAT_EQ(camCopy.getPerspectiveVerticalFov(), math::radians(45.0f));

	// Move constructor.
	scene::SceneCamera camMove(std::move(cam));
	EXPECT_EQ(camMove.getProjectionType(), scene::SceneCamera::ProjectionType::Perspective);
}
