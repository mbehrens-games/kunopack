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

#define VDP_VRAM_SIZE (1 << 16) /* 64 K (2048 cells, 32 bytes each) */

#define VDP_MAX_CELLS (VDP_VRAM_SIZE / VDP_BYTES_PER_CELL) /* 2048 */

/* 4 K words (so, 8 K bytes) */
#define VDP_NAME_ENTRY_SIZE 2
#define VDP_NAME_TABLE_SIZE (VDP_NAME_ENTRY_SIZE * VDP_MAX_CELLS)

#define VDP_SYMBOL_ENTRY_SIZE 1
#define VDP_SYMBOL_TABLE_SIZE (VDP_SYMBOL_ENTRY_SIZE * VDP_MAX_CELLS)

/* rom data variables */
unsigned short G_art_palette[VDP_COLORS_PER_PAL];
unsigned short G_art_elements[VDP_NAME_TABLE_SIZE];
unsigned char  G_art_cells[VDP_VRAM_SIZE];

unsigned short G_art_num_elements;
unsigned short G_art_num_cells;

static unsigned char S_art_palette_defined;

/* image variables */
#define ART_ANIM_FLAG_LOOP      0x0001
#define ART_ANIM_FLAG_PING_PONG 0x0002

static FILE* S_art_fp;

static unsigned short S_art_image_w;
static unsigned short S_art_image_h;

static unsigned short S_art_frame_rows;
static unsigned short S_art_frame_columns;
static unsigned short S_art_num_frames;
static unsigned short S_art_anim_ticks;
static unsigned short S_art_anim_flags;

/* gif */
#define ART_GIF_FLAG_GCT_EXISTS   0x0001
#define ART_GIF_FLAG_LCT_EXISTS   0x0002
#define ART_GIF_FLAG_PAL_FOUND    0x0004
#define ART_GIF_FLAG_DELAY_FOUND  0x0008
#define ART_GIF_FLAG_INTERLACED   0x0010

#define ART_GIF_DICT_MAX_ENTRIES  4096 /* 12 bits */
#define ART_GIF_DICT_MAX_BYTES    (2 * ART_GIF_DICT_MAX_ENTRIES)

static unsigned short S_art_gif_color_table_size;

static unsigned short S_art_gif_colors[VDP_COLORS_PER_PAL];

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

static unsigned short S_art_lzw_dict[ART_GIF_DICT_MAX_BYTES];
static unsigned short S_art_lzw_dict_size;

/* image buffers */
#define ART_MAX_FRAME_ROWS    16
#define ART_MAX_FRAME_COLUMNS 16
#define ART_MAX_NUM_FRAMES    8

#define ART_MAX_CELLS_PER_FRAME  (ART_MAX_FRAME_ROWS * ART_MAX_FRAME_COLUMNS)
#define ART_MAX_PIXELS_PER_FRAME (ART_MAX_CELLS_PER_FRAME * VDP_PIXELS_PER_CELL)

/* we multiply by 2 here, because a ping-pong animation */
/* will describe repeated frames before being reduced   */
#define ART_PIXELS_BUFFER_SIZE (2 * ART_MAX_NUM_FRAMES * ART_MAX_PIXELS_PER_FRAME)

static unsigned char  S_art_lzw_image_buf[ART_MAX_PIXELS_PER_FRAME];
static unsigned long  S_art_lzw_image_size;

/* the decompressed buffer is in words, because in the middle of    */
/* decompressing it may need to hold the codes (up to 12 bits each) */
static unsigned short S_art_decomp_image_buf[ART_MAX_PIXELS_PER_FRAME];
static unsigned long  S_art_decomp_image_size;

static unsigned char  S_art_pixels_buf[ART_PIXELS_BUFFER_SIZE];
static unsigned long  S_art_pixels_size;

/******************************************************************************/
/* art_clear_rom_data_vars()                                                  */
/******************************************************************************/
int art_clear_rom_data_vars()
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

  S_art_palette_defined = 0;

  return 0;
}

/******************************************************************************/
/* art_clear_image_vars()                                                     */
/******************************************************************************/
int art_clear_image_vars()
{
  unsigned long k;

  /* image variables */
  S_art_fp = NULL;

  S_art_image_w = 0;
  S_art_image_h = 0;

  S_art_frame_rows = 0;
  S_art_frame_columns = 0;
  S_art_num_frames = 0;
  S_art_anim_ticks = 0;
  S_art_anim_flags = 0x0000;

  /* image buffers */
  for (k = 0; k < ART_MAX_PIXELS_PER_FRAME; k++)
    S_art_lzw_image_buf[k] = 0;

  S_art_lzw_image_size = 0;

  for (k = 0; k < ART_MAX_PIXELS_PER_FRAME; k++)
    S_art_decomp_image_buf[k] = 0;

  S_art_decomp_image_size = 0;

  for (k = 0; k < ART_PIXELS_BUFFER_SIZE; k++)
    S_art_pixels_buf[k] = 0;

  S_art_pixels_size = 0;

  return 0;
}

/******************************************************************************/
/* art_clear_gif_lzw_vars()                                                   */
/******************************************************************************/
int art_clear_gif_lzw_vars()
{
  unsigned long k;

  /* gif */
  S_art_gif_color_table_size = 0;

  S_art_gif_sub_left = 0;
  S_art_gif_sub_top = 0;
  S_art_gif_sub_w = 0;
  S_art_gif_sub_h = 0;

  S_art_gif_flags = 0x0000;

  for (k = 0; k < VDP_COLORS_PER_PAL; k++)
    S_art_gif_colors[k] = 0;

  /* lzw */
  S_art_lzw_root_bits = 0;
  S_art_lzw_code_bits = 0;

  S_art_lzw_num_roots = 0;
  S_art_lzw_num_codes = 0;

  for (k = 0; k < ART_GIF_DICT_MAX_BYTES; k++)
    S_art_lzw_dict[k] = 0;

  S_art_lzw_dict_size = 0;

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
  if (fread(buf, sizeof(unsigned char), 3, S_art_fp) < 3)
    return 1;

  if ((buf[0] != 0x47) || (buf[1] != 0x49) || (buf[2] != 0x46))
    return 1;

  /* read "89a" */
  if (fread(buf, sizeof(unsigned char), 3, S_art_fp) < 3)
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
  if (fread(buf, sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;  

  S_art_image_w = (256 * buf[1]) + buf[0];

  if (fread(buf, sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  S_art_image_h = (256 * buf[1]) + buf[0];

  /* read packed fields */
  if (fread(buf, sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  if (buf[0] & 0x80)
  {
    S_art_gif_flags |= ART_GIF_FLAG_GCT_EXISTS;
    S_art_gif_color_table_size = 1 << ((buf[0] & 0x07) + 1);
  }
  else
    S_art_gif_color_table_size = 0;

  /* read background color and aspect ratio */
  if (fread(buf, sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  /* check that image width and height are valid */
  if ((S_art_image_w % VDP_CELL_W_H) != 0)
    return 1;

  if ((S_art_image_h % VDP_CELL_W_H) != 0)
    return 1;

  S_art_frame_rows = S_art_image_h / VDP_CELL_W_H;
  S_art_frame_columns = S_art_image_w / VDP_CELL_W_H;

  if ((S_art_frame_rows == 0) || (S_art_frame_rows > ART_MAX_FRAME_ROWS))
    return 1;

  if ((S_art_frame_columns == 0) || (S_art_frame_columns > ART_MAX_FRAME_COLUMNS))
    return 1;

  return 0;
}

/******************************************************************************/
/* art_gif_color_table()                                                      */
/******************************************************************************/
int art_gif_color_table()
{
  unsigned long k;

  unsigned char  buf[4];
  unsigned short val;

  unsigned short pal_colors[VDP_COLORS_PER_PAL];

  if (S_art_fp == NULL)
    return 1;

  /* initialize colors */
  for (k = 0; k < VDP_COLORS_PER_PAL; k++)
    pal_colors[k] = 0x0000;

  /* read palette */
  for (k = 0; k < S_art_gif_color_table_size; k++)
  {
    if (fread(buf, sizeof(unsigned char), 3, S_art_fp) < 3)
      return 1;

    if ((!(S_art_gif_flags & ART_GIF_FLAG_PAL_FOUND)) && (k < VDP_COLORS_PER_PAL))
    {
      /* convert to 15 bit rgb */
      val = ((buf[0] << 7) & 0x7C00) | ((buf[1] << 2) & 0x03E0) | ((buf[2] >> 3) & 0x001F);
      pal_colors[k] = val;
    }
  }

  /* save palette to buffer */
  if (!(S_art_gif_flags & ART_GIF_FLAG_PAL_FOUND))
  {
    for (k = 0; k < VDP_COLORS_PER_PAL; k++)
      S_art_gif_colors[k] = pal_colors[k];
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
  unsigned short delay_time;

  if (S_art_fp == NULL)
    return 1;

  /* read block size */
  if (fread(buf, sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  if (buf[0] != 4)
    return 1;

  /* read packed fields */
  if (fread(buf, sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  /* read delay time */
  if (fread(buf, sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  if (!(S_art_gif_flags & ART_GIF_FLAG_DELAY_FOUND))
  {
    delay_time = (256 * buf[1]) + buf[0];
    S_art_gif_flags |= ART_GIF_FLAG_DELAY_FOUND;

    /* convert delay time from 1/100ths of a second to 1/60ths */
    S_art_anim_ticks = 3 * (delay_time / 5);

    if (S_art_anim_ticks > 63)
      S_art_anim_ticks = 63;
    else
    {
      if (delay_time % 5 == 1)
        S_art_anim_ticks += 1;
      else if (delay_time % 5 == 2)
        S_art_anim_ticks += 1;
      else if (delay_time % 5 == 3)
        S_art_anim_ticks += 2;
      else if (delay_time % 5 == 4)
        S_art_anim_ticks += 2;
    } 
  }

  /* read transparent color */
  if (fread(buf, sizeof(unsigned char), 1, S_art_fp) < 1)
    return 1;

  /* block terminator */
  if (fread(buf, sizeof(unsigned char), 1, S_art_fp) < 1)
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
  if (fread(buf, sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  S_art_gif_sub_left = (256 * buf[1]) + buf[0];

  if (fread(buf, sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  S_art_gif_sub_top = (256 * buf[1]) + buf[0];

  if (fread(buf, sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  S_art_gif_sub_w = (256 * buf[1]) + buf[0];

  if (fread(buf, sizeof(unsigned char), 2, S_art_fp) < 2)
    return 1;

  S_art_gif_sub_h = (256 * buf[1]) + buf[0];

  /* read packed fields */
  if (fread(buf, sizeof(unsigned char), 1, S_art_fp) < 1)
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
      if ((S_art_lzw_image_size + c) > ART_MAX_PIXELS_PER_FRAME)
        return 1;

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
  unsigned long k;

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

  S_art_lzw_dict_size = S_art_lzw_num_roots + 2;

  return 0;
}

/******************************************************************************/
/* art_gif_decompress_image()                                                 */
/******************************************************************************/
int art_gif_decompress_image()
{
  unsigned long k;

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

  S_art_decomp_image_size = 0;

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
          if ((S_art_decomp_image_size + 1) > ART_MAX_PIXELS_PER_FRAME)
            return 1;

          S_art_decomp_image_buf[decomp_index] = S_art_lzw_dict[2 * code + 0];
          S_art_decomp_image_size += 1;
        }
        else
        {
          if ((S_art_decomp_image_size + 2) > ART_MAX_PIXELS_PER_FRAME)
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
        if ((S_art_decomp_image_size + 2) > ART_MAX_PIXELS_PER_FRAME)
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

        if ((S_art_decomp_image_size + 1) > ART_MAX_PIXELS_PER_FRAME)
          return 1;

        S_art_decomp_image_size += 1;

        for (k = S_art_decomp_image_size - 2; k > decomp_index; k--)
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
  unsigned long k;

  unsigned short pixel_addr;
  unsigned short pixel_offset;

  unsigned short last_addr;

  /* create space for this frame */
  S_art_pixels_size += S_art_image_w * S_art_image_h;

  if (S_art_pixels_size > ART_PIXELS_BUFFER_SIZE)
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
/* art_check_for_ping_pong_animation()                                        */
/******************************************************************************/
int art_check_for_ping_pong_animation()
{
  unsigned short k;
  unsigned short m;

  unsigned short pixel_addr_1;
  unsigned short pixel_addr_2;

  /* check number of frames first */
  if (S_art_num_frames > (2 * (ART_MAX_NUM_FRAMES - 1)))
    return 1;

  if ((S_art_num_frames < 4) || ((S_art_num_frames % 2) != 0))
    return 0;

  /* compare potential ping-pong frames */
  for (k = 1; k < S_art_num_frames / 2; k++)
  {
    pixel_addr_1 = k * (S_art_image_w * S_art_image_h);
    pixel_addr_2 = (S_art_num_frames - k) * (S_art_image_w * S_art_image_h);

    for (m = 0; m < (S_art_image_w * S_art_image_h); m++)
    {
      if (S_art_pixels_buf[pixel_addr_1 + m] != S_art_pixels_buf[pixel_addr_2 + m])
        return 0;
    }
  }

  /* ping-pong animation was found, so set the flag */
  S_art_anim_flags |= ART_ANIM_FLAG_PING_PONG;
  S_art_num_frames = (S_art_num_frames / 2) + 1;

  return 0;
}

/******************************************************************************/
/* art_add_palette()                                                          */
/******************************************************************************/
int art_add_palette()
{
  unsigned short k;

  /* check if we already added a palette */
  if (S_art_palette_defined == 1)
    return 0;

  /* add the gif colors to the palette! */
  for (k = 0; k < VDP_COLORS_PER_PAL; k++)
    G_art_palette[k] = S_art_gif_colors[k];

  S_art_palette_defined = 1;

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

      pixel_addr = k * (S_art_image_w * S_art_image_h);
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
/* art_load_gif()                                                             */
/******************************************************************************/
int art_load_gif(char* filename)
{
  unsigned char block_type;
  unsigned char ext_label;

  /* make sure filename is valid */
  if (filename == NULL)
    return 1;

  /* reset file-related variables */
  art_clear_image_vars();
  art_clear_gif_lzw_vars();

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
    if (fread(&block_type, sizeof(unsigned char), 1, S_art_fp) < 1)
      goto nope;

    /* extension */
    if (block_type == 0x21)
    {
      if (fread(&ext_label, sizeof(unsigned char), 1, S_art_fp) < 1)
        goto nope;

      /* app extension */
      if (ext_label == 0xFF)
      {
        if (art_gif_app_extension())
          goto nope;
      }
      /* comment extension */
      else if (ext_label == 0xFE)
      {
        if (art_gif_comment_extension())
          goto nope;
      }
      /* plain text extension */
      else if (ext_label == 0x01)
      {
        if (art_gif_plain_text_extension())
          goto nope;
      }
      /* graphic control extension */
      else if (ext_label == 0xF9)
      {
        if (art_gif_graphic_control_extension())
          goto nope;
      }
    }
    /* image descriptor */
    else if (block_type == 0x2C)
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
    else if (block_type == 0x3B)
      break;
  }

  /* close the file */
  fclose(S_art_fp);

  /* check for ping-pong animation and number of frames */
  if (art_check_for_ping_pong_animation())
    return 1;

  if ((S_art_num_frames == 0) || (S_art_num_frames > ART_MAX_NUM_FRAMES))
    return 1;

  /* for now, we set all animations to looping,     */
  /* instead of checking the netscape app extension */
  S_art_anim_flags |= ART_ANIM_FLAG_LOOP;

  /* add everything to the rom data buffers */
  art_add_palette();
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
/* art_add_chunks_to_rom()                                                    */
/******************************************************************************/
int art_add_chunks_to_rom()
{
  unsigned short chunk_index_palette;
  unsigned short chunk_index_elements;
  unsigned short chunk_index_cells;

  /* palette */
  if (rom_add_chunk_words(&chunk_index_palette, 
                          G_art_palette, VDP_COLORS_PER_PAL))
  {
    return 1;
  }

  /* nametable */
  if (rom_add_chunk_words(&chunk_index_elements, 
                          G_art_elements, G_art_num_elements * VDP_NAME_ENTRY_SIZE))
  {
    return 1;
  }

  /* cells */
  if (rom_add_chunk_bytes(&chunk_index_cells, 
                          G_art_cells, G_art_num_cells * VDP_BYTES_PER_CELL))
  {
    return 1;
  }

  printf( "Chunk Indices: %d %d %d\n", 
          chunk_index_palette, chunk_index_elements, chunk_index_cells);

  return 0;
}

