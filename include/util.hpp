#ifndef UTIL_H
#define UTIL_H

#include "tecs.hpp"
#include <nds.h>

constexpr inline SpriteSize sprite_size(int width, int height) {
  switch (width) {
  case 8:
    switch (height) {
    case 8:
      return SpriteSize_8x8;
    case 16:
      return SpriteSize_8x16;
    case 32:
      return SpriteSize_8x32;
    }
  case 16:
    switch (height) {
    case 8:
      return SpriteSize_16x8;
    case 16:
      return SpriteSize_16x16;
    case 32:
      return SpriteSize_16x32;
    }
  case 32:
    switch (height) {
    case 8:
      return SpriteSize_32x8;
    case 16:
      return SpriteSize_32x16;
    case 32:
      return SpriteSize_32x32;
    case 64:
      return SpriteSize_32x64;
    }
  case 64:
    switch (height) {
    case 32:
      return SpriteSize_64x32;
    case 64:
      return SpriteSize_64x64;
    }
  default:
    // Invalid width-height combination.
    assert(false);
  }
}

void make_sprite(Tecs::Coordinator &ecs, Tecs::Entity entity,
                 unusual::id_manager<int, SPRITE_COUNT> &sprite_id_manager,
                 void *graphics_offset, SpriteSize size,
                 SpriteColorFormat color_format, int palette_index, int width,
                 int height);

#endif /* UTIL_H */
