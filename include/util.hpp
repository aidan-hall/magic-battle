#ifndef UTIL_H
#define UTIL_H

#include "components.hpp"
#include "ndspp.hpp"
#include "tecs-system.hpp"
#include "tecs.hpp"
#include "unusual_id_manager.hpp"
#include <cstdint>
#include <nds.h>
#include <nds/arm9/sprite.h>
#include <nds/arm9/video.h>
#include <nds/dma.h>
#include <span>

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
    break;
  case 16:
    switch (height) {
    case 8:
      return SpriteSize_16x8;
    case 16:
      return SpriteSize_16x16;
    case 32:
      return SpriteSize_16x32;
    }
    break;
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
    break;
  case 64:
    switch (height) {
    case 32:
      return SpriteSize_64x32;
    case 64:
      return SpriteSize_64x64;
    }
    break;
  default:
    // Invalid width-height combination.
    assert(false);
  }
  assert(false);
  return SpriteSize_8x8;
}

struct SpriteData {
  SpriteSize size;
  int width;
  int height;
  // How many tiles/sprites there are.
  int tiles;
  unusual::id_manager<int, 16> &palette_index_manager;
  int palette_index;
  SpriteColorFormat color_format;
  const uint8_t *gfx;
  void *vram_memory;
  OamState *oam;
  SpriteData(OamState *oam, const uint8_t *gfx, int width, int height,
             int tiles, unusual::id_manager<int, 16> &palette_index_manager,
             SpriteColorFormat color_format, const uint8_t *palette,
             int palette_length, _ext_palette palette_memory)
      : size{sprite_size(width, height)}, width{width}, height{height},
        tiles{tiles}, palette_index_manager{palette_index_manager},
        palette_index{palette_index_manager.allocate()},
        color_format{color_format}, gfx{gfx},
        vram_memory{oamAllocateGfx(oam, size, color_format)}, oam{oam} {
    set_active_tile(0);
    dmaCopy(palette, &palette_memory[palette_index][0], palette_length);
  }
  ~SpriteData() {
    oamFreeGfx(oam, gfx);
    palette_index_manager.release(palette_index);
  }

  void set_active_tile(int n) {
    assert(0 <= n and n < tiles);
    dmaCopy(gfx + SPRITE_SIZE_PIXELS(size) * n, vram_memory,
            SPRITE_SIZE_PIXELS(size));
  }
};

void make_sprite(Tecs::Coordinator &ecs, Tecs::Entity entity,
                 unusual::id_manager<int, SPRITE_COUNT> &sprite_id_manager,
                 SpriteData &sprite_data);

void make_fireball(Tecs::Coordinator &ecs, Vec3 position, Vec3 velocity,
                   uint16_t *gfx);
bool circle_circle(Vec3 a_position, nds::fix a_radius_squared, Vec3 b_position, nds::fix b_radius_squared);


#endif /* UTIL_H */
