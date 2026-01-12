/******************************************************************************/
/* art.h (load art files)                                                     */
/******************************************************************************/

#ifndef ART_H
#define ART_H

/* header: 4 bytes                  */
/* palette: 16 colors, 2 bytes each */
/* cells: 256 cells, 32 bytes each  */
#define ART_ROM_DATA_MAX_BYTES (4 + (2 * 16) + (256 * 32))

extern unsigned char  G_art_rom_data_buf[ART_ROM_DATA_MAX_BYTES];
extern unsigned short G_art_rom_data_size;

/* function declarations */
int art_init();

int art_load_gif(char* filename);

#endif

