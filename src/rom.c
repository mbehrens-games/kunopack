/******************************************************************************/
/* rom.c (faux game cartridge)                                                */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rom.h"

/* chunk table format                         */
/* 1) number of chunks (2 bytes)              */
/* 2) the chunk table entries (6 bytes each)  */
/*    a) chunk address (3 bytes)              */
/*    b) chunk size (3 bytes)                 */

#define ROM_MAX_CHUNKS 65535

#define ROM_CHUNK_TABLE_COUNT_BYTES  2

#define ROM_CHUNK_ENTRY_ADDR_OFFSET  0
#define ROM_CHUNK_ENTRY_ADDR_BYTES   3

#define ROM_CHUNK_ENTRY_SIZE_OFFSET  3
#define ROM_CHUNK_ENTRY_SIZE_BYTES   3

#define ROM_CHUNK_TABLE_ENTRY_BYTES  6

#define ROM_CHUNK_TABLE_SIZE(num_entries)                                      \
  (ROM_CHUNK_TABLE_COUNT_BYTES + (ROM_CHUNK_TABLE_ENTRY_BYTES * num_entries))

#define ROM_CHUNK_ENTRY_LOC(entry_index)                                       \
  (ROM_CHUNK_TABLE_COUNT_BYTES + (ROM_CHUNK_TABLE_ENTRY_BYTES * (entry_index)))

#define ROM_CHUNK_ADDR_LOC(entry_index)                                        \
  (ROM_CHUNK_ENTRY_LOC(entry_index) + ROM_CHUNK_ENTRY_ADDR_OFFSET)

#define ROM_CHUNK_SIZE_LOC(entry_index)                                        \
  (ROM_CHUNK_ENTRY_LOC(entry_index) + ROM_CHUNK_ENTRY_SIZE_OFFSET)

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

  unsigned short num_chunks;

  unsigned long  data_block_addr;
  unsigned long  data_block_size;

  unsigned long  chunk_addr;
  unsigned long  chunk_size;
  unsigned long  chunk_accum;

  /* make sure rom size is valid */
  if (G_rom_size > ROM_MAX_BYTES)
    return 1;

  /* obtain chunk table size */
  if (G_rom_size >= ROM_CHUNK_TABLE_COUNT_BYTES)
  {
    ROM_READ_16BE(num_chunks, 0)
  }
  else
    return 1;

  /* obtain data block size */
  data_block_addr = ROM_CHUNK_TABLE_SIZE(num_chunks);

  if (G_rom_size >= data_block_addr)
    data_block_size = G_rom_size - data_block_addr;
  else
    return 1;

  /* validate chunk table */
  chunk_accum = 0;

  for (k = 0; k < num_chunks; k++)
  {
    ROM_READ_24BE(chunk_addr, ROM_CHUNK_ADDR_LOC(k))
    ROM_READ_24BE(chunk_size, ROM_CHUNK_SIZE_LOC(k))

    if (chunk_accum != chunk_addr)
      return 1;

    if (chunk_size == 0)
      return 1;

    if (chunk_addr >= data_block_size)
      return 1;

    chunk_accum += chunk_size;
  }

  if (chunk_accum != data_block_size)
    return 1;

  return 0;
}

/******************************************************************************/
/* rom_format()                                                               */
/******************************************************************************/
int rom_format()
{
  rom_clear();

  /* just define a zero-entry chunk table         */
  /* since the rom is already cleared to zeroes,  */
  /* we just increase the rom size to cover this. */
  G_rom_size = ROM_CHUNK_TABLE_COUNT_BYTES;

  return 0;
}

/******************************************************************************/
/* rom_create_chunk()                                                         */
/******************************************************************************/
int rom_create_chunk(unsigned long num_bytes)
{
  unsigned short num_chunks;
  unsigned short chunk_index;

  unsigned long  data_block_addr;
  unsigned long  data_block_size;

  /* check input variables */
  if (num_bytes == 0)
    return 1;

  /* make sure there is space for the new table entry and chunk */
  if ((G_rom_size + ROM_CHUNK_TABLE_ENTRY_BYTES + num_bytes) >= ROM_MAX_BYTES)
    return 1;

  /* insert space for new chunk table entry */
  ROM_READ_16BE(num_chunks, 0)

  if (num_chunks == ROM_MAX_CHUNKS)
    return 1;

  data_block_addr = ROM_CHUNK_TABLE_SIZE(num_chunks);
  data_block_size = G_rom_size - data_block_addr;

  if (data_block_size > 0)
  {
    memmove(&G_rom_data[data_block_addr + ROM_CHUNK_TABLE_ENTRY_BYTES], 
            &G_rom_data[data_block_addr], 
            data_block_size);
  }

  G_rom_size += ROM_CHUNK_TABLE_ENTRY_BYTES;

  chunk_index = num_chunks;

  num_chunks += 1;
  ROM_WRITE_16BE(0, num_chunks)

  /* fill out chunk table entry */
  ROM_WRITE_24BE(ROM_CHUNK_ADDR_LOC(chunk_index), data_block_size)
  ROM_WRITE_24BE(ROM_CHUNK_SIZE_LOC(chunk_index), num_bytes)

  /* update the rom size and return */
  G_rom_size += num_bytes;

  return 0;
}

/******************************************************************************/
/* rom_add_chunk_bytes()                                                      */
/******************************************************************************/
int rom_add_chunk_bytes(unsigned short* chunk_index_cb, 
                        unsigned char* data, unsigned long num_bytes)
{
  unsigned short num_chunks;

  unsigned long  data_block_addr;
  unsigned long  chunk_addr;

  /* check input variables */
  if (data == NULL)
    return 1;

  if (num_bytes == 0)
    return 0;

  /* create new chunk */
  if (rom_create_chunk(num_bytes))
    return 1;

  /* determine new chunk address */
  ROM_READ_16BE(num_chunks, 0)

  data_block_addr = ROM_CHUNK_TABLE_SIZE(num_chunks);

  ROM_READ_24BE(chunk_addr, ROM_CHUNK_ADDR_LOC(num_chunks - 1))

  /* copy the data to the chunk */
  memcpy(&G_rom_data[data_block_addr + chunk_addr], data, num_bytes);

  /* set chunk index callback and return */
  *chunk_index_cb = num_chunks - 1;

  return 0;
}

/******************************************************************************/
/* rom_add_chunk_words()                                                      */
/******************************************************************************/
int rom_add_chunk_words(unsigned short* chunk_index_cb, 
                        unsigned short* data, unsigned long num_words)
{
  unsigned long  k;

  unsigned short num_chunks;

  unsigned long  data_block_addr;
  unsigned long  chunk_addr;

  /* check input variables */
  if (data == NULL)
    return 1;

  if (num_words == 0)
    return 0;

  /* create new chunk */
  if (rom_create_chunk(2 * num_words))
    return 1;

  /* determine new chunk address */
  ROM_READ_16BE(num_chunks, 0)

  data_block_addr = ROM_CHUNK_TABLE_SIZE(num_chunks);

  ROM_READ_24BE(chunk_addr, ROM_CHUNK_ADDR_LOC(num_chunks - 1))

  /* copy the data to the chunk */
  for (k = 0; k < num_words; k++)
  {
    ROM_WRITE_16BE(data_block_addr + chunk_addr + 2 * k, data[k])
  }

  /* set chunk index callback and return */
  *chunk_index_cb = num_chunks - 1;

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

