#ifndef ITEM_MODELS_PRESET_ITEM_MODEL_H_
#define ITEM_MODELS_PRESET_ITEM_MODEL_H_

#include <QAbstractItemModel>
#include <memory>
#include <vector>

#include "OrbitSession.h"

namespace ItemModels {

class PresetItemModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  enum class Column { kPresetName, kProcessName, kEnd };
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

  void SetPresets(std::vector<Preset> presets);
  void AddPreset(Preset preset);

  static const Preset* PresetFromModelIndex(const QModelIndex&);

 private:
  std::vector<Preset> presets_;
};
}  // namespace ItemModels

Q_DECLARE_METATYPE(const Preset*);

#endif  // ITEM_MODELS_PRESET_ITEM_MODEL_H_