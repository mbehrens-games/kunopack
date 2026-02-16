/******************************************************************************/
/* art.h (load art files)                                                     */
/******************************************************************************/

#ifndef ART_H
#define ART_H

/* rom data buffers */
extern unsigned short G_art_palette[];
extern unsigned short G_art_elements[];
extern unsigned char  G_art_cells[];

extern unsigned short G_art_num_elements;
extern unsigned short G_art_num_cells;

/* function declarations */
int art_clear_file_vars();
int art_clear_image_vars();

int art_load_pbm(char* filename, unsigned short num_frames);

int art_add_files_to_rom();

#endif

