#include "nds/arm9/math.h"
#include "nds/arm9/sprite.h"
#include "nds/arm9/trig_lut.h"
#include "nds/arm9/video.h"
#include "nds/dma.h"
#include "nds/input.h"
#include "nds/touch.h"
#include "ndspp.hpp"
#include "tecs-system.hpp"
#include "tecs.hpp"
#include "unusual_id_manager.hpp"
#include <algorithm>
#include <alien1.h>
#include <array>
#include <cstdint>
#include <gl2d.h>
#include <nds.h>
#include <player.h>
#include <stdbool.h>
#include <stdio.h>
#include <unordered_set>

unusual::id_manager<int, 128> sprite_id_manager;
unusual::id_manager<int, 32> affine_index_manager;

using namespace nds;

struct Vec3 {
  fix x;
  fix y;
  fix z;
};

struct Transform {
  Vec3 pos;
  int32_t rotation;
};

struct Velocity {
  Vec3 v;
};

struct SpriteInfo {
  int id;
  int affine;
  int palette_index;
};

struct Alien {};

using namespace Tecs;

void make_sprite(Coordinator &ecs, Entity entity, void *graphics_offset,
                 SpriteSize size, SpriteColorFormat color_format,
                 int palette_index) {
  ecs.addComponent<SpriteInfo>(entity);
  const SpriteInfo &sprite_info = ecs.getComponent<SpriteInfo>(entity) =
      SpriteInfo{sprite_id_manager.allocate(), affine_index_manager.allocate()};
  // oamSetGfx(&oamMain, sprite_id.value, size, color_format, graphics_offset);
  oamSet(&oamMain, sprite_info.id, 5, 5, 0, palette_index, size, color_format,
         graphics_offset, sprite_info.affine, true, false, false, false, false);
}

void draw_sprites(Coordinator &ecs,
                  const std::unordered_set<Entity> &entities) {
  for (const Entity entity : entities) {
    const auto id = ecs.getComponent<SpriteInfo>(entity);
    const auto transform = ecs.getComponent<Transform>(entity);
    oamSetXY(&oamMain, id.id, static_cast<int32_t>(transform.pos.x),
             static_cast<int32_t>(transform.pos.y));
    oamRotateScale(&oamMain, id.affine, transform.rotation, 1 << 8, 1 << 8);
  }
}

void apply_velocity(Coordinator &ecs,
                    const std::unordered_set<Entity> &entities) {
  for (const Entity entity : entities) {
    auto &position = ecs.getComponent<Transform>(entity).pos;
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

  ecs.registerComponent<Transform>();
  ecs.registerComponent<Velocity>();
  ecs.registerComponent<SpriteInfo>();
  ecs.registerComponent<Alien>();
  ecs.registerComponent<PhysicsSystemTag>();
  ecs.registerComponent<RenderingSystemTag>();

  const auto rendering_system_interest =
      makeSystemInterest(ecs, ecs.componentMask<RenderingSystemTag>());
  const auto physics_system_interest =
      makeSystemInterest(ecs, ecs.componentMask<PhysicsSystemTag>());

  ecs.addComponents(ecs.newEntity(), SingleEntitySetSystem{draw_sprites},
                    RenderingSystemTag{},
                    InterestedClient{ecs.interests.registerInterests(
                        {{ecs.componentMask<SpriteInfo, Transform>()}})});

  ecs.addComponents(ecs.newEntity(), SingleEntitySetSystem{apply_velocity},
                    PhysicsSystemTag{},
                    InterestedClient{ecs.interests.registerInterests(
                        {{ecs.componentMask<Transform, Velocity>()}})});

  ecs.addComponents(
      ecs.newEntity(),
      SingleEntitySetSystem{
          [](Coordinator &ecs, const std::unordered_set<Entity> &entities) {
            for (const auto entity : entities) {
              ecs.getComponent<Transform>(entity).rotation += degreesToAngle(5);
            }
          }},
      PhysicsSystemTag{},
      InterestedClient{ecs.interests.registerInterests(
          {{ecs.componentMask<Transform, Alien>()}})});

  ecs.addComponents(
      ecs.newEntity(),
      SingleEntitySetSystem{
          [&touch_position](Coordinator &ecs,
                            const std::unordered_set<Entity> &entities) {
            if (keysHeld() & KEY_TOUCH) {
              for (const auto entity : entities) {
                Vec3 *velocity = &ecs.getComponent<Velocity>(entity).v;
                const Vec3 &position = ecs.getComponent<Transform>(entity).pos;
                velocity->x = fix::from_int(touch_position.px) - position.x;
                velocity->y = fix::from_int(touch_position.py) - position.y;

                normalizef32(reinterpret_cast<int32 *>(velocity));
              }
            }
          }},
      PhysicsSystemTag{},
      InterestedClient{
          ecs.interests.registerInterests({{ecs.componentMask<Alien>()}})});

  lcdMainOnBottom();
  videoSetMode(MODE_0_2D);

  vramSetBankA(VRAM_A_MAIN_SPRITE);
  oamInit(&oamMain, SpriteMapping_1D_32, true);

  u16 *alien_gfx;
  alien_gfx =
      oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);
  dmaCopy(alien1Tiles, alien_gfx, 32 * 32);

  // Load palettes
  vramSetBankF(VRAM_F_LCD);
  dmaCopy(alien1Pal, &VRAM_F_EXT_SPR_PALETTE[0][0], alien1PalLen);
  dmaCopy(playerPal, &VRAM_F_EXT_SPR_PALETTE[1][0], playerPalLen);
  vramSetBankF(VRAM_F_SPRITE_EXT_PALETTE);

  // dmaCopy(alien1Pal, SPRITE_PALETTE, alien1PalLen);

  const u8 *alien1TilesU8 = reinterpret_cast<const u8 *>(alien1Tiles);
  const u8 *wiggly = alien1TilesU8 + 32 * 32;

  std::array<Tecs::Entity, 5> aliens;
  std::for_each(aliens.begin(), aliens.end(),
                [&ecs](auto &e) { e = ecs.newEntity(); });
  {
    int x = 5;
    int y = 10;
    int a = degreesToAngle(0);
    for (const auto alien : aliens) {
      make_sprite(ecs, alien, alien_gfx, SpriteSize_32x32,
                  SpriteColorFormat_256Color, 0);
      ecs.addComponent<Alien>(alien);
      ecs.addComponent<Transform>(alien);
      ecs.getComponent<Transform>(alien) =
          Transform{fix::from_int(x), fix::from_int(y), a};
      ecs.addComponent<Velocity>(alien);

      ecs.getComponent<Velocity>(alien) =
          Velocity{{fix::from_float(1.5f), fix::from_float(1.5f)}};
      x += 20;
      y += 30;
      a += DEGREES_IN_CIRCLE / (2 * aliens.size());
    }
  }

  Entity player = ecs.newEntity();
  ecs.addComponents(player, Transform{Vec3{fix::from_int(SCREEN_WIDTH / 2),
                                           fix::from_int(SCREEN_HEIGHT / 2)},
                                      0});
  u16 *player_gfx =
      oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);
  dmaCopy(playerTiles, player_gfx, 32 * 32);

  // dmaCopy(playerPal, &VRAM_F_EXT_SPR_PALETTE[0][0], playerPalLen);

  make_sprite(ecs, player, player_gfx, SpriteSize_32x32,
              SpriteColorFormat_256Color, 1);

  // auto &position = ecs.getComponent<Position>(aliens[1]);
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
      dmaCopy(wiggly, alien_gfx, 32 * 32);
    } else {
      dmaCopy(alien1Tiles, alien_gfx, 32 * 32);
    }

    if (held & KEY_START)
      break;
    runSystems(ecs, physics_system_interest);
    if (held & KEY_TOUCH) {
      Vec3 &position = ecs.getComponent<Transform>(player).pos;
      position.x = fix::from_int(touch_position.px - 32);
      position.y = fix::from_int(touch_position.py - 32);
    }
  }
}
