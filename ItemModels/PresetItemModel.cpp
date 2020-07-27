#include "ItemModels/PresetItemModel.h"

#include <OrbitBase/Logging.h>

#include <filesystem>
#include <functional>

#include "ItemModels/Update.h"

namespace ItemModels {

const Preset* PresetItemModel::PresetFromModelIndex(const QModelIndex& idx) {
  CHECK(idx.isValid());
  return idx.data(Qt::UserRole).value<const Preset*>();
}

int PresetItemModel::columnCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(Column::kEnd);
}

QVariant PresetItemModel::data(const QModelIndex& idx, int role) const {
  CHECK(idx.isValid());
  CHECK(idx.model() == static_cast<const QAbstractItemModel*>(this));
  CHECK(idx.row() >= 0 && idx.row() < presets_.size());
  CHECK(idx.column() >= 0 && idx.column() < static_cast<int>(Column::kEnd));

  const auto& preset = presets_[idx.row()];

  if (role == Qt::UserRole) {
    return QVariant::fromValue(preset);
  }

  if (role == Qt::DisplayRole) {
    switch (static_cast<Column>(idx.column())) {
      case Column::kPresetName:
        return QString::fromStdString(
            std::filesystem::path{preset.m_FileName}.filename());
      case Column::kProcessName:
        return QString::fromStdString(
            std::filesystem::path{preset.m_ProcessFullPath}.filename());
      case Column::kEnd:
        Q_UNREACHABLE();
    }
  }

  // When the EditRole is requested, we return the unformatted raw value, which
  // means the CPU Usage is returned as a double.
  if (role == Qt::EditRole) {
    switch (static_cast<Column>(idx.column())) {
      case Column::kPresetName:
        return QString::fromStdString(preset.m_FileName);
      case Column::kProcessName:
        return QString::fromStdString(preset.m_ProcessFullPath);
      case Column::kEnd:
        Q_UNREACHABLE();
    }
  }

  if (role == Qt::ToolTipRole) {
    // We don't care about the column when showing tooltip. It's the same for
    // the whole row.
    return QString::fromStdString(preset.m_FileName);
  }

  return {};
}

Qt::ItemFlags PresetItemModel::flags(const QModelIndex& idx) const {
  CHECK(idx.isValid());

  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren;
}

QVariant PresetItemModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const {
  if (orientation == Qt::Vertical) {
    return {};
  }

  if (role == Qt::DisplayRole) {
    switch (static_cast<Column>(section)) {
      case Column::kPresetName:
        return "Preset";
      case Column::kProcessName:
        return "Process";
      case Column::kEnd:
        Q_UNREACHABLE();
    }
  }

  return {};
}

QModelIndex PresetItemModel::index(int row, int column,
                                   const QModelIndex& parent) const {
  if (parent.isValid()) {
    return {};
  }

  if (row >= 0 && row < presets_.size() && column >= 0 &&
      column < static_cast<int>(Column::kEnd)) {
    return createIndex(row, column);
  }

  return {};
}
QModelIndex PresetItemModel::parent(const QModelIndex& parent) const {
  return {};
}
int PresetItemModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }

  return presets_.size();
}

void PresetItemModel::SetPresets(std::vector<Preset> new_presets) {
  const auto is_smaller = [](const Preset& lhs, const Preset& rhs) -> bool {
    return lhs.m_FileName < rhs.m_FileName;
  };

  const auto is_equal = std::equal_to<Preset>{};

  const auto update_element = [&](const auto& dest, const auto& src) {
    *dest = std::move(*src);
    const auto current_row =
        static_cast<int>(std::distance(presets_.begin(), dest));
    emit dataChanged(
        index(current_row, 0, QModelIndex{}),
        index(current_row, static_cast<int>(Column::kEnd) - 1, QModelIndex{}));
    return dest;
  };

  const auto insert_elements = [&](const auto dest_begin, const auto src_begin,
                                   const auto src_end) {
    const auto current_row =
        static_cast<int>(std::distance(presets_.begin(), dest_begin));
    const auto size = std::distance(src_begin, src_end);
    beginInsertRows(QModelIndex{}, current_row, current_row + size - 1);
    const auto next = presets_.insert(dest_begin, src_begin, src_end);
    endInsertRows();
    return next;
  };

  const auto remove_elements = [&](auto begin, auto end) {
    const auto current_row =
        static_cast<int>(std::distance(presets_.begin(), begin));

    beginRemoveRows(QModelIndex{}, current_row, presets_.size() - 1);
    const auto next = presets_.erase(begin, end);
    endRemoveRows();
    return next;
  };

  Update(presets_, std::move(new_presets), is_smaller, is_equal, update_element,
         insert_elements, remove_elements);
}

}  // namespace ItemModels