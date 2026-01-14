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
  rom_clear();

  /* load test gif file */
  art_load_gif("dq3_witch.gif");

  /* write out rom file */
  rom_save("test.kn1");

  return 0;
}

