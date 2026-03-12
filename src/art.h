/******************************************************************************/
/* art.h (load art files)                                                     */
/******************************************************************************/

#ifndef ART_H
#define ART_H

/* rom data buffers */
extern unsigned short G_art_nametable[];
extern unsigned short G_art_num_entries;

extern unsigned short G_art_pals[];
extern unsigned short G_art_num_pals;

extern unsigned char  G_art_cells[];
extern unsigned long  G_art_num_cells;

/* function declarations */
int art_clear_rom_data_vars();

int art_load_gif(char* filename);

int art_add_chunks_to_rom();

#endif

