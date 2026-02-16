/******************************************************************************/
/* kunopack (KUNO-1 ROM Packer) - No Shinobi Knows Me 2025                    */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "art.h"
#include "rom.h"

/******************************************************************************/
/* main()                                                                     */
/******************************************************************************/
int main(int argc, char *argv[])
{
  rom_format();

  art_clear_file_vars();
  art_clear_image_vars();

  art_load_pbm("chun_li_standing_anim.iff", 3);
  art_add_files_to_rom();

  rom_save("test.kn1");

  return 0;
}

