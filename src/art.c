/******************************************************************************/
/* art.c (load art files)                                                     */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"

#include "rom.h"

/* graphics constants (same as in vdp.h in the main code) */
#define ART_COLORS_PER_PAL  16

#define ART_CELL_W_H        8
#define ART_PIXELS_PER_CELL (ART_CELL_W_H * ART_CELL_W_H)
#define ART_BYTES_PER_CELL  (ART_PIXELS_PER_CELL / 2)

/* image buffer constants */
#define ART_CELLS_PER_IMAGE   256

#define ART_PIXELS_MAX_BYTES  (ART_CELLS_PER_IMAGE * ART_PIXELS_PER_CELL)
#define ART_CELLS_MAX_BYTES   (ART_CELLS_PER_IMAGE * ART_BYTES_PER_CELL)

/* globals */
unsigned char   G_art_rom_data_buf[ART_ROM_DATA_MAX_BYTES];
unsigned short  G_art_rom_data_size;

/* file pointer, image info */
static FILE* S_art_fp;

static unsigned short S_art_image_w;
static unsigned short S_art_image_h;

static unsigned short S_art_num_frames;
static unsigned short S_art_delay_time;

/* gif */
#define ART_GIF_FLAG_GCT_EXISTS   0x0001
#define ART_GIF_FLAG_LCT_EXISTS   0x0002
#define ART_GIF_FLAG_PAL_FOUND    0x0004
#define ART_GIF_FLAG_DELAY_FOUND  0x0008
#define ART_GIF_FLAG_INTERLACED   0x0010

#define ART_GIF_DICT_MAX_ENTRIES  4096 /* 12 bits */
#define ART_GIF_DICT_MAX_BYTES    (2 * ART_GIF_DICT_MAX_ENTRIES)

static unsigned short S_art_palette[ART_COLORS_PER_PAL];

static unsigned short S_art_gif_color_table_size;

static unsigned short S_art_gif_sub_left;
static unsigned short S_art_gif_sub_top;
static unsigned short S_art_gif_sub_w;
static unsigned short S_art_gif_sub_h;

static unsigned short S_art_gif_flags;

/* lzw */
static unsigned char  S_art_lzw_root_bits;
static unsigned char  S_art_lzw_code_bits;

static unsigned short S_art_lzw_num_roots;
static unsigned short S_art_lzw_num_codes;

static unsigned char  S_art_lzw_dict[ART_GIF_DICT_MAX_BYTES];
static unsigned short S_art_lzw_dict_size;

/* image buffers */
static unsigned char  S_art_lzw_image_buf[ART_PIXELS_MAX_BYTES];
static unsigned short S_art_lzw_image_size;

static unsigned char  S_art_decomp_image_buf[ART_PIXELS_MAX_BYTES];
static unsigned short S_art_decomp_image_size;

static unsigned char  S_art_pixels_buf[ART_PIXELS_MAX_BYTES];
static unsigned short S_art_pixels_size;

static unsigned char  S_art_cells_buf[ART_CELLS_MAX_BYTES];
static unsigned short S_art_cells_size;

/******************************************************************************/
/* art_init()                                                                 */
/******************************************************************************/
int art_init()
{
  int k;

  /* globals */
  for (k = 0; k < ART_ROM_DATA_MAX_BYTES; k++)
    G_art_rom_data_buf[k] = 0;

  G_art_rom_data_size = 0;

  /* file pointer, image info */
  S_art_fp = NULL;

  S_art_image_w = 0;
  S_art_image_h = 0;

  S_art_num_frames = 0;
  S_art_delay_time = 0;

  /* gif */
  for (k = 0; k < ART_COLORS_PER_PAL; k++)
    S_art_palette[k] = 0x0000;

  S_art_gif_color_table_size = 0;

  S_art_gif_sub_left = 0;
  S_art_gif_sub_top = 0;
  S_art_gif_sub_w = 0;
  S_art_gif_sub_h = 0;

  S_art_gif_flags = 0x0000;

  /* lzw */
  S_art_lzw_root_bits = 0;
  S_art_lzw_code_bits = 0;

  S_art_lzw_num_roots = 0;
  S_art_lzw_num_codes = 0;

  for (k = 0; k < ART_GIF_DICT_MAX_BYTES; k++)
    S_art_lzw_dict[k] = 0;

  S_art_lzw_dict_size = 0;

  for (k = 0; k < ART_PIXELS_MAX_BYTES; k++)
    S_art_lzw_image_buf[k] = 0;

  S_art_lzw_image_size = 0;

  for (k = 0; k < ART_PIXELS_MAX_BYTES; k++)
    S_art_decomp_image_buf[k] = 0;

  S_art_decomp_image_size = 0;

  for (k = 0; k < ART_PIXELS_MAX_BYTES; k++)
    S_art_pixels_buf[k] = 0;

  S_art_pixels_size = 0;

  for (k = 0; k < ART_CELLS_MAX_BYTES; k++)
    S_art_cells_buf[k] = 0;

  S_art_cells_size = 0;

  return 0;
}

/******************************************************************************/
/* art_gif_header()                                                           */
/******************************************************************************/
int art_gif_header()
{
  unsigned char buf[4];

  if (S_art_fp == NULL)
    return 1;

  /* read "GIF" */
  if (fread(&buf[0], sizeof(unsigned char), 3, S_art_fp) < 3)
    return 1;

  if ((buf[0] != 0x47) || (buf[1] != 0x49) || (buf[2] != 0x46))
    return 1;

  /* read "89a" */
  if (fread(&buf[0], sizeof(unsigned char), 3, S_art_fp) < 3)
    return 1;

  if ((buf[0] != 0x38) || (buf[1] != 0x39) || (buf[2] != 0x61))
    return 1;

  return 0;
}

/******************************************************************************/
/* art_gif_logical_screen_descriptor()                                        */
/******************************************************************************/
int art_gif_logical_screen_descriptor()
{
  unsigned char buf[4];

  if (S_art_fp == NULL)
    return 1;

  /* read image width and height */
  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;  

  S_art_image_w = (256 * buf[1]) + buf[0];

  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  S_art_image_h = (256 * buf[1]) + buf[0];

  /* read packed fields */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  if (buf[0] & 0x80)
  {
    S_art_gif_flags |= ART_GIF_FLAG_GCT_EXISTS;
    S_art_gif_color_table_size = 1 << ((buf[0] & 0x07) + 1);
  }
  else
    S_art_gif_color_table_size = 0;

  /* read background color and aspect ratio */
  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  /* check that image width and height are valid */
  if ((S_art_image_w % 8) != 0)
    return 1;

  if ((S_art_image_h % 8) != 0)
    return 1;

  if ((S_art_image_w * S_art_image_h) > ART_PIXELS_MAX_BYTES)
    return 1;

  return 0;
}

/******************************************************************************/
/* art_gif_color_table()                                                      */
/******************************************************************************/
int art_gif_color_table()
{
  int k;

  unsigned char  buf[4];
  unsigned short val;

  if (S_art_fp == NULL)
    return 1;

  /* read palette */
  for (k = 0; k < S_art_gif_color_table_size; k++)
  {
    if (fread(&buf[0], sizeof(unsigned char), 3, S_art_fp) < 3)
      return 1;

    if ((!(S_art_gif_flags & ART_GIF_FLAG_PAL_FOUND)) && (k < ART_COLORS_PER_PAL))
    {
      /* convert to 15 bit rgb */
      val = ((buf[0] << 7) & 0x7C00) | ((buf[1] << 2) & 0x03E0) | ((buf[2] >> 3) & 0x001F);
      S_art_palette[k] = val;
    }
  }

  S_art_gif_flags |= ART_GIF_FLAG_PAL_FOUND;

  return 0;
}

/******************************************************************************/
/* art_gif_app_extension()                                                    */
/******************************************************************************/
int art_gif_app_extension()
{
  unsigned char c;

  if (S_art_fp == NULL)
    return 1;

  /* note: we just skip all app extensions */

  /* read block size */
  if (fread(&c, sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  if (c != 11)
    return 1;

  /* skip header block */
  if (fseek(S_art_fp, 11, SEEK_CUR))
    return 1;

  /* skip sub-blocks */
  while (1)
  {
    if (fread(&c, sizeof(unsigned char), 1, S_art_fp) < 1)
      return 1;

    /* sub-block */
    if (c > 0)
    {
      if (fseek(S_art_fp, c, SEEK_CUR))
        return 1;
    }
    /* block terminator */
    else
      break;
  }

  return 0;
}

/******************************************************************************/
/* art_gif_comment_extension()                                                */
/******************************************************************************/
int art_gif_comment_extension()
{
  unsigned char c;

  if (S_art_fp == NULL)
    return 1;

  /* note: we just skip all comment extensions */

  /* skip sub-blocks */
  while (1)
  {
    if (fread(&c, sizeof(unsigned char), 1, S_art_fp) < 1)
      return 1;

    /* sub-block */
    if (c > 0)
    {
      if (fseek(S_art_fp, c, SEEK_CUR))
        return 1;
    }
    /* block terminator */
    else
      break;
  }

  return 0;
}

/******************************************************************************/
/* art_gif_plain_text_extension()                                             */
/******************************************************************************/
int art_gif_plain_text_extension()
{
  unsigned char c;

  if (S_art_fp == NULL)
    return 1;

  /* note: we just skip all plain text extensions */

  /* read block size */
  if (fread(&c, sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  if (c != 12)
    return 1;

  /* skip header block */
  if (fseek(S_art_fp, 12, SEEK_CUR))
    return 1;

  /* skip sub-blocks */
  while (1)
  {
    if (fread(&c, sizeof(unsigned char), 1, S_art_fp) < 1)
      return 1;

    /* sub-block */
    if (c > 0)
    {
      if (fseek(S_art_fp, c, SEEK_CUR))
        return 1;
    }
    /* block terminator */
    else
      break;
  }

  return 0;
}

/******************************************************************************/
/* art_gif_graphic_control_extension()                                        */
/******************************************************************************/
int art_gif_graphic_control_extension()
{
  unsigned char buf[4];

  if (S_art_fp == NULL)
    return 1;

  /* read block size */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  if (buf[0] != 4)
    return 1;

  /* read packed fields */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  /* read delay time */
  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  if (!(S_art_gif_flags & ART_GIF_FLAG_DELAY_FOUND))
  {
    S_art_delay_time = (256 * buf[1]) + buf[0];
    S_art_gif_flags |= ART_GIF_FLAG_DELAY_FOUND;
  }

  /* read transparent color */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  /* block terminator */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  if (buf[0] != 0)
    return 1;

  return 0;
}

/******************************************************************************/
/* art_gif_image_descriptor()                                                 */
/******************************************************************************/
int art_gif_image_descriptor()
{
  unsigned char buf[4];

  if (S_art_fp == NULL)
    return 1;

  /* read sub-image corner and dimensions */
  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  S_art_gif_sub_left = (256 * buf[1]) + buf[0];

  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  S_art_gif_sub_top = (256 * buf[1]) + buf[0];

  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  S_art_gif_sub_w = (256 * buf[1]) + buf[0];

  if (fread(&buf[0], sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  S_art_gif_sub_h = (256 * buf[1]) + buf[0];

  /* read packed fields */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  S_art_gif_flags &= ~ART_GIF_FLAG_LCT_EXISTS;
  S_art_gif_flags &= ~ART_GIF_FLAG_INTERLACED;

  if (buf[0] & 0x80)
  {
    S_art_gif_flags |= ART_GIF_FLAG_LCT_EXISTS;
    S_art_gif_color_table_size = 1 << ((buf[0] & 0x07) + 1);
  }
  else
    S_art_gif_color_table_size = 0;

  if (buf[0] & 0x40)
    S_art_gif_flags |= ART_GIF_FLAG_INTERLACED;

  /* check that sub-image corner and dimensions are valid */
  if ((S_art_gif_sub_left + S_art_gif_sub_w) > S_art_image_w)
    return 1;

  if ((S_art_gif_sub_top + S_art_gif_sub_h) > S_art_image_h)
    return 1;

  return 0;
}

/******************************************************************************/
/* art_gif_image_data()                                                       */
/******************************************************************************/
int art_gif_image_data()
{
  unsigned char c;

  if (S_art_fp == NULL)
    return 1;

  /* read lzw minimum code size */
  if (fread(&S_art_lzw_root_bits, sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  S_art_lzw_image_size = 0;

  /* read sub-blocks */
  while (1)
  {
    if (fread(&c, sizeof(unsigned char), 1, S_art_fp) < 1)
      return 1;

    /* sub-block */
    if (c > 0)
    {
      if (fread(&S_art_lzw_image_buf[S_art_lzw_image_size], sizeof(unsigned char), c, S_art_fp) < c)
        return 1;

      S_art_lzw_image_size += c;
    }
    /* block terminator */
    else
      break;
  }

  return 0;
}

/******************************************************************************/
/* art_gif_init_dictionary()                                                  */
/******************************************************************************/
int art_gif_init_dictionary()
{
  int k;

  /* clear dictionary */
  for (k = 0; k < ART_GIF_DICT_MAX_BYTES; k++)
    S_art_lzw_dict[k] = 0;

  S_art_lzw_dict_size = 0;

  /* initialize number of roots and codes */
  S_art_lzw_code_bits = S_art_lzw_root_bits + 1;

  S_art_lzw_num_roots = 1 << S_art_lzw_root_bits;
  S_art_lzw_num_codes = 1 << S_art_lzw_code_bits;

  /* determine number of roots and add them to the table */
  for (k = 0; k < S_art_lzw_num_roots; k++)
    S_art_lzw_dict[2 * k + 0] = k;

  S_art_lzw_dict_size = k + 2;

  return 0;
}

/******************************************************************************/
/* art_gif_decompress_image()                                                 */
/******************************************************************************/
int art_gif_decompress_image()
{
  int k;

  unsigned short dict_index;
  unsigned short lzw_index;
  unsigned short decomp_index;

  unsigned short bit;
  unsigned short mask;

  unsigned short code;
  unsigned short prev;

  /* initialize the dictionary */
  art_gif_init_dictionary();

  /* start decompressing! */
  lzw_index = 0;
  bit = 0;
  mask = 0x01;

  code = 0x0000;
  prev = 0x0000;

  while (lzw_index < S_art_lzw_image_size)
  {
    /* read the next code from the bitstream */
    prev = code;
    code = 0x0000;

    for (k = 0; k < S_art_lzw_code_bits; k++)
    {
      if (bit == 0)
        mask = 0x01;
      else
        mask = 0x01 << bit; 

      if (S_art_lzw_image_buf[lzw_index] & mask)
      {
        if (k == 0)
          code |= 0x0001;
        else
          code |= 0x0001 << k;
      }

      bit += 1;

      lzw_index += bit / 8;
      bit = bit % 8;
    }

    /* clear code */
    if (code == S_art_lzw_num_roots)
    {
      art_gif_init_dictionary();
      continue;
    }
    /* end of stream */
    else if (code == (S_art_lzw_num_roots + 1))
      break;
    /* lookup code in dictionary */
    else
    {
      decomp_index = S_art_decomp_image_size;

      /* previously encountered code */
      if (code < S_art_lzw_dict_size)
      {
        /* determine first character of current code */
        dict_index = code;

        while (dict_index >= S_art_lzw_num_roots)
          dict_index = S_art_lzw_dict[2 * dict_index + 0];

        /* output the string for the current code */
        if (code < S_art_lzw_num_roots)
        {
          if ((S_art_decomp_image_size + 1) > ART_PIXELS_MAX_BYTES)
            return 1;

          S_art_decomp_image_buf[decomp_index] = S_art_lzw_dict[2 * code + 0];
          S_art_decomp_image_size += 1;
        }
        else
        {
          if ((S_art_decomp_image_size + 2) > ART_PIXELS_MAX_BYTES)
            return 1;

          S_art_decomp_image_buf[decomp_index + 0] = S_art_lzw_dict[2 * code + 0];
          S_art_decomp_image_buf[decomp_index + 1] = S_art_lzw_dict[2 * code + 1];
          S_art_decomp_image_size += 2;
        }
      }
      /* newly encountered code */
      else
      {
        /* note: the previous code should not be a clear code here */

        /* determine first character of previous code */
        dict_index = prev;

        while (dict_index >= S_art_lzw_num_roots)
          dict_index = S_art_lzw_dict[2 * dict_index + 0];

        /* output the string for the previous code, */
        /* concatenated with its first character    */
        if ((S_art_decomp_image_size + 2) > ART_PIXELS_MAX_BYTES)
          return 1;

        S_art_decomp_image_buf[decomp_index + 0] = prev;
        S_art_decomp_image_buf[decomp_index + 1] = dict_index;
        S_art_decomp_image_size += 2;
      }

      /* add new entry to the dictionary */
      /* if the previous code was a clear code, we skip this */
      if (prev != S_art_lzw_num_roots)
      {
        S_art_lzw_dict[2 * S_art_lzw_dict_size + 0] = prev;
        S_art_lzw_dict[2 * S_art_lzw_dict_size + 1] = dict_index;
        S_art_lzw_dict_size += 1;

        if (S_art_lzw_dict_size == S_art_lzw_num_codes)
        {
          S_art_lzw_code_bits += 1;

          if (S_art_lzw_code_bits > 12)
            art_gif_init_dictionary();
          else
            S_art_lzw_num_codes = 1 << S_art_lzw_code_bits;
        }
      }

      /* expand code down to the roots */
      while (S_art_decomp_image_buf[decomp_index] >= S_art_lzw_num_roots)
      {
        dict_index = S_art_decomp_image_buf[decomp_index];

        if ((S_art_decomp_image_size + 1) > ART_PIXELS_MAX_BYTES)
          return 1;

        S_art_decomp_image_size += 1;

        for (k = S_art_decomp_image_size - 1; k > decomp_index; k--)
          S_art_decomp_image_buf[k + 1] = S_art_decomp_image_buf[k];

        S_art_decomp_image_buf[decomp_index + 0] = S_art_lzw_dict[2 * dict_index + 0];
        S_art_decomp_image_buf[decomp_index + 1] = S_art_lzw_dict[2 * dict_index + 1];
      }
    }
  }

  return 0;
}

/******************************************************************************/
/* art_gif_copy_image_to_pixels()                                             */
/******************************************************************************/
int art_gif_copy_image_to_pixels()
{
  int k;

  unsigned short pixel_addr;
  unsigned short pixel_offset;

  unsigned short last_addr;

  /* create space for this frame */
  S_art_pixels_size += S_art_image_w * S_art_image_h;

  if (S_art_pixels_size > ART_PIXELS_MAX_BYTES)
    return 1;

  /* clear 1st frame, or copy the last frame to this one */
  pixel_addr = S_art_num_frames * (S_art_image_w * S_art_image_h);

  if (S_art_num_frames == 0)
  {
    for (k = 0; k < (S_art_image_w * S_art_image_h); k++)
      S_art_pixels_buf[pixel_addr + k] = 0;
  }
  else
  {
    last_addr = pixel_addr - (S_art_image_w * S_art_image_h);

    for (k = 0; k < (S_art_image_w * S_art_image_h); k++)
      S_art_pixels_buf[pixel_addr + k] = S_art_pixels_buf[last_addr + k];
  }

  /* copy decompressed pixels to this frame */
  for (k = 0; k < (S_art_gif_sub_w * S_art_gif_sub_h); k++)
  {
    pixel_offset = S_art_gif_sub_left;
    pixel_offset += S_art_gif_sub_top * S_art_image_w;

    pixel_offset += k % S_art_gif_sub_w;
    pixel_offset += (k / S_art_gif_sub_w) * S_art_image_w;
    
    S_art_pixels_buf[pixel_addr + pixel_offset] = S_art_decomp_image_buf[k];
  }

  S_art_num_frames += 1;

  return 0;
}

/******************************************************************************/
/* art_convert_pixels_to_cells()                                              */
/******************************************************************************/
int art_convert_pixels_to_cells()
{
  int k;
  int m;
  int n;

  unsigned short num_rows;
  unsigned short num_columns;
  unsigned short num_cells;

  unsigned short cell_addr;
  unsigned short cell_offset;

  unsigned short pixel_addr;
  unsigned short pixel_offset;

  unsigned char val;

  /* determine how many cells are to be created */
  num_rows = S_art_image_h / ART_CELL_W_H;
  num_columns = S_art_image_w / ART_CELL_W_H;
  num_cells = num_rows * num_columns;

  if ((S_art_num_frames * num_cells) > ART_CELLS_PER_IMAGE)
    return 1;

  /* create cells */
  for (k = 0; k < S_art_num_frames; k++)
  {
    for (m = 0; m < num_cells; m++)
    {
      /* determine cell & pixel addresses */
      cell_addr = ART_BYTES_PER_CELL * ((k * num_cells) + m);
    
      pixel_addr = k * (S_art_image_w * S_art_image_h);
      pixel_addr += ART_CELL_W_H * S_art_image_w * (m / num_columns);
      pixel_addr += ART_CELL_W_H * (m % num_columns);

      for (n = 0; n < ART_PIXELS_PER_CELL; n++)
      {
        /* determine cell & pixel offsets */
        cell_offset = n / 2;

        pixel_offset = S_art_image_w * (n / ART_CELL_W_H);
        pixel_offset += n % ART_CELL_W_H;

        /* obtain pixel value */
        val = S_art_pixels_buf[pixel_addr + pixel_offset];

        /* write this pixel value to the cells buffer */
        if (n % 2 == 0)
        {
          S_art_cells_buf[cell_addr + cell_offset] &= 0x0F;
          S_art_cells_buf[cell_addr + cell_offset] |= (val << 4) & 0xF0;
        }
        else
        {
          S_art_cells_buf[cell_addr + cell_offset] &= 0xF0;
          S_art_cells_buf[cell_addr + cell_offset] |= val & 0x0F;
        }
      }
    }
  }

  S_art_cells_size = S_art_num_frames * num_cells * ART_BYTES_PER_CELL;

  return 0;
}

/******************************************************************************/
/* art_compose_rom_data()                                                     */
/******************************************************************************/
int art_compose_rom_data()
{
  int k;

  /* initialize data */
  for (k = 0; k < ART_ROM_DATA_MAX_BYTES; k++)
    G_art_rom_data_buf[k] = 0;

  G_art_rom_data_size = 0;

  /* add header */
  G_art_rom_data_buf[0] = (((S_art_image_w / 8) - 1) << 4) & 0xF0;
  G_art_rom_data_buf[0] |= ((S_art_image_h / 8) - 1) & 0x0F;

  G_art_rom_data_buf[1] = (S_art_num_frames - 1) & 0x0F;

  G_art_rom_data_buf[2] = (S_art_delay_time >> 8) & 0x0F;
  G_art_rom_data_buf[3] = S_art_delay_time & 0x0F;

  G_art_rom_data_size += 4;

  /* add palette */
  for (k = 0; k < ART_COLORS_PER_PAL; k++)
  {
    G_art_rom_data_buf[G_art_rom_data_size + 0] = (S_art_palette[k] >> 8) & 0x0F;
    G_art_rom_data_buf[G_art_rom_data_size + 1] = S_art_palette[k] & 0x0F;

    G_art_rom_data_size += 2;
  }

  /* add cells */
  memcpy(&G_art_rom_data_buf[G_art_rom_data_size], &S_art_cells_buf[0], S_art_cells_size);

  G_art_rom_data_size += S_art_cells_size;

  return 0;
}

/******************************************************************************/
/* art_load_gif()                                                             */
/******************************************************************************/
int art_load_gif(char* filename)
{
  unsigned char buf[4];

  /* make sure filename is valid */
  if (filename == NULL)
    return 1;

  /* reset variables */
  art_init();

  /* open the file */
  S_art_fp = fopen(filename, "rb");

  if (S_art_fp == NULL)
    return 1;

  /* start parsing the file */
  if (art_gif_header())
    goto nope;

  if (art_gif_logical_screen_descriptor())
    goto nope;

  if ((S_art_gif_flags & ART_GIF_FLAG_GCT_EXISTS) && art_gif_color_table())
    goto nope;

  while (1)
  {
    if (fread(&buf[0], sizeof(unsigned char), 1, S_art_fp) < 1)
      goto nope;

    /* extension */
    if (buf[0] == 0x21)
    {
      if (fread(&buf[1], sizeof(unsigned char), 1, S_art_fp) < 1)
        goto nope;

      /* app extension */
      if (buf[1] == 0xFF)
      {
        if (art_gif_app_extension())
          goto nope;
      }
      /* comment extension */
      else if (buf[1] == 0xFE)
      {
        if (art_gif_comment_extension())
          goto nope;
      }
      /* plain text extension */
      else if (buf[1] == 0x01)
      {
        if (art_gif_plain_text_extension())
          goto nope;
      }
      /* graphic control extension */
      else if (buf[1] == 0xF9)
      {
        if (art_gif_graphic_control_extension())
          goto nope;
      }
    }
    /* image descriptor */
    else if (buf[0] == 0x2C)
    {
      if (art_gif_image_descriptor())
        goto nope;

      if ((S_art_gif_flags & ART_GIF_FLAG_LCT_EXISTS) && art_gif_color_table())
        goto nope;

      if (art_gif_image_data())
        goto nope;

      if (art_gif_decompress_image())
        goto nope;

      if (art_gif_copy_image_to_pixels())
        goto nope;
    }
    /* trailer */
    else if (buf[0] == 0x3B)
      break;
  }

  /* close the file */
  fclose(S_art_fp);

  /* convert to rom data and add it to the rom */
  art_convert_pixels_to_cells();
  art_compose_rom_data();

  rom_add_sprite(&G_art_rom_data_buf[0], G_art_rom_data_size);

  goto ok;

nope:
  fclose(S_art_fp);
  return 1;

ok:
  return 0;
}

