/******************************************************************************/
/* art.h (load art files)                                                     */
/******************************************************************************/

#ifndef ART_H
#define ART_H

/* rom file buffer */
extern unsigned char  G_art_rom_file_buf[];
extern unsigned short G_art_rom_file_size;

/* function declarations */
int art_init();

int art_load_gif(char* filename);

#endif

