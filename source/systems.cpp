#include "systems.hpp"
#include "components.hpp"
#include "ndspp.hpp"
#include "tecs.hpp"
#include "util.hpp"
#include <nds.h>
#include <nds/arm9/exceptions.h>

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
    const auto id = ecs.getComponent<SpriteInfo>(entity);
    const auto transform = ecs.getComponent<Position>(entity);
    oamSetXY(&oamMain, id.id, static_cast<int32_t>(transform.pos.x - id.width2),
             static_cast<int32_t>(transform.pos.y - id.height2));
  }
}

void following_ai(Coordinator &ecs,
                  const std::unordered_set<Entity> &entities) {
  for (const auto entity : entities) {
    Vec3 *velocity = &ecs.getComponent<Velocity>(entity).v;
    const Following &follow = ecs.getComponent<Following>(entity);
    const Vec3 &position = ecs.getComponent<Position>(entity).pos;
    const Vec3 &target_position = ecs.getComponent<Position>(follow.target).pos;

    velocity->x = target_position.x - position.x;
    velocity->y = target_position.y - position.y;

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
          printf("Collision between %d and %d\n", a, b);
          a_collision.callback(ecs, a, b);
        }
      }
    }
  }
}
