/******************************************************************************/
/* rom.c (faux game cartridge)                                                */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rom.h"

#define ROM_MAX_BYTES (4 * 1024 * 1024) /* 4 MB */

/* file table format:                                     */
/* number of entries (16 bit value): 2 bytes              */
/* each entry (24 bit address & size): 6 bytes            */
/* addresses are relative (from start of the file table)  */
#define ROM_TABLE_COUNT_BYTES 2
#define ROM_TABLE_ENTRY_BYTES 6
#define ROM_ENTRY_ADDR_OFFSET 0
#define ROM_ENTRY_SIZE_OFFSET 3

#define ROM_FILE_TABLE_SIZE(num_entries)                                       \
  (ROM_TABLE_COUNT_BYTES + (ROM_TABLE_ENTRY_BYTES * num_entries))

#define ROM_FILE_ENTRY_LOC(entry)                                              \
  (ROM_TABLE_COUNT_BYTES + (ROM_TABLE_ENTRY_BYTES * (entry)) + 0)

#define ROM_FILE_ADDR_LOC(entry)                                               \
  (ROM_TABLE_COUNT_BYTES + (ROM_TABLE_ENTRY_BYTES * (entry)) + ROM_ENTRY_ADDR_OFFSET)

#define ROM_FILE_SIZE_LOC(entry)                                               \
  (ROM_TABLE_COUNT_BYTES + (ROM_TABLE_ENTRY_BYTES * (entry)) + ROM_ENTRY_SIZE_OFFSET)

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

/* file pointer variables */
static unsigned short S_rom_folder_index;
static unsigned short S_rom_file_index;
static unsigned long  S_rom_fp_addr;
static unsigned long  S_rom_fp_size;

/******************************************************************************/
/* rom_clear()                                                                */
/******************************************************************************/
int rom_clear()
{
  unsigned long k;

  /* zero out the bytes */
  for (k = 0; k < ROM_MAX_BYTES; k++)
    G_rom_data[k] = 0x00;

  G_rom_size = 0;

  /* reset file pointer variables */
  S_rom_folder_index = 0;
  S_rom_file_index = 0;
  S_rom_fp_addr = 0;
  S_rom_fp_size = 0;

  return 0;
}

/******************************************************************************/
/* rom_validate()                                                             */
/******************************************************************************/
int rom_validate()
{
  unsigned long k;
  unsigned long m;

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
  rom_accum = ROM_FILE_TABLE_SIZE(ROM_NUM_FOLDERS);

  for (k = 0; k < ROM_NUM_FOLDERS; k++)
  {
    ROM_READ_24BE(folder_addr, ROM_FILE_ADDR_LOC(k))
    ROM_READ_24BE(folder_size, ROM_FILE_SIZE_LOC(k))

    if (folder_addr != rom_accum)
      return 1; 

    ROM_READ_16BE(num_files, folder_addr + 0)

    /* the folder accumulator adds up the file sizes,     */
    /* and should end up equaling the size of the folder  */
    folder_accum = ROM_FILE_TABLE_SIZE(num_files);

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
/* rom_format()                                                               */
/******************************************************************************/
int rom_format()
{
  unsigned long k;

  unsigned long tmp_addr;

  /* reset the rom */
  rom_clear();

  /* add space for skeleton file table */
  G_rom_size = ROM_FILE_TABLE_SIZE(ROM_NUM_FOLDERS);
  G_rom_size += ROM_NUM_FOLDERS * ROM_FILE_TABLE_SIZE(0);

  /* set up skeleton file table */
  ROM_WRITE_16BE(0, ROM_NUM_FOLDERS)

  for (k = 0; k < ROM_NUM_FOLDERS; k++)
  {
    tmp_addr = ROM_FILE_TABLE_SIZE(ROM_NUM_FOLDERS);
    tmp_addr += k * ROM_FILE_TABLE_SIZE(0);

    /* folder address & size */
    ROM_WRITE_24BE(0 + ROM_FILE_ADDR_LOC(k), tmp_addr)
    ROM_WRITE_24BE(0 + ROM_FILE_SIZE_LOC(k), ROM_FILE_TABLE_SIZE(0))

    /* initial file count in each folder */
    ROM_WRITE_16BE(tmp_addr, 0)
  }

  return 0;
}

/******************************************************************************/
/* rom_create_file()                                                          */
/******************************************************************************/
int rom_create_file(unsigned short folder_index)
{
  unsigned short k;

  unsigned long folder_addr;
  unsigned long folder_size;

  unsigned long entry_addr;
  unsigned long tmp_addr;

  unsigned short file_index;

  unsigned short num_files;

  /* check input variables */
  if (folder_index >= ROM_NUM_FOLDERS)
    return 1;

  /* make sure there is space for the new entry */
  if ((G_rom_size + ROM_TABLE_ENTRY_BYTES) > ROM_MAX_BYTES)
    return 1;

  /* determine new entry address (end of current file table) */
  ROM_READ_24BE(folder_addr, 0 + ROM_FILE_ADDR_LOC(folder_index))

  ROM_READ_16BE(num_files, folder_addr)
  file_index = num_files;
  num_files += 1;
  ROM_WRITE_16BE(folder_addr, num_files)

  entry_addr = ROM_FILE_TABLE_SIZE(file_index);

  /* move data after the new entry, if any, to create space */
  if (entry_addr < G_rom_size)
  {
    memmove(&G_rom_data[folder_addr + entry_addr + ROM_TABLE_ENTRY_BYTES], 
            &G_rom_data[folder_addr + entry_addr], 
            G_rom_size - (folder_addr + entry_addr));
  }

  /* update rom size */
  G_rom_size += ROM_TABLE_ENTRY_BYTES;

  /* update addresses of preceding files in the file table */
  for (k = 0; k < num_files - 1; k++)
  {
    ROM_READ_24BE(tmp_addr, folder_addr + ROM_FILE_ADDR_LOC(k))
    tmp_addr += ROM_TABLE_ENTRY_BYTES;
    ROM_WRITE_24BE(folder_addr + ROM_FILE_ADDR_LOC(k), tmp_addr)
  }

  /* update folder size in top level table */
  ROM_READ_24BE(folder_size, 0 + ROM_FILE_SIZE_LOC(folder_index))
  folder_size += ROM_TABLE_ENTRY_BYTES;
  ROM_WRITE_24BE(0 + ROM_FILE_SIZE_LOC(folder_index), folder_size)

  /* update subsequent folder addresses in top level table */
  for (k = folder_index + 1; k < ROM_NUM_FOLDERS; k++)
  {
    ROM_READ_24BE(tmp_addr, 0 + ROM_FILE_ADDR_LOC(k))
    tmp_addr += ROM_TABLE_ENTRY_BYTES;
    ROM_WRITE_24BE(0 + ROM_FILE_ADDR_LOC(k), tmp_addr)
  }

  /* fill in entry */
  ROM_WRITE_24BE(folder_addr + ROM_FILE_ADDR_LOC(file_index), folder_size)
  ROM_WRITE_24BE(folder_addr + ROM_FILE_SIZE_LOC(file_index), 0)

  /* update file pointer variables */
  S_rom_folder_index = folder_index;
  S_rom_file_index = file_index;
  S_rom_fp_addr = folder_addr + folder_size;
  S_rom_fp_size = 0;

  return 0;
}

/******************************************************************************/
/* rom_add_block_to_file()                                                    */
/******************************************************************************/
int rom_add_block_to_file(unsigned long num_bytes)
{
  unsigned long k;

  unsigned long folder_addr;
  unsigned long folder_size;

  unsigned long file_addr;
  unsigned long file_size;

  unsigned long block_addr;
  unsigned long tmp_addr;

  unsigned short num_files;

  /* check input variables and file pointer variables */
  if (S_rom_fp_addr == 0)
    return 1;

  if (num_bytes == 0)
    return 0;

  /* make sure there is space for the new block */
  if ((G_rom_size + num_bytes) > ROM_MAX_BYTES)
    return 1;

  /* get folder address & size */
  if (S_rom_folder_index >= ROM_NUM_FOLDERS)
    return 1;

  ROM_READ_24BE(folder_addr, 0 + ROM_FILE_ADDR_LOC(S_rom_folder_index))
  ROM_READ_24BE(folder_size, 0 + ROM_FILE_SIZE_LOC(S_rom_folder_index))
  ROM_READ_16BE(num_files, folder_addr)

  /* get file address & size */
  if (S_rom_file_index >= num_files)
    return 1;

  ROM_READ_24BE(file_addr, folder_addr + ROM_FILE_ADDR_LOC(S_rom_file_index))
  ROM_READ_24BE(file_size, folder_addr + ROM_FILE_SIZE_LOC(S_rom_file_index))

  /* insert block at end of file */
  block_addr = file_addr + file_size;

  /* move data after the new block, if any, to create space */
  if ((folder_addr + block_addr) < G_rom_size)
  {
    memmove(&G_rom_data[folder_addr + block_addr + num_bytes], 
            &G_rom_data[folder_addr + block_addr], 
            G_rom_size - (folder_addr + block_addr));
  }

  /* update rom size */
  G_rom_size += num_bytes;

  /* update file size in the file table */
  file_size += num_bytes;
  ROM_WRITE_24BE(folder_addr + ROM_FILE_SIZE_LOC(S_rom_file_index), file_size)

  /* update subsequent file addresses in the file table */
  for (k = S_rom_file_index + 1; k < num_files; k++)
  {
    ROM_READ_24BE(tmp_addr, folder_addr + ROM_FILE_ADDR_LOC(k))
    tmp_addr += num_bytes;
    ROM_WRITE_24BE(folder_addr + ROM_FILE_ADDR_LOC(k), tmp_addr)
  }

  /* update folder size in top level table */
  ROM_READ_24BE(folder_size, 0 + ROM_FILE_SIZE_LOC(S_rom_folder_index))
  folder_size += num_bytes;
  ROM_WRITE_24BE(0 + ROM_FILE_SIZE_LOC(S_rom_folder_index), folder_size)

  /* update subsequent folder addresses in top level table */
  for (k = S_rom_folder_index + 1; k < ROM_NUM_FOLDERS; k++)
  {
    ROM_READ_24BE(tmp_addr, 0 + ROM_FILE_ADDR_LOC(k))
    tmp_addr += num_bytes;
    ROM_WRITE_24BE(0 + ROM_FILE_ADDR_LOC(k), tmp_addr)
  }

  /* update file pointer variables */
  S_rom_fp_size += num_bytes;

  return 0;
}

/******************************************************************************/
/* rom_add_bytes_to_file()                                                    */
/******************************************************************************/
int rom_add_bytes_to_file(unsigned char* data, unsigned long num_bytes)
{
  unsigned long block_abs_addr;

  /* check input variables and file pointer variables */
  if (S_rom_fp_addr == 0)
    return 1;

  if (data == NULL)
    return 1;

  if (num_bytes == 0)
    return 0;

  /* determine address of current end of file */
  block_abs_addr = S_rom_fp_addr + S_rom_fp_size;

  /* create new block */
  if (rom_add_block_to_file(num_bytes))
    return 1;

  /* copy the data to the block */
  memcpy(&G_rom_data[block_abs_addr], data, num_bytes);

  return 0;
}

/******************************************************************************/
/* rom_add_words_to_file()                                                    */
/******************************************************************************/
int rom_add_words_to_file(unsigned short* data, unsigned long num_words)
{
  unsigned long k;

  unsigned long block_abs_addr;

  /* check input variables and file pointer variables */
  if (S_rom_fp_addr == 0)
    return 1;

  if (data == NULL)
    return 1;

  if (num_words == 0)
    return 0;

  /* determine address of current end of file */
  block_abs_addr = S_rom_fp_addr + S_rom_fp_size;

  /* create new block */
  if (rom_add_block_to_file(2 * num_words))
    return 1;

  /* copy the data to the block */
  for (k = 0; k < num_words; k++)
  {
    ROM_WRITE_16BE(block_abs_addr + 2 * k, data[k])
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
  if (fwrite(&G_rom_data[0], sizeof(unsigned char), G_rom_size, fp) < G_rom_size)
    return 1;

  /* close the rom file */
  fclose(fp);

  return 0;
}

