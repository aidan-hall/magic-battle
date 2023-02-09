#include "util.hpp"
#include "components.hpp"
#include "ndspp.hpp"
#include "tecs.hpp"
#include "unusual_id_manager.hpp"
#include <maxmod9.h>
#include <nds.h>
#include <nds/arm9/math.h>
#include <nds/arm9/sprite.h>
#include <nds/timers.h>
#include <soundbank.h>

using namespace Tecs;
void make_sprite(Coordinator &ecs, Entity entity,
                 unusual::id_manager<int, SPRITE_COUNT> &sprite_id_manager,
                 SpriteData &sprite_data) {
  ecs.addComponent<SpriteInfo>(entity);
  const auto &[sprite_info] = ecs.addComponents(
      entity,
      SpriteInfo{sprite_id_manager.allocate(),
                 // Width and height are doubled to allow room for rotation
                 nds::fix::from_int(sprite_data.width) / 2,
                 nds::fix::from_int(sprite_data.height) / 2});
  oamSet(&oamMain, sprite_info.id, 5, 5, 0, sprite_data.palette_index,
         sprite_data.size, sprite_data.color_format, sprite_data.vram_memory,
         -1, false, false, false, false, false);
}

nds::fix radius_squared_from_diameter(nds::fix diameter) {
  nds::fix radius = diameter / 2;
  return radius * radius;
}

Entity make_fireball(Coordinator &ecs, Vec3 position, Vec3 target,
                     unusual::id_manager<int, SPRITE_COUNT> &sprite_id_manager,
                     SpriteData &sprite) {
  Entity fireball = ecs.newEntity();

  // constexpr nds::fix FIREBALL_SPEED = nds::fix::from_float(2.0f);

  make_sprite(ecs, fireball, sprite_id_manager, sprite);
  Vec3 velocity = {target.x - position.x, target.y - position.y, {0}};
  normalizef32(reinterpret_cast<int32_t *>(&velocity));

  // velocity.x = velocity.x * FIREBALL_SPEED;
  // velocity.y = velocity.y * FIREBALL_SPEED;

  ecs.addComponents(
      fireball, Position{position}, Velocity{velocity},
      Collision{ZOMBIE_LAYER, PLAYER_ATTACK_LAYER,
                radius_squared_from_diameter(nds::fix::from_int(sprite.width)),
                take_damage},
      TimerCallback{cpuGetTiming() + BUS_CLOCK * 4,
                    [](Tecs::Coordinator &ecs, const Tecs::Entity entity) {
                      ecs.addComponent<DeathMark>(entity);
                    }},
      Health{2});
  return fireball;
}

Tecs::Entity
make_zombie(Coordinator &ecs, Vec3 position, Tecs::Entity player,
            nds::fix speed,
            unusual::id_manager<int, SPRITE_COUNT> &sprite_id_manager,
            SpriteData &sprite) {
  using namespace nds;
  Tecs::Entity zombie = ecs.newEntity();
  const nds::fix zombie_radius_squared =
      radius_squared_from_diameter(nds::fix::from_int(sprite.width));
  make_sprite(ecs, zombie, sprite_id_manager, sprite);
  ecs.addComponent<Zombie>(zombie);

  ecs.addComponents(zombie, Position{position}, Velocity{},
                    Collision{PLAYER_ATTACK_LAYER | PLAYER_LAYER, ZOMBIE_LAYER,
                              zombie_radius_squared, take_damage},
                    Following{player, speed}, Health{1});
  return zombie;
}

Entity make_explosion(Coordinator &ecs, Vec3 position,
                      unusual::id_manager<int, SPRITE_COUNT> &sprite_id_manager,
                      SpriteData &sprite) {
  Entity explosion = ecs.newEntity();

  make_sprite(ecs, explosion, sprite_id_manager, sprite);
  // const auto affine_index = affine_index_manager.allocate();
  // oamSetAffineIndex(&oamMain, ecs.getComponent<SpriteInfo>(explosion).id,
  //                   affine_index, true);

  ecs.addComponents(
      explosion, Position{position},
      Collision{ZOMBIE_LAYER, PLAYER_ATTACK_LAYER,
                radius_squared_from_diameter(nds::fix::from_int(sprite.width)),
                take_damage},
      // Affine{affine_index, 0, 1 << 8},
      Health{20}, TimerCallback{cpuGetTiming() + BUS_CLOCK * 1, self_destruct});
  return explosion;
}

bool circle_circle(Vec3 a_position, nds::fix a_radius_squared, Vec3 b_position,
                   nds::fix b_radius_squared) {
  const nds::fix x_diff = a_position.x - b_position.x;
  const nds::fix y_diff = a_position.y - b_position.y;
  return (x_diff * x_diff) + (y_diff * y_diff) <
         (a_radius_squared + b_radius_squared);
}

void self_destruct(Coordinator &ecs, Entity self) {
  ecs.addComponent<DeathMark>(self);
}

void take_damage(Coordinator &ecs, Entity self, Entity other) {
  std::ignore = other;
  mmEffect(SFX_HIT);
  ecs.getComponent<Health>(self).value--;
}

void wait_for_start() {
  while (1) {
    swiWaitForVBlank();
    scanKeys();
    // Burn RNG values
    rand();
    int pressed = keysDown();
    if (pressed & KEY_START)
      break;
  }
}

constexpr SpriteSize sprite_size(int width, int height) {
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

SpriteData::SpriteData(OamState *oam, const uint8_t *gfx, int width, int height,
                       int tiles,
                       unusual::id_manager<int, 16> &palette_index_manager,
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
SpriteData::~SpriteData() {
  oamFreeGfx(oam, gfx);
  palette_index_manager.release(palette_index);
}

void SpriteData::set_active_tile(int n) {
  assert(0 <= n and n < tiles);
  dmaCopy(gfx + SPRITE_SIZE_PIXELS(size) * n, vram_memory,
          SPRITE_SIZE_PIXELS(size));
}
