// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SourcePathsMapping/Mapping.h"

#include <absl/strings/match.h>

#include <filesystem>
#include <system_error>

namespace orbit_source_paths_mapping {

std::optional<std::filesystem::path> MapToFirstMatchingTarget(
    absl::Span<const Mapping> mappings, const std::filesystem::path& source_path) {
  for (const auto& mapping : mappings) {
    if (absl::StartsWith(source_path.string(), mapping.source_path.string())) {
      std::string target = mapping.target_path.string();
      target.append(
          std::string_view{source_path.string()}.substr(mapping.source_path.string().size()));
      return std::filesystem::path{target};
    }
  }

  return std::nullopt;
}

std::optional<std::filesystem::path> MapToFirstExistingTarget(
    absl::Span<const Mapping> mappings, const std::filesystem::path& source_path) {
  for (const auto& mapping : mappings) {
    if (absl::StartsWith(source_path.string(), mapping.source_path.string())) {
      std::string target = mapping.target_path.string();
      target.append(
          std::string_view{source_path.string()}.substr(mapping.source_path.string().size()));
      auto target_path = std::filesystem::path{target};

      std::error_code error{};
      if (std::filesystem::is_regular_file(target_path, error)) return target_path;
    }
  }

  return std::nullopt;
}
}  // namespace orbit_source_paths_mapping