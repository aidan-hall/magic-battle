#include "Sounds_bin.h"
#include "components.hpp"
#include "nds/arm9/sprite.h"
#include "nds/arm9/video.h"
#include "ndspp.hpp"
#include "soundbank.h"
#include "systems.hpp"
#include "tecs-system.hpp"
#include "tecs.hpp"
#include "unusual_id_manager.hpp"
#include "util.hpp"
#include <ExplosionSprite_gfx.h>
#include <ExplosionSprite_pal.h>
#include <FireballSprite_gfx.h>
#include <FireballSprite_pal.h>
#include <PlayerSprite_gfx.h>
#include <PlayerSprite_pal.h>
#include <StoneBackground_gfx.h>
#include <StoneBackground_pal.h>
#include <ZombieSprite_gfx.h>
#include <ZombieSprite_pal.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <gl2d.h>
#include <maxmod9.h>
#include <mm_types.h>
#include <nds.h>
#include <nds/arm9/background.h>
#include <nds/arm9/console.h>
#include <nds/arm9/decompress.h>
#include <nds/arm9/input.h>
#include <nds/arm9/videoGL.h>
#include <nds/dma.h>
#include <nds/input.h>
#include <nds/interrupts.h>
#include <nds/system.h>
#include <nds/timers.h>
#include <nds/touch.h>
#include <stdbool.h>
#include <stdio.h>
#include <unordered_map>
#include <unordered_set>

unusual::id_manager<int, SPRITE_COUNT> sprite_id_manager;
unusual::id_manager<int, MATRIX_COUNT> affine_index_manager;
unusual::id_manager<int, 16> palette_index_manager;

enum class Spell {
  Fireball,
  Teleport,
  Explosion,
};

std::unordered_map<Spell, const char *> spell_strings = {
    {Spell::Fireball, "fireball"},
    {Spell::Teleport, "teleport"},
    {Spell::Explosion, "explosion"},
};

Spell selected_spell = Spell::Fireball;

constexpr nds::fix MAX_MAGIC = nds::fix::from_float(100.0f);
constexpr nds::fix MAGIC_BUILD_RATE = nds::fix::from_float(0.3f);

constexpr nds::fix FIREBALL_MAGIC = nds::fix::from_float(25.0f);
constexpr nds::fix TELEPORT_MAGIC = nds::fix::from_float(0.0f);
constexpr nds::fix EXPLOSION_MAGIC = nds::fix::from_float(50.0f);

nds::fix magic_meter = MAX_MAGIC;

constexpr nds::fix FIX_SCREEN_WIDTH = nds::fix::from_int(SCREEN_WIDTH);
constexpr nds::fix FIX_SCREEN_HEIGHT = nds::fix::from_int(SCREEN_HEIGHT);

constexpr int32_t FPS = 60;
constexpr nds::fix FRAME_DURATION = nds::fix::from_float(1.0f / FPS);
constexpr nds::fix ZOMBIE_SPEED = nds::fix::from_float(0.25f);
constexpr int32_t ZOMBIE_INCREASE_PERIOD = 20 * FPS;
constexpr int INITIAL_ZOMBIE_RATE = 0.015f * RAND_MAX;
constexpr int ZOMBIE_INCREASE = 0.002f * RAND_MAX;

using namespace nds;

using namespace Tecs;

int main(void) {
  srand(PersonalData->rtcOffset % UINT16_MAX);
  // NDS Setup
  consoleDemoInit();

  lcdMainOnBottom();
  videoSetMode(MODE_5_2D);

  vramSetBankA(VRAM_A_MAIN_BG);
  int bg3 = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
  dmaCopy(StoneBackground_gfx, bgGetGfxPtr(bg3), StoneBackground_gfx_size);
  // dmaCopy(StoneBackground_map, bgGetMapPtr(bg3), StoneBackground_map_size);
  dmaCopy(StoneBackground_pal, &BG_PALETTE[0], StoneBackground_pal_size);

  oamInit(&oamMain, SpriteMapping_1D_32, true);

  // Load palettes and sprite data
  vramSetBankF(VRAM_F_LCD);
  SpriteData zombie_sprite(&oamMain, ZombieSprite_gfx, 16, 16, 4,
                           palette_index_manager, SpriteColorFormat_256Color,
                           ZombieSprite_pal, ZombieSprite_pal_size,
                           VRAM_F_EXT_SPR_PALETTE);
  SpriteData player_sprite(&oamMain, PlayerSprite_gfx, 16, 16, 1,
                           palette_index_manager, SpriteColorFormat_256Color,
                           PlayerSprite_pal, PlayerSprite_pal_size,
                           VRAM_F_EXT_SPR_PALETTE);
  SpriteData fireball_sprite(&oamMain, FireballSprite_gfx, 8, 8, 1,
                             palette_index_manager, SpriteColorFormat_256Color,
                             FireballSprite_pal, FireballSprite_pal_size,
                             VRAM_F_EXT_SPR_PALETTE);
  SpriteData explosion_sprite(&oamMain, ExplosionSprite_gfx, 64, 64, 1,
                              palette_index_manager, SpriteColorFormat_256Color,
                              ExplosionSprite_pal, ExplosionSprite_pal_size,
                              VRAM_F_EXT_SPR_PALETTE);
  vramSetBankF(VRAM_F_SPRITE_EXT_PALETTE);

  mmInitDefaultMem((mm_addr)Sounds_bin);
  mmLoadEffect(SFX_EXPLOSION);
  mmLoadEffect(SFX_TELEPORT);
  mmLoadEffect(SFX_HIT);
  mmLoadEffect(SFX_FIREBALL);

  consoleClear();
  printf("Magic Battle NDS\n\nArt: Aidan Hall\nSound: Aidan Hall\nCode: Aidan "
         "Hall\n(C) Aidan Hall 2023.\n\nPress start.\n");
  wait_for_start();

  while (1) {
    printf("Controls:\n\nTap to use a spell.\nNormally: Fireball.\nHolding "
           "Left or Y: Teleport."
           "\nHolding Right or A: Explosion (at player's location)\n"
           "Spells cost magic, which recharges over time.\nYou "
           "can take 10 hits.\n\nPress select to quit.\nPress start to pause.\n\nPress start "
           "to start!\n");
    wait_for_start();
    // ECS Setup
    Tecs::Coordinator ecs;
    registerSystemComponents(ecs);

    const ComponentMask POSITION_COMPONENT = ecs.registerComponent<Position>();
    const ComponentMask VELOCITY_COMPONENT = ecs.registerComponent<Velocity>();
    const ComponentMask SPRITEINFO_COMPONENT =
        ecs.registerComponent<SpriteInfo>();
    // const ComponentMask ZOMBIE_COMPONENT =
    ecs.registerComponent<Zombie>();
    const ComponentMask PHYSICSSYSTEMTAG_COMPONENT =
        ecs.registerComponent<PhysicsSystemTag>();
    const ComponentMask RENDERINGSYSTEMTAG_COMPONENT =
        ecs.registerComponent<RenderingSystemTag>();
    const ComponentMask COLLISION_COMPONENT =
        ecs.registerComponent<Collision>();
    const ComponentMask ADMINSYSTEMTAG_COMPONENT =
        ecs.registerComponent<AdminSystemTag>();
    const ComponentMask CLEANUPSYSTEMTAG_COMPONENT =
        ecs.registerComponent<CleanupSystemTag>();
    // const ComponentMask FINALCLEANUPSYSTEMTAG_COMPONENT =
    //     ecs.registerComponent<FinalCleanupSystemTag>();
    const ComponentMask DEATHMARK_COMPONENT =
        ecs.registerComponent<DeathMark>();

    const ComponentMask FOLLOWING_COMPONENT =
        ecs.registerComponent<Following>();
    // const ComponentMask AFFINE_COMPONENT = ecs.registerComponent<Affine>();
    const ComponentMask TIMERCALLBACK_COMPONENT =
        ecs.registerComponent<TimerCallback>();
    const ComponentMask HEALTH_COMPONENT = ecs.registerComponent<Health>();

    const auto rendering_system_interest =
        makeSystemInterest(ecs, RENDERINGSYSTEMTAG_COMPONENT);
    const auto physics_system_interest =
        makeSystemInterest(ecs, PHYSICSSYSTEMTAG_COMPONENT);
    const auto admin_system_interest =
        makeSystemInterest(ecs, ADMINSYSTEMTAG_COMPONENT);
    const auto cleanup_system_interest =
        makeSystemInterest(ecs, CLEANUPSYSTEMTAG_COMPONENT);
    // const auto finalcleanup_system_interest =
    //     makeSystemInterest(ecs, FINALCLEANUPSYSTEMTAG_COMPONENT);

    ecs.addComponents(ecs.newEntity(), SingleEntitySetSystem{draw_sprites},
                      RenderingSystemTag{},
                      InterestedClient{ecs.interests.registerInterests(
                          {{POSITION_COMPONENT | SPRITEINFO_COMPONENT}})});

    // ecs.addComponents(
    //     ecs.newEntity(), PerEntitySystem{affine_rendering},
    //     RenderingSystemTag{},
    //     InterestedClient{ecs.interests.registerInterests({{AFFINE_COMPONENT}})});

    ecs.addComponents(ecs.newEntity(), SingleEntitySetSystem{apply_velocity},
                      PhysicsSystemTag{},
                      InterestedClient{ecs.interests.registerInterests(
                          {{POSITION_COMPONENT | VELOCITY_COMPONENT}})});

    ecs.addComponents(ecs.newEntity(), SingleEntitySetSystem{following_ai},
                      PhysicsSystemTag{},
                      InterestedClient{ecs.interests.registerInterests(
                          {{FOLLOWING_COMPONENT}})});

    ecs.addComponents(ecs.newEntity(),
                      SingleEntitySetSystem{circular_collision_detection},
                      PhysicsSystemTag{},
                      InterestedClient{ecs.interests.registerInterests(
                          {{POSITION_COMPONENT | COLLISION_COMPONENT}})});

    ecs.addComponents(
        ecs.newEntity(),
        SingleEntitySetSystem{
            [](Coordinator &ecs, const std::unordered_set<Entity> &entities) {
              const uint32_t now = cpuGetTiming();
              for (const auto entity : entities) {

                const auto tc = ecs.getComponent<TimerCallback>(entity);
                if (tc.time <= now) {
                  tc.callback(ecs, entity);
                }
              }
            }},
        AdminSystemTag{},
        InterestedClient{
            ecs.interests.registerInterests({{TIMERCALLBACK_COMPONENT}})});

    ecs.addComponents(
        ecs.newEntity(),
        SingleEntitySetSystem{
            [](Coordinator &ecs, const std::unordered_set<Entity> &entities) {
              for (const auto entity : entities) {

                const auto health = ecs.getComponent<Health>(entity);
                if (health.value <= 0) {
                  // TODO: Resolve this in 1 frame
                  ecs.addComponent<DeathMark>(entity);
                  // ecs.queueDestroyEntity(entity);
                }
              }
            }},
        AdminSystemTag{},
        InterestedClient{
            ecs.interests.registerInterests({{HEALTH_COMPONENT}})});

    // ecs.addComponents(
    //     ecs.newEntity(),
    //     PerEntitySystem{[](Coordinator &ecs, const Entity entity) {
    //       Collision &collision = ecs.getComponent<Collision>(entity);
    //       const nds::fix &width2 =
    //       ecs.getComponent<SpriteInfo>(entity).width2; const Affine &affine =
    //       ecs.getComponent<Affine>(entity); const nds::fix scaled_radius =
    //       nds::fix{affine.scale << 4} * width2; printf("scaled radius: %f\n",
    //       static_cast<float>(scaled_radius)); collision.radius_squared =
    //       scaled_radius * scaled_radius;
    //     }},
    //     PhysicsSystemTag{},
    //     InterestedClient{ecs.interests.registerInterests(
    //         {{COLLISION_COMPONENT | AFFINE_COMPONENT |
    //         SPRITEINFO_COMPONENT}})});

    // ecs.addComponents(ecs.newEntity(),
    //                   PerEntitySystem{[](Coordinator &ecs, const Entity
    //                   entity)
    //                   {
    //                     Affine &affine = ecs.getComponent<Affine>(entity);
    //                     const int32_t rate =
    //                         ecs.getComponent<GrowthRate>(entity).rate;
    //                     affine.scale += rate;
    //                     if (affine.scale > 1 << 10) {
    //                       ecs.addComponent<DeathMark>(entity);
    //                     }
    //                   }},
    //                   PhysicsSystemTag{},
    //                   InterestedClient{ecs.interests.registerInterests(
    //                       {{AFFINE_COMPONENT | GROWTHRATE_COMPONENT}})});

    // Death Mark Handling.
    ecs.addComponents(ecs.newEntity(), PerEntitySystem{sprite_id_reclamation},
                      CleanupSystemTag{},
                      InterestedClient{ecs.interests.registerInterests(
                          {{DEATHMARK_COMPONENT | SPRITEINFO_COMPONENT}})});
    // ecs.addComponents(ecs.newEntity(),
    // PerEntitySystem{affine_index_reclamation},
    //                   AdminSystemTag{},
    //                   InterestedClient{ecs.interests.registerInterests(
    //                       {{DEATHMARK_COMPONENT | AFFINE_COMPONENT}})});

    ecs.addComponents(
        ecs.newEntity(),
        PerEntitySystem{[](Coordinator &ecs, const Entity entity) {
          ecs.queueDestroyEntity(entity);
        }},
        CleanupSystemTag{},
        InterestedClient{
            ecs.interests.registerInterests({{DEATHMARK_COMPONENT}})});

    // // Final cleanup system
    // ecs.addComponents(
    //     ecs.newEntity(),
    //     PerEntitySystem{[](Coordinator &ecs, const Entity entity) {
    //       printf("Cleaning up entity: %d\n", entity);
    //       ecs.addComponent<DeathMark>(entity);
    //     }},
    //     FinalCleanupSystemTag{},
    //     InterestedClient{ecs.interests.registerInterests({{~0}})});

    // Player target setup
    constexpr Vec3 player_start_pos = Vec3{fix::from_int(SCREEN_WIDTH / 2),
                                       fix::from_int(SCREEN_HEIGHT / 2), 0};
    Entity player_target = ecs.newEntity();
    ecs.addComponents(player_target, Position{player_start_pos});

    // Player setup
    Entity player = ecs.newEntity();
    printf("Player is entity %d\n", player);
    ecs.addComponents(player, Position{player_start_pos}, Velocity{},
                      Following{player_target, nds::fix::from_float(5.0f)},
                      Health{10},
                      Collision{ZOMBIE_LAYER, PLAYER_LAYER,
                                radius_squared_from_diameter(
                                    nds::fix::from_int(player_sprite.width)),
                                take_damage});

    make_sprite(ecs, player, sprite_id_manager, player_sprite);

    // Zombie setup

    int zombie_rate = INITIAL_ZOMBIE_RATE;
    int16_t zombie_clock = 0;
    int16_t zombie_level = 1;
    nds::fix alive_clock = {0};
    cpuStartTiming(0);
    while (1) {
      alive_clock += FRAME_DURATION;
      runSystems(ecs, rendering_system_interest);
      // printf("Player health: %d\n", ecs.getComponent<Health>(player).value);

      swiWaitForVBlank();
      consoleClear();
      oamUpdate(&oamMain);
      scanKeys();
      int held = keysCurrent();

      if (held & KEY_SELECT)
        break;

      if (held & KEY_A) {
        zombie_sprite.set_active_tile(1);
      } else {
        zombie_sprite.set_active_tile(0);
      }

      int pressed = keysDown();
      if (pressed & KEY_START) {
        printf("Paused. Press start to resume.\n");
        wait_for_start();
      }

      runSystems(ecs, physics_system_interest);

      if (held & (KEY_LEFT | KEY_Y)) {
        selected_spell = Spell::Teleport;
      } else if (held & (KEY_A | KEY_RIGHT)) {
        selected_spell = Spell::Explosion;
      } else {
        selected_spell = Spell::Fireball;
      }

      int down = keysDown();
      touchPosition touch_position;
      if (down & KEY_TOUCH) {
        touchRead(&touch_position);
        Vec3 &position = ecs.getComponent<Position>(player_target).pos;
        Vec3 target_position;
        target_position.x = fix::from_int(touch_position.px);
        target_position.y = fix::from_int(touch_position.py);
        if (selected_spell == Spell::Teleport and
            magic_meter > TELEPORT_MAGIC) {
          // teleport
          position = target_position;
          magic_meter -= TELEPORT_MAGIC;
          mmEffect(SFX_TELEPORT);
        } else if (selected_spell == Spell::Fireball and
                   magic_meter > FIREBALL_MAGIC) {

          make_fireball(ecs, position, target_position, sprite_id_manager,
                        fireball_sprite);
          mmEffect(SFX_FIREBALL);
          magic_meter -= FIREBALL_MAGIC;
        } else if (selected_spell == Spell::Explosion and
                   magic_meter > EXPLOSION_MAGIC) {
          make_explosion(ecs, position, sprite_id_manager, explosion_sprite);
          mmEffect(SFX_EXPLOSION);
          magic_meter -= EXPLOSION_MAGIC;
        }
      }

      if (magic_meter < MAX_MAGIC) {
        magic_meter += MAGIC_BUILD_RATE;
      } else {
        magic_meter = MAX_MAGIC;
      }

      int8_t player_health = ecs.getComponent<Health>(player).value;
      if (player_health <= 0) {
        break;
      }
      printf("Time Alive: %f\n\nHealth: %d\nMagic: %f\n\nFireball: "
             "%ld\nTeleport (Left/Y): %ld\nExplosion (Right/A): "
             "%ld\nSelected spell: %s\n\nZombie Level: %d",
             static_cast<float>(alive_clock), player_health,
             static_cast<float>(magic_meter),
             static_cast<int32_t>(FIREBALL_MAGIC),
             static_cast<int32_t>(TELEPORT_MAGIC),
             static_cast<int32_t>(EXPLOSION_MAGIC),
             spell_strings.at(selected_spell), zombie_level);

      // Increase zombie rate
      zombie_clock += 1;
      if (zombie_clock > ZOMBIE_INCREASE_PERIOD) {
        zombie_rate += ZOMBIE_INCREASE;
        zombie_level += 1;
        zombie_clock = 0;
      }

      // Randomly spawn a zombie

      if (rand() < zombie_rate) {
        constexpr nds::fix OFFSCREEN_MARGIN = nds::fix::from_float(5.0f);
        Vec3 zombie_position = {};
        switch (rand() % 4) {
        case 0:
          // on the left
          zombie_position.x = nds::fix{0} - OFFSCREEN_MARGIN;
          zombie_position.y = nds::fix::from_int(rand() % SCREEN_HEIGHT);
          break;
        case 1:
          // on the right
          zombie_position.x = FIX_SCREEN_WIDTH + OFFSCREEN_MARGIN;
          zombie_position.y = nds::fix::from_int(rand() % SCREEN_HEIGHT);
          break;
        case 2:
          // on the top
          zombie_position.x = nds::fix::from_int(rand() % SCREEN_WIDTH);
          zombie_position.y = nds::fix{0} - OFFSCREEN_MARGIN;
          break;
        case 3:
          // on the bottom
          zombie_position.x = nds::fix::from_int(rand() % SCREEN_WIDTH);
          zombie_position.y = FIX_SCREEN_HEIGHT + OFFSCREEN_MARGIN;
          break;
        }
        make_zombie(ecs, zombie_position, player, ZOMBIE_SPEED,
                    sprite_id_manager, zombie_sprite);
      }

      runSystems(ecs, admin_system_interest);
      runSystems(ecs, cleanup_system_interest);
      ecs.destroyQueued();
    }

    consoleClear();
    oamClear(&oamMain, 0, SPRITE_COUNT - 1);
    sprite_id_manager = {};
    affine_index_manager = {};
    printf("Game Over!\nYou survived for:\n%f seconds.\n\n",
           static_cast<float>(alive_clock));
  }
}
