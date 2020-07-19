// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIDGETS_FILTER_BAR_TABLE_VIEW_WIDGET_H_
#define WIDGETS_FILTER_BAR_TABLE_VIEW_WIDGET_H_

#include <QLabel>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QWidget>
#include <memory>

namespace Ui {
class FilterBarTableViewWidget;
}

namespace Widgets {

class FilterBarTableViewWidget : public QWidget {
  Q_OBJECT

 public:
  explicit FilterBarTableViewWidget(QWidget* parent = nullptr);
  ~FilterBarTableViewWidget() override;

  void SetModel(QAbstractItemModel* model) {
    proxy_model_.setSourceModel(model);
  }

  QTableView* GetTableView();
  const QTableView* GetTableView() const;

  QSortFilterProxyModel* GetProxyModel() { return &proxy_model_; }
  const QSortFilterProxyModel* GetProxyModel() const { return &proxy_model_; }

  QLabel* GetLabel();
  const QLabel* GetLabel() const;

 private:
  std::unique_ptr<Ui::FilterBarTableViewWidget> ui;
  QSortFilterProxyModel proxy_model_;
};
}  // namespace Widgets
#endif  // WIDGETS_FILTER_BAR_TABLE_VIEW_WIDGET_H_