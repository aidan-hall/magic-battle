#include "tecs.hpp"
#include <alien1.h>
#include <gl2d.h>
#include <nds.h>
#include <stdbool.h>
#include <stdio.h>


int main(void) {
  lcdMainOnBottom();
  videoSetMode(MODE_0_2D);

  vramSetBankA(VRAM_A_MAIN_SPRITE);
  oamInit(&oamMain, SpriteMapping_1D_32, false);
  /* bool wiggly = false; */

  Tecs::Coordinator ecs;


  u16 *alien_gfx;
  alien_gfx =
      oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);
  dmaCopy(alien1Tiles, alien_gfx, 32 * 32);
  const u8 *alien1TilesU8 = reinterpret_cast<const u8 *>(alien1Tiles);
  const u8 *wiggly = alien1TilesU8 + 32 * 32;

  dmaCopy(alien1Pal, SPRITE_PALETTE, alien1PalLen);

  // consoleDemoInit();
  // iprintf("Hello World!\n");
  int n = degreesToAngle(50);

  while (1) {

    oamRotateScale(&oamMain, 0, n, 1 << 8, 1 << 8);
    oamSet(&oamMain, 0, 5, 5, 0, 0, SpriteSize_32x32,
           SpriteColorFormat_256Color, alien_gfx, 0, true, false, false, false,
           false);

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

    if (held & KEY_LEFT) {
      n += 50;
    }
    if (held & KEY_RIGHT) {
      n -= 50;
    }
  }
}
