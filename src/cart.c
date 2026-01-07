/******************************************************************************/
/* cart.c (faux game cartridge)                                               */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "cart.h"

#include "vdp.h"

/******************************************************************************/
/* cart save()                                                               */
/******************************************************************************/
int cart_save(char* filename)
{
  FILE* fp;

  char signature[8];

  unsigned int chunk_size;

  /* make sure filename is valid */
  if (filename == NULL)
    return 1;

  /* open the file */
  fp = fopen(filename, "wb");

  if (fp == NULL)
    return 1;

  /* write cart header */
  signature[0] = 'K';
  signature[1] = 'U';
  signature[2] = 'N';
  signature[3] = 'O';
  signature[4] = '1';
  signature[5] = 'R';
  signature[6] = 'O';
  signature[7] = 'M';

  if (fwrite(signature, sizeof(char), 8, fp) < 8)
    return 1;

  /* write palettes chunk */
  if (fwrite(&G_vdp_pal_count, sizeof(short), 1, fp) < 1)
    return 1;

  chunk_size = G_vdp_pal_count * VDP_COLORS_PER_PAL;

  if (fwrite(&G_vdp_pals[0], sizeof(unsigned short), chunk_size, fp) < chunk_size)
    return 1;

  /* write cells chunk */
  if (fwrite(&G_vdp_cell_count, sizeof(short), 1, fp) < 1)
    return 1;

  chunk_size = G_vdp_cell_count * VDP_BYTES_PER_CELL;

  if (fwrite(&G_vdp_cells[0], sizeof(unsigned char), chunk_size, fp) < chunk_size)
    return 1;

  /* write sprite defs chunk */
  if (fwrite(&G_vdp_sprite_def_count, sizeof(short), 1, fp) < 1)
    return 1;

  chunk_size = G_vdp_sprite_def_count;

  if (fwrite(&G_vdp_sprite_defs[0], sizeof(struct sprite_def), chunk_size, fp) < chunk_size)
    return 1;

  /* close the file */
  fclose(fp);

  return 0;
}

