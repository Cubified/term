/*
 * term.c: a tiny terminal emulator
 *
 * Specific TODO list:
 *  - Fix character duplication on mid-word
 *      backspace
 *  - Fix cursor rendering
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
    y = 0,
    x_prev = 0,
    y_prev = 0;
uint32_t fg = FG_DEFAULT,
         bg = BG_DEFAULT;
char mod = 0;
uint64_t *screen_buf;

//////////////////////////////
// STATIC DEFINITIONS
//
// TODO: More
//
static void term_esc(char func, int args[256], int num, char *str);
static void term_draw();
static void term_redraw();

//////////////////////////////
// ESCAPE CODE PARSING
//
#define ESC_EXEC term_esc
#include "esc.h"

void term_esc(char func, int args[ESC_MAX], int num, char *str){
  int i;

  switch(func){
    case ESC_FUNC_CURSOR_POS:
    case ESC_FUNC_CURSOR_POS_ALT:
      y = (num >= 1 ? args[0] : 0);
      x = (num >= 2 ? args[1] : 0);
      break;
    case ESC_FUNC_CURSOR_UP:
      y_prev = y;
      y -= (num > 0 ? args[0] : 1);
      break;
    case ESC_FUNC_CURSOR_DOWN:
      y_prev = y;
      y += (num > 0 ? args[0] : 1);
      break;
    case ESC_FUNC_CURSOR_RIGHT:
      x_prev = x;
      x += (num > 0 ? args[0] : 1);
      break;
    case ESC_FUNC_CURSOR_LEFT:
      x_prev = x;
      x -= (num > 0 ? args[0] : 1);
      break;
    case ESC_FUNC_CURSOR_LINE_NEXT:
      y_prev = y;
      x = 0;
      y += (num > 0 ? args[0] : 1);
      break;
    case ESC_FUNC_CURSOR_LINE_PREV:
      y_prev = y;
      x = 0;
      y -= (num > 0 ? args[0] : 1);
      break;
    case ESC_FUNC_CURSOR_COL:
      x_prev = x;
      x = args[0];
      break;

    case ESC_FUNC_ERASE_SCREEN:
      x_prev = 0;
      y_prev = 0;
      XClearWindow(dpy, win);
      break;
    case ESC_FUNC_ERASE_LINE:
      XClearArea(
        dpy,
        win,
        (x*CHAR_W)+LEFTMOST, y*CHAR_H,
        WIDTH*CHAR_W, CHAR_H,
        False
      );
      break;

    case ESC_FUNC_GRAPHICS:
      switch(args[0]){
        case ESC_GFX_NOCHANGE:
          break;
        case ESC_GFX_RESET:
          fg = FG_DEFAULT;
          break;
        default:
          fg = args[0];
          break;
      }
      switch(args[1]){
        case ESC_GFX_NOCHANGE:
          break;
        case ESC_GFX_RESET:
          bg = BG_DEFAULT;
          break;
        default:
          bg = args[1];
          break;
      }
      switch(args[2]){
        case ESC_GFX_RESET:
          mod = 0;
          break;
        default:
          mod |= args[2];
          break;
      }
      break;
    case ESC_FUNC_GRAPHICS_MODE:
    case ESC_FUNC_GRAPHICS_MODE_RESET:
      if(args[0] == ESC_QUESTION){
        if(args[1] == 25){
          /* Show/hide cursor here */
        }
      }
      break;

    case ESC_FUNC_DELETE:
      /* TODO */
      break;
  }

  if(x < 0) { x = 0; }
  if(x > WIDTH) { x = WIDTH; }
  if(y < 0) { y = 0; }
  if(y > HEIGHT) { y = HEIGHT; }

  printf("Escape sequence:\n String: %s\n Function: %i\n", str, func);
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
void term_init(){
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
      StructureNotifyMask |
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
  uint64_t cell;
  char c;

  /* Print last character read from pty */
  cell = screen_buf[(y_prev*WIDTH)+x_prev];
  c = cell & 0xff; /* TODO: wchar/unicode support */

  XClearArea(
    dpy,
    win,
    (x_prev*CHAR_W)+LEFTMOST, y_prev*CHAR_H,
    2, CHAR_H,
    False
  );

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
      (x_prev*CHAR_W)+LEFTMOST, y_prev*CHAR_H,
      CHAR_W, CHAR_H
    );
    XSetForeground(
      dpy,
      DefaultGC(dpy, DefaultScreen(dpy)),
      (cell >> 14) & 0xffffff
    );
    XDrawString(
      dpy,
      win,
      DefaultGC(dpy, DefaultScreen(dpy)),
      (x_prev*CHAR_W)+LEFTMOST, (y_prev*CHAR_H)+TOPMOST,
      &c,
      1
    );
  }

  /* Print cursor, changing character's color if necessary */
  cell = screen_buf[(y*WIDTH)+x];
  c = cell & 0xff; /* TODO: wchar/unicode support */
  if(c == 0) { c = ' '; }

  /* TODO: This is still broken */

  XSetForeground(
    dpy,
    DefaultGC(dpy, DefaultScreen(dpy)),
    fg
  );
  XFillRectangle(
    dpy,
    win,
    DefaultGC(dpy, DefaultScreen(dpy)),
    (x*CHAR_W)+LEFTMOST, y*CHAR_H,
    2, CHAR_H
  );

  /*
  XSetForeground(
    dpy,
    DefaultGC(dpy, DefaultScreen(dpy)),
    fg
  );
  XFillRectangle(
    dpy,
    win,
    DefaultGC(dpy, DefaultScreen(dpy)),
    (x*CHAR_W)+LEFTMOST, y*CHAR_H,
    CHAR_W, CHAR_H
  );
  XSetForeground(
    dpy,
    DefaultGC(dpy, DefaultScreen(dpy)),
    bg
  );
  XDrawString(
    dpy,
    win,
    DefaultGC(dpy, DefaultScreen(dpy)),
    (x*CHAR_W)+LEFTMOST, (y*CHAR_H)+TOPMOST,
    &c,
    1
  );
  */

  XFlush(dpy);
  return;
}

void term_redraw(){
  int x_i, y_i,
      x_o = x_prev,
      y_o = y_prev;

  for(y_i=0;y_i<HEIGHT;y_i++){
    for(x_i=0;x_i<WIDTH;x_i++){
      x_prev = x_i;
      y_prev = y_i;
      term_draw();
    }
  }

  x_prev = x_o;
  y_prev = y_o;
}

void term_key(XKeyEvent key){
  char buf[32];
  int i, num;
  KeySym ksym;

  num = XLookupString(&key, buf, sizeof(buf), &ksym, 0);
  switch(ksym){
    case XK_Left:
      write(pty_m, "\x1b[D", 3);
      break;
    case XK_Right:
      write(pty_m, "\x1b[C", 3);
      break;
    case XK_Up:
      write(pty_m, "\x1b[A", 3);
      break;
    case XK_Down:
      write(pty_m, "\x1b[B", 3);
      break;
    default:
      for(i = 0; i < num; i++){ write(pty_m, &buf[i], 1); }
      break;
    }
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
          printf("Bell\n");
          break;
        case '\x1b':
          esc_ind = -1;
          break;
        case '\b':
          /* TODO: This fix for wrapping is broken */
          x--;
          if(x < 0){
            x = WIDTH-1;
            y--;
          }
          screen_buf[(y*WIDTH)+x] = 0;
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
            
            x_prev = x;
            y_prev = y;

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
          case ConfigureNotify:
            term_redraw();
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
