/******************************************************************************/
/* rom.c (faux game cartridge)                                                */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rom.h"

/* file table format                          */
/* 1) number of files (2 bytes)               */
/* 2) the file table entries (12 bytes each)  */
/*    a) file address (3 bytes)               */
/*    b) file size (3 bytes)                  */
/*    c) file name (6 bytes)                  */

#define ROM_MAX_FILES 65535

#define ROM_FILE_TABLE_COUNT_BYTES  2

#define ROM_FILE_ENTRY_ADDR_OFFSET  0
#define ROM_FILE_ENTRY_ADDR_BYTES   3

#define ROM_FILE_ENTRY_SIZE_OFFSET  3
#define ROM_FILE_ENTRY_SIZE_BYTES   3

#define ROM_FILE_ENTRY_NAME_OFFSET  6
#define ROM_FILE_ENTRY_NAME_BYTES   6

#define ROM_FILE_TABLE_ENTRY_BYTES  12

#define ROM_FILE_TABLE_SIZE(num_entries)                                       \
  (ROM_FILE_TABLE_COUNT_BYTES + (ROM_FILE_TABLE_ENTRY_BYTES * num_entries))

#define ROM_FILE_ENTRY_LOC(entry_index)                                        \
  (ROM_FILE_TABLE_COUNT_BYTES + (ROM_FILE_TABLE_ENTRY_BYTES * (entry_index)))

#define ROM_FILE_ADDR_LOC(entry_index)                                         \
  (ROM_FILE_ENTRY_LOC(entry_index) + ROM_FILE_ENTRY_ADDR_OFFSET)

#define ROM_FILE_SIZE_LOC(entry_index)                                         \
  (ROM_FILE_ENTRY_LOC(entry_index) + ROM_FILE_ENTRY_SIZE_OFFSET)

#define ROM_FILE_NAME_LOC(entry_index)                                         \
  (ROM_FILE_ENTRY_LOC(entry_index) + ROM_FILE_ENTRY_NAME_OFFSET)

/* big endian read / write macros */

#define ROM_WRITE_BYTE(addr, val)                                              \
  G_rom_data[(addr) + 0] = (val) & 0xFF;

#define ROM_WRITE_16BE(addr, val)                                              \
  G_rom_data[(addr) + 0] = ((val) >> 8) & 0xFF;                                \
  G_rom_data[(addr) + 1] = (val) & 0xFF;

#define ROM_WRITE_24BE(addr, val)                                              \
  G_rom_data[(addr) + 0] = ((val) >> 16) & 0xFF;                               \
  G_rom_data[(addr) + 1] = ((val) >> 8) & 0xFF;                                \
  G_rom_data[(addr) + 2] = (val) & 0xFF;

#define ROM_READ_BYTE(val, addr)                                               \
  (val) = G_rom_data[(addr) + 0] & 0xFF;

#define ROM_READ_16BE(val, addr)                                               \
  (val) = (G_rom_data[(addr) + 0] << 8) & 0xFF00;                              \
  (val) |= G_rom_data[(addr) + 1] & 0x00FF;

#define ROM_READ_24BE(val, addr)                                               \
  (val) =  (G_rom_data[(addr) + 0] << 16) & 0xFF0000;                          \
  (val) |= (G_rom_data[(addr) + 1] << 8) & 0x00FF00;                           \
  (val) |=  G_rom_data[(addr) + 2] & 0x0000FF;

/* the rom! */

#define ROM_MAX_BYTES (4 * 1024 * 1024) /* 4 MB */

unsigned char G_rom_data[ROM_MAX_BYTES];
unsigned long G_rom_size;

/******************************************************************************/
/* rom_clear()                                                                */
/******************************************************************************/
int rom_clear()
{
  unsigned long k;

  for (k = 0; k < ROM_MAX_BYTES; k++)
    G_rom_data[k] = 0x00;

  G_rom_size = 0;

  return 0;
}

/******************************************************************************/
/* rom_validate()                                                             */
/******************************************************************************/
int rom_validate()
{
  unsigned short k;

  unsigned short num_files;

  unsigned long  data_block_addr;
  unsigned long  data_block_size;

  unsigned long  file_addr;
  unsigned long  file_size;
  unsigned long  file_accum;

  /* make sure rom size is valid */
  if (G_rom_size > ROM_MAX_BYTES)
    return 1;

  /* obtain file table size */
  if (G_rom_size >= ROM_FILE_TABLE_COUNT_BYTES)
  {
    ROM_READ_16BE(num_files, 0)
  }
  else
    return 1;

  /* obtain data block size */
  data_block_addr = ROM_FILE_TABLE_SIZE(num_files);

  if (G_rom_size >= data_block_addr)
    data_block_size = G_rom_size - data_block_addr;
  else
    return 1;

  /* validate file table */
  file_accum = 0;

  for (k = 0; k < num_files; k++)
  {
    ROM_READ_24BE(file_addr, ROM_FILE_ADDR_LOC(k))
    ROM_READ_24BE(file_size, ROM_FILE_SIZE_LOC(k))

    if (file_accum != file_addr)
      return 1;

    if (file_size == 0)
      return 1;

    if (file_addr >= data_block_size)
      return 1;

    file_accum += file_size;
  }

  if (file_accum != data_block_size)
    return 1;

  return 0;
}

/******************************************************************************/
/* rom_format()                                                               */
/******************************************************************************/
int rom_format()
{
  rom_clear();

  /* just define a zero-entry file table          */
  /* since the rom is already cleared to zeroes,  */
  /* we just increase the rom size to cover this. */
  G_rom_size = ROM_FILE_TABLE_COUNT_BYTES;

  return 0;
}

/******************************************************************************/
/* rom_create_file()                                                          */
/******************************************************************************/
int rom_create_file(unsigned long num_bytes)
{
  unsigned short k;

  unsigned short num_files;
  unsigned short file_index;

  unsigned long  data_block_addr;
  unsigned long  data_block_size;

  /* check input variables */
  if (num_bytes == 0)
    return 1;

  /* make sure there is space for the new table entry and file */
  if ((G_rom_size + ROM_FILE_TABLE_ENTRY_BYTES + num_bytes) >= ROM_MAX_BYTES)
    return 1;

  /* insert space for new file table entry */
  ROM_READ_16BE(num_files, 0)

  if (num_files == ROM_MAX_FILES)
    return 1;

  data_block_addr = ROM_FILE_TABLE_SIZE(num_files);
  data_block_size = G_rom_size - data_block_addr;

  if (data_block_size > 0)
  {
    memmove(&G_rom_data[data_block_addr + ROM_FILE_TABLE_ENTRY_BYTES], 
            &G_rom_data[data_block_addr], 
            data_block_size);
  }

  G_rom_size += ROM_FILE_TABLE_ENTRY_BYTES;

  file_index = num_files;

  num_files += 1;
  ROM_WRITE_16BE(0, num_files)

  /* fill out file table entry */
  ROM_WRITE_24BE(ROM_FILE_ADDR_LOC(file_index), data_block_size)
  ROM_WRITE_24BE(ROM_FILE_SIZE_LOC(file_index), num_bytes)

  for (k = 0; k < ROM_FILE_ENTRY_NAME_BYTES; k++)
  {
    ROM_WRITE_BYTE(ROM_FILE_NAME_LOC(file_index) + k, 0)
  }

  /* update the rom size and return */
  G_rom_size += num_bytes;

  return 0;
}

/******************************************************************************/
/* rom_add_file_bytes()                                                       */
/******************************************************************************/
int rom_add_file_bytes(unsigned char* data, unsigned long num_bytes)
{
  unsigned short num_files;

  unsigned long  data_block_addr;
  unsigned long  file_addr;

  /* check input variables */
  if (data == NULL)
    return 1;

  if (num_bytes == 0)
    return 0;

  /* create new file */
  if (rom_create_file(num_bytes))
    return 1;

  /* determine new file address */
  ROM_READ_16BE(num_files, 0)

  data_block_addr = ROM_FILE_TABLE_SIZE(num_files);

  ROM_READ_24BE(file_addr, ROM_FILE_ADDR_LOC(num_files - 1))

  /* copy the data to the file */
  memcpy(&G_rom_data[data_block_addr + file_addr], data, num_bytes);

  return 0;
}

/******************************************************************************/
/* rom_add_file_words()                                                       */
/******************************************************************************/
int rom_add_file_words(unsigned short* data, unsigned long num_words)
{
  unsigned long  k;

  unsigned short num_files;

  unsigned long  data_block_addr;
  unsigned long  file_addr;

  /* check input variables */
  if (data == NULL)
    return 1;

  if (num_words == 0)
    return 0;

  /* create new file */
  if (rom_create_file(2 * num_words))
    return 1;

  /* determine new file address */
  ROM_READ_16BE(num_files, 0)

  data_block_addr = ROM_FILE_TABLE_SIZE(num_files);

  ROM_READ_24BE(file_addr, ROM_FILE_ADDR_LOC(num_files - 1))

  /* copy the data to the file */
  for (k = 0; k < num_words; k++)
  {
    ROM_WRITE_16BE(data_block_addr + file_addr + 2 * k, data[k])
  }

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

  /* open the rom file */
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
  if (fwrite(G_rom_data, sizeof(unsigned char), G_rom_size, fp) < G_rom_size)
    return 1;

  /* close the rom file */
  fclose(fp);

  return 0;
}

