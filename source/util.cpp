#include "util.hpp"
#include "components.hpp"
#include "ndspp.hpp"
#include "tecs.hpp"
#include "unusual_id_manager.hpp"
#include <nds.h>
#include <nds/arm9/sprite.h>

using namespace Tecs;
void make_sprite(Coordinator &ecs, Entity entity,
                 unusual::id_manager<int, SPRITE_COUNT> &sprite_id_manager,
                 SpriteData &sprite_data) {
  ecs.addComponent<SpriteInfo>(entity);
  const auto &[sprite_info, _] = ecs.addComponents(
      entity,
      SpriteInfo{sprite_id_manager.allocate(),
                 // Width and height are doubled to allow room for rotation
                 nds::fix::from_int(sprite_data.width) / 2, nds::fix::from_int(sprite_data.height) / 2},
      DeathCallback{
          [&sprite_id_manager](Tecs::Coordinator &ecs, Tecs::Entity self) {
            printf("Releasing sprite id of %d\n", self);
            const auto id = ecs.getComponent<SpriteInfo>(self).id;
            // Hide the sprite
            oamClearSprite(&oamMain, id);
            sprite_id_manager.release(id);
          }});
  oamSet(&oamMain, sprite_info.id, 5, 5, 0, sprite_data.palette_index, sprite_data.size, sprite_data.color_format,
         sprite_data.vram_memory, -1, false, false, false, false, false);
}

// void make_fireball(Tecs::Coordinator &ecs, Vec3 position, Vec3 velocity, uint16_t * gfx) {
//   Entity fireball = ecs.newEntity();
//   ecs.addComponents(fireball, Position{position}, Velocity{velocity});
// }
