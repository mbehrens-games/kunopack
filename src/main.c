/******************************************************************************/
/* kunopack (KUNO-1 ROM Packer) - No Shinobi Knows Me 2025-2026               */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "art.h"
#include "con.h"
#include "rom.h"

/******************************************************************************/
/* main()                                                                     */
/******************************************************************************/
int main(int argc, char *argv[])
{
  rom_format();

  /* parse con file */
  con_load_file("test_spriteset.con");

#if 0
  /* create test sprite set */
  art_clear_rom_data_vars();

  art_load_gif("mai_standing.gif");

  art_add_files_to_rom();
#endif

  /* save the rom! */
  rom_save("test.kn1");

  return 0;
}

