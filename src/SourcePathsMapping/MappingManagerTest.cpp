// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <qcoreapplication.h>

#include "SourcePathsMapping/Mapping.h"
#include "SourcePathsMapping/MappingManager.h"

namespace orbit_source_paths_mapping {

const Mapping mapping0{"/build/project", "/home/user/project"};
const Mapping mapping1{"/src/project2", "/home/user/project"};
const Mapping mapping2{"/src/project", "/home/user/project"};

TEST(MappingManager, SetAndGet) {
  QCoreApplication::setApplicationName("MappingManager.SetAndGet");

  MappingManager manager{};

  std::vector<Mapping> mappings{mapping0, mapping1, mapping2};
  manager.SetMappings(mappings);
  EXPECT_EQ(manager.GetMappings(), mappings);

  manager.SetMappings({});
}

TEST(MappingManager, Append) {
  QCoreApplication::setApplicationName("MappingManager.Append");

  MappingManager manager{};

  std::vector<Mapping> mappings{mapping0, mapping1};
  manager.SetMappings(mappings);

  manager.AppendMapping(mapping2);
  mappings.emplace_back(mapping2);

  EXPECT_EQ(manager.GetMappings(), mappings);

  manager.SetMappings({});
}

TEST(MappingManager, SaveAndLoad) {
  QCoreApplication::setApplicationName("MappingManager.SaveAndLoad");

  std::vector<Mapping> mappings{mapping0, mapping1, mapping2};
  {
    MappingManager manager{};
    manager.SetMappings(mappings);
  }

  {
    MappingManager manager{};
    EXPECT_EQ(manager.GetMappings(), mappings);
  }
}
}  // namespace orbit_source_paths_mapping