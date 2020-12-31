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
  0x000000, /* Black   */
  0x990000, /* Red     */
  0x009900, /* Green   */
  0x999900, /* Yellow  */
  0x000099, /* Blue    */
  0x990099, /* Magenta */
  0x009999, /* Cyan    */
  0xffffff  /* White/default (may not be necessary) */
};

const int esc_palette_8_bright[] = {
  0x000000, /* Black   */
  0xff0000, /* Red     */
  0x00ff00, /* Green   */
  0xffff00, /* Yellow  */
  0x0000ff, /* Blue    */
  0xff00ff, /* Magenta */
  0x00ffff, /* Cyan    */
  0xffffff  /* White/default (is necessary) */
};

/* TODO */
const int esc_palette_256[] = {
  0, 1, 2, 3
};

#endif
