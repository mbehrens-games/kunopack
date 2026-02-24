/******************************************************************************/
/* rom.h (faux game cartridge)                                                */
/******************************************************************************/

#ifndef ROM_H
#define ROM_H

extern unsigned char G_rom_data[];
extern unsigned long G_rom_size;

/* function declarations */
int rom_clear();
int rom_validate();
int rom_format();

int rom_add_file_bytes(unsigned char* data, unsigned long num_bytes);
int rom_add_file_words(unsigned short* data, unsigned long num_words);

int rom_save(char* filename);

#endif

