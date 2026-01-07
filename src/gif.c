/******************************************************************************/
/* gif.c (load gif images)                                                    */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "gif.h"
#include "vdp.h"

#define GIF_FLAG_GCT_EXISTS   0x0001
#define GIF_FLAG_LCT_EXISTS   0x0002
#define GIF_FLAG_PAL_FOUND    0x0004
#define GIF_FLAG_DELAY_FOUND  0x0008
#define GIF_FLAG_INTERLACED   0x0010

/* max bits is 65536 (16 bit index), so max bytes is 8192 */
#define GIF_IMAGE_MAX_BYTES   8192

#define GIF_DICT_MAX_ENTRIES  4096 /* 12 bits */
#define GIF_DICT_MAX_BYTES    (2 * GIF_DICT_MAX_ENTRIES)

static FILE* S_gif_fp;

static unsigned short S_gif_image_w;
static unsigned short S_gif_image_h;

static unsigned short S_gif_color_res;
static unsigned short S_gif_color_table_size;

static unsigned short S_gif_num_frames;
static unsigned short S_gif_delay_time;

static unsigned short S_gif_sub_left;
static unsigned short S_gif_sub_top;
static unsigned short S_gif_sub_w;
static unsigned short S_gif_sub_h;

static unsigned short S_gif_flags;

static unsigned short S_gif_pal_table[VDP_COLORS_PER_PAL];

static unsigned char  S_gif_lzw_root_bits;
static unsigned char  S_gif_lzw_code_bits;

static unsigned short S_gif_lzw_num_roots;
static unsigned short S_gif_lzw_num_codes;

static unsigned char  S_gif_lzw_dict[GIF_DICT_MAX_BYTES];
static unsigned short S_gif_lzw_dict_size;

static unsigned char  S_gif_lzw_image_buf[GIF_IMAGE_MAX_BYTES];
static unsigned short S_gif_lzw_image_size;

static unsigned char  S_gif_decomp_image_buf[GIF_IMAGE_MAX_BYTES];
static unsigned short S_gif_decomp_image_size;

static unsigned char  S_gif_frame_image_buf[GIF_IMAGE_MAX_BYTES];
static unsigned short S_gif_frame_image_size;

/******************************************************************************/
/* gif_reset()                                                                */
/******************************************************************************/
int gif_reset()
{
  int k;

  S_gif_fp = NULL;

  S_gif_image_w = 0;
  S_gif_image_h = 0;

  S_gif_color_res = 0;
  S_gif_color_table_size = 0;

  S_gif_num_frames = 0;
  S_gif_delay_time = 0;

  S_gif_sub_left = 0;
  S_gif_sub_top = 0;
  S_gif_sub_w = 0;
  S_gif_sub_h = 0;

  S_gif_flags = 0x0000;

  for (k = 0; k < VDP_COLORS_PER_PAL; k++)
    S_gif_pal_table[k] = 0x0000;

  S_gif_lzw_root_bits = 0;
  S_gif_lzw_code_bits = 0;

  S_gif_lzw_num_roots = 0;
  S_gif_lzw_num_codes = 0;

  for (k = 0; k < GIF_DICT_MAX_BYTES; k++)
    S_gif_lzw_dict[k] = 0;

  S_gif_lzw_dict_size = 0;

  for (k = 0; k < GIF_IMAGE_MAX_BYTES; k++)
    S_gif_lzw_image_buf[k] = 0;

  S_gif_lzw_image_size = 0;

  for (k = 0; k < GIF_IMAGE_MAX_BYTES; k++)
    S_gif_decomp_image_buf[k] = 0;

  S_gif_decomp_image_size = 0;

  for (k = 0; k < GIF_IMAGE_MAX_BYTES; k++)
    S_gif_frame_image_buf[k] = 0;

  S_gif_frame_image_size = 0;

  return 0;
}

/******************************************************************************/
/* gif_read_header()                                                          */
/******************************************************************************/
int gif_read_header()
{
  unsigned char buf[4];

  if (S_gif_fp == NULL)
    return 1;

  /* read "GIF" */
  if (fread(&buf[0], sizeof(unsigned char), 3, S_gif_fp) < 3)
    return 1;

  if ((buf[0] != 0x47) || (buf[1] != 0x49) || (buf[2] != 0x46))
    return 1;

  /* read "89a" */
  if (fread(&buf[0], sizeof(unsigned char), 3, S_gif_fp) < 3)
    return 1;

  if ((buf[0] != 0x38) || (buf[1] != 0x39) || (buf[2] != 0x61))
    return 1;

  return 0;
}

/******************************************************************************/
/* gif_read_logical_screen_descriptor()                                       */
/******************************************************************************/
int gif_read_logical_screen_descriptor()
{
  unsigned char buf[4];

  if (S_gif_fp == NULL)
    return 1;

  /* read image width and height */
  if (fread(&buf[0], sizeof(unsigned char), 2, S_gif_fp) < 2)
    return 1;  

  S_gif_image_w = (256 * buf[1]) + buf[0];

  if (fread(&buf[0], sizeof(unsigned char), 2, S_gif_fp) < 2)
    return 1;

  S_gif_image_h = (256 * buf[1]) + buf[0];

  /* read packed fields */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_gif_fp) < 1)
    return 1;

  if (buf[0] & 0x80)
  {
    S_gif_flags |= GIF_FLAG_GCT_EXISTS;
    S_gif_color_table_size = 1 << ((buf[0] & 0x07) + 1);
  }
  else
    S_gif_color_table_size = 0;

  S_gif_color_res = ((buf[0] >> 4) & 0x07) + 1;

  /* read background color and aspect ratio */
  if (fread(&buf[0], sizeof(unsigned char), 2, S_gif_fp) < 2)
    return 1;

  /* check that image width and height are valid */
  if ((S_gif_image_w % 8) != 0)
    return 1;

  if ((S_gif_image_h % 8) != 0)
    return 1;

  if ((S_gif_image_w * S_gif_image_h) > GIF_IMAGE_MAX_BYTES)
    return 1;

  printf("Image: W: %d, H: %d\n", S_gif_image_w, S_gif_image_h);

  return 0;
}

/******************************************************************************/
/* gif_read_color_table()                                                     */
/******************************************************************************/
int gif_read_color_table()
{
  int k;

  unsigned char  buf[4];
  unsigned short val;

  if (S_gif_fp == NULL)
    return 1;

  /* read palette */
  for (k = 0; k < S_gif_color_table_size; k++)
  {
    if (fread(&buf[0], sizeof(unsigned char), 3, S_gif_fp) < 3)
      return 1;

    if ((!(S_gif_flags & GIF_FLAG_PAL_FOUND)) && (k < VDP_COLORS_PER_PAL))
    {
      /* convert to 12 bit rgb */
      val = ((buf[0] << 4) & 0x0F00) | (buf[1] & 0x00F0) | ((buf[2] >> 4) & 0x000F);
      S_gif_pal_table[k] = val;
    }
  }

  S_gif_flags |= GIF_FLAG_PAL_FOUND;

  return 0;
}

/******************************************************************************/
/* gif_read_app_extension()                                                   */
/******************************************************************************/
int gif_read_app_extension()
{
  unsigned char c;

  if (S_gif_fp == NULL)
    return 1;

  /* note: we just skip all app extensions */

  /* read block size */
  if (fread(&c, sizeof(unsigned char), 1, S_gif_fp) < 1)
    return 1;

  if (c != 11)
    return 1;

  /* skip header block */
  if (fseek(S_gif_fp, 11, SEEK_CUR))
    return 1;

  /* skip sub-blocks */
  while (1)
  {
    if (fread(&c, sizeof(unsigned char), 1, S_gif_fp) < 1)
      return 1;

    /* sub-block */
    if (c > 0)
    {
      if (fseek(S_gif_fp, c, SEEK_CUR))
        return 1;
    }
    /* block terminator */
    else
      break;
  }

  return 0;
}

/******************************************************************************/
/* gif_read_comment_extension()                                               */
/******************************************************************************/
int gif_read_comment_extension()
{
  unsigned char c;

  if (S_gif_fp == NULL)
    return 1;

  /* note: we just skip all comment extensions */

  /* skip sub-blocks */
  while (1)
  {
    if (fread(&c, sizeof(unsigned char), 1, S_gif_fp) < 1)
      return 1;

    /* sub-block */
    if (c > 0)
    {
      if (fseek(S_gif_fp, c, SEEK_CUR))
        return 1;
    }
    /* block terminator */
    else
      break;
  }

  return 0;
}

/******************************************************************************/
/* gif_read_plain_text_extension()                                            */
/******************************************************************************/
int gif_read_plain_text_extension()
{
  unsigned char c;

  if (S_gif_fp == NULL)
    return 1;

  /* note: we just skip all plain text extensions */

  /* read block size */
  if (fread(&c, sizeof(unsigned char), 1, S_gif_fp) < 1)
    return 1;

  if (c != 12)
    return 1;

  /* skip header block */
  if (fseek(S_gif_fp, 12, SEEK_CUR))
    return 1;

  /* skip sub-blocks */
  while (1)
  {
    if (fread(&c, sizeof(unsigned char), 1, S_gif_fp) < 1)
      return 1;

    /* sub-block */
    if (c > 0)
    {
      if (fseek(S_gif_fp, c, SEEK_CUR))
        return 1;
    }
    /* block terminator */
    else
      break;
  }

  return 0;
}

/******************************************************************************/
/* gif_read_graphic_control_extension()                                       */
/******************************************************************************/
int gif_read_graphic_control_extension()
{
  unsigned char buf[4];

  if (S_gif_fp == NULL)
    return 1;

  /* read block size */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_gif_fp) < 1)
    return 1;

  if (buf[0] != 4)
    return 1;

  /* read packed fields */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_gif_fp) < 1)
    return 1;

  /* read delay time */
  if (fread(&buf[0], sizeof(unsigned char), 2, S_gif_fp) < 2)
    return 1;

  if (!(S_gif_flags & GIF_FLAG_DELAY_FOUND))
  {
    S_gif_delay_time = (256 * buf[1]) + buf[0];
    S_gif_flags |= GIF_FLAG_DELAY_FOUND;
  }

  /* read transparent color */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_gif_fp) < 1)
    return 1;

  /* block terminator */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_gif_fp) < 1)
    return 1;

  if (buf[0] != 0)
    return 1;

  return 0;
}

/******************************************************************************/
/* gif_read_image_descriptor()                                                */
/******************************************************************************/
int gif_read_image_descriptor()
{
  unsigned char buf[4];

  if (S_gif_fp == NULL)
    return 1;

  /* read sub-image corner and dimensions */
  if (fread(&buf[0], sizeof(unsigned char), 2, S_gif_fp) < 2)
    return 1;

  S_gif_sub_left = (256 * buf[1]) + buf[0];

  if (fread(&buf[0], sizeof(unsigned char), 2, S_gif_fp) < 2)
    return 1;

  S_gif_sub_top = (256 * buf[1]) + buf[0];

  if (fread(&buf[0], sizeof(unsigned char), 2, S_gif_fp) < 2)
    return 1;

  S_gif_sub_w = (256 * buf[1]) + buf[0];

  if (fread(&buf[0], sizeof(unsigned char), 2, S_gif_fp) < 2)
    return 1;

  S_gif_sub_h = (256 * buf[1]) + buf[0];

  /* read packed fields */
  if (fread(&buf[0], sizeof(unsigned char), 1, S_gif_fp) < 1)
    return 1;

  S_gif_flags &= ~GIF_FLAG_LCT_EXISTS;
  S_gif_flags &= ~GIF_FLAG_INTERLACED;

  if (buf[0] & 0x80)
  {
    S_gif_flags |= GIF_FLAG_LCT_EXISTS;
    S_gif_color_table_size = 1 << ((buf[0] & 0x07) + 1);
  }
  else
    S_gif_color_table_size = 0;

  if (buf[0] & 0x40)
    S_gif_flags |= GIF_FLAG_INTERLACED;

  /* check that sub-image corner and dimensions are valid */
  if ((S_gif_sub_left + S_gif_sub_w) > S_gif_image_w)
    return 1;

  if ((S_gif_sub_top + S_gif_sub_h) > S_gif_image_h)
    return 1;

  printf( "Sub-Image: Corner (%d, %d), W: %d, H: %d\n", 
          S_gif_sub_left, S_gif_sub_top, S_gif_sub_w, S_gif_sub_h);

  if (S_gif_flags & GIF_FLAG_LCT_EXISTS)
    printf("Local Color Table Found.\n");

  if (S_gif_flags & GIF_FLAG_INTERLACED)
    printf("Interlaced.\n");

  return 0;
}

/******************************************************************************/
/* gif_read_image_data()                                                      */
/******************************************************************************/
int gif_read_image_data()
{
  unsigned char c;

  if (S_gif_fp == NULL)
    return 1;

  /* read lzw minimum code size */
  if (fread(&S_gif_lzw_root_bits, sizeof(unsigned char), 1, S_gif_fp) < 1)
    return 1;

  S_gif_lzw_image_size = 0;

  /* read sub-blocks */
  while (1)
  {
    if (fread(&c, sizeof(unsigned char), 1, S_gif_fp) < 1)
      return 1;

    /* sub-block */
    if (c > 0)
    {
      if (fread(&S_gif_lzw_image_buf[S_gif_lzw_image_size], sizeof(unsigned char), c, S_gif_fp) < c)
        return 1;

      S_gif_lzw_image_size += c;
    }
    /* block terminator */
    else
      break;
  }

  printf("LZW Data Size: %d\n", S_gif_lzw_image_size);

  return 0;
}

/******************************************************************************/
/* gif_reset_dictionary()                                                     */
/******************************************************************************/
int gif_reset_dictionary()
{
  int k;

  /* clear dictionary */
  for (k = 0; k < GIF_DICT_MAX_BYTES; k++)
    S_gif_lzw_dict[k] = 0;

  S_gif_lzw_dict_size = 0;

  /* initialize number of roots and codes */
  S_gif_lzw_code_bits = S_gif_lzw_root_bits + 1;

  S_gif_lzw_num_roots = 1 << S_gif_lzw_root_bits;
  S_gif_lzw_num_codes = 1 << S_gif_lzw_code_bits;

  /* determine number of roots and add them to the table */
  for (k = 0; k < S_gif_lzw_num_roots; k++)
    S_gif_lzw_dict[2 * k + 0] = k;

  S_gif_lzw_dict_size = k + 2;

  return 0;
}

/******************************************************************************/
/* gif_decompress_image_data()                                                */
/******************************************************************************/
int gif_decompress_image_data()
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
  gif_reset_dictionary();

  /* start decompressing! */
  lzw_index = 0;
  bit = 0;
  mask = 0x01;

  code = 0x0000;
  prev = 0x0000;

  while (lzw_index < S_gif_lzw_image_size)
  {
    /* read the next code from the bitstream */
    prev = code;
    code = 0x0000;

    for (k = 0; k < S_gif_lzw_code_bits; k++)
    {
      if (bit == 0)
        mask = 0x01;
      else
        mask = 0x01 << bit; 

      if (S_gif_lzw_image_buf[lzw_index] & mask)
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
    if (code == S_gif_lzw_num_roots)
    {
      gif_reset_dictionary();
      continue;
    }
    /* end of stream */
    else if (code == (S_gif_lzw_num_roots + 1))
      break;
    /* lookup code in dictionary */
    else
    {
      decomp_index = S_gif_decomp_image_size;

      /* previously encountered code */
      if (code < S_gif_lzw_dict_size)
      {
        /* determine first character of current code */
        dict_index = code;

        while (dict_index >= S_gif_lzw_num_roots)
          dict_index = S_gif_lzw_dict[2 * dict_index + 0];

        /* output the string for the current code */
        if (code < S_gif_lzw_num_roots)
        {
          if ((S_gif_decomp_image_size + 1) > GIF_IMAGE_MAX_BYTES)
            return 1;

          S_gif_decomp_image_buf[decomp_index] = S_gif_lzw_dict[2 * code + 0];
          S_gif_decomp_image_size += 1;
        }
        else
        {
          if ((S_gif_decomp_image_size + 2) > GIF_IMAGE_MAX_BYTES)
            return 1;

          S_gif_decomp_image_buf[decomp_index + 0] = S_gif_lzw_dict[2 * code + 0];
          S_gif_decomp_image_buf[decomp_index + 1] = S_gif_lzw_dict[2 * code + 1];
          S_gif_decomp_image_size += 2;
        }
      }
      /* newly encountered code */
      else
      {
        /* note: the previous code should not be a clear code here */

        /* determine first character of previous code */
        dict_index = prev;

        while (dict_index >= S_gif_lzw_num_roots)
          dict_index = S_gif_lzw_dict[2 * dict_index + 0];

        /* output the string for the previous code, */
        /* concatenated with its first character    */
        if ((S_gif_decomp_image_size + 2) > GIF_IMAGE_MAX_BYTES)
          return 1;

        S_gif_decomp_image_buf[decomp_index + 0] = prev;
        S_gif_decomp_image_buf[decomp_index + 1] = dict_index;
        S_gif_decomp_image_size += 2;
      }

      /* add new entry to the dictionary */
      /* if the previous code was a clear code, we skip this */
      if (prev != S_gif_lzw_num_roots)
      {
        S_gif_lzw_dict[2 * S_gif_lzw_dict_size + 0] = prev;
        S_gif_lzw_dict[2 * S_gif_lzw_dict_size + 1] = dict_index;
        S_gif_lzw_dict_size += 1;

        if (S_gif_lzw_dict_size == S_gif_lzw_num_codes)
        {
          S_gif_lzw_code_bits += 1;

          if (S_gif_lzw_code_bits > 12)
            gif_reset_dictionary();
          else
            S_gif_lzw_num_codes = 1 << S_gif_lzw_code_bits;
        }
      }

      /* expand code down to the roots */
      while (S_gif_decomp_image_buf[decomp_index] >= S_gif_lzw_num_roots)
      {
        dict_index = S_gif_decomp_image_buf[decomp_index];

        if ((S_gif_decomp_image_size + 1) > GIF_IMAGE_MAX_BYTES)
          return 1;

        S_gif_decomp_image_size += 1;

        for (k = S_gif_decomp_image_size - 1; k > decomp_index; k--)
          S_gif_decomp_image_buf[k + 1] = S_gif_decomp_image_buf[k];

        S_gif_decomp_image_buf[decomp_index + 0] = S_gif_lzw_dict[2 * dict_index + 0];
        S_gif_decomp_image_buf[decomp_index + 1] = S_gif_lzw_dict[2 * dict_index + 1];
      }
    }
  }

  return 0;
}

/******************************************************************************/
/* gif_copy_image_data_to_frame()                                             */
/******************************************************************************/
int gif_copy_image_data_to_frame()
{
  int k;

  unsigned short pixel_addr;
  unsigned short pixel_offset;

  unsigned short last_addr;

  /* create space for this frame */
  S_gif_frame_image_size += S_gif_image_w * S_gif_image_h;

  if (S_gif_frame_image_size > GIF_IMAGE_MAX_BYTES)
    return 1;

  /* clear 1st frame, or copy the last frame to this one */
  pixel_addr = S_gif_num_frames * (S_gif_image_w * S_gif_image_h);

  if (S_gif_num_frames == 0)
  {
    for (k = 0; k < (S_gif_image_w * S_gif_image_h); k++)
      S_gif_frame_image_buf[pixel_addr + k] = 0;
  }
  else
  {
    last_addr = pixel_addr - (S_gif_image_w * S_gif_image_h);

    for (k = 0; k < (S_gif_image_w * S_gif_image_h); k++)
      S_gif_frame_image_buf[pixel_addr + k] = S_gif_frame_image_buf[last_addr + k];
  }

  /* copy decompressed pixels to this frame */
  for (k = 0; k < (S_gif_sub_w * S_gif_sub_h); k++)
  {
    pixel_offset = S_gif_sub_left;
    pixel_offset += S_gif_sub_top * S_gif_image_w;

    pixel_offset += k % S_gif_sub_w;
    pixel_offset += (k / S_gif_sub_w) * S_gif_image_w;
    
    S_gif_frame_image_buf[pixel_addr + pixel_offset] = S_gif_decomp_image_buf[k];
  }

  S_gif_num_frames += 1;

#if 0
  /* testing: print frame pixels */
  int m;

  printf("Printing Frame %d:\n", S_gif_num_frames);

  for (m = 0; m < S_gif_image_h; m++)
  {
    for (k = 0; k < S_gif_image_w; k++)
    {
      pixel_offset = (m * S_gif_image_w) + k;

      printf("%d ", S_gif_frame_image_buf[pixel_addr + pixel_offset]);
    }
    
    printf("\n");
  }
#endif

  return 0;
}

/******************************************************************************/
/* gif_add_palette_to_vdp()                                                   */
/******************************************************************************/
int gif_add_palette_to_vdp()
{
  int k;

  unsigned short pal_addr;

  /* make sure there is space for this palette */
  if ((G_vdp_pal_count + 1) > VDP_NUM_PALS)
    return 1;

  /* add palette to vdp */
  pal_addr = G_vdp_pal_count * VDP_COLORS_PER_PAL;

  for (k = 0; k < VDP_COLORS_PER_PAL; k++)
    G_vdp_pals[pal_addr + k] = S_gif_pal_table[k];

  G_vdp_pal_count += 1;

  return 0;
}

/******************************************************************************/
/* gif_add_cells_to_vdp()                                                     */
/******************************************************************************/
int gif_add_cells_to_vdp()
{
  int k;
  int m;
  int n;

  unsigned short num_rows;
  unsigned short num_columns;
  unsigned short num_cells;
  unsigned short total_cells;

  unsigned short cell_addr;
  unsigned short cell_offset;

  unsigned short pixel_addr;
  unsigned short pixel_offset;

  unsigned char val;

  /* determine how many cells are to be added */
  num_rows = S_gif_image_h / VDP_CELL_W_H;
  num_columns = S_gif_image_w / VDP_CELL_W_H;
  num_cells = num_rows * num_columns;

  total_cells = S_gif_num_frames * num_cells;

  /* make sure there is space for these cells */
  if ((G_vdp_cell_count + total_cells) > VDP_NUM_CELLS)
    return 1;

  /* add cells to vdp */
  for (k = 0; k < S_gif_num_frames; k++)
  {
    for (m = 0; m < num_cells; m++)
    {
      /* determine cell & pixel addresses */
      cell_addr = VDP_BYTES_PER_CELL * (G_vdp_cell_count + (k * num_cells) + m);
    
      pixel_addr = k * (S_gif_image_w * S_gif_image_h);
      pixel_addr += VDP_CELL_W_H * S_gif_image_w * (m / num_columns);
      pixel_addr += VDP_CELL_W_H * (m % num_columns);

      for (n = 0; n < VDP_PIXELS_PER_CELL; n++)
      {
        /* determine cell & pixel offsets */
        cell_offset = n / 2;

        pixel_offset = S_gif_image_w * (n / VDP_CELL_W_H);
        pixel_offset += n % VDP_CELL_W_H;

        /* obtain pixel value */
        val = S_gif_frame_image_buf[pixel_addr + pixel_offset];

        /* write this pixel value to the cells buffer */
        if (n % 2 == 0)
        {
          G_vdp_cells[cell_addr + cell_offset] &= 0x0F;
          G_vdp_cells[cell_addr + cell_offset] |= (val << 4) & 0xF0;
        }
        else
        {
          G_vdp_cells[cell_addr + cell_offset] &= 0xF0;
          G_vdp_cells[cell_addr + cell_offset] |= val & 0x0F;
        }
      }
    }
  }

  G_vdp_cell_count += total_cells;

  return 0;
}

/******************************************************************************/
/* gif_read_sprite()                                                          */
/******************************************************************************/
int gif_read_sprite(char* filename)
{
  unsigned char buf[4];

  /* make sure filename is valid */
  if (filename == NULL)
    return 1;

  /* reset static variables */
  gif_reset();

  /* open the file */
  S_gif_fp = fopen(filename, "rb");

  if (S_gif_fp == NULL)
    return 1;

  /* start parsing the file */
  if (gif_read_header())
    goto nope;

  if (gif_read_logical_screen_descriptor())
    goto nope;

  if ((S_gif_flags & GIF_FLAG_GCT_EXISTS) && gif_read_color_table())
    goto nope;

  while (1)
  {
    if (fread(&buf[0], sizeof(unsigned char), 1, S_gif_fp) < 1)
      goto nope;

    /* extension */
    if (buf[0] == 0x21)
    {
      if (fread(&buf[1], sizeof(unsigned char), 1, S_gif_fp) < 1)
        goto nope;

      /* app extension */
      if (buf[1] == 0xFF)
      {
        if (gif_read_app_extension())
          goto nope;
      }
      /* comment extension */
      else if (buf[1] == 0xFE)
      {
        if (gif_read_comment_extension())
          goto nope;
      }
      /* plain text extension */
      else if (buf[1] == 0x01)
      {
        if (gif_read_plain_text_extension())
          goto nope;
      }
      /* graphic control extension */
      else if (buf[1] == 0xF9)
      {
        if (gif_read_graphic_control_extension())
          goto nope;
      }
    }
    /* image descriptor */
    else if (buf[0] == 0x2C)
    {
      if (gif_read_image_descriptor())
        goto nope;

      if ((S_gif_flags & GIF_FLAG_LCT_EXISTS) && gif_read_color_table())
        goto nope;

      if (gif_read_image_data())
        goto nope;

      if (gif_decompress_image_data())
        goto nope;

      if (gif_copy_image_data_to_frame())
        goto nope;
    }
    /* trailer */
    else if (buf[0] == 0x3B)
      break;
  }

  /* close the file */
  fclose(S_gif_fp);

  printf("We made it.\n");

  gif_add_palette_to_vdp();
  gif_add_cells_to_vdp();

  printf("Added palette and cells.\n");

  goto ok;

nope:
  fclose(S_gif_fp);
  return 1;

ok:
  return 0;
}

