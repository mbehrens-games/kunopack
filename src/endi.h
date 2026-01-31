/******************************************************************************/
/* endi.h (big endian buffer macros)                                          */
/******************************************************************************/

#ifndef ENDI_H
#define ENDI_H

#define ENDI_WRITE_16BE(buf, addr, val)                                        \
  buf[(addr) + 0] = ((val) >> 8) & 0xFF;                                       \
  buf[(addr) + 1] = (val) & 0xFF;

#define ENDI_WRITE_24BE(buf, addr, val)                                        \
  buf[(addr) + 0] = ((val) >> 16) & 0xFF;                                      \
  buf[(addr) + 1] = ((val) >> 8) & 0xFF;                                       \
  buf[(addr) + 2] = (val) & 0xFF;

#define ENDI_READ_16BE(val, buf, addr)                                         \
  (val) = (buf[(addr) + 0] << 8) & 0xFF00;                                     \
  (val) |= buf[(addr) + 1] & 0x00FF;

#define ENDI_READ_24BE(val, buf, addr)                                         \
  (val) = (buf[(addr) + 0] << 16) & 0xFF0000;                                  \
  (val) |= (buf[(addr) + 1] << 8) & 0x00FF00;                                  \
  (val) |= buf[(addr) + 2] & 0x0000FF;

#endif

