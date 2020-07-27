#ifndef ITEM_MODELS_UPDATE_H_
#define ITEM_MODELS_UPDATE_H_

#include <algorithm>

namespace ItemModels {

template <typename Dest, typename Src, typename Compare, typename Equal,
          typename Update, typename Insert, typename Remove>
void Update(Dest& dest, Src src, const Compare& is_smaller,
            const Equal& is_equal, const Update& update_element,
            const Insert& insert_elements, const Remove& remove_elements) {

  // Just like STL, we consider two elements equivalent,
  // iff !(lhs < rhs || rhs < lhs).
  const auto is_equivalent = [&](const auto& lhs, const auto& rhs) -> bool {
    return !(is_smaller(lhs, rhs) || is_smaller(rhs, lhs));
  };

  std::sort(src.begin(), src.end(), is_smaller);

  auto dest_iter = dest.begin();
  auto src_iter = src.begin();

  while (dest_iter != dest.end() && src_iter != src.end()) {
    const int current_row =
        static_cast<int>(std::distance(dest.begin(), dest_iter));

    if (is_equivalent(dest_iter, src_iter)) {
      if (!is_equal(dest_iter, src_iter)) {
        dest_iter = update_element(dest_iter, src_iter);
        // *dest_iter = std::move(*src_iter);
        // emit dataChanged(
        //     index(current_row, 0, {}),
        //     index(current_row, static_cast<int>(Column::kEnd) - 1, {}));
      }
      ++dest_iter;
      ++src_iter;
    } else if (is_smaller(dest_iter, src_iter)) {
      dest_iter = remove_elements(dest_iter, std::next(dest_iter));
      //   beginRemoveRows({}, current_row, current_row);
      //   dest_iter = dest.erase(dest_iter);
      //   endRemoveRows();
    } else {
      dest_iter = insert_elements(dest_iter, src_iter, std::next(src_iter));
      //   beginInsertRows({}, current_row, current_row);
      //   dest_iter = dest.insert(dest_iter, *src_iter);
      //   endInsertRows();
      ++dest_iter;
      ++src_iter;
    }
  }

  if (dest_iter == dest.end() && src_iter != src.end()) {
    insert_elements(dest_iter, src_iter, src.end());
    // beginInsertRows({}, dest.size(), src.size() - 1);
    // dest.insert(dest_iter, src_iter, src.end());
    // endInsertRows();
  } else if (dest_iter != dest.end() && src_iter == src.end()) {
    remove_elements(dest_iter, dest.end());
    // beginRemoveRows({}, src.size(), dest.size() - 1);
    // dest.erase(dest_iter, dest.end());
    // endRemoveRows();
  }

  CHECK(dest.size() == src.size());
}

}  // namespace ItemModels

#endif  // ITEM_MODELS_UPDATE_H_
