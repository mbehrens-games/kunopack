/******************************************************************************/
/* comp.c (compile rom folder)                                                */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "comp.h"

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
int comp_parse_subfolder()
{
  return 0;
}

/******************************************************************************/
/* comp_parse_folder()                                                        */
/******************************************************************************/
int comp_parse_folder()
{
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
  unsigned long  present_flags;
  unsigned char  valid_flag;

  /* open the directory */
  dp = opendir(S_comp_root_path_buf);

  if (dp == NULL)
    return 1;

  /* initialize present flags */
  present_flags = 0;

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

    valid_flag = 0;

    for (k = 0; k < COMP_NUM_ROOT_ENTRIES; k++)
    {
      if (!strcmp(e->d_name, S_comp_root_entry_names[k]))
      {
        if (k == 0)
          present_flags |= 1;
        else
          present_flags |= (1 << k);

        valid_flag = 1;
      }
    }

    if (valid_flag == 0)
      goto nope;

    printf("File: %s\n", e->d_name);

    e = readdir(dp);
  }

  printf("Present Flags: %ld\n", present_flags);

  /* make sure all necessary files and folders are present */
  if (!(present_flags & 1))
    goto nope;

  for (k = 1; k < COMP_NUM_ROOT_ENTRIES; k++)
  {
    if (!(present_flags & (1 << k)))
      goto nope;
  }

  /* close the directory */
  closedir(dp);

  /* parse the folders! */
  for (k = 0; k < COMP_NUM_ROOT_ENTRIES; k++)
  {
    strcpy(S_comp_folder_path_buf, S_comp_root_path_buf);
    strcat(S_comp_folder_path_buf, S_comp_root_entry_names[k]);
    strcat(S_comp_folder_path_buf, "/");

    printf("Folder Path: %s\n", S_comp_folder_path_buf);
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

