
#include "testHelper.h"

#include <core/task/Scheduler.h>

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
	EXPECT_EQ(scheduler.pushTask(Task([&] {})), 1);
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

		scheduler.pushTask(Task([&] {}, [&] { counter++; }));
		ts.forceUpdate(std::chrono::milliseconds(100));
		scheduler.frame(ts);
		std::this_thread::sleep_for(std::chrono::milliseconds(5));//slowdown a little before checking
		EXPECT_EQ(counter, 0);
		ts.forceUpdate(std::chrono::milliseconds(100));
		scheduler.frame(ts);
		std::this_thread::sleep_for(std::chrono::milliseconds(5));//slowdown a little before checking
		EXPECT_EQ(counter, 1);
		scheduler.pushTask(Task([&] {}, [&] { counter++; }));
		scheduler.pushTask(Task([&] {}, [&] { counter++; }));
		scheduler.pushTask(Task([&] {}, [&] { counter++; }));
		EXPECT_TRUE(scheduler.isTaskInQueue(4));
		scheduler.clearQueue();
		EXPECT_EQ(counter, 1);
		EXPECT_FALSE(scheduler.isTaskInQueue(4));
		// this task should be destroyed with the scheduler, with no possibility to run.
		EXPECT_EQ(scheduler.pushTask(Task([&] { counter++; }, [&] { counter++; })), 5);
	}
	EXPECT_EQ(counter, 1);
}


TEST(core_task, SchedulerTimers) {
	Scheduler scheduler;
	Timestep ts;
	std::atomic_uint32_t counter = 0;
	auto t1 = scheduler.pushTimer(
			{.exec = [&] { ++counter; }, .frequency = std::chrono::milliseconds(300), .async = false, .iteration = 0});
	auto t2 = scheduler.pushTimer(
			{.exec = [&] { ++counter; }, .frequency = std::chrono::milliseconds(200), .async = true, .iteration = 0});
	auto t3 = scheduler.pushTimer(
			{.exec = [&] { ++counter; }, .frequency = std::chrono::milliseconds(100), .async = false, .iteration = 3});
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
