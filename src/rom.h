/******************************************************************************/
/* rom.h (faux game cartridge)                                                */
/******************************************************************************/

#ifndef ROM_H
#define ROM_H

#define ROM_MAX_BYTES (4 * 1024 * 1024) /* 4 MB */

extern unsigned char G_rom_data[ROM_MAX_BYTES];
extern unsigned long G_rom_size;

enum
{
  ROM_FOLDER_SPRITES = 0, 
  ROM_FOLDER_TILES, 
  ROM_NUM_FOLDERS
};

/* function declarations */
int rom_clear();
int rom_validate();

int rom_add_file( unsigned short folder, 
                  unsigned char* data, unsigned long num_bytes);

int rom_save(char* filename);

#endif

