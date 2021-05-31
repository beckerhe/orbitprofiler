// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitQt/MoveFilesDialog.h"

#include <QPlainTextEdit>
#include <QString>

#include "ui_MoveFilesDialog.h"

namespace orbit_qt {

MoveFilesDialog::MoveFilesDialog() : QDialog(nullptr), ui_(new Ui::MoveFilesDialog) {
  ui_->setupUi(this);
}

MoveFilesDialog::~MoveFilesDialog() noexcept = default;

void MoveFilesDialog::AddText(std::string_view text) {
  ui_->log->append(QString::fromUtf8(text.data(), text.size()));
}

void MoveFilesDialog::EnableCloseButton() { ui_->closeButton->setEnabled(true); }

}  // namespace orbit_qt
