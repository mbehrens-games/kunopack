/******************************************************************************/
/* kunopack (KUNO-1 ROM Packer) - No Shinobi Knows Me 2025                    */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "art.h"
#include "rom.h"

/******************************************************************************/
/* main()                                                                     */
/******************************************************************************/
int main(int argc, char *argv[])
{
  int m; /* for testing */

  rom_clear();

  /* load test gif file */
  art_load_gif("dq3_witch.gif");

#if 0
  /* test that the palette was read correctly */
  for (m = 0; m < VDP_COLORS_PER_PAL; m++)
  {
    printf( "Palette Color %d: (%d, %d, %d)\n", m, 
            ((G_vdp_pals[m] >> 7) & 0xF8) | ((G_vdp_pals[m] >> 12) & 0x07), 
            ((G_vdp_pals[m] >> 2) & 0xF8) | ((G_vdp_pals[m] >> 7) & 0x07), 
            ((G_vdp_pals[m] << 3) & 0xF8) | ((G_vdp_pals[m] >> 2) & 0x07)
          );
  }
  
  /* test that the first cell is stored correctly */
  for (m = 0; m < VDP_BYTES_PER_CELL; m += 4)
  {
    printf("%d %d ", (G_vdp_cells[m + 0] >> 4) & 0x0F, G_vdp_cells[m + 0] & 0x0F);
    printf("%d %d ", (G_vdp_cells[m + 1] >> 4) & 0x0F, G_vdp_cells[m + 1] & 0x0F);
    printf("%d %d ", (G_vdp_cells[m + 2] >> 4) & 0x0F, G_vdp_cells[m + 2] & 0x0F);
    printf("%d %d ", (G_vdp_cells[m + 3] >> 4) & 0x0F, G_vdp_cells[m + 3] & 0x0F);
    printf("\n");
  }

  printf("\n");

  /* test that the second cell is stored correctly */
  for (m = VDP_BYTES_PER_CELL; m < 2 * VDP_BYTES_PER_CELL; m += 4)
  {
    printf("%d %d ", (G_vdp_cells[m + 0] >> 4) & 0x0F, G_vdp_cells[m + 0] & 0x0F);
    printf("%d %d ", (G_vdp_cells[m + 1] >> 4) & 0x0F, G_vdp_cells[m + 1] & 0x0F);
    printf("%d %d ", (G_vdp_cells[m + 2] >> 4) & 0x0F, G_vdp_cells[m + 2] & 0x0F);
    printf("%d %d ", (G_vdp_cells[m + 3] >> 4) & 0x0F, G_vdp_cells[m + 3] & 0x0F);
    printf("\n");
  }
#endif

#if 0
  /* set up test sprite def */
  G_vdp_sprite_defs[0].cell_index = 0;
  G_vdp_sprite_defs[0].dim_frames_anim =  0x0002;
  G_vdp_sprite_defs[0].dim_frames_anim |= (0x0001 << 2) & 0x000C;

  G_vdp_sprite_def_count += 1;
#endif

  /* write out rom file */
  rom_save("test.kn1");

  return 0;
}

