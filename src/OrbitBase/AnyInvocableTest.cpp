// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "OrbitBase/AnyInvocable.h"

namespace orbit_base {

static int returns_magic_number() { return 42; }

TEST(AnyInvocable, ShouldStoreAndCallLambda) {
  AnyInvocable<int()> invocable{[]() { return 42; }};
  EXPECT_NE(invocable, nullptr);
  EXPECT_TRUE(invocable);

  EXPECT_EQ(invocable(), 42);
}

TEST(AnyInvocable, ShouldStoreAndCallFunctionPointer) {
  AnyInvocable<int()> invocable{&returns_magic_number};
  EXPECT_NE(invocable, nullptr);
  EXPECT_TRUE(invocable);

  EXPECT_EQ(invocable(), 42);
}

TEST(AnyInvocable, ShouldStoreAndCallMoveOnlyLambda) {
  AnyInvocable<int()> invocable{[val = std::make_unique<int>(42)]() { return *val; }};
  EXPECT_NE(invocable, nullptr);
  EXPECT_TRUE(invocable);

  EXPECT_EQ(invocable(), 42);
}

TEST(AnyInvocable, ShouldBeMovableAndStillCallable) {
  AnyInvocable<int()> first{[val = std::make_unique<int>(42)]() { return *val; }};

  auto second = std::move(first);
  EXPECT_EQ(first, nullptr); // NOLINT
  EXPECT_NE(second, nullptr);

  EXPECT_EQ(second(), 42);
}

}  // namespace orbit_base