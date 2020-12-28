/*
 * term.c: a tiny terminal emulator
 *
 * TODO:
 *  - Store plain text and escape sequences
 *      in some way which is both easily-
 *      differentiable as well as easy to
 *      modify one using the other
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <pty.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

//////////////////////////////
// CONFIG FILE
//
#include "config.h"

//////////////////////////////
// GLOBAL VARIABLES
//
Display *dpy;
Window win;
int run = 1,
    pty_m,
    pty_s,
    x = 0,
    y = 0;
uint32_t fg = 0xffffff,
         bg = 0x000000;
char mod = 0;
uint64_t *screen_buf;

//////////////////////////////
// STATIC DEFINITIONS
//
// TODO: More
//
static void term_esc(char func, int args[2], int num);
static void term_draw();

//////////////////////////////
// ESCAPE CODE PARSING
//
#define ESC_EXEC term_esc
#include "esc.h"

void term_esc(char func, int args[3], int num){
  int i;

  switch(func){
    case ESC_FUNC_GRAPHICS:
      fg = args[0];
      bg = args[1];
      break;
    case ESC_FUNC_CURSOR_POS:
    case ESC_FUNC_CURSOR_POS_ALT:
      x = args[0];
      y = args[1];
      break;
    case ESC_FUNC_CURSOR_UP:
      y -= args[0];
      break;
    case ESC_FUNC_CURSOR_DOWN:
      y += args[0];
      break;
    case ESC_FUNC_CURSOR_RIGHT:
      x += args[0];
      break;
    case ESC_FUNC_CURSOR_LEFT:
      x -= args[0];
      break;
    case ESC_FUNC_CURSOR_LINE_NEXT:
      x = 0;
      y += args[0];
      break;
    case ESC_FUNC_CURSOR_LINE_PREV:
      x = 0;
      y -= args[0];
      break;
    case ESC_FUNC_CURSOR_COL:
      x = args[0];
      break;
  }

  printf("Escape sequence:\n Function: %i\n", func);
  for(i=0;i<num;i++){
    printf(" Args[%i]: %i\n", i, args[i]);
  }
}

//////////////////////////////
// ENUMS AND TYPEDEFS
//
enum term_status_codes {
  /* Succes/log codes */
  TERM_SUCCESS
    = 0,
  TERM_LOG_STARTUP
    = -1,
  TERM_LOG_SHUTDOWN
    = -2,

  /* Warning codes */
  TERM_WARN_ESC
    = -100,

  /* Error codes */
  TERM_ERR_DISPLAY
    = 1,
  TERM_ERR_PTY
    = 2,
  TERM_ERR_TTY
    = 3
};

//////////////////////////////
// LOG FUNCTIONS
//
// TODO: varargs
//
void log_info(int status){
  switch(status){
    case TERM_LOG_STARTUP:
      printf("term starting up.\n");
      break;
    case TERM_LOG_SHUTDOWN:
      printf("term shutting down.\n");
      break;
  }
}

void log_warn(int status, char *str){
  switch(status){
    case TERM_WARN_ESC:
      printf("Warning: Unknown escape sequence \"%s\".\n", str);
      break;
  }
}

void log_error(int status){
  switch(status){
    case TERM_ERR_DISPLAY:
      fprintf(stderr, "Error: Failed to open display.\n");
      break;
    case TERM_ERR_PTY:
      fprintf(stderr, "Error: Failed to open pseudoterminal.\n");
      break;
    case TERM_ERR_TTY:
      fprintf(stderr, "Error: Failed to attach shell to TTY.\n");
      break;
  }
  exit(status);
}

//////////////////////////////
// TERM CORE
//
int term_init(){
  XSetWindowAttributes attrs;
  struct winsize ws;

  log_info(TERM_LOG_STARTUP);

  /* Screen buffer */
  screen_buf = calloc(WIDTH*HEIGHT, sizeof(uint64_t));

  /* X */
  dpy = XOpenDisplay(NULL);
  if(dpy == NULL){
    log_error(TERM_ERR_DISPLAY);
  }

  attrs.background_pixel = BlackPixel(dpy, DefaultScreen(dpy));
  attrs.event_mask
    = SubstructureNotifyMask |
      ExposureMask |
      KeyPressMask |
      ButtonPressMask;

  win = XCreateWindow(
    dpy,
    DefaultRootWindow(dpy),
    0, 0,
    WIDTH, HEIGHT,
    0,
    DefaultDepth(dpy, DefaultScreen(dpy)),
    InputOutput,
    DefaultVisual(dpy, DefaultScreen(dpy)),
    CWBackPixel|CWEventMask,
    &attrs
  );
  XMapWindow(dpy, win);
  XFlush(dpy);

  /* pty */
  if(openpty(&pty_m, &pty_s, NULL, NULL, NULL)
      != 0){
    log_error(TERM_ERR_PTY);
  }

  ws.ws_col = WIDTH;
  ws.ws_row = HEIGHT;
  ioctl(pty_m, TIOCSWINSZ, &ws);

  if(fork() == 0){
    close(pty_m);
    setsid();
    if(ioctl(pty_s, TIOCSCTTY, NULL) == -1){
      log_error(TERM_ERR_TTY);
    }

    dup2(pty_s, STDIN_FILENO);
    dup2(pty_s, STDOUT_FILENO);
    dup2(pty_s, STDERR_FILENO);
    close(pty_s);

    execvp(SHELL, NULL);
  } else {
    close(pty_s);
  }
}

void term_draw(){
  int x_i,
      y_i;
  uint64_t cell;
  char c;
  uint64_t fore;

  XClearWindow(dpy, win);

  for(y_i=0;y_i<HEIGHT;y_i++){
    for(x_i=0;x_i<WIDTH;x_i++){
      cell = screen_buf[(y_i*WIDTH)+x_i];
      c = cell & 0xff;

      if(c != 0){
        XSetForeground(
          dpy,
          DefaultGC(dpy, DefaultScreen(dpy)),
          (cell >> 38) & 0xffffff
        );
        XFillRectangle(
          dpy,
          win,
          DefaultGC(dpy, DefaultScreen(dpy)),
          (x_i*CHAR_W)+LEFTMOST, y_i*CHAR_H,
          CHAR_W, CHAR_H
        );
        fore = cell >> 14;
        XSetForeground(
          dpy,
          DefaultGC(dpy, DefaultScreen(dpy)),
          (cell >> 14) & 0xffffff
        );
        XDrawString(
          dpy,
          win,
          DefaultGC(dpy, DefaultScreen(dpy)),
          (x_i*CHAR_W)+LEFTMOST, (y_i*CHAR_H)+TOPMOST,
          &c,
          1
        );
      }
    }
  }

  XSetForeground(
    dpy,
    DefaultGC(dpy, DefaultScreen(dpy)),
    0xffffffff
  );
  XFillRectangle(
    dpy,
    win,
    DefaultGC(dpy, DefaultScreen(dpy)),
    (x*CHAR_W)+LEFTMOST, y*CHAR_H,
    CHAR_W, CHAR_H
  );

  XFlush(dpy);
  return;
}

void term_key(XKeyEvent key){
  char buf[32];
  int i, num;
  KeySym ksym;

  num = XLookupString(&key, buf, sizeof(buf), &ksym, 0);
  for(i = 0; i < num; i++){ write(pty_m, &buf[i], 1); }
}

void term_loop(){
  XEvent evt;
  fd_set set;
  int maxfd,
      esc_ind = -2;
  char pty_c;
  char esc_seq[ESC_MAX];

  maxfd = (pty_m > ConnectionNumber(dpy) ? pty_m : ConnectionNumber(dpy));

  while(run){
    FD_ZERO(&set);
    FD_SET(pty_m, &set);
    FD_SET(ConnectionNumber(dpy), &set);

    select(maxfd+1, &set, NULL, NULL, NULL);

    if(FD_ISSET(pty_m, &set)){
      if(read(pty_m, &pty_c, 1) <= 0){ return; }

      switch(pty_c){
        case '\a':
          /* TODO */
          break;
        case '\x1b':
          esc_ind = -1;
          break;
        case '\b':
          /* TODO */
          break;
        case '\r':
          x = 0;
          break;
        case '\n':
          y++;
          break;
        default:
          if(esc_ind >= 0){
            esc_seq[esc_ind++] = pty_c;
            if(ESC_IS_FUNCTION(pty_c)){
              esc_seq[esc_ind] = '\0';
              if(esc_parse(esc_seq) != ESC_SUCCESS){
                log_warn(TERM_WARN_ESC, esc_seq);
              }
              esc_ind = -2;
            }
          } else if(esc_ind == -2){
            /* This could be optimized by compressing it into a one-liner
             *   (which was the case initially), but this would require
             *   changing mod, fg, and bg to uint64_t, using more memory
             *   than necessary.  Also, this is (slightly) clearer than
             *   the one-liner at first glance.
             */
            screen_buf[((y*WIDTH)+x)] = mod;
            screen_buf[((y*WIDTH)+x)] <<= 24;
            screen_buf[((y*WIDTH)+x)] |= bg;
            screen_buf[((y*WIDTH)+x)] <<= 24;
            screen_buf[((y*WIDTH)+x)] |= fg;
            screen_buf[((y*WIDTH)+x)] <<= 14;
            screen_buf[((y*WIDTH)+x)] |= pty_c;
            x++;
            if(x >= WIDTH){
              x = 0;
              y++;
            }
          } else {
            esc_ind++;
          }
          break;
      }

      term_draw();
    }

    if(FD_ISSET(ConnectionNumber(dpy), &set)){
      while(XPending(dpy)){
        XNextEvent(dpy, &evt);
        switch(evt.type){
          case Expose:
            term_draw();
            break;
          case ButtonPress:
            break;
          case KeyPress:
            term_key(evt.xkey);
            break;
        }
      }
    }
  }
}

void term_shutdown(){
  free(screen_buf);

  log_info(TERM_LOG_SHUTDOWN);

  XUnmapWindow(dpy, win);
  XCloseDisplay(dpy);
}

//////////////////////////////
// MAIN
//
int main(){
  term_init();
  term_loop();
  term_shutdown();
  return 0;
}
