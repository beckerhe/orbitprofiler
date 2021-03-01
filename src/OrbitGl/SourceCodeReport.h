// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_SOURCE_CODE_REPORT_H_
#define ORBIT_GL_SOURCE_CODE_REPORT_H_

#include <absl/container/flat_hash_map.h>
#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <utility>

#include "CodeReport.h"
#include "ElfUtils/ElfFile.h"
#include "OrbitClientData/PostProcessedSamplingData.h"
#include "capture_data.pb.h"

namespace orbit_gl {

class SourceCodeReport : public CodeReport {
 public:
  explicit SourceCodeReport(std::string_view source_file,
                            const orbit_client_protos::FunctionInfo& function,
                            uint64_t absolute_address, orbit_elf_utils::ElfFile* elf_file,
                            const PostProcessedSamplingData& sampling_data,
                            uint32_t total_samples_in_capture);

  [[nodiscard]] uint32_t GetNumSamplesInFunction() const override {
    return total_samples_in_function_;
  }
  [[nodiscard]] uint32_t GetNumSamples() const override { return total_samples_in_capture_; }
  [[nodiscard]] uint32_t GetNumSamplesAtLine(size_t line) const override;

 private:
  absl::flat_hash_map<size_t, size_t> number_of_samples_per_line;
  uint32_t total_samples_in_function_ = 0;
  uint32_t total_samples_in_capture_ = 0;
};

}  // namespace orbit_gl

#endif  // ORBIT_GL_DISASSEMBLY_REPORT_H_
