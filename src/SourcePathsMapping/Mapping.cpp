// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SourcePathsMapping/Mapping.h"

#include <absl/strings/match.h>

#include <filesystem>
#include <optional>
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

std::optional<Mapping> InferMappingFromExample(const std::filesystem::path& source_path,
                                               const std::filesystem::path& target_path) {
  if (source_path.filename() != target_path.filename()) return std::nullopt;
  if (source_path == target_path) return std::nullopt;

  auto source = source_path;
  auto target = target_path;

  while (source.has_filename() && target.has_filename() && source.filename() == target.filename()) {
    source = source.parent_path();
    target = target.parent_path();
  }

  return Mapping{source, target};
}

}  // namespace orbit_source_paths_mapping