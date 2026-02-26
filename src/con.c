/******************************************************************************/
/* con.c (load configuration files)                                           */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "con.h"

#include "art.h"

enum
{
  CON_TOKEN_BLANK = 0, 
  CON_TOKEN_EOF, 
  CON_TOKEN_ERROR, 
  /* top level tables */
  CON_TOKEN_SPRITESET, 
  /* spriteset fields */
  CON_TOKEN_SPRITE, 
  /* common fields */
  CON_TOKEN_NAME, 
  CON_TOKEN_INTEGER, 
  CON_TOKEN_FILENAME, 
  /* syntax characters */
  CON_TOKEN_OPEN_CURLY_BRACE, 
  CON_TOKEN_CLOSE_CURLY_BRACE 
};

#define CON_CHARACTER_IS_UPPERCASE(c)                                          \
  ((c >= 'A') && (c <= 'Z'))

#define CON_CHARACTER_IS_LOWERCASE(c)                                          \
  ((c >= 'a') && (c <= 'z'))

#define CON_CHARACTER_IS_LETTER(c)                                             \
  (CON_CHARACTER_IS_UPPERCASE(c) || CON_CHARACTER_IS_LOWERCASE(c))

#define CON_CHARACTER_IS_DIGIT(c)                                              \
  ((c >= '0') && (c <= '9'))

#define CON_CHARACTER_IS_ALPHANUMERIC(c)                                       \
  (CON_CHARACTER_IS_LETTER(c) || CON_CHARACTER_IS_DIGIT(c))

#define CON_CHARACTER_IS_VALID_IN_IDENTIFIER(c)                                \
  (CON_CHARACTER_IS_ALPHANUMERIC(c) || (c == '_'))

#define CON_CHARACTER_IS_VALID_IN_FILENAME(c)                                  \
  (CON_CHARACTER_IS_ALPHANUMERIC(c) || (c == '_') || (c == '.'))

#define CON_CHARACTER_IS_WHITESPACE(c)                                         \
  ( (c == ' ')  || (c == '\n') || (c == '\r') ||                               \
    (c == '\f') || (c == '\t') || (c == '\v'))

#define CON_ADVANCE_AND_CHECK_TOKEN(expected)                                  \
  ((con_advance_token()) || (S_con_token != expected))

#define CON_STRING_MAX_SIZE 256

/* file pointer variables */
static FILE*          S_con_fp;
static unsigned short S_con_token;
static char           S_con_string_buf[CON_STRING_MAX_SIZE + 1];
static unsigned short S_con_string_size;

/******************************************************************************/
/* con_clear_parse_vars()                                                     */
/******************************************************************************/
int con_clear_parse_vars()
{
  unsigned short k;

  S_con_fp = NULL;
  S_con_token = CON_TOKEN_BLANK;

  for (k = 0; k < (CON_STRING_MAX_SIZE + 1); k++)
    S_con_string_buf[k] = '\0';

  S_con_string_size = 0;

  return 0;
}

/******************************************************************************/
/* con_advance_token()                                                        */
/******************************************************************************/
int con_advance_token()
{
  char c;

  /* read next character, and advance over any leading whitespace */
  do
  {
    if (fread(&c, sizeof(char), 1, S_con_fp) < 1)
    {
      S_con_token = CON_TOKEN_EOF;
      return 0;
    }
  } while (CON_CHARACTER_IS_WHITESPACE(c));

  /* identifier */
  if (CON_CHARACTER_IS_LETTER(c))
  {
    /* read the rest of the identifier */
    S_con_string_size = 0;

    do
    {
      if (S_con_string_size < CON_STRING_MAX_SIZE)
      {
        S_con_string_buf[S_con_string_size] = c; 
        S_con_string_size += 1;
      }

      if (fread(&c, sizeof(char), 1, S_con_fp) < 1)
        c = '\0';

    } while (CON_CHARACTER_IS_VALID_IN_IDENTIFIER(c));

    S_con_string_buf[S_con_string_size] = '\0'; 
    S_con_string_size += 1;

    /* check if this identifier is a defined keyword */
    if (!strcmp(S_con_string_buf, "spriteset"))
      S_con_token = CON_TOKEN_SPRITESET;
    else if (!strcmp(S_con_string_buf, "sprite"))
      S_con_token = CON_TOKEN_SPRITE;
    else
      S_con_token = CON_TOKEN_NAME;
  }
  /* integer */
  else if (CON_CHARACTER_IS_DIGIT(c))
  {
    /* read the rest of the integer */
    S_con_string_size = 0;

    do
    {
      if (S_con_string_size < CON_STRING_MAX_SIZE)
      {
        S_con_string_buf[S_con_string_size] = c; 
        S_con_string_size += 1;
      }

      if (fread(&c, sizeof(char), 1, S_con_fp) < 1)
        c = '\0';

    } while (CON_CHARACTER_IS_DIGIT(c));

    S_con_string_buf[S_con_string_size] = '\0'; 
    S_con_string_size += 1;

    S_con_token = CON_TOKEN_INTEGER;
  }
  /* filename */
  else if (c == '"')
  {
    /* advance over opening quote mark */
    if (fread(&c, sizeof(char), 1, S_con_fp) < 1)
    {
      S_con_token = CON_TOKEN_ERROR;
      return 1;
    }

    /* read the filename */
    S_con_string_size = 0;

    while (CON_CHARACTER_IS_VALID_IN_FILENAME(c))
    {
      if (S_con_string_size < CON_STRING_MAX_SIZE)
      {
        S_con_string_buf[S_con_string_size] = c; 
        S_con_string_size += 1;
      }

      if (fread(&c, sizeof(char), 1, S_con_fp) < 1)
      {
        S_con_token = CON_TOKEN_ERROR;
        return 1;
      }
    }

    S_con_string_buf[S_con_string_size] = '\0'; 
    S_con_string_size += 1;

    S_con_token = CON_TOKEN_FILENAME;

    /* check for closing quote mark */
    if (c != '"')
    {
      S_con_token = CON_TOKEN_ERROR;
      return 1;
    }
  }
  /* curly braces */
  else if (c == '{')
    S_con_token = CON_TOKEN_OPEN_CURLY_BRACE;
  else if (c == '}')
    S_con_token = CON_TOKEN_CLOSE_CURLY_BRACE;
  /* unknown token */
  else
  {
    S_con_token = CON_TOKEN_ERROR;
    return 1;
  }

  return 0;
}

/******************************************************************************/
/* con_parse_sprite()                                                         */
/******************************************************************************/
int con_parse_sprite()
{
  /* read name */
  if (CON_ADVANCE_AND_CHECK_TOKEN(CON_TOKEN_NAME))
    return 1;

  printf("Sprite Name: %s\n", S_con_string_buf);

  /* read filename */
  if (CON_ADVANCE_AND_CHECK_TOKEN(CON_TOKEN_FILENAME))
    return 1;

  printf("Sprite Filename: %s\n", S_con_string_buf);

  art_load_gif(S_con_string_buf);

  return 0;
}

/******************************************************************************/
/* con_parse_spriteset()                                                      */
/******************************************************************************/
int con_parse_spriteset()
{
  /* read name */
  if (CON_ADVANCE_AND_CHECK_TOKEN(CON_TOKEN_NAME))
    return 1;

  printf("Spriteset Name: %s\n", S_con_string_buf);

  /* read opening curly brace */
  if (CON_ADVANCE_AND_CHECK_TOKEN(CON_TOKEN_OPEN_CURLY_BRACE))
    return 1;

  /* read sprites */
  art_clear_rom_data_vars();

  if (con_advance_token())
    return 1;

  while (S_con_token == CON_TOKEN_SPRITE)
  {
    if (con_parse_sprite())
      return 1;

    /* read next sprite, or closing curly brace */
    if (con_advance_token())
      return 1;
  }

  art_add_chunks_to_rom();

  /* check closing curly brace */
  if (S_con_token != CON_TOKEN_CLOSE_CURLY_BRACE)
    return 1;

  return 0;
}

/******************************************************************************/
/* con_load_file()                                                            */
/******************************************************************************/
int con_load_file(char* filename)
{
  /* make sure filename is valid */
  if (filename == NULL)
    return 1;

  /* reset file-related variables */
  con_clear_parse_vars();

  /* open the file */
  S_con_fp = fopen(filename, "rb");

  if (S_con_fp == NULL)
    return 1;

  /* start parsing the file */
  S_con_token = CON_TOKEN_BLANK;

  while (S_con_token != CON_TOKEN_EOF)
  {
    if (con_advance_token())
      goto nope;

    if (S_con_token == CON_TOKEN_SPRITESET)
    {
      if (con_parse_spriteset())
        goto nope;
    }
    else
      goto nope;
  }

  /* close the file */
  fclose(S_con_fp);

  goto ok;

nope:
  fclose(S_con_fp);
  return 1;

ok:
  return 0;
}

