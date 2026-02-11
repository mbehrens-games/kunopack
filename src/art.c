/******************************************************************************/
/* art.c (load art files)                                                     */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"

#include "rom.h"

/* graphics constants (same as in vdp.c in the main code) */
#define VDP_COLORS_PER_PAL  16

#define VDP_CELL_W_H        8
#define VDP_PIXELS_PER_CELL (VDP_CELL_W_H * VDP_CELL_W_H)
#define VDP_BYTES_PER_CELL  (VDP_PIXELS_PER_CELL / 2)

#define VDP_CRAM_SIZE (4 * 64 + 4)  /* 4 16-color palettes, 1 4-color palette */
#define VDP_VRAM_SIZE (1 << 16)     /* 64 K (2048 cells, 32 bytes each) */

#define VDP_MAX_CELLS (VDP_VRAM_SIZE / VDP_BYTES_PER_CELL) /* 2048 */

/* 4 K words (so, 8 K bytes) */
#define VDP_NAME_ENTRY_SIZE 2
#define VDP_NAME_TABLE_SIZE (VDP_NAME_ENTRY_SIZE * VDP_MAX_CELLS)

#define VDP_SYMBOL_ENTRY_SIZE 1
#define VDP_SYMBOL_TABLE_SIZE (VDP_SYMBOL_ENTRY_SIZE * VDP_MAX_CELLS)

enum
{
  VDP_ANIM_TYPE_ONCE = 0, 
  VDP_ANIM_TYPE_LOOP, 
  VDP_ANIM_TYPE_BOUNCE, 
  VDP_ANIM_TYPE_RAGGED 
};

/* rom data buffers */
unsigned short G_art_palette[VDP_COLORS_PER_PAL];
unsigned short G_art_elements[VDP_NAME_TABLE_SIZE];
unsigned char  G_art_cells[VDP_VRAM_SIZE];

unsigned short G_art_num_elements;
unsigned short G_art_num_cells;

static unsigned char S_art_palette_found;

/* image variables */
static unsigned short S_art_image_w;
static unsigned short S_art_image_h;

static unsigned short S_art_frame_rows;
static unsigned short S_art_frame_columns;
static unsigned short S_art_num_frames;
static unsigned short S_art_anim_ticks;
static unsigned short S_art_anim_type;

/* file variables */
enum
{
  ART_PBM_CHUNK_NAME_NONE = 0, 
  ART_PBM_CHUNK_NAME_BMHD, 
  ART_PBM_CHUNK_NAME_CMAP, 
  ART_PBM_CHUNK_NAME_BODY, 
  ART_PBM_CHUNK_NAME_UNKNOWN 
};

static FILE* S_art_fp;

static unsigned short S_art_chunk_name;
static unsigned short S_art_file_colors;
static unsigned char  S_art_file_compression;

#define ART_BUFFER_IS_CHUNK_NAME(buf, ch_1, ch_2, ch_3, ch_4)                  \
  ( (buf[0] == ch_1) &&                                                        \
    (buf[1] == ch_2) &&                                                        \
    (buf[2] == ch_3) &&                                                        \
    (buf[3] == ch_4))

/* image buffer */
#define ART_IMAGE_BUFFER_SIZE (2 * VDP_VRAM_SIZE)

static unsigned char  S_art_pixels_buf[ART_IMAGE_BUFFER_SIZE];
static unsigned long  S_art_pixels_size;

/******************************************************************************/
/* art_clear_file_vars()                                                      */
/******************************************************************************/
int art_clear_file_vars()
{
  unsigned long k;

  /* rom data buffers */
  for (k = 0; k < VDP_COLORS_PER_PAL; k++)
    G_art_palette[k] = 0;

  for (k = 0; k < VDP_NAME_TABLE_SIZE; k++)
    G_art_elements[k] = 0;

  for (k = 0; k < VDP_VRAM_SIZE; k++)
    G_art_cells[k] = 0;

  G_art_num_elements = 0;
  G_art_num_cells = 0;

  S_art_palette_found = 0;

  return 0;
}

/******************************************************************************/
/* art_clear_image_vars()                                                     */
/******************************************************************************/
int art_clear_image_vars()
{
  unsigned long k;

  /* image variables */
  S_art_image_w = 8;
  S_art_image_h = 8;

  S_art_frame_rows = 1;
  S_art_frame_columns = 1;
  S_art_num_frames = 1;
  S_art_anim_ticks = 0;
  S_art_anim_type = VDP_ANIM_TYPE_ONCE;

  /* file variables */
  S_art_fp = NULL;

  S_art_chunk_name = ART_PBM_CHUNK_NAME_NONE;

  S_art_file_colors = 0;
  S_art_file_compression = 0;

  /* image buffer */
  for (k = 0; k < ART_IMAGE_BUFFER_SIZE; k++)
    S_art_pixels_buf[k] = 0;

  S_art_pixels_size = 0;

  return 0;
}

/******************************************************************************/
/* art_add_cells()                                                            */
/******************************************************************************/
int art_add_cells()
{
  unsigned long k;
  unsigned long m;
  unsigned long n;

  unsigned short frame_cells;

  unsigned short cell_addr;
  unsigned short cell_offset;

  unsigned short pixel_addr;
  unsigned short pixel_offset;

  unsigned char val;

  /* determine how many cells are to be created */
  frame_cells = S_art_frame_rows * S_art_frame_columns;

  if (G_art_num_cells + (S_art_num_frames * frame_cells) > VDP_MAX_CELLS)
    return 1;

  /* create cells */
  for (k = 0; k < S_art_num_frames; k++)
  {
    for (m = 0; m < frame_cells; m++)
    {
      /* determine cell & pixel addresses */
      cell_addr = VDP_BYTES_PER_CELL * G_art_num_cells;
      cell_addr += VDP_BYTES_PER_CELL * ((k * frame_cells) + m);

      pixel_addr = k * (S_art_frame_columns * VDP_CELL_W_H);
      pixel_addr += VDP_CELL_W_H * S_art_image_w * (m / S_art_frame_columns);
      pixel_addr += VDP_CELL_W_H * (m % S_art_frame_columns);

      for (n = 0; n < VDP_PIXELS_PER_CELL; n++)
      {
        /* determine cell & pixel offsets */
        cell_offset = n / 2;

        pixel_offset = S_art_image_w * (n / VDP_CELL_W_H);
        pixel_offset += n % VDP_CELL_W_H;

        /* obtain pixel value */
        val = S_art_pixels_buf[pixel_addr + pixel_offset];

        /* write this pixel value to the cells buffer */
        if (n % 2 == 0)
        {
          G_art_cells[cell_addr + cell_offset] &= 0x0F;
          G_art_cells[cell_addr + cell_offset] |= (val << 4) & 0xF0;
        }
        else
        {
          G_art_cells[cell_addr + cell_offset] &= 0xF0;
          G_art_cells[cell_addr + cell_offset] |= val & 0x0F;
        }
      }
    }
  }

  G_art_num_cells += S_art_num_frames * frame_cells;

  return 0;
}

/******************************************************************************/
/* art_add_sprite()                                                           */
/******************************************************************************/
int art_add_sprite()
{
  unsigned short val;

  if ((G_art_num_elements + 1) >= VDP_NAME_TABLE_SIZE)
    return 1;

  /* 1st word: dimensions, number of frames, animation frame length */
  val = ((S_art_frame_columns - 1) << 12) & 0xF000;
  val |= ((S_art_frame_rows - 1) << 8) & 0x0F00;
  val |= ((S_art_num_frames - 1) << 5) & 0x00E0;

  G_art_elements[2 * G_art_num_elements + 0] = val;

  /* 2nd word: index to the first cell in vram */
  val = G_art_num_cells & 0x07FF;

  G_art_elements[2 * G_art_num_elements + 1] = val;

  G_art_num_elements += 1;

  return 0;
}

/******************************************************************************/
/* art_read_pbm_header()                                                      */
/******************************************************************************/
int art_read_pbm_header(unsigned long subchunk_size)
{
  unsigned char buf[4];

  /* make sure file is open */
  if (S_art_fp == NULL)
    return 1;

  /* check subchunk size */
  if (subchunk_size != 20)
    return 1;

  /* image width and height */
  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  S_art_image_w = (buf[0] << 8) & 0xFF00;
  S_art_image_w |= buf[1] & 0x00FF;

  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  S_art_image_h = (buf[0] << 8) & 0xFF00;
  S_art_image_h |= buf[1] & 0x00FF;

  /* image top left corner (skipped) */
  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  /* number of planes */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  if (buf[0] == 4)
    S_art_file_colors = 16;
  else if (buf[0] == 8)
    S_art_file_colors = 256;
  else
    return 1;

  /* mask (skipped) */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  /* compression */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  if (buf[0] == 0)
    S_art_file_compression = 0;
  else
    S_art_file_compression = 1;

  /* padding byte */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  /* transparent color (skipped) */
  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  /* pixel aspect ratio (skipped) */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  /* page width and height (skipped) */
  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  return 0;
}

/******************************************************************************/
/* art_read_pbm_color_map()                                                   */
/******************************************************************************/
int art_read_pbm_color_map(unsigned long subchunk_size)
{
  unsigned short k;

  unsigned char  buf[4];
  unsigned short val;

  /* check subchunk size */
  if (subchunk_size != (S_art_file_colors * 3))
    return 1;

  /* read the colors */
  for (k = 0; k < S_art_file_colors; k++)
  {
    if (fread(&buf[0], sizeof(unsigned char), 3, S_art_fp) < 3)
      return 1;

    if ((S_art_palette_found == 0) && (k < VDP_COLORS_PER_PAL))
    {
      val =  (buf[0] << 7) & 0x7C00;
      val |= (buf[1] << 2) & 0x03E0;
      val |= (buf[2] >> 3) & 0x001F;

      G_art_palette[k] = val;
    }
  }

  S_art_palette_found = 1;

  return 0;
}

/******************************************************************************/
/* art_read_pbm_body()                                                        */
/******************************************************************************/
int art_read_pbm_body(unsigned long subchunk_size)
{
  unsigned short k;

  unsigned long  num_pixels;
  unsigned char  tmp_ch;
  unsigned short run_length;

  /* determine number of pixels */
  num_pixels = S_art_image_w * S_art_image_h;

  /* read the pixels */
  if (S_art_file_compression == 0)
  {
    if (fread(&S_art_pixels_buf[0], sizeof(unsigned char), num_pixels, S_art_fp) < num_pixels)
      return 1;

    S_art_pixels_size = num_pixels;
  }
  else
  {
    S_art_pixels_size = 0;

    while (S_art_pixels_size < num_pixels)
    {
      if (fread(&tmp_ch, sizeof(unsigned char), 1, S_art_fp) < 1)
        return 1;

      /* repeated run */
      if (tmp_ch > 128)
      {
        run_length = 257 - tmp_ch;

        if (fread(&tmp_ch, sizeof(unsigned char), 1, S_art_fp) < 1)
          return 1;

        for (k = 0; k < run_length; k++)
          S_art_pixels_buf[S_art_pixels_size + k] = tmp_ch;

        S_art_pixels_size += run_length;
      }
      /* literal run */
      else if (tmp_ch < 128)
      {
        run_length = tmp_ch + 1;

        for (k = 0; k < run_length; k++)
        {
          if (fread(&tmp_ch, sizeof(unsigned char), 1, S_art_fp) < 1)
            return 1;

          S_art_pixels_buf[S_art_pixels_size + k] = tmp_ch;
        }

        S_art_pixels_size += run_length;
      }
      /* end */
      else
        break;
    }
  }

  /* optional padding byte */
  if ((subchunk_size % 2) != 0)
  {
    if (fread(&tmp_ch, sizeof(unsigned char), 1, S_art_fp) < 1)
      return 1;
  }

  return 0;
}

/******************************************************************************/
/* art_load_pbm()                                                             */
/******************************************************************************/
int art_load_pbm(char* filename, unsigned short num_frames)
{
  unsigned char buf[4];
  unsigned long chunk_size;
  unsigned long subchunk_size;

  /* make sure filename is valid */
  if (filename == NULL)
    return 1;

  /* reset local variables */
  art_clear_image_vars();

  /* open the file */
  S_art_fp = fopen(filename, "rb");

  if (S_art_fp == NULL)
    return 1;

  /* read top level chunk */
  if (fread(&buf[0], sizeof(unsigned char), 4, S_art_fp) < 4)
    goto nope;

  if (!ART_BUFFER_IS_CHUNK_NAME(buf, 'F', 'O', 'R', 'M'))
    goto nope;

  if (fread(&buf[0], sizeof(unsigned char), 4, S_art_fp) < 4)
    goto nope;

  chunk_size =  (buf[0] << 24) & 0xFF000000;
  chunk_size |= (buf[1] << 16) & 0x00FF0000;
  chunk_size |= (buf[2] <<  8) & 0x0000FF00;
  chunk_size |= buf[3] & 0x000000FF;

  if (fread(&buf[0], sizeof(unsigned char), 4, S_art_fp) < 4)
    goto nope;

  if (!ART_BUFFER_IS_CHUNK_NAME(buf, 'P', 'B', 'M', ' '))
    goto nope;

  /* start processing subchunks */
  while (fread(&buf[0], sizeof(unsigned char), 4, S_art_fp) == 4)
  {
    /* subchunk name */
    /* (we already read it from the file in the loop condition) */
    if (ART_BUFFER_IS_CHUNK_NAME(buf, 'B', 'M', 'H', 'D'))
      S_art_chunk_name = ART_PBM_CHUNK_NAME_BMHD;
    else if (ART_BUFFER_IS_CHUNK_NAME(buf, 'C', 'M', 'A', 'P'))
      S_art_chunk_name = ART_PBM_CHUNK_NAME_CMAP;
    else if (ART_BUFFER_IS_CHUNK_NAME(buf, 'B', 'O', 'D', 'Y'))
      S_art_chunk_name = ART_PBM_CHUNK_NAME_BODY;
    else
      S_art_chunk_name = ART_PBM_CHUNK_NAME_UNKNOWN;

    /* subchunk size */
    if (fread(&buf[0], sizeof(unsigned char), 4, S_art_fp) < 4)
      goto nope;

    subchunk_size =  (buf[0] << 24) & 0xFF000000;
    subchunk_size |= (buf[1] << 16) & 0x00FF0000;
    subchunk_size |= (buf[2] <<  8) & 0x0000FF00;
    subchunk_size |= buf[3] & 0x000000FF;

    /* process subchunk, or skip it! */
    if (S_art_chunk_name == ART_PBM_CHUNK_NAME_BMHD)
    {
      if (art_read_pbm_header(subchunk_size))
        return 1;
    }
    else if (S_art_chunk_name == ART_PBM_CHUNK_NAME_CMAP)
    {
      if (art_read_pbm_color_map(subchunk_size))
        return 1;
    }
    else if (S_art_chunk_name == ART_PBM_CHUNK_NAME_BODY)
    {
      if (art_read_pbm_body(subchunk_size))
        return 1;
    }
    else
      fseek(S_art_fp, subchunk_size, SEEK_CUR);
  }

  /* close the file */
  fclose(S_art_fp);

  /* validate image variables */
  S_art_num_frames = num_frames;
  S_art_anim_ticks = 0;
  S_art_anim_type = VDP_ANIM_TYPE_ONCE;

  if ((S_art_image_w % 8) != 0)
    return 1;

  if ((S_art_image_h % 8) != 0)
    return 1;

  if ((S_art_num_frames == 0) || (S_art_num_frames > 8))
    return 1;

  if ((S_art_image_w % S_art_num_frames) != 0)
    return 1;

  S_art_frame_rows = S_art_image_h / VDP_CELL_W_H;
  S_art_frame_columns = S_art_image_w / (S_art_num_frames * VDP_CELL_W_H);

  if ((S_art_frame_rows == 0) || (S_art_frame_rows > 16))
    return 1;

  if ((S_art_frame_columns == 0) || (S_art_frame_columns > 16))
    return 1;

  /* add sprite and cells */
  art_add_sprite();
  art_add_cells();

  goto ok;

nope:
  fclose(S_art_fp);
  return 1;

ok:
  return 0;
}

/******************************************************************************/
/* art_add_file_to_rom()                                                      */
/******************************************************************************/
int art_add_file_to_rom()
{
  rom_create_file(ROM_FOLDER_SPRITES);

  /* palette */
  if (rom_add_words_to_file(&G_art_palette[0], VDP_COLORS_PER_PAL))
    return 1;

  /* uncompressed size, compressed size, then the nametable */
  if (rom_add_words_to_file(&G_art_num_elements, 1))
    return 1;

  if (rom_add_words_to_file(&G_art_num_elements, 1))
    return 1;

  if (rom_add_words_to_file(&G_art_elements[0], G_art_num_elements * VDP_NAME_ENTRY_SIZE))
    return 1;

  /* uncompressed size, compressed size, then the cells */
  if (rom_add_words_to_file(&G_art_num_cells, 1))
    return 1;

  if (rom_add_words_to_file(&G_art_num_cells, 1))
    return 1;

  if (rom_add_bytes_to_file(&G_art_cells[0], G_art_num_cells * VDP_BYTES_PER_CELL))
    return 1;

  return 0;
}

