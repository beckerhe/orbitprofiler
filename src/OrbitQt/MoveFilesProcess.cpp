// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitQt/MoveFilesProcess.h"

#include "OrbitBase/File.h"
#include "OrbitBase/Logging.h"
#include "Path.h"

namespace orbit_qt {

MoveFilesProcess::MoveFilesProcess() : QObject(nullptr) {
  background_thread_.start();
  moveToThread(&background_thread_);
}

MoveFilesProcess::~MoveFilesProcess() noexcept {
  background_thread_.quit();
  background_thread_.wait();
}

void MoveFilesProcess::Start() {
  QMetaObject::invokeMethod(this, [this]() { Run(); });
}

void MoveFilesProcess::ReportError(const std::string& error_message) {
  ERROR("%s", error_message);
  emit generalError(error_message);
}

void MoveFilesProcess::TryMoveFilesAndRemoveDirIfNeeded(const std::filesystem::path& src_dir,
                                                        const std::filesystem::path& dest_dir) {
  auto file_exists_or_error = orbit_base::FileExists(src_dir);
  if (file_exists_or_error.has_error()) {
    ReportError(absl::StrFormat("Unable to stat \"%s\": %s", src_dir.string(),
                                file_exists_or_error.error().message()));
  } else if (!file_exists_or_error.value()) {
    return;
  }

  auto files_or_error = orbit_base::ListFilesInDirectory(src_dir);
  if (files_or_error.has_error()) {
    ReportError(absl::StrFormat("Unable to list files in \"%s\": %s", src_dir.string(),
                                files_or_error.error().message()));
    return;
  }

  emit moveStarted(src_dir, dest_dir, files_or_error.value().size());
  for (const auto& file_path : files_or_error.value()) {
    const auto& file_name = file_path.filename();
    const auto& new_file_path = dest_dir / file_name;
    LOG("Moving \"%s\" to \"%s\"...", file_path.string(), new_file_path.string());
    auto move_result = orbit_base::MoveFile(file_path, new_file_path);
    if (move_result.has_error()) {
      ReportError(absl::StrFormat(R"(Unable to move "%s" to "%s": %s)", file_path.string(),
                                  new_file_path.string(), move_result.error().message()));
    }
  }
  emit moveDone();

  auto remove_result = orbit_base::RemoveFile(src_dir);
  if (remove_result.has_error()) {
    ReportError(absl::StrFormat("Unable to remove \"%s\": %s", src_dir.string(),
                                remove_result.error().message()));
  }
}

void MoveFilesProcess::Run() {
  CHECK(QThread::currentThread() == thread());
  TryMoveFilesAndRemoveDirIfNeeded(orbit_core::GetCaptureDirPriorTo1_65(),
                                   orbit_core::CreateOrGetCaptureDir());
  TryMoveFilesAndRemoveDirIfNeeded(orbit_core::GetPresetDirPriorTo1_65(),
                                   orbit_core::CreateOrGetPresetDir());
  emit processFinished();
}

}  // namespace orbit_qt