#include "util.hpp"
#include "components.hpp"
#include "ndspp.hpp"
#include "unusual_id_manager.hpp"
#include <nds.h>

using namespace Tecs;
void make_sprite(Coordinator &ecs, Entity entity,
                 unusual::id_manager<int, SPRITE_COUNT> &sprite_id_manager,
                 void *graphics_offset, SpriteSize size,
                 SpriteColorFormat color_format, int palette_index, int width,
                 int height) {
  ecs.addComponent<SpriteInfo>(entity);
  const SpriteInfo &sprite_info = ecs.getComponent<SpriteInfo>(entity) =
      SpriteInfo{sprite_id_manager.allocate(),
                 // Width and height are doubled to allow room for rotation
                 nds::fix::from_int(width) / 2, nds::fix::from_int(height) / 2};
  oamSet(&oamMain, sprite_info.id, 5, 5, 0, palette_index, size, color_format,
         graphics_offset, -1, false, false, false, false, false);
}
