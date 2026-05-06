
#include "testHelper.h"

#include <core/task/ParallelUtils.h>
#include <core/task/Scheduler.h>
#include <core/task/SchedulerImpl.h>

using namespace owl::core;
using namespace owl::core::task;

TEST(core_task, SchedulerBasic) {
	Scheduler scheduler;
	Timestep ts;

	// non existing tasks.
	EXPECT_FALSE(scheduler.isTaskFinished(85));
	EXPECT_FALSE(scheduler.isTaskRunning(85));
	EXPECT_FALSE(scheduler.isTaskInQueue(85));

	// do a frame with empty queue
	ts.forceUpdate(std::chrono::milliseconds(100));
	scheduler.frame(ts);
	std::this_thread::sleep_for(std::chrono::milliseconds(5));//slowdown a little before checking

	// add an empty task
	EXPECT_EQ(scheduler.pushTask(Task([&] -> void {})), 1);
	EXPECT_FALSE(scheduler.isTaskFinished(1));
	EXPECT_FALSE(scheduler.isTaskRunning(1));
	EXPECT_TRUE(scheduler.isTaskInQueue(1));

	// faking a new frame
	ts.forceUpdate(std::chrono::milliseconds(100));
	scheduler.frame(ts);
	std::this_thread::sleep_for(std::chrono::milliseconds(5));//slowdown a little before checking

	EXPECT_FALSE(scheduler.isTaskFinished(1));
	EXPECT_TRUE(scheduler.isTaskRunning(1));
	EXPECT_FALSE(scheduler.isTaskInQueue(1));

	// faking a new frame
	ts.forceUpdate(std::chrono::milliseconds(100));
	scheduler.frame(ts);
	std::this_thread::sleep_for(std::chrono::milliseconds(5));//slowdown a little before checking

	EXPECT_TRUE(scheduler.isTaskFinished(1));
	EXPECT_FALSE(scheduler.isTaskRunning(1));
	EXPECT_FALSE(scheduler.isTaskInQueue(1));
}

TEST(core_task, SchedulerTasks) {
	uint8_t counter = 0;
	{
		Scheduler scheduler;
		Timestep ts;

		scheduler.pushTask(Task([&] -> void {}, [&] -> void { counter++; }));
		ts.forceUpdate(std::chrono::milliseconds(100));
		scheduler.frame(ts);
		std::this_thread::sleep_for(std::chrono::milliseconds(5));//slowdown a little before checking
		EXPECT_EQ(counter, 0);
		ts.forceUpdate(std::chrono::milliseconds(100));
		scheduler.frame(ts);
		std::this_thread::sleep_for(std::chrono::milliseconds(5));//slowdown a little before checking
		EXPECT_EQ(counter, 1);
		scheduler.pushTask(Task([&] -> void {}, [&] -> void { counter++; }));
		scheduler.pushTask(Task([&] -> void {}, [&] -> void { counter++; }));
		scheduler.pushTask(Task([&] -> void {}, [&] -> void { counter++; }));
		EXPECT_TRUE(scheduler.isTaskInQueue(4));
		scheduler.clearQueue();
		EXPECT_EQ(counter, 1);
		EXPECT_FALSE(scheduler.isTaskInQueue(4));
		// this task should be destroyed with the scheduler, with no possibility to run.
		EXPECT_EQ(scheduler.pushTask(Task([&] -> void { counter++; }, [&] -> void { counter++; })), 5);
	}
	EXPECT_EQ(counter, 1);
}


TEST(core_task, SchedulerTimers) {
	Scheduler scheduler;
	Timestep ts;
	std::atomic_uint32_t counter = 0;
	const auto t1 = scheduler.pushTimer({.exec = [&] -> void { ++counter; },
										 .frequency = std::chrono::milliseconds(300),
										 .async = false,
										 .iteration = 0});
	const auto t2 = scheduler.pushTimer({.exec = [&] -> void { ++counter; },
										 .frequency = std::chrono::milliseconds(200),
										 .async = true,
										 .iteration = 0});
	const auto t3 = scheduler.pushTimer({.exec = [&] -> void { ++counter; },
										 .frequency = std::chrono::milliseconds(100),
										 .async = false,
										 .iteration = 3});
	// all must run once at the beginning! -> task 2 not yet executed but a task should have been created
	scheduler.frame(ts);// t1, t2->async, t3(1)
	EXPECT_EQ(counter, 2);
	EXPECT_TRUE(scheduler.isTaskInQueue(1));

	ts.forceUpdate(std::chrono::milliseconds(101));
	scheduler.frame(ts);// a_t2, t3(2)
	std::this_thread::sleep_for(std::chrono::milliseconds(5));//slowdown a little before checking
	EXPECT_EQ(counter, 4);

	t1.lock()->setPaused(true);
	t1.lock()->togglePaused();
	t2.lock()->togglePaused();
	t2.lock()->setPaused(false);
	t3.lock()->pause();
	ts.forceUpdate(std::chrono::milliseconds(101));// total 202
	scheduler.frame(ts);// t2->async
	EXPECT_EQ(counter, 4);
	EXPECT_TRUE(scheduler.isTaskInQueue(2));

	ts.forceUpdate(std::chrono::milliseconds(101));// total 303
	scheduler.frame(ts);// t1, a_t2
	std::this_thread::sleep_for(std::chrono::milliseconds(5));//slowdown a little before checking
	EXPECT_EQ(counter, 6);

	t3.lock()->resume();
	ts.forceUpdate(std::chrono::milliseconds(101));// total 404
	scheduler.frame(ts);// t2->async, t3(3->X)
	EXPECT_EQ(counter, 7);


	scheduler.clearTimers();
	ts.forceUpdate(std::chrono::milliseconds(101));// total 505
	scheduler.frame(ts);// a_t2
	std::this_thread::sleep_for(std::chrono::milliseconds(5));//slowdown a little before checking
	EXPECT_EQ(counter, 8);
}

TEST(core_task, SchedulerConcurrency) {
	Scheduler scheduler;
	Timestep const ts;
	const auto hwThreads = std::thread::hardware_concurrency();
	std::atomic_uint32_t running = 0;
	std::atomic_uint32_t maxConcurrent = 0;
	std::atomic_uint32_t completed = 0;

	// Submit hwThreads tasks that all run simultaneously.
	for (unsigned i = 0; i < hwThreads; ++i) {
		scheduler.pushTask(Task(
				[&] -> void {
					++running;
					// Record peak concurrency.
					const auto cur = running.load();
					auto prev = maxConcurrent.load();
					while (cur > prev && !maxConcurrent.compare_exchange_weak(prev, cur)) {}
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
					--running;
				},
				[&] -> void { ++completed; }));
	}
	scheduler.waitEmptyQueue();
	EXPECT_EQ(completed, hwThreads);
	// Should achieve more concurrency than the old hardcoded limit of 5.
	if (hwThreads > 5) {
		EXPECT_GT(maxConcurrent, 5u);
	}
}

TEST(core_task, SchedulerStress) {
	Scheduler scheduler;
	constexpr size_t taskCount = 100;
	std::atomic_uint32_t counter = 0;

	for (size_t i = 0; i < taskCount; ++i) {
		scheduler.pushTask(Task([&] -> void { ++counter; }));
	}
	scheduler.waitEmptyQueue();
	EXPECT_EQ(counter, taskCount);
}

TEST(core_task, ParallelForEach) {
	tf::Executor executor{4};
	constexpr size_t count = 1000;
	std::vector<std::atomic_int> values(count);
	for (auto& v: values) { v.store(0); }

	std::vector<size_t> indices(count);
	std::iota(indices.begin(), indices.end(), 0);

	std::function<void(size_t)> const func = [&](const size_t iIdx) -> void { values[iIdx].store(1); };
	parallelForEach(executor, indices.begin(), indices.end(), func);

	for (size_t i = 0; i < count; ++i) { EXPECT_EQ(values[i].load(), 1) << "Index " << i << " was not processed"; }
}

TEST(core_task, ParallelForIndex) {
	tf::Executor executor{4};
	constexpr size_t count = 500;
	std::vector<std::atomic_int> values(count);
	for (auto& v: values) { v.store(0); }

	std::function<void(size_t)> const func = [&](const size_t iIdx) -> void {
		values[iIdx].store(static_cast<int>(iIdx * 2));
	};
	parallelForIndex(executor, size_t{0}, count, size_t{1}, func);

	for (size_t i = 0; i < count; ++i) {
		EXPECT_EQ(values[i].load(), static_cast<int>(i * 2)) << "Index " << i << " has wrong value";
	}
}
