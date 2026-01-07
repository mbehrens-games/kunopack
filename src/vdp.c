/******************************************************************************/
/* vdp.c (faux graphics chip)                                                 */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "vdp.h"

unsigned short  G_vdp_pals[VDP_PALS_SIZE];
short           G_vdp_pal_count;

unsigned char   G_vdp_cells[VDP_CELLS_SIZE];
short           G_vdp_cell_count;

struct sprite_def G_vdp_sprite_defs[VDP_NUM_SPRITE_DEFS];
short             G_vdp_sprite_def_count;

/******************************************************************************/
/* vdp_reset()                                                                */
/******************************************************************************/
int vdp_reset()
{
  int m;

  /* palettes */
  for (m = 0; m < VDP_PALS_SIZE; m++)
    G_vdp_pals[m] = 0x0000;

  G_vdp_pal_count = 0;

  /* cells */
  for (m = 0; m < VDP_CELLS_SIZE; m++)
    G_vdp_cells[m] = 0;

  G_vdp_cell_count = 0;

  /* definitions */
  for (m = 0; m < VDP_NUM_SPRITE_DEFS; m++)
  {
    G_vdp_sprite_defs[m].cell_index = 0x0000;
    G_vdp_sprite_defs[m].dim_frames_anim = 0x0000;
  }

  G_vdp_sprite_def_count = 0;

  return 0;
}

