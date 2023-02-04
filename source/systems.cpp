#include "components.hpp"
#include "systems.hpp"
#include <nds.h>

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
