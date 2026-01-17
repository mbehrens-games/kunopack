/******************************************************************************/
/* art.h (load art files)                                                     */
/******************************************************************************/

#ifndef ART_H
#define ART_H

/* rom data buffers */
extern unsigned char  G_art_pals_buf[];
extern unsigned short G_art_pals_size;

extern unsigned char  G_art_cells_buf[];
extern unsigned short G_art_cells_size;

extern unsigned char  G_art_sprites_buf[];
extern unsigned short G_art_sprites_size;

/* function declarations */
int art_init();

int art_load_gif(char* filename);

#endif

