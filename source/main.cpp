#include "nds/arm9/sprite.h"
#include "nds/arm9/video.h"
#include "ndspp.hpp"
#include "tecs-system.hpp"
#include "tecs.hpp"
#include "unusual_id_manager.hpp"
#include <ZombieSprite_gfx.h>
#include <ZombieSprite_pal.h>
#include <PlayerSprite_gfx.h>
#include <PlayerSprite_pal.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <gl2d.h>
#include <nds.h>
#include <stdbool.h>
#include <stdio.h>
#include <unordered_set>

unusual::id_manager<int, SPRITE_COUNT> sprite_id_manager;
unusual::id_manager<int, MATRIX_COUNT> affine_index_manager;

using namespace nds;

struct Vec3 {
  fix x;
  fix y;
  fix z;
};

struct Position {
  Vec3 pos;
  int32_t rotation;
};

struct Velocity {
  Vec3 v;
};

// struct RotateScale {
//   // Affine index [0-31].
//   int32_t affine;
//   int32_t rotation;
//   // Default scale is unscaled.
//   int32_t x_scale = 1 << 8;
//   int32_t y_scale = 1 << 8;
// };

struct SpriteInfo {
  int id;
  // Width divided by 2
  fix width2;
  // Height divided by 2
  fix height2;
};

struct Zombie {};

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

using namespace Tecs;

void make_sprite(Coordinator &ecs, Entity entity, void *graphics_offset,
                 SpriteSize size, SpriteColorFormat color_format,
                 int palette_index, int width, int height) {
  ecs.addComponent<SpriteInfo>(entity);
  const SpriteInfo &sprite_info = ecs.getComponent<SpriteInfo>(entity) =
      SpriteInfo{sprite_id_manager.allocate(),
                 // Width and height are doubled to allow room for rotation
                 fix::from_int(width)/2, fix::from_int(height)/2};
  oamSet(&oamMain, sprite_info.id, 5, 5, 0, palette_index, size, color_format,
         graphics_offset, -1, false, false, false, false, false);
}

void draw_sprites(Coordinator &ecs,
                  const std::unordered_set<Entity> &entities) {
  for (const Entity entity : entities) {
    const auto id = ecs.getComponent<SpriteInfo>(entity);
    const auto transform = ecs.getComponent<Position>(entity);
    oamSetXY(&oamMain, id.id, static_cast<int32_t>(transform.pos.x - id.width2),
             static_cast<int32_t>(transform.pos.y - id.height2));
  }
}

void apply_velocity(Coordinator &ecs,
                    const std::unordered_set<Entity> &entities) {
  for (const Entity entity : entities) {
    auto &position = ecs.getComponent<Position>(entity).pos;
    const Vec3 &velocity = ecs.getComponent<Velocity>(entity).v;
    position.x = position.x + velocity.x;
    position.y = position.y + velocity.y;
  }
}

struct RenderingSystemTag {};

struct PhysicsSystemTag {};

int main(void) {
  consoleDemoInit();
  printf("Hello world.\n");
  printf("Hello world.\n");
  Tecs::Coordinator ecs;
  touchPosition touch_position;

  registerSystemComponents(ecs);

  ecs.registerComponent<Position>();
  ecs.registerComponent<Velocity>();
  ecs.registerComponent<SpriteInfo>();
  ecs.registerComponent<Zombie>();
  ecs.registerComponent<PhysicsSystemTag>();
  ecs.registerComponent<RenderingSystemTag>();

  const auto rendering_system_interest =
      makeSystemInterest(ecs, ecs.componentMask<RenderingSystemTag>());
  const auto physics_system_interest =
      makeSystemInterest(ecs, ecs.componentMask<PhysicsSystemTag>());

  ecs.addComponents(ecs.newEntity(), SingleEntitySetSystem{draw_sprites},
                    RenderingSystemTag{},
                    InterestedClient{ecs.interests.registerInterests(
                        {{ecs.componentMask<SpriteInfo, Position>()}})});

  ecs.addComponents(ecs.newEntity(), SingleEntitySetSystem{apply_velocity},
                    PhysicsSystemTag{},
                    InterestedClient{ecs.interests.registerInterests(
                        {{ecs.componentMask<Position, Velocity>()}})});

  ecs.addComponents(
      ecs.newEntity(),
      SingleEntitySetSystem{
          [&touch_position](Coordinator &ecs,
                            const std::unordered_set<Entity> &entities) {
            if (keysHeld() & KEY_TOUCH) {
              for (const auto entity : entities) {
                Vec3 *velocity = &ecs.getComponent<Velocity>(entity).v;
                const Vec3 &position = ecs.getComponent<Position>(entity).pos;
                velocity->x = fix::from_int(touch_position.px) - position.x;
                velocity->y = fix::from_int(touch_position.py) - position.y;

                normalizef32(reinterpret_cast<int32 *>(velocity));
              }
            }
          }},
      PhysicsSystemTag{},
      InterestedClient{
          ecs.interests.registerInterests({{ecs.componentMask<Zombie>()}})});

  lcdMainOnBottom();
  videoSetMode(MODE_0_2D);

  vramSetBankA(VRAM_A_MAIN_SPRITE);
  oamInit(&oamMain, SpriteMapping_1D_32, true);

  u16 *zombie_gfx;
  constexpr int zombie_width = 16;
  constexpr int zombie_height = 16;
  constexpr SpriteSize zombie_size = sprite_size(zombie_width, zombie_height);
  zombie_gfx = oamAllocateGfx(&oamMain, zombie_size, SpriteColorFormat_256Color);
  dmaCopy(ZombieSprite_gfx, zombie_gfx, SPRITE_SIZE_PIXELS(zombie_size));

  // Load palettes
  vramSetBankF(VRAM_F_LCD);
  dmaCopy(ZombieSprite_pal, &VRAM_F_EXT_SPR_PALETTE[0][0], ZombieSprite_pal_size);
  dmaCopy(PlayerSprite_pal, &VRAM_F_EXT_SPR_PALETTE[1][0],
          PlayerSprite_pal_size);
  vramSetBankF(VRAM_F_SPRITE_EXT_PALETTE);

  const u8 *ZombieSprite_gfxU8 = reinterpret_cast<const u8 *>(ZombieSprite_gfx);
  const u8 *wiggly = ZombieSprite_gfxU8 + SPRITE_SIZE_PIXELS(zombie_size);

  std::array<Tecs::Entity, 10> zombies;
  std::for_each(zombies.begin(), zombies.end(),
                [&ecs](auto &e) { e = ecs.newEntity(); });
  {
    int x = 5;
    int y = 10;
    int a = degreesToAngle(0);
    for (const auto zombie : zombies) {
      make_sprite(ecs, zombie, zombie_gfx, zombie_size, SpriteColorFormat_256Color,
                  0, zombie_width, zombie_height);
      ecs.addComponent<Zombie>(zombie);
      ecs.addComponent<Position>(zombie);
      ecs.getComponent<Position>(zombie) =
          Position{{fix::from_int(x), fix::from_int(y), 0}, a};
      ecs.addComponent<Velocity>(zombie);

      ecs.getComponent<Velocity>(zombie) =
          Velocity{{fix::from_float(1.5f), fix::from_float(1.5f), 0}};
      x += 20;
      y += 30;
      a += DEGREES_IN_CIRCLE / (2 * zombies.size());
    }
  }

  Entity player = ecs.newEntity();
  ecs.addComponents(player, Position{Vec3{fix::from_int(SCREEN_WIDTH / 2),
                                           fix::from_int(SCREEN_HEIGHT / 2), 0},
                                      0});
  constexpr int player_width = 32;
  constexpr int player_height = 32;
  constexpr SpriteSize player_size = sprite_size(player_width, player_height);
  u16 *player_gfx =
      oamAllocateGfx(&oamMain, player_size, SpriteColorFormat_256Color);
  dmaCopy(PlayerSprite_gfx, player_gfx, SPRITE_SIZE_PIXELS(player_size));

  // dmaCopy(playerPal, &VRAM_F_EXT_SPR_PALETTE[0][0], playerPalLen);

  make_sprite(ecs, player, player_gfx, player_size, SpriteColorFormat_256Color,
              1, player_width, player_height);

  // auto &position = ecs.getComponent<Position>(zombies[1]);
  // position.x = nds::fix(40);
  // position.y = nds::fix(40);

  while (1) {
    touchRead(&touch_position);

    runSystems(ecs, rendering_system_interest);

    swiWaitForVBlank();
    oamUpdate(&oamMain);
    scanKeys();
    int held = keysCurrent();

    if (held & KEY_A) {
      dmaCopy(wiggly, zombie_gfx, SPRITE_SIZE_PIXELS(zombie_size));
    } else {
      dmaCopy(ZombieSprite_gfx, zombie_gfx, SPRITE_SIZE_PIXELS(SpriteSize_16x16));
    }

    if (held & KEY_START)
      break;
    runSystems(ecs, physics_system_interest);

    printf("\x1b[10;0HTouch x = %04i, %04i\n", touch_position.rawx,
           touch_position.px);
    printf("Touch y = %04i, %04i\n", touch_position.rawy, touch_position.py);

    if (held & KEY_TOUCH) {
      Vec3 &position = ecs.getComponent<Position>(player).pos;
      position.x = fix::from_int(touch_position.px);
      position.y = fix::from_int(touch_position.py);
    }
  }
}
