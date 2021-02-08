// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <memory>

#include "OrbitBase/AnyMovable.h"

namespace orbit_base {

TEST(AnyMovable, DefaultConstruction) {
  AnyMovable any{};
  EXPECT_FALSE(any.HasValue());
}

TEST(AnyMovable, ShouldCarryInt) {
  AnyMovable any{42};

  EXPECT_TRUE(any.HasValue());
  EXPECT_EQ(any.type().hash_code(), typeid(42).hash_code());
}

TEST(AnyMovable, ShouldCarryUniquePtr) {
  AnyMovable any{std::make_unique<int>(42)};

  EXPECT_TRUE(any.HasValue());
  EXPECT_EQ(any.type().hash_code(), typeid(std::unique_ptr<int>).hash_code());
}

TEST(AnyMovable, ShouldInPlaceConstructInt) {
  AnyMovable any{std::in_place_type<int>, 42};

  EXPECT_TRUE(any.HasValue());
  EXPECT_EQ(any.type().hash_code(), typeid(int).hash_code());
}

TEST(AnyMovable, ShouldInPlaceConstructUniquePtr) {
  AnyMovable any{std::in_place_type<std::unique_ptr<int>>, new int{42}};

  EXPECT_TRUE(any.HasValue());
  EXPECT_EQ(any.type().hash_code(), typeid(std::unique_ptr<int>).hash_code());
}

TEST(AnyMovable, ShouldEmplaceInt) {
  AnyMovable any{};
  any.Emplace<int>(42);

  EXPECT_TRUE(any.HasValue());
  EXPECT_EQ(any.type().hash_code(), typeid(int).hash_code());
}

TEST(AnyMovable, ShouldEmplaceUniquePtr) {
  AnyMovable any{};
  any.Emplace<std::unique_ptr<int>>(new int{42});

  EXPECT_TRUE(any.HasValue());
  EXPECT_EQ(any.type().hash_code(), typeid(std::unique_ptr<int>).hash_code());
}

TEST(any_movable_cast, ShouldExtractInt) {
  AnyMovable any{42};

  auto* ptr = any_movable_cast<int>(&any);
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(*ptr, 42);
}
TEST(any_movable_cast, ShouldExtractUniquePtr) {
  AnyMovable any{std::make_unique<int>(42)};

  auto* ptr = any_movable_cast<std::unique_ptr<int>>(&any);
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(**ptr, 42);
}

TEST(any_movable_cast, ShouldRefuseExtractingWrongType) {
  AnyMovable any{std::make_unique<int>(42)};

  auto* ptr = any_movable_cast<int>(&any);
  EXPECT_EQ(ptr, nullptr);
}

TEST(MakeAny, ShouldInPlaceConstructInt) {
  auto any = MakeAnyMovable<int>(42);

  EXPECT_TRUE(any.HasValue());
  EXPECT_EQ(any.type().hash_code(), typeid(int).hash_code());
}

TEST(MakeAny, ShouldInPlaceConstructUniquePtr) {
  auto any = MakeAnyMovable<std::unique_ptr<int>>(new int{42});

  EXPECT_TRUE(any.HasValue());
  EXPECT_EQ(any.type().hash_code(), typeid(std::unique_ptr<int>).hash_code());
}

}  // namespace orbit_base