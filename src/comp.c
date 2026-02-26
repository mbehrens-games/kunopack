/******************************************************************************/
/* comp.c (compile the rom folder)                                            */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "comp.h"

#include "art.h"

/* character macros */
#define COMP_CHARACTER_IS_UPPERCASE(c)                                         \
  ((c >= 'A') && (c <= 'Z'))

#define COMP_CHARACTER_IS_LOWERCASE(c)                                         \
  ((c >= 'a') && (c <= 'z'))

#define COMP_CHARACTER_IS_LETTER(c)                                            \
  (COMP_CHARACTER_IS_UPPERCASE(c) || COMP_CHARACTER_IS_LOWERCASE(c))

#define COMP_CHARACTER_IS_DIGIT(c)                                             \
  ((c >= '0') && (c <= '9'))

#define COMP_CHARACTER_IS_ALPHANUMERIC(c)                                      \
  (COMP_CHARACTER_IS_LETTER(c) || COMP_CHARACTER_IS_DIGIT(c))

#define COMP_CHARACTER_IS_VALID_IN_FILENAME(c)                                 \
  (COMP_CHARACTER_IS_ALPHANUMERIC(c) || (c == '_') || (c == '.'))

#define COMP_CHARACTER_IS_NOT_VALID_IN_FILENAME(c)                             \
  (!(COMP_CHARACTER_IS_VALID_IN_FILENAME(c)))

/* root level entries */
enum
{
  COMP_ROOT_ENTRY_SPRITES = 0, 
  COMP_ROOT_ENTRY_STARTUP_INI, 
  COMP_NUM_ROOT_ENTRIES 
};

static const char* S_comp_root_entry_names[COMP_NUM_ROOT_ENTRIES] = 
  { "Sprites", 
    "startup.ini" 
  };

/* paths */
#define COMP_PATH_MAX_SIZE 1024

static char S_comp_root_path_buf[COMP_PATH_MAX_SIZE];
static char S_comp_folder_path_buf[COMP_PATH_MAX_SIZE];
static char S_comp_subfolder_path_buf[COMP_PATH_MAX_SIZE];
static char S_comp_file_path_buf[COMP_PATH_MAX_SIZE];

/******************************************************************************/
/* comp_reset_parse_vars()                                                    */
/******************************************************************************/
int comp_reset_parse_vars()
{
  unsigned short k;

  for (k = 0; k < COMP_PATH_MAX_SIZE; k++)
    S_comp_root_path_buf[k] = '\0';

  for (k = 0; k < COMP_PATH_MAX_SIZE; k++)
    S_comp_folder_path_buf[k] = '\0';

  for (k = 0; k < COMP_PATH_MAX_SIZE; k++)
    S_comp_subfolder_path_buf[k] = '\0';

  for (k = 0; k < COMP_PATH_MAX_SIZE; k++)
    S_comp_file_path_buf[k] = '\0';

  return 0;
}

/******************************************************************************/
/* comp_parse_subfolder()                                                     */
/******************************************************************************/
int comp_parse_subfolder(unsigned short root_entry)
{
  DIR* dp;
  struct dirent* e;

  /* open the directory */
  dp = opendir(S_comp_subfolder_path_buf);

  if (dp == NULL)
    return 1;

  /* check all the files and folders in the directory */
  e = readdir(dp);

  while (e != NULL)
  {
    /* skip dot, dot-dot, and hidden files */
    if (e->d_name[0] == '.')
    {
      e = readdir(dp);
      continue;
    }

    /* assemble file path */
    strcpy(S_comp_file_path_buf, S_comp_subfolder_path_buf);
    strcat(S_comp_file_path_buf, e->d_name);

    printf("File Path: %s\n", S_comp_file_path_buf);

    /* load the file */
    if (root_entry == COMP_ROOT_ENTRY_SPRITES)
      art_load_gif(S_comp_file_path_buf);

    e = readdir(dp);
  }

  /* close the directory */
  closedir(dp);

  return 0;
}

/******************************************************************************/
/* comp_parse_folder()                                                        */
/******************************************************************************/
int comp_parse_folder(unsigned short root_entry)
{
  DIR* dp;
  struct dirent* e;

  /* open the directory */
  dp = opendir(S_comp_folder_path_buf);

  if (dp == NULL)
    return 1;

  /* check all the files and folders in the directory */
  e = readdir(dp);

  while (e != NULL)
  {
    /* skip dot, dot-dot, and hidden files */
    if (e->d_name[0] == '.')
    {
      e = readdir(dp);
      continue;
    }

    /* reset data buffers for new subfolder */
    if (root_entry == COMP_ROOT_ENTRY_SPRITES)
      art_clear_rom_data_vars();

    /* parse subfolder */
    strcpy(S_comp_subfolder_path_buf, S_comp_folder_path_buf);
    strcat(S_comp_subfolder_path_buf, e->d_name);
    strcat(S_comp_subfolder_path_buf, "/");

    printf("Subfolder Path: %s\n", S_comp_subfolder_path_buf);

    comp_parse_subfolder(root_entry);

    /* write subfolder files to the rom */
    if (root_entry == COMP_ROOT_ENTRY_SPRITES)
      art_add_chunks_to_rom();

    e = readdir(dp);
  }

  /* close the directory */
  closedir(dp);

  return 0;
}

/******************************************************************************/
/* comp_parse_root()                                                          */
/******************************************************************************/
int comp_parse_root()
{
  DIR* dp;
  struct dirent* e;

  unsigned short k;
  unsigned char  is_present[COMP_NUM_ROOT_ENTRIES];

  /* open the directory */
  dp = opendir(S_comp_root_path_buf);

  if (dp == NULL)
    return 1;

  /* initialize present flags */
  for (k = 0; k < COMP_NUM_ROOT_ENTRIES; k++)
    is_present[k] = 0;

  /* check all the files and folders in the directory */
  e = readdir(dp);

  while (e != NULL)
  {
    /* skip dot, dot-dot, and hidden files */
    if (e->d_name[0] == '.')
    {
      e = readdir(dp);
      continue;
    }

    /* determine if this file is one of the necessary entries */
    for (k = 0; k < COMP_NUM_ROOT_ENTRIES; k++)
    {
      if (!strcmp(e->d_name, S_comp_root_entry_names[k]))
        is_present[k] = 1;
    }

    printf("File: %s\n", e->d_name);

    e = readdir(dp);
  }

  /* make sure all necessary files and folders are present */
  for (k = 0; k < COMP_NUM_ROOT_ENTRIES; k++)
  {
    if (is_present[k] == 0)
      goto nope;
  }

  /* close the directory */
  closedir(dp);

  /* parse the folders! (and the startup ini) */
  for (k = 0; k < COMP_NUM_ROOT_ENTRIES; k++)
  {
    if (k == COMP_ROOT_ENTRY_STARTUP_INI)
    {
      strcpy(S_comp_folder_path_buf, S_comp_root_path_buf);
      strcat(S_comp_folder_path_buf, S_comp_root_entry_names[k]);
    }
    else
    {
      strcpy(S_comp_folder_path_buf, S_comp_root_path_buf);
      strcat(S_comp_folder_path_buf, S_comp_root_entry_names[k]);
      strcat(S_comp_folder_path_buf, "/");

      printf("Folder Path: %s\n", S_comp_folder_path_buf);

      comp_parse_folder(k);
    }
  }

  goto ok;

nope:
  closedir(dp);
  return 1;

ok:
  return 0;
}

/******************************************************************************/
/* comp_pack_rom()                                                            */
/******************************************************************************/
int comp_pack_rom(char* name)
{
  unsigned short k;
  unsigned char  null_flag;

  /* make sure name is valid */
  if (name == NULL)
    return 1;

  /* reset variables */
  comp_reset_parse_vars();

  /* add current directory to path */
  S_comp_root_path_buf[0] = '\0';
  strcat(S_comp_root_path_buf, "./");

  /* make sure the root folder name is valid */
  if (!strcmp(name, ".") || !strcmp(name, ".."))
    return 1;

  null_flag = 0;

  for (k = 0; k < COMP_PATH_MAX_SIZE; k++)
  {
    if (name[k] == '\0')
    {
      null_flag = 1;
      break;
    }
    else if (COMP_CHARACTER_IS_NOT_VALID_IN_FILENAME(name[k]))
      return 1;
  }

  if (null_flag == 0)
    return 1;

  /* add root folder name onto path */
  if ((strlen(S_comp_root_path_buf) + strlen(name) + 1) > COMP_PATH_MAX_SIZE)
    return 1;

  strcat(S_comp_root_path_buf, name);
  strcat(S_comp_root_path_buf, "/");

  /* print current path (testing) */
  printf("Root Path: %s\n", S_comp_root_path_buf);

  /* parse the root folder */
  comp_parse_root();

  return 0;
}

