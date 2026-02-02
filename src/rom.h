/******************************************************************************/
/* rom.h (faux game cartridge)                                                */
/******************************************************************************/

#ifndef ROM_H
#define ROM_H

extern unsigned char G_rom_data[];
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
int rom_format();

int rom_create_file(unsigned short folder, unsigned short* file_number_cb);

int rom_add_bytes_to_file(unsigned short folder, unsigned short file_number, 
                          unsigned char* data, unsigned long num_bytes);

int rom_add_words_to_file(unsigned short folder, unsigned short file_number, 
                          unsigned short* data, unsigned long num_words);

int rom_save(char* filename);

#endif

