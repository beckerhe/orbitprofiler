// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "Core.h"
#include "Serialization.h"

//-----------------------------------------------------------------------------
struct PresetModule {
  std::string m_Name;
  std::vector<uint64_t> m_FunctionHashes;

  ORBIT_SERIALIZABLE;
};

//-----------------------------------------------------------------------------
class Preset {
 public:
  Preset();
  ~Preset();

  ORBIT_SERIALIZABLE;

  std::string m_FileName;
  std::string m_ProcessFullPath;
  std::string m_WorkingDirectory;
  std::string m_Arguments;
  std::map<std::string, PresetModule> m_Modules;

  friend bool operator==(const Preset& lhs, const Preset& rhs) {
    return std::tie(lhs.m_FileName, lhs.m_ProcessFullPath,
                    lhs.m_WorkingDirectory, lhs.m_Arguments, lhs.m_Modules) ==
           std::tie(rhs.m_FileName, rhs.m_ProcessFullPath,
                    rhs.m_WorkingDirectory, rhs.m_Arguments, rhs.m_Modules);
  }

  friend bool operator!=(const Preset& lhs, const Preset& rhs) {
    return !(lhs == rhs);
  }
};
