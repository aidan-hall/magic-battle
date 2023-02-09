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

void wait_for_start();

constexpr SpriteSize sprite_size(int width, int height);

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
             int palette_length, _ext_palette palette_memory);
  ~SpriteData();

  void set_active_tile(int n);
};

void make_sprite(Tecs::Coordinator &ecs, Tecs::Entity entity,
                 unusual::id_manager<int, SPRITE_COUNT> &sprite_id_manager,
                 SpriteData &sprite_data);

Tecs::Entity
make_fireball(Tecs::Coordinator &ecs, Vec3 position, Vec3 target,
              unusual::id_manager<int, SPRITE_COUNT> &sprite_id_manager,
              SpriteData &sprite);

Tecs::Entity
make_zombie(Tecs::Coordinator &ecs, Vec3 position, Tecs::Entity player,
            nds::fix speed,
            unusual::id_manager<int, SPRITE_COUNT> &sprite_id_manager,
            SpriteData &sprite);

Tecs::Entity
make_explosion(Tecs::Coordinator &ecs, Vec3 position,
               unusual::id_manager<int, SPRITE_COUNT> &sprite_id_manager,
               SpriteData &sprite);

bool circle_circle(Vec3 a_position, nds::fix a_radius_squared, Vec3 b_position,
                   nds::fix b_radius_squared);

void take_damage(Tecs::Coordinator &ecs, Tecs::Entity self, Tecs::Entity other);
void self_destruct(Tecs::Coordinator &ecs, Tecs::Entity self);
nds::fix radius_squared_from_diameter(nds::fix diameter);

#endif /* UTIL_H */
