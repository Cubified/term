/*
 * esc.h: a portable escape code parser
 *
 * Example usage:
 *
 *  static void my_escape_handler(char func, int args[5], int num);
 *
 *  #define ESC_EXEC my_escape_handler
 *  #include "esc.h"
 *
 *  esc_parse("5;15H"); // Will execute my_escape_handler(ESC_FUNC_CURSOR_POS, [5, 15], 2);
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
  ESC_EQUAL    = -20200906
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
      args[5];
  char function = str[str_len-1],
       arg_cur[256],
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

  ESC_EXEC(function, args, arg_no, str);

  return ESC_SUCCESS;
}

#endif
