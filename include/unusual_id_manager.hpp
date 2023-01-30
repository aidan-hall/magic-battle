#ifndef UNUSUAL_ID_MANAGER_H
#define UNUSUAL_ID_MANAGER_H

#include "unusual_circular_queue.hpp"
#include <cstddef>

namespace unusual {
  template <typename T, size_t N> struct id_manager {
    T largest_unused = 0;
    circular_queue<T, N> recycled_ids;

    inline void release(const T &id) {
      recycled_ids.enqueue(id);
    }

    inline T allocate() {
      if (recycled_ids.empty()) {
        return largest_unused++;
      } else {
        const auto id = recycled_ids.front();
        recycled_ids.dequeue();
        return id;
      }
    }
    
  };
}

#endif /* UNUSUAL_ID_MANAGER_H */
