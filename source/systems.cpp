#include "systems.hpp"
#include "components.hpp"
#include "ndspp.hpp"
#include "tecs.hpp"
#include "util.hpp"
#include <nds.h>
#include <nds/arm9/exceptions.h>
#include <nds/arm9/sprite.h>

extern unusual::id_manager<int, SPRITE_COUNT> sprite_id_manager;
extern unusual::id_manager<int, MATRIX_COUNT> affine_index_manager;
extern unusual::id_manager<int, 16> palette_index_manager;

using namespace Tecs;
void apply_velocity(Tecs::Coordinator &ecs,
                    const std::unordered_set<Tecs::Entity> &entities) {
  for (const Entity entity : entities) {
    auto &position = ecs.getComponent<Position>(entity).pos;
    const Vec3 &velocity = ecs.getComponent<Velocity>(entity).v;
    position.x = position.x + velocity.x;
    position.y = position.y + velocity.y;
  }
}

void draw_sprites(Coordinator &ecs,
                  const std::unordered_set<Entity> &entities) {
  for (const Entity entity : entities) {
    const auto info = ecs.getComponent<SpriteInfo>(entity);
    const auto position = ecs.getComponent<Position>(entity).pos;
    if (nds::fix::from_int(0) <= position.x and
        position.x <= nds::fix::from_int(SCREEN_WIDTH) and
        nds::fix::from_int(0) <= position.y and
        position.y <= nds::fix::from_int(SCREEN_HEIGHT)) {
      oamSetHidden(&oamMain, info.id, false);
      oamSetXY(&oamMain, info.id,
               static_cast<int32_t>(position.x - info.width2),
               static_cast<int32_t>(position.y - info.height2));
    } else {
      oamSetHidden(&oamMain, info.id, true);
    }
  }
}

constexpr nds::fix FOLLOW_CUTOFF = nds::fix::from_float(3.0f);

void following_ai(Coordinator &ecs,
                  const std::unordered_set<Entity> &entities) {
  for (const auto entity : entities) {
    Vec3 *velocity = &ecs.getComponent<Velocity>(entity).v;
    const Following &follow = ecs.getComponent<Following>(entity);
    const Vec3 &position = ecs.getComponent<Position>(entity).pos;
    const Vec3 &target_position = ecs.getComponent<Position>(follow.target).pos;

    velocity->x = target_position.x - position.x;
    // Don't move if the target is too close.
    if (nds::fix::abs(velocity->x) <= FOLLOW_CUTOFF) {
      velocity->x = {0};
    }
    velocity->y = target_position.y - position.y;
    if (nds::fix::abs(velocity->y) <= FOLLOW_CUTOFF) {
      velocity->y = {0};
    }

    normalizef32(reinterpret_cast<int32 *>(velocity));
    velocity->x = velocity->x * follow.speed;
    velocity->y = velocity->y * follow.speed;
  }
}

void circular_collision_detection(Coordinator &ecs,
                                  const std::unordered_set<Entity> &entities) {
  for (const auto a : entities) {
    const Collision &a_collision = ecs.getComponent<Collision>(a);
    const Vec3 &a_position = ecs.getComponent<Position>(a).pos;
    for (const auto b : entities) {
      if (b == a)
        continue;

      const Collision &b_collision = ecs.getComponent<Collision>(b);
      if ((a_collision.mask & b_collision.layer) != 0) {
        const Vec3 &b_position = ecs.getComponent<Position>(b).pos;
        if (circle_circle(a_position, a_collision.radius_squared, b_position,
                          b_collision.radius_squared)) {
          a_collision.callback(ecs, a, b);
        }
      }
    }
  }
}

void sprite_id_reclamation(Coordinator &ecs, const Entity entity) {
  const auto id = ecs.getComponent<SpriteInfo>(entity).id;
  // Hide the sprite
  oamClearSprite(&oamMain, id);
  sprite_id_manager.release(id);
}
void affine_index_reclamation(Coordinator &ecs, const Entity entity) {
  const auto index = ecs.getComponent<Affine>(entity).affine_index;
  affine_index_manager.release(index);
}

void affine_rendering(Coordinator &ecs, const Entity entity) {
  const auto affine = ecs.getComponent<Affine>(entity);
  printf("entity: %d scale: %ld index: %ld\n", entity, affine.scale,
         affine.affine_index);
  oamRotateScale(&oamMain, affine.affine_index, affine.rotation, affine.scale,
                 affine.scale);
}
