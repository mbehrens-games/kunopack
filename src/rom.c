/******************************************************************************/
/* rom.c (faux game cartridge)                                                */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rom.h"

/* top level table format:                                */
/* number of folders (16 bit value) -> 2 bytes            */
/* 1 entry per folder (24 bit address & size) -> 6 bytes  */
/* addresses are absolute (from start of the rom)         */

/* folder table format:                                               */
/* number of files (16 bit value) -> 2 bytes                          */
/* 1 entry per file (12 byte name, 24 bit address & size) -> 18 bytes */
/* addresses are relative (from start of folder table)                */
#define ROM_TOP_LEVEL_TABLE_SIZE()                                             \
  (2 + (6 * ROM_NUM_FOLDERS))

#define ROM_FOLDER_TABLE_SIZE(num_files)                                       \
  (2 + (18 * num_files))

#define ROM_FOLDER_ENTRY_LOC(folder)                                           \
  (2 + (6 * (folder)) + 0)

#define ROM_FOLDER_ADDR_LOC(folder)                                            \
  (2 + (6 * (folder)) + 0)

#define ROM_FOLDER_SIZE_LOC(folder)                                            \
  (2 + (6 * (folder)) + 3)

#define ROM_FILE_ENTRY_LOC(file)                                               \
  (2 + (18 * (file)) + 0)

#define ROM_FILE_NAME_LOC(file)                                                \
  (2 + (18 * (file)) + 0)

#define ROM_FILE_ADDR_LOC(file)                                                \
  (2 + (18 * (file)) + 12)

#define ROM_FILE_SIZE_LOC(file)                                                \
  (2 + (18 * (file)) + 15)

#define ROM_WRITE_16BE(addr, val)                                              \
  G_rom_data[(addr) + 0] = ((val) >> 8) & 0xFF;                                \
  G_rom_data[(addr) + 1] = (val) & 0xFF;

#define ROM_WRITE_24BE(addr, val)                                              \
  G_rom_data[(addr) + 0] = ((val) >> 16) & 0xFF;                               \
  G_rom_data[(addr) + 1] = ((val) >> 8) & 0xFF;                                \
  G_rom_data[(addr) + 2] = (val) & 0xFF;

#define ROM_READ_16BE(val, addr)                                               \
  (val) = (G_rom_data[(addr) + 0] << 8) & 0xFF00;                              \
  (val) |= G_rom_data[(addr) + 1] & 0x00FF;

#define ROM_READ_24BE(val, addr)                                               \
  (val) =  (G_rom_data[(addr) + 0] << 16) & 0xFF0000;                          \
  (val) |= (G_rom_data[(addr) + 1] << 8) & 0x00FF00;                           \
  (val) |=  G_rom_data[(addr) + 2] & 0x0000FF;

unsigned char G_rom_data[ROM_MAX_BYTES];
unsigned long G_rom_size;

/******************************************************************************/
/* rom_insert_block()                                                         */
/******************************************************************************/
int rom_insert_block(unsigned long addr, unsigned long num_bytes)
{
  unsigned int k;

  /* make sure address is valid */
  /* note: the new block is appended if address = rom size */
  if (addr > G_rom_size)
    return 1;

  /* make sure this block will fit in the rom data array */
  if ((G_rom_size + num_bytes) > ROM_MAX_BYTES)
    return 1;

  /* move the bytes after the address, if any, to create more space */
  if (addr < G_rom_size)
    memmove(&G_rom_data[addr + num_bytes], &G_rom_data[addr], (G_rom_size - addr));

  /* zero the new space */
  for (k = 0; k < num_bytes; k++)
    G_rom_data[addr + k] = 0;

  /* update rom size */
  G_rom_size += num_bytes;

  return 0;
}

/******************************************************************************/
/* rom_clear()                                                                */
/******************************************************************************/
int rom_clear()
{
  int k;

  unsigned long tmp_addr;

  /* zero out the bytes */
  for (k = 0; k < ROM_MAX_BYTES; k++)
    G_rom_data[k] = 0x00;

  G_rom_size = 0;

  /* add in skeleton file table */

  /* add space for folder count & top level entries */
  if (rom_insert_block(G_rom_size, 2))
    return 1;

  if (rom_insert_block(G_rom_size, 6 * ROM_NUM_FOLDERS))
    return 1;

  /* add space for file count in each folder */
  if (rom_insert_block(G_rom_size, 2 * ROM_NUM_FOLDERS))
    return 1;

  /* set up top level table */
  ROM_WRITE_16BE(0, ROM_NUM_FOLDERS)

  tmp_addr = 2 + (6 * ROM_NUM_FOLDERS);

  for (k = 0; k < ROM_NUM_FOLDERS; k++)
  {
    /* folder address & size */
    ROM_WRITE_24BE(ROM_FOLDER_ADDR_LOC(k), tmp_addr)
    ROM_WRITE_24BE(ROM_FOLDER_SIZE_LOC(k), 2)

    /* initial file count in folder */
    ROM_WRITE_16BE(tmp_addr, 0)

    tmp_addr += 2;
  }
  
  return 0;
}

/******************************************************************************/
/* rom_validate()                                                             */
/******************************************************************************/
int rom_validate()
{
  int k;
  int m;

  unsigned short num_folders;
  unsigned short num_files;

  unsigned long folder_addr;
  unsigned long folder_size;

  unsigned long file_addr;
  unsigned long file_size;

  unsigned long rom_accum;
  unsigned long folder_accum;

  /* make sure the file table is accurate */
  ROM_READ_16BE(num_folders, 0)

  if (num_folders != ROM_NUM_FOLDERS)
    return 1;

  /* the rom accumulator adds up the folder sizes,  */
  /* and should end up equaling the size of the rom */
  rom_accum = ROM_TOP_LEVEL_TABLE_SIZE();

  for (k = 0; k < ROM_NUM_FOLDERS; k++)
  {
    ROM_READ_24BE(folder_addr, ROM_FOLDER_ADDR_LOC(k))
    ROM_READ_24BE(folder_size, ROM_FOLDER_SIZE_LOC(k))

    if (folder_addr != rom_accum)
      return 1; 

    ROM_READ_16BE(num_files, folder_addr + 0)

    /* the folder accumulator adds up the file sizes,     */
    /* and should end up equaling the size of the folder  */
    folder_accum = ROM_FOLDER_TABLE_SIZE(num_files);

    for (m = 0; m < num_files; m++)
    {
      ROM_READ_24BE(file_addr, folder_addr + ROM_FILE_ADDR_LOC(m))
      ROM_READ_24BE(file_size, folder_addr + ROM_FILE_SIZE_LOC(m))

      if (file_addr != folder_accum)
        return 1;

      folder_accum += file_size;
    }

    if (folder_accum != folder_size)
      return 1;

    rom_accum += folder_size;
  }

  if (rom_accum != G_rom_size)
    return 1;

  return 0;
}

/******************************************************************************/
/* rom_add_file()                                                             */
/******************************************************************************/
int rom_add_file( int folder, char* name, 
                  unsigned char* data, unsigned long num_bytes)
{
  int k;

  unsigned long folder_addr;
  unsigned long entry_addr;
  unsigned long file_addr;

  unsigned long tmp_addr;
  unsigned long tmp_size;

  unsigned short num_files;

  /* check input variables */
  if ((folder < 0) || (folder >= ROM_NUM_FOLDERS))
    return 1;

#if 0
  if (name == NULL)
    return 1;
#endif

  if (data == NULL)
    return 1;

  if (num_bytes == 0)
    return 1;

  /* read folder address from top level table */
  ROM_READ_24BE(folder_addr, ROM_FOLDER_ADDR_LOC(folder))

  /* read number of files */
  ROM_READ_16BE(num_files, folder_addr + 0)

  /* determine (relative) addresses for new entry and file */
  if (num_files == 0)
  {
    entry_addr = 2;
    file_addr = 2;
  }
  else
  {
    /* address of 1st file becomes the new entry's address */
    ROM_READ_24BE(entry_addr, folder_addr + ROM_FILE_ENTRY_LOC(0))

    /* address & size of last file determine the new file's address */
    ROM_READ_24BE(file_addr, folder_addr + ROM_FILE_ADDR_LOC(num_files - 1))
    ROM_READ_24BE(tmp_size, folder_addr + ROM_FILE_SIZE_LOC(num_files - 1))

    file_addr += tmp_size;
  }

  /* insert space for the new entry and file */

  /* note that we shift the file address to account for everything  */
  /* after the entry being shifted after we insert space for it     */
  file_addr += 18;

  if (rom_insert_block(folder_addr + entry_addr, 18))
    return 1;

  if (rom_insert_block(folder_addr + file_addr, num_bytes))
    return 1;

  /* update addresses in the file table */
  for (k = 0; k < num_files; k++)
  {
    ROM_READ_24BE(tmp_addr, folder_addr + ROM_FILE_ADDR_LOC(k))
    ROM_WRITE_24BE(folder_addr + ROM_FILE_ADDR_LOC(k), tmp_addr + 18)
  }

  /* add in new entry to the file table */
  ROM_WRITE_24BE(folder_addr + entry_addr + 12, file_addr)
  ROM_WRITE_24BE(folder_addr + entry_addr + 15, num_bytes)

  /* update file count */
  ROM_WRITE_16BE(folder_addr, num_files + 1)

  /* update folder size in top level table */
  ROM_READ_24BE(tmp_size, ROM_FOLDER_SIZE_LOC(folder))
  ROM_WRITE_24BE(ROM_FOLDER_SIZE_LOC(folder), tmp_size + 18 + num_bytes)

  /* update folder addresses in top level table */
  for (k = folder + 1; k < ROM_NUM_FOLDERS; k++)
  {
    ROM_READ_24BE(tmp_addr, ROM_FOLDER_ADDR_LOC(k))
    ROM_WRITE_24BE(ROM_FOLDER_ADDR_LOC(k), tmp_addr + 18 + num_bytes)
  }

  /* write file data */
  memcpy(&G_rom_data[folder_addr + file_addr], data, num_bytes);

  return 0;
}

/******************************************************************************/
/* rom_save()                                                                 */
/******************************************************************************/
int rom_save(char* filename)
{
  FILE* fp;

  char signature[8];
  char type[4];

  /* make sure filename is valid */
  if (filename == NULL)
    return 1;

  /* make sure the rom is valid */
  if (rom_validate())
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
  signature[4] = 'I';
  signature[5] = 'C';
  signature[6] = 'H';
  signature[7] = 'I';

  type[0] = 'C';
  type[1] = 'A';
  type[2] = 'R';
  type[3] = 'T';

  if (fwrite(signature, sizeof(char), 8, fp) < 8)
    return 1;

  if (fwrite(type, sizeof(char), 4, fp) < 4)
    return 1;

  /* write rom data */
  if (fwrite(&G_rom_data[0], sizeof(unsigned char), G_rom_size, fp) < G_rom_size)
    return 1;

  /* close the file */
  fclose(fp);

  return 0;
}

