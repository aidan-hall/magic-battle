#include "components.hpp"
#include "nds/arm9/sprite.h"
#include "nds/arm9/video.h"
#include "ndspp.hpp"
#include "systems.hpp"
#include "tecs-system.hpp"
#include "tecs.hpp"
#include "unusual_id_manager.hpp"
#include "util.hpp"
#include <FireballSprite_gfx.h>
#include <FireballSprite_pal.h>
#include <PlayerSprite_gfx.h>
#include <PlayerSprite_pal.h>
#include <ZombieSprite_gfx.h>
#include <ZombieSprite_pal.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <gl2d.h>
#include <nds.h>
#include <nds/arm9/console.h>
#include <nds/arm9/input.h>
#include <nds/arm9/videoGL.h>
#include <nds/dma.h>
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
};

std::unordered_map<Spell, const char *> spell_strings = {
    {Spell::Fireball, "fireball"},
    {Spell::Teleport, "teleport"},
};

Spell selected_spell = Spell::Fireball;

constexpr nds::fix MAX_MAGIC = nds::fix::from_float(100.0f);
constexpr nds::fix MAGIC_BUILD_RATE = nds::fix::from_float(0.3f);
constexpr nds::fix FIREBALL_MAGIC = nds::fix::from_float(25.0f);
constexpr nds::fix TELEPORT_MAGIC = nds::fix::from_float(50.0f);
nds::fix magic_meter = MAX_MAGIC;

constexpr nds::fix FIX_SCREEN_WIDTH = nds::fix::from_int(SCREEN_WIDTH);
constexpr nds::fix FIX_SCREEN_HEIGHT = nds::fix::from_int(SCREEN_HEIGHT);

constexpr nds::fix ZOMBIE_SPEED = nds::fix::from_float(0.25f);

using namespace nds;

using namespace Tecs;

int main(void) {
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
  const ComponentMask COLLISION_COMPONENT = ecs.registerComponent<Collision>();
  const ComponentMask DEATHCALLBACK_COMPONENT =
      ecs.registerComponent<DeathCallback>();
  const ComponentMask ADMINSYSTEMTAG_COMPONENT =
      ecs.registerComponent<AdminSystemTag>();
  const ComponentMask DEATHMARK_COMPONENT = ecs.registerComponent<DeathMark>();

  const ComponentMask FOLLOWING_COMPONENT = ecs.registerComponent<Following>();

  const auto rendering_system_interest =
      makeSystemInterest(ecs, RENDERINGSYSTEMTAG_COMPONENT);
  const auto physics_system_interest =
      makeSystemInterest(ecs, PHYSICSSYSTEMTAG_COMPONENT);
  const auto admin_system_interest =
      makeSystemInterest(ecs, ADMINSYSTEMTAG_COMPONENT);

  ecs.addComponents(ecs.newEntity(), SingleEntitySetSystem{draw_sprites},
                    RenderingSystemTag{},
                    InterestedClient{ecs.interests.registerInterests(
                        {{POSITION_COMPONENT | SPRITEINFO_COMPONENT}})});

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

  // Death Mark Handling.
  ecs.addComponents(ecs.newEntity(),
                    PerEntitySystem{[](Coordinator &ecs, const Entity entity) {
                      ecs.queueDestroyEntity(entity);
                    }},
                    AdminSystemTag{},
                    InterestedClient{ecs.interests.registerInterests(
                        {{DEATHMARK_COMPONENT, DEATHCALLBACK_COMPONENT}})});
  ecs.addComponents(ecs.newEntity(),
                    PerEntitySystem{[](Coordinator &ecs, const Entity entity) {
                      ecs.getComponent<DeathCallback>(entity).callback(ecs,
                                                                       entity);
                      ecs.queueDestroyEntity(entity);
                    }},
                    AdminSystemTag{},
                    InterestedClient{ecs.interests.registerInterests(
                        {{DEATHMARK_COMPONENT | DEATHCALLBACK_COMPONENT}})});

  consoleDemoInit();

  lcdMainOnBottom();
  videoSetMode(MODE_0_2D);

  vramSetBankA(VRAM_A_MAIN_SPRITE);
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
  vramSetBankF(VRAM_F_SPRITE_EXT_PALETTE);

  // Player target setup
  const Vec3 player_start_pos = Vec3{fix::from_int(SCREEN_WIDTH / 2),
                                     fix::from_int(SCREEN_HEIGHT / 2), 0};
  Entity player_target = ecs.newEntity();
  ecs.addComponents(player_target, Position{player_start_pos});

  // Player setup
  Entity player = ecs.newEntity();
  ecs.addComponents(player, Position{player_start_pos}, Velocity{},
                    Following{player_target, nds::fix::from_float(5.0f)});

  make_sprite(ecs, player, sprite_id_manager, player_sprite);

  ecs.addComponents(player,
                    Collision{0, 0,
                              radius_squared_from_diameter(
                                  nds::fix::from_int(player_sprite.width)),
                              self_destruct});

  // Zombie setup
  std::array<Tecs::Entity, 20> zombies;
  std::for_each(zombies.begin(), zombies.end(),
                [&ecs](auto &e) { e = ecs.newEntity(); });
  {
    int x = SCREEN_WIDTH - 5;
    int y = 10;

    for (int i = 0; i < 3; ++i) {
      make_zombie(ecs, Vec3{nds::fix::from_int(x), nds::fix::from_int(y), {}},
                  player, ZOMBIE_SPEED, sprite_id_manager, zombie_sprite);
      x -= 20;
      y += 30;
    }
  }

  while (1) {
    runSystems(ecs, rendering_system_interest);

    swiWaitForVBlank();
    oamUpdate(&oamMain);
    scanKeys();
    int held = keysCurrent();

    if (held & KEY_A) {
      zombie_sprite.set_active_tile(1);
    } else {
      zombie_sprite.set_active_tile(0);
    }

    consoleClear();

    if (held & KEY_START)
      break;
    runSystems(ecs, physics_system_interest);

    if (held & (KEY_L | KEY_R)) {
      selected_spell = Spell::Teleport;
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
      if (selected_spell == Spell::Teleport and magic_meter > TELEPORT_MAGIC) {
        // teleport
        position = target_position;
        magic_meter -= TELEPORT_MAGIC;
      } else if (selected_spell == Spell::Fireball and
                 magic_meter > FIREBALL_MAGIC) {

        make_fireball(ecs, position, target_position, sprite_id_manager,
                      fireball_sprite);
        magic_meter -= FIREBALL_MAGIC;
      }
    }

    if (magic_meter < MAX_MAGIC) {
      magic_meter += MAGIC_BUILD_RATE;
    } else {
      magic_meter = MAX_MAGIC;
    }

    printf("Magic: %f\n\nFireball: %f\nTeleport: %f\n\nSelected spell: %s\n",
           static_cast<float>(magic_meter), static_cast<float>(FIREBALL_MAGIC),
           static_cast<float>(TELEPORT_MAGIC),
           spell_strings.at(selected_spell));

    // Randomly spawn a zombie
    if (rand() < 0.015f * RAND_MAX) {
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
      make_zombie(ecs, zombie_position, player, ZOMBIE_SPEED, sprite_id_manager, zombie_sprite);
    }

    runSystems(ecs, admin_system_interest);
    ecs.destroyQueued();
  }
}
