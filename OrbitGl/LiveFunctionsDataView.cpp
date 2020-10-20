// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "LiveFunctionsDataView.h"

#include "App.h"
#include "LiveFunctionsController.h"
#include "OrbitBase/Profiling.h"
#include "OrbitClientData/FunctionUtils.h"
#include "TextBox.h"
#include "TimeGraph.h"
#include "TimerChain.h"
#include "capture_data.pb.h"

using orbit_client_protos::FunctionInfo;
using orbit_client_protos::FunctionStats;

LiveFunctionsDataView::LiveFunctionsDataView(LiveFunctionsController* live_functions)
    : DataView(DataViewType::kLiveFunctions), live_functions_(live_functions) {
  update_period_ms_ = 300;
  OnDataChanged();
}

const std::vector<DataView::Column>& LiveFunctionsDataView::GetColumns() {
  static const std::vector<Column> columns = [] {
    std::vector<Column> columns;
    columns.resize(kNumColumns);
    columns[kColumnSelected] = {"Hooked", .0f, SortingOrder::kDescending};
    columns[kColumnName] = {"Function", .4f, SortingOrder::kAscending};
    columns[kColumnCount] = {"Count", .0f, SortingOrder::kDescending};
    columns[kColumnTimeTotal] = {"Total", .075f, SortingOrder::kDescending};
    columns[kColumnTimeAvg] = {"Avg", .075f, SortingOrder::kDescending};
    columns[kColumnTimeMin] = {"Min", .075f, SortingOrder::kDescending};
    columns[kColumnTimeMax] = {"Max", .075f, SortingOrder::kDescending};
    columns[kColumnModule] = {"Module", .1f, SortingOrder::kAscending};
    columns[kColumnAddress] = {"Address", .0f, SortingOrder::kAscending};
    return columns;
  }();
  return columns;
}

std::string LiveFunctionsDataView::GetValue(int row, int column) {
  if (row >= static_cast<int>(GetNumElements())) {
    return "";
  }

  const FunctionInfo& function = *GetSelectedFunction(row);
  const FunctionStats& stats = GOrbitApp->GetCaptureData().GetFunctionStatsOrDefault(function);

  switch (column) {
    case kColumnSelected:
      return GOrbitApp->IsFunctionSelected(function) ? FunctionsDataView::kSelectedFunctionString
                                                     : FunctionsDataView::kUnselectedFunctionString;
    case kColumnName:
      return FunctionUtils::GetDisplayName(function);
    case kColumnCount:
      return absl::StrFormat("%lu", stats.count());
    case kColumnTimeTotal:
      return GetPrettyTime(absl::Nanoseconds(stats.total_time_ns()));
    case kColumnTimeAvg:
      return GetPrettyTime(absl::Nanoseconds(stats.average_time_ns()));
    case kColumnTimeMin:
      return GetPrettyTime(absl::Nanoseconds(stats.min_ns()));
    case kColumnTimeMax:
      return GetPrettyTime(absl::Nanoseconds(stats.max_ns()));
    case kColumnModule:
      return function.loaded_module_path();
    case kColumnAddress:
      return absl::StrFormat("0x%llx", FunctionUtils::GetAbsoluteAddress(function));
    default:
      return "";
  }
}

#define ORBIT_FUNC_SORT(Member)                                                      \
  [&](int a, int b) {                                                                \
    return OrbitUtils::Compare(functions[a].Member, functions[b].Member, ascending); \
  }
#define ORBIT_STAT_SORT(Member)                                              \
  [&](int a, int b) {                                                        \
    const FunctionStats& stats_a =                                           \
        GOrbitApp->GetCaptureData().GetFunctionStatsOrDefault(functions[a]); \
    const FunctionStats& stats_b =                                           \
        GOrbitApp->GetCaptureData().GetFunctionStatsOrDefault(functions[b]); \
    return OrbitUtils::Compare(stats_a.Member, stats_b.Member, ascending);   \
  }
#define ORBIT_CUSTOM_FUNC_SORT(Func)                                               \
  [&](int a, int b) {                                                              \
    return OrbitUtils::Compare(Func(functions[a]), Func(functions[b]), ascending); \
  }

void LiveFunctionsDataView::DoSort() {
  bool ascending = sorting_orders_[sorting_column_] == SortingOrder::kAscending;
  std::function<bool(int a, int b)> sorter = nullptr;

  const std::vector<FunctionInfo>& functions = functions_;

  switch (sorting_column_) {
    case kColumnSelected:
      sorter = ORBIT_CUSTOM_FUNC_SORT(GOrbitApp->IsFunctionSelected);
      break;
    case kColumnName:
      sorter = ORBIT_CUSTOM_FUNC_SORT(FunctionUtils::GetDisplayName);
      break;
    case kColumnCount:
      sorter = ORBIT_STAT_SORT(count());
      break;
    case kColumnTimeTotal:
      sorter = ORBIT_STAT_SORT(total_time_ns());
      break;
    case kColumnTimeAvg:
      sorter = ORBIT_STAT_SORT(average_time_ns());
      break;
    case kColumnTimeMin:
      sorter = ORBIT_STAT_SORT(min_ns());
      break;
    case kColumnTimeMax:
      sorter = ORBIT_STAT_SORT(max_ns());
      break;
    case kColumnModule:
      sorter = ORBIT_CUSTOM_FUNC_SORT(FunctionUtils::GetLoadedModuleName);
      break;
    case kColumnAddress:
      sorter = ORBIT_FUNC_SORT(address());
      break;
    default:
      break;
  }

  if (sorter) {
    std::stable_sort(indices_.begin(), indices_.end(), sorter);
  }
}

const std::string LiveFunctionsDataView::kMenuActionSelect = "Hook";
const std::string LiveFunctionsDataView::kMenuActionUnselect = "Unhook";
const std::string LiveFunctionsDataView::kMenuActionJumpToFirst = "Jump to first";
const std::string LiveFunctionsDataView::kMenuActionJumpToLast = "Jump to last";
const std::string LiveFunctionsDataView::kMenuActionJumpToMin = "Jump to min";
const std::string LiveFunctionsDataView::kMenuActionJumpToMax = "Jump to max";
const std::string LiveFunctionsDataView::kMenuActionDisassembly = "Go to Disassembly";
const std::string LiveFunctionsDataView::kMenuActionIterate = "Add iterator(s)";
const std::string LiveFunctionsDataView::kMenuActionFrameTrack = "Add frame track(s)";
const std::string LiveFunctionsDataView::kMenuActionRemoveFrameTrack = "Remove frame track(s)";

std::vector<std::string> LiveFunctionsDataView::GetContextMenu(
    int clicked_index, const std::vector<int>& selected_indices) {
  bool enable_select = false;
  bool enable_unselect = false;
  bool enable_iterator = false;
  bool enable_disassembly = false;
  bool enable_frame_track = false;
  bool enable_remove_frame_track = false;

  const CaptureData& capture_data = GOrbitApp->GetCaptureData();
  for (int index : selected_indices) {
    const FunctionInfo& selected_function = *GetSelectedFunction(index);
    const uint64_t absolute_address = FunctionUtils::GetAbsoluteAddress(selected_function);

    // Is that function actually inside a module of the process (i.e. can we disassemble)?
    const FunctionInfo* actual_function =
        capture_data.FindFunctionByAddress(absolute_address, false);
    const bool function_exists = actual_function != nullptr;

    const FunctionStats& stats = capture_data.GetFunctionStatsOrDefault(selected_function);
    enable_select |= function_exists && !GOrbitApp->IsFunctionSelected(selected_function);
    enable_unselect |= function_exists && GOrbitApp->IsFunctionSelected(selected_function);
    // We need at least one function call to a function so that adding iterators makes sense.
    enable_iterator |= stats.count() > 0;
    // We need at least two function calls to a function so that it's possible to use it
    // as a frame marker.
    enable_frame_track |= stats.count() > 1 && added_frame_tracks_.count(absolute_address) == 0;
    enable_remove_frame_track |= added_frame_tracks_.count(absolute_address) > 0;
    enable_disassembly |= function_exists;
  }

  std::vector<std::string> menu;
  if (enable_select) menu.emplace_back(kMenuActionSelect);
  if (enable_unselect) menu.emplace_back(kMenuActionUnselect);
  if (enable_disassembly && selected_indices.size() == 1) menu.emplace_back(kMenuActionDisassembly);

  if (enable_iterator) {
    menu.emplace_back(kMenuActionIterate);
  }
  if (enable_frame_track) {
    menu.emplace_back(kMenuActionFrameTrack);
  }
  if (enable_remove_frame_track) {
    menu.emplace_back(kMenuActionRemoveFrameTrack);
  }

  // For now, these actions only make sense when one function is selected,
  // so we don't show them otherwise.
  if (selected_indices.size() == 1) {
    const FunctionInfo& function = *GetSelectedFunction(selected_indices[0]);
    const FunctionStats& stats = capture_data.GetFunctionStatsOrDefault(function);
    if (stats.count() > 0) {
      menu.insert(menu.end(), {kMenuActionJumpToFirst, kMenuActionJumpToLast, kMenuActionJumpToMin,
                               kMenuActionJumpToMax});
    }
  }
  Append(menu, DataView::GetContextMenu(clicked_index, selected_indices));
  return menu;
}

void LiveFunctionsDataView::OnContextMenu(const std::string& action, int menu_index,
                                          const std::vector<int>& item_indices) {
  const CaptureData& capture_data = GOrbitApp->GetCaptureData();
  if (action == kMenuActionSelect || action == kMenuActionUnselect ||
      action == kMenuActionDisassembly) {
    for (int i : item_indices) {
      FunctionInfo* selected_function = GetSelectedFunction(i);
      const uint64_t absolute_address = FunctionUtils::GetAbsoluteAddress(*selected_function);
      // Is that function actually inside a module of the process?
      if (capture_data.FindFunctionByAddress(absolute_address, false) == nullptr) {
        continue;
      }
      if (action == kMenuActionSelect) {
        GOrbitApp->SelectFunction(*selected_function);
      } else if (action == kMenuActionUnselect) {
        GOrbitApp->DeselectFunction(*selected_function);
      } else if (action == kMenuActionDisassembly) {
        int32_t pid = capture_data.process_id();
        GOrbitApp->Disassemble(pid, *selected_function);
      }
    }
  } else if (action == kMenuActionJumpToFirst) {
    CHECK(item_indices.size() == 1);
    auto function_address =
        FunctionUtils::GetAbsoluteAddress(*GetSelectedFunction(item_indices[0]));
    auto first_box = GCurrentTimeGraph->FindNextFunctionCall(
        function_address, std::numeric_limits<uint64_t>::lowest());
    if (first_box != nullptr) {
      GCurrentTimeGraph->SelectAndZoom(first_box);
    }
  } else if (action == kMenuActionJumpToLast) {
    CHECK(item_indices.size() == 1);
    auto function_address =
        FunctionUtils::GetAbsoluteAddress(*GetSelectedFunction(item_indices[0]));
    auto last_box = GCurrentTimeGraph->FindPreviousFunctionCall(
        function_address, std::numeric_limits<uint64_t>::max());
    if (last_box != nullptr) {
      GCurrentTimeGraph->SelectAndZoom(last_box);
    }
  } else if (action == kMenuActionJumpToMin) {
    CHECK(item_indices.size() == 1);
    const FunctionInfo& function = *GetSelectedFunction(item_indices[0]);
    auto [min_box, _] = GetMinMax(function);
    if (min_box != nullptr) {
      GCurrentTimeGraph->SelectAndZoom(min_box);
    }
  } else if (action == kMenuActionJumpToMax) {
    CHECK(item_indices.size() == 1);
    const FunctionInfo& function = *GetSelectedFunction(item_indices[0]);
    auto [_, max_box] = GetMinMax(function);
    if (max_box != nullptr) {
      GCurrentTimeGraph->SelectAndZoom(max_box);
    }
  } else if (action == kMenuActionIterate) {
    for (int i : item_indices) {
      FunctionInfo* selected_function = GetSelectedFunction(i);
      const FunctionStats& stats =
          GOrbitApp->GetCaptureData().GetFunctionStatsOrDefault(*selected_function);
      if (stats.count() > 0) {
        live_functions_->AddIterator(selected_function);
      }
    }
  } else if (action == kMenuActionFrameTrack) {
    for (int i : item_indices) {
      FunctionInfo* function = GetSelectedFunction(i);
      const FunctionStats& stats = GOrbitApp->GetCaptureData().GetFunctionStatsOrDefault(*function);
      uint64_t function_address = FunctionUtils::GetAbsoluteAddress(*function);
      if (stats.count() > 1 && added_frame_tracks_.count(function_address) == 0) {
        live_functions_->AddFrameTrack(*function);
        added_frame_tracks_.insert(function_address);
      }
    }
  } else if (action == kMenuActionRemoveFrameTrack) {
    for (int i : item_indices) {
      FunctionInfo* function = GetSelectedFunction(i);
      uint64_t function_address = FunctionUtils::GetAbsoluteAddress(*function);
      if (added_frame_tracks_.count(function_address) > 0) {
        added_frame_tracks_.erase(function_address);
        live_functions_->RemoveFrameTrack(*function);
      }
    }
  } else {
    DataView::OnContextMenu(action, menu_index, item_indices);
  }
}

void LiveFunctionsDataView::DoFilter() {
  std::vector<uint32_t> indices;

  std::vector<std::string> tokens = absl::StrSplit(ToLower(filter_), ' ');

  for (size_t i = 0; i < functions_.size(); ++i) {
    const FunctionInfo& function = functions_[i];
    std::string name = ToLower(FunctionUtils::GetDisplayName(function));

    bool match = true;

    for (std::string& filter_token : tokens) {
      if (name.find(filter_token) == std::string::npos) {
        match = false;
        break;
      }
    }

    if (match) {
      indices.push_back(i);
    }
  }

  indices_ = indices;

  // Filter drawn textboxes
  absl::flat_hash_set<uint64_t> visible_functions;
  for (size_t i = 0; i < indices_.size(); ++i) {
    FunctionInfo* func = GetSelectedFunction(i);
    visible_functions.insert(FunctionUtils::GetAbsoluteAddress(*func));
  }
  GOrbitApp->SetVisibleFunctions(std::move(visible_functions));
}

void LiveFunctionsDataView::OnDataChanged() {
  functions_.clear();
  added_frame_tracks_.clear();
  const absl::flat_hash_map<uint64_t, orbit_client_protos::FunctionInfo>& selected_functions =
      GOrbitApp->GetCaptureData().selected_functions();
  size_t functions_count = selected_functions.size();
  indices_.resize(functions_count);
  size_t i = 0;
  for (const auto& pair : selected_functions) {
    functions_.push_back(pair.second);
    indices_[i] = i;
    ++i;
  }

  DataView::OnDataChanged();
}

void LiveFunctionsDataView::OnTimer() {
  if (GOrbitApp->IsCapturing()) {
    OnSort(sorting_column_, {});
  }
}

FunctionInfo* LiveFunctionsDataView::GetSelectedFunction(unsigned int row) {
  CHECK(row < functions_.size());
  return &(functions_[indices_[row]]);
}

std::pair<TextBox*, TextBox*> LiveFunctionsDataView::GetMinMax(const FunctionInfo& function) const {
  auto function_address = FunctionUtils::GetAbsoluteAddress(function);
  TextBox* min_box = nullptr;
  TextBox* max_box = nullptr;
  std::vector<std::shared_ptr<TimerChain>> chains =
      GCurrentTimeGraph->GetAllThreadTrackTimerChains();
  for (auto& chain : chains) {
    if (!chain) continue;
    for (auto& block : *chain) {
      for (size_t i = 0; i < block.size(); i++) {
        TextBox& box = block[i];
        if (box.GetTimerInfo().function_address() == function_address) {
          uint64_t elapsed_nanos = box.GetTimerInfo().end() - box.GetTimerInfo().start();
          if (min_box == nullptr ||
              elapsed_nanos < (min_box->GetTimerInfo().end() - min_box->GetTimerInfo().start())) {
            min_box = &box;
          }
          if (max_box == nullptr ||
              elapsed_nanos > (max_box->GetTimerInfo().end() - max_box->GetTimerInfo().start())) {
            max_box = &box;
          }
        }
      }
    }
  }
  return std::make_pair(min_box, max_box);
}
