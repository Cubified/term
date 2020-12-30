/*
 * test_esc.c: Test for esc.h
 */

#include <stdio.h>

static void esc_handler(char func, int args[5], int num, char *str);

#define ESC_EXEC esc_handler
#include "../esc.h"

void esc_handler(char func, int args[5], int num, char *str){
  printf("    Func: %c\n", func);
  for(int i=0;i<num;i++){
    printf("      Arg: %i\n", args[i]);
  }
}

int main(int argc, char **argv){
  printf("Testing escape sequence parser:\n");

  printf("  Test 1: Basic escape sequence\n");
  if(esc_parse("2;5H")) printf("    Test failed.\n");

  printf("  Test 2: Graphics sequence\n");
  if(esc_parse("32;8;128;255;0m")) printf("    Test failed.\n");

  return 0;
}
