/*
 * truecolor_stresstest.c: Test terminal's truecolor printing ability
 *
 * Should produce a clean green-magenta gradient, with black in the top
 *   left and white in the bottom right
 */

#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <math.h>

int main(){
  int x, y;
  struct winsize ws;

  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

  printf("\x1b[?25l\n");

  for(y=0;y<ws.ws_row;y++){
    for(x=0;x<ws.ws_col;x++){
      printf("\x1b[48;2;%i;%i;%im ", (int)floor(((double)y/(double)ws.ws_row)*255.0f), (int)floor(((double)x/(double)ws.ws_col)*255.0f), (int)floor(((double)y/(double)ws.ws_row)*255.0f));
    }
  }

  printf("\x1b[0m\x1b[?25h\x1b[0;0H\n");

  return 0;
}
