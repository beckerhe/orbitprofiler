// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_MAIN_THREAD_EXECUTOR_H_
#define ORBIT_GL_MAIN_THREAD_EXECUTOR_H_

#include <absl/types/span.h>

#include <chrono>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

#include "OrbitBase/Action.h"
#include "OrbitBase/Future.h"
#include "OrbitBase/FutureHelpers.h"
#include "OrbitBase/Promise.h"
#include "OrbitBase/PromiseHelpers.h"

// This class implements a mechanism for landing
// actions to the main thread. As a general rule
// waiting on sockets and processing should be
// happening off the main thread and the main thread
// should only be responsible for updating user
// interface and models.
//
// Usage example:
// /* A caller who wants to process something on the main thread,
//  * note that this is not-blocking operation and will be processed
//  * at some time in the future on the main thread.
//  */
//
// manager->Schedule(CreateAction([data]{
//   UpdateSomethingWith(data);
// }));
class MainThreadExecutor : public std::enable_shared_from_this<MainThreadExecutor> {
 public:
  MainThreadExecutor() = default;
  virtual ~MainThreadExecutor() = default;

  // Schedules the action to be performed on the main thread.
  virtual void Schedule(std::unique_ptr<Action> action) = 0;

  // Schedule schedules the function object `functor` to be executed
  // on `*this`. The execution occures asynchronously, meaning this
  // function call will only push the function object to a queue. It
  // will be picked up by an event loop cycle.
  //
  // Note: The function object is only executed if `*this` is still alive when the event loop picks
  // up the scheduled task.
  template <typename F>
  auto Schedule(F&& functor) -> orbit_base::Future<std::decay_t<decltype(functor())>> {
    using ReturnType = std::decay_t<decltype(functor())>;

    orbit_base::Promise<ReturnType> promise;
    orbit_base::Future<ReturnType> future = promise.GetFuture();

    auto function_wrapper = [functor = std::forward<F>(functor),
                             promise = std::move(promise)]() mutable {
      orbit_base::CallTaskAndSetResultInPromise<ReturnType> helper{&promise};
      helper.Call(functor);
    };
    Schedule(CreateAction(std::move(function_wrapper)));

    return future;
  }

  // ScheduleAfter schedules the continuation `functor` to be executed
  // on `*this` after `future` has completed.
  //
  // Note: The continuation is only executed if `*this` is still alive when `future` completes.
  template <typename T, typename F>
  auto ScheduleAfter(const orbit_base::Future<T>& future, F&& functor) {
    CHECK(future.IsValid());

    using ReturnType = typename orbit_base::ContinuationReturnType<T, F>::Type;

    // Since std::function is copyable, promise and functor also need to be copyable.
    // That's why we wrap both in shared_ptr-based wrappers. That's not ideal.
    // std::function in SharedState should be replaced by absl::any_invocable as soon as it is
    // released.
    auto promise = std::make_shared<orbit_base::Promise<ReturnType>>();
    orbit_base::Future<ReturnType> resulting_future = promise->GetFuture();
    orbit_base::CopyableFunctionObjectContainer copyable_function{std::forward<F>(functor)};

    auto continuation = [copyable_function = std::move(copyable_function),
                         executor_weak_ptr = weak_from_this(),
                         promise = std::move(promise)](auto&&... argument) mutable {
      auto executor = executor_weak_ptr.lock();
      if (executor == nullptr) return;

      auto function_wrapper =
          [copyable_function = std::move(copyable_function), promise = std::move(promise),
           argument = std::make_tuple(std::forward<decltype(argument)>(argument)...)]() mutable {
            orbit_base::CallTaskAndSetResultInPromise<ReturnType> helper{promise.get()};
            std::apply([&](auto... args) { helper.Call(copyable_function, args...); },
                       std::move(argument));
          };
      executor->Schedule(CreateAction(std::move(function_wrapper)));
    };

    orbit_base::RegisterContinuationOrCallDirectly(future, std::move(continuation));
    return resulting_future;
  }

  enum class WaitResult { kCompleted, kTimedOut, kAborted };
  [[nodiscard]] virtual WaitResult WaitFor(const orbit_base::Future<void>& future,
                                           std::chrono::milliseconds timeout) = 0;

  [[nodiscard]] virtual WaitResult WaitFor(const orbit_base::Future<void>& future) = 0;

  [[nodiscard]] virtual WaitResult WaitForAll(absl::Span<orbit_base::Future<void>> futures,
                                              std::chrono::milliseconds timeout) = 0;

  [[nodiscard]] virtual WaitResult WaitForAll(absl::Span<orbit_base::Future<void>> futures) = 0;

  virtual void AbortWaitingJobs() = 0;
};

template <typename F>
auto TrySchedule(const std::weak_ptr<MainThreadExecutor>& executor, F&& function_object)
    -> std::optional<decltype(std::declval<MainThreadExecutor>().Schedule(function_object))> {
  auto executor_ptr = executor.lock();
  if (executor_ptr == nullptr) return std::nullopt;

  return std::make_optional(executor_ptr->Schedule(std::forward<F>(function_object)));
}

#endif  // ORBIT_GL_MAIN_THREAD_EXECUTOR_H_
