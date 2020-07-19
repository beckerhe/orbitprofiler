#ifndef ITEM_MODELS_PROCESS_ITEM_MODEL_H_
#define ITEM_MODELS_PROCESS_ITEM_MODEL_H_

#include <qabstractitemmodel.h>
#include <qnamespace.h>

#include <QAbstractItemModel>
#include <vector>

#include "process.pb.h"

namespace ItemModels {

class ProcessItemModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  enum class Column { kPid, kName, kCpu, kEnd };
  using QAbstractItemModel::QAbstractItemModel;

  int columnCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& idx,
                int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& idx) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  QModelIndex index(int row, int column,
                    const QModelIndex& parent = {}) const override;
  QModelIndex parent(const QModelIndex& parent) const override;
  int rowCount(const QModelIndex& parent = {}) const override;

  void SetProcesses(std::vector<ProcessInfo> processes);

  static const ProcessInfo* ProcessInfoFromModelIndex(const QModelIndex&);

 private:
  std::vector<ProcessInfo> processes_;
};
}  // namespace ItemModels

Q_DECLARE_METATYPE(const ProcessInfo*);

#endif  // ITEM_MODELS_PROCESS_ITEM_MODEL_H_