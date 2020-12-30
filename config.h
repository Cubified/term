/*
 * config.h: term configuration
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define SHELL "/bin/bash"

#define WIDTH  71
#define HEIGHT 307

#define CHAR_W   6
#define CHAR_H   12
#define TOPMOST  10
#define LEFTMOST 2

#define FG_DEFAULT 0xffffff
#define BG_DEFAULT 0x000000

/* TODO */
const int esc_palette_8[] = {
  0,
  1,
  2,
  3,
  4,
  5,
  6,
  7
};

const int esc_palette_8_bright[] = {
  0,
  1,
  2,
  3,
  4,
  5,
  6,
  7
};

const int esc_palette_256[] = {
  0, 1, 2, 3
};

#endif
