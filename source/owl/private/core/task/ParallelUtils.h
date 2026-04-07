/**
 * @file ParallelUtils.h
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/external/taskflow.h"

namespace owl::core::task {

/**
 * @brief Execute a function in parallel over a container range.
 * @tparam Iterator Iterator type.
 * @tparam Callable Callable type.
 * @param[in,out] ioExecutor The Taskflow executor to use.
 * @param[in] iBegin Begin iterator.
 * @param[in] iEnd End iterator.
 * @param[in] iFunc Function to apply to each element.
 */
template<typename Iterator, typename Callable>
void parallelForEach(tf::Executor& ioExecutor, Iterator iBegin, Iterator iEnd, Callable&& iFunc) {
	tf::Taskflow taskflow;
	taskflow.for_each(iBegin, iEnd, std::forward<Callable>(iFunc));
	ioExecutor.run(taskflow).wait();
}

/**
 * @brief Execute a function in parallel over an index range.
 * @tparam IndexType Index type (must be integral).
 * @tparam Callable Callable type.
 * @param[in,out] ioExecutor The Taskflow executor to use.
 * @param[in] iBegin Start index.
 * @param[in] iEnd End index (exclusive).
 * @param[in] iStep Step size.
 * @param[in] iFunc Function to apply to each index.
 */
template<typename IndexType, typename Callable>
void parallelForIndex(tf::Executor& ioExecutor, IndexType iBegin, IndexType iEnd, IndexType iStep,
					  Callable&& iFunc) {
	tf::Taskflow taskflow;
	taskflow.for_each_index(iBegin, iEnd, iStep, std::forward<Callable>(iFunc));
	ioExecutor.run(taskflow).wait();
}

}// namespace owl::core::task
