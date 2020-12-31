/*
 * esc.h: a portable escape code parser
 *
 * Example usage:
 *
 *  static void my_escape_handler(char func, int args[ESC_MAX], int num, char *str);
 *
 *  #define ESC_EXEC my_escape_handler
 *  #include "esc.h"
 *
 *  esc_parse("5;15H"); // Will execute my_escape_handler(ESC_FUNC_CURSOR_POS, [5, 15], 2, "5;15H");
 */

#ifndef __ESC_H
#define __ESC_H

#include <string.h>
#include <stdlib.h>

#ifndef ESC_EXEC
#  error "The ESC_EXEC macro has not been defined (or has been defined after #including esc.h), please define it."
#endif

/* Arbitrary value useful for initializing a static char array */
#define ESC_MAX 256

/* Useful for determining if the end of an escape sequence has been reached */
#define ESC_IS_FUNCTION(c) ((c >= 'A' && c <= 'z') || c == '\x7f')
#define ESC_IS_ARG(c) (c >= '0' && c <= '9')

int esc_parse(char *str);
void esc_parse_gfx(char func, int args[ESC_MAX], int num, char *str);

enum esc_functions {
  /* Cursor functions */
  ESC_FUNC_CURSOR_POS
    = 'H',
  ESC_FUNC_CURSOR_POS_ALT
    = 'f',
  ESC_FUNC_CURSOR_UP
    = 'A',
  ESC_FUNC_CURSOR_DOWN
    = 'B',
  ESC_FUNC_CURSOR_RIGHT
    = 'C',
  ESC_FUNC_CURSOR_LEFT
    = 'D',
  ESC_FUNC_CURSOR_LINE_NEXT
    = 'E',
  ESC_FUNC_CURSOR_LINE_PREV
    = 'F',
  ESC_FUNC_CURSOR_COL
    = 'G',
  ESC_FUNC_CURSOR_REPORT
    = 'R',
  ESC_FUNC_CURSOR_SAVE
    = 's',
  ESC_FUNC_CURSOR_RESTORE
    = 'u',

  /* Erase functions */
  ESC_FUNC_ERASE_SCREEN
    = 'J',
  ESC_FUNC_ERASE_LINE
    = 'K',

  /* Graphics functions
   *  (both color and
   *  screen functions)
   */
  ESC_FUNC_GRAPHICS
    = 'm',
  ESC_FUNC_GRAPHICS_MODE
    = 'h',
  ESC_FUNC_GRAPHICS_MODE_RESET
    = 'l',

  /* Other functions */
  ESC_FUNC_DELETE
    = '\x7f'
};

enum esc_arguments {
  ESC_QUESTION = -20200905,
  ESC_EQUAL    = -20200906,

  ESC_GFX_NOCHANGE  = -20201229,
  ESC_GFX_RESET     = -20201230,

  ESC_GFX_BOLD      = 1,
  ESC_GFX_ITALIC    = 2,
  ESC_GFX_UNDERLINE = 4
};

enum esc_return_codes {
  ESC_SUCCESS
    = 0,
  ESC_FAIL_MISPLACED_QUESTION
    = 1,
  ESC_FAIL_MISPLACED_EQUAL
    = 2,
  ESC_FAIL_INT_CONV
    = 3
};

/*
 * Parse an escape code,
 * where str is an escape sequence
 * which does not include \e[
 */
int esc_parse(char *str){
  int str_len = strlen(str),
      str_ind = -1,
      arg_ind = 0,
      arg_no = 0,
      args[ESC_MAX];
  char function = str[str_len-1],
       arg_cur[ESC_MAX],
       *arg_err;

  /* Not strictly necessary, but safe */
  memset(arg_cur, 0, sizeof(arg_cur));

  while(++str_ind < str_len-1){
    switch(str[str_ind]){
      case ';':
        arg_cur[arg_ind] = '\0';
        args[arg_no++] = strtol(arg_cur, &arg_err, 10);
        arg_ind = 0;

        if(arg_cur == arg_err){
          return ESC_FAIL_INT_CONV;
        }
        break;
      case '?':
        if(arg_no == 0 && arg_ind == 0){
          args[arg_no++] = ESC_QUESTION;
        } else {
          return ESC_FAIL_MISPLACED_QUESTION;
        }
        break;
      case '=':
        if(arg_no == 0 && arg_ind == 0){
          args[arg_no++] = ESC_EQUAL;
        } else {
          return ESC_FAIL_MISPLACED_EQUAL;
        }
        break;
      default:
        arg_cur[arg_ind++] = str[str_ind];
        break;
    }
  }

  if(ESC_IS_ARG(arg_cur[0])){
    arg_cur[arg_ind] = '\0';
    args[arg_no++] = strtol(arg_cur, &arg_err, 10);
    if(arg_cur == arg_err){
      return ESC_FAIL_INT_CONV;
    }
  }

  if(function == ESC_FUNC_GRAPHICS){
    esc_parse_gfx(function, args, arg_no, str);
  } else {
    ESC_EXEC(function, args, arg_no, str);
  }

  return ESC_SUCCESS;
}

/*
 * Parse a graphics escape
 * sequence, called automatically
 * from esc_parse() to abstract
 * away graphics sequences and
 * return raw color values
 */
void esc_parse_gfx(char func, int args[ESC_MAX], int num, char *str){
  int out[3] = { ESC_GFX_NOCHANGE, ESC_GFX_NOCHANGE, 0 },
      accept_cmds = 1,
      affect_prop = 0,
      expect_args = 0,
      i;

  if(num == 0){
    out[0] = ESC_GFX_RESET;
    out[1] = ESC_GFX_RESET;
    out[2] = ESC_GFX_RESET;
  }

  for(i=0;i<num;i++){
    if(accept_cmds){
      /* TODO: Is this bad practice? If/else ladder looks just as ugly */
      switch(args[i]){
        case 0:
          out[0] = ESC_GFX_RESET;
          out[1] = ESC_GFX_RESET;
          out[2] = ESC_GFX_RESET;
          break;
        case 1:
          out[2] |= ESC_GFX_UNDERLINE;
          break;
        case 2:
          accept_cmds = 0;
          expect_args = 3;
          out[affect_prop] = 0;
          break;
        case 3:
          out[2] |= ESC_GFX_ITALIC;
          break;
        case 4:
          out[2] |= ESC_GFX_UNDERLINE;
          break;
        case 5:
          accept_cmds = 0;
          expect_args = -1;
          break;
        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
          out[0] = esc_palette_8[args[i]-30];
          break;
        case 38:
          affect_prop = 0;
          break;
        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
          out[1] = esc_palette_8[args[i]-40];
          break;
        case 48:
          affect_prop = 1;
          break;
        case 90:
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
        case 96:
        case 97:
          out[0] = esc_palette_8_bright[args[i]-90];
          break;
        case 100:
        case 101:
        case 102:
        case 103:
        case 104:
        case 105:
        case 106:
        case 107:
          out[1] = esc_palette_8_bright[args[i]-100];
          break;
      }
    } else {
      if(expect_args == -1){
        out[affect_prop] = esc_palette_256[args[i]];
        expect_args++;
      }

      if(expect_args > 0){
        out[affect_prop] |= args[i];
        if(expect_args > 1){
          out[affect_prop] <<= 8;
        }
        expect_args--;
      }

      if(expect_args == 0){
        accept_cmds = 1;
      }
    }
  }

  ESC_EXEC(func, out, 3, str);
}

#endif
