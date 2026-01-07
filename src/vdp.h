/******************************************************************************/
/* vdp.h (faux graphics chip)                                                 */
/******************************************************************************/

#ifndef VDP_H
#define VDP_H

/* palettes */
#define VDP_COLORS_PER_PAL  16
#define VDP_NUM_PALS        512
#define VDP_PALS_SIZE       (VDP_COLORS_PER_PAL * VDP_NUM_PALS)

extern unsigned short G_vdp_pals[VDP_PALS_SIZE];
extern short          G_vdp_pal_count;

/* cells */
#define VDP_CELL_W_H        8
#define VDP_PIXELS_PER_CELL (VDP_CELL_W_H * VDP_CELL_W_H)  /* 8x8 */
#define VDP_BYTES_PER_CELL  (VDP_PIXELS_PER_CELL / 2)

#define VDP_NUM_CELLS       (16 * 1024)
#define VDP_CELLS_SIZE      (VDP_BYTES_PER_CELL * VDP_NUM_CELLS)

extern unsigned char  G_vdp_cells[VDP_CELLS_SIZE];
extern short          G_vdp_cell_count;

/* definitions */
struct sprite_def
{
  unsigned short cell_index;
  unsigned short dim_frames_anim;
};

#define VDP_NUM_SPRITE_DEFS 1024

extern struct sprite_def  G_vdp_sprite_defs[VDP_NUM_SPRITE_DEFS];
extern short              G_vdp_sprite_def_count;

/* function declarations */
int vdp_reset();

#endif

