/*
 * term.c: a tiny terminal emulator
 *
 * Specific TODO list:
 *  - Fix TODOs littered throughout
 *  - Add functioning scrollback
 *  - Fix backspace in bash
 *      when current line has
 *      more than one type
 *      of character
 *  - Fix left arrow key causing
 *      character deletion in
 *      bash
 *  - Fix miscellaneous errors
 *      causing various programs
 *      to break (vim, tmux, fish,
 *      etc.)
 *  - Use alternative rendering
 *      pipeline which reduces
 *      latency/redraw times
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <pty.h>
#include <locale.h>
#include <wchar.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

//////////////////////////////
// CONFIG FILE
//
#include "config.h"

//////////////////////////////
// PREPROCESSOR
//
#define UTF8_PUSH_BYTE(ind) wc|=buf[n+ind]&0xff;wc<<=8
#define UTF8_PUSH_BYTE_END(size) wc|=buf[n]&0xff;n+=size

#define TERM_CURRENT_X x
#define TERM_CURRENT_Y y

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
    = 3,
  X_FONT_SET
    = 4
};

enum term_config_opts {
  /* Cursor styles */
  TERM_CURSOR_NONE
    = 1,
  TERM_CURSOR_LINE
    = 2,
  TERM_CURSOR_BLOCK
    = 4
};

typedef __uint128_t uint128_t;

//////////////////////////////
// GLOBAL VARIABLES
//
Display *dpy;
Window win;
XFontSet fnt;
int run = 1,
    pty_m,
    pty_s,
    x = 0,
    y = 0,
    x_next = 0,
    y_next = 0,
    x_cur_prev = 0,
    y_cur_prev = 0,
    term_width = 100,
    term_height = 100,
    esc_ind = -2,
    cursor_style = CURSOR_STYLE,
    viewport = 0;
uint32_t fg = FG_DEFAULT,
         bg = BG_DEFAULT;
char mod = 0;
char esc_seq[256];
uint128_t *screen_buf;

//////////////////////////////
// STATIC DEFINITIONS
//
// TODO: More
//
static void term_esc(char func, int args[256], int num, char *str);
static void term_draw(int pos_x, int pos_y);
static void term_draw_cursor();
static void term_redraw_line();
static void term_redraw();
static void term_write(char *buf, int len);
static void term_putchar(wchar_t wc);
static void term_key(XKeyEvent key);
static void term_resize();
static void term_loop();
static void term_shutdown();

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
      x_next = x;
      y_next = y;
      break;
    case ESC_FUNC_CURSOR_UP:
      y = y_next;
      y_next -= (num > 0 ? args[0] : 1);
      break;
    case ESC_FUNC_CURSOR_DOWN:
      y = y_next;
      y_next += (num > 0 ? args[0] : 1);
      break;
    case ESC_FUNC_CURSOR_RIGHT:
      x = x_next;
      x_next += (num > 0 ? args[0] : 1);
      term_draw_cursor();
      break;
    case ESC_FUNC_CURSOR_LEFT:
      /* Important! term writes this escape
       *   sequence to the shell when left
       *   arrow is pressed, but most shells
       *   turn it into a backspace character
       *   (meaning the code below is almost
       *   never executed)
       */
      x = x_next;
      x_next -= (num > 0 ? args[0] : 1);
      break;
    case ESC_FUNC_CURSOR_LINE_NEXT:
      y = y_next;
      x = 0;
      y_next += (num > 0 ? args[0] : 1);
      break;
    case ESC_FUNC_CURSOR_LINE_PREV:
      y = y_next;
      x = 0;
      y_next -= (num > 0 ? args[0] : 1);
      break;
    case ESC_FUNC_CURSOR_COL:
      x = x_next;
      x_next = args[0];
      break;
    case ESC_FUNC_CURSOR_REPORT:
    case ESC_FUNC_CURSOR_REPORT_ALT:
      /* TODO */
      break;

    case ESC_FUNC_ERASE_SCREEN:
      if(num == 0){
        args[0] = 0;
      }
      switch(args[0]){
        case 0:
          memset(&screen_buf[(y*term_width)+x], 0, ((term_width*term_height)-((y*term_width)+x))*sizeof(uint128_t));
          break;
        case 1:
          memset(screen_buf, 0, ((y*term_width)+x+1)*sizeof(uint128_t));
          break;
        case 2:
          memset(screen_buf, 0, term_width*term_height*sizeof(uint128_t));
          break;
      }
      term_redraw();
      break;
    case ESC_FUNC_ERASE_LINE:
      if(num == 0){
        args[0] = 0;
      }
      switch(args[0]){
        case 0:
          memset(&screen_buf[(y*term_width)+x], 0, (term_width-x)*sizeof(uint128_t));
          break;
        case 1:
          memset(&screen_buf[y*term_width], 0, x*sizeof(uint128_t));
          break;
        case 2:
          memset(&screen_buf[y*term_width], 0, term_width*sizeof(uint128_t));
          break;
      }
      term_redraw_line(TERM_CURRENT_Y);
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
          cursor_style = 
            (func == ESC_FUNC_GRAPHICS_MODE ?
              (cursor_style & ~TERM_CURSOR_NONE) :
              (cursor_style | TERM_CURSOR_NONE)
            );
        } else if(args[1] == 2004){
          /* TODO: Bracketed paste here? Bash 5.1 spams this whereas 5.0 did not */
        }
      }
      break;

    case ESC_FUNC_DELETE:
      /* TODO */
      break;
  }

  if(x_next < 0) { x_next = 0; }
  if(x_next > term_width) { x_next = term_width; }
  if(y_next < 0) { y_next = 0; }
  if(y_next > term_height) { y_next = term_height; }

  printf("Escape sequence:\n String: %s\n Function: %i\n", str, func);
  for(i=0;i<num;i++){
    printf(" Args[%i]: %i\n", i, args[i]);
  }
  }

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
    case X_FONT_SET:
      fprintf(stderr, "Error: Failed to create X font set.\n");
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
  char **missing_list,
       *def_string;
  int missing_count;

  log_info(TERM_LOG_STARTUP);

  /* Screen buffer */
  screen_buf = calloc(term_width*term_height*SCROLLBACK_SIZE, sizeof(uint128_t));

  /* Locale */
  setlocale(LC_ALL, "");

  /* X */
  dpy = XOpenDisplay(NULL);
  if(dpy == NULL){
    log_error(TERM_ERR_DISPLAY);
  }

  attrs.background_pixel = BG_DEFAULT;
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
    term_width, term_height,
    0,
    DefaultDepth(dpy, DefaultScreen(dpy)),
    InputOutput,
    DefaultVisual(dpy, DefaultScreen(dpy)),
    CWBackPixel|CWEventMask,
    &attrs
  );
  XMapWindow(dpy, win);
  XFlush(dpy);

  fnt = XCreateFontSet(
    dpy,
    FONT_STRING,
    &missing_list,
    &missing_count,
    &def_string
  );
  if(fnt == NULL){
    log_error(X_FONT_SET);
  }
  XFreeStringList(missing_list);

  /* pty */
  if(openpty(&pty_m, &pty_s, NULL, NULL, NULL)
      != 0){
    log_error(TERM_ERR_PTY);
  }

  ws.ws_col = term_width;
  ws.ws_row = term_height;
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

void term_draw(int pos_x, int pos_y){
  uint128_t cell;
  wchar_t c;

  cell = screen_buf[(pos_y*term_width)+pos_x];
  c = cell & 0xffffffff;

  if(c != 0){
    XSetForeground(
      dpy,
      DefaultGC(dpy, DefaultScreen(dpy)),
      (cell >> 56) & 0xffffff
    );
    XFillRectangle(
      dpy,
      win,
      DefaultGC(dpy, DefaultScreen(dpy)),
      (pos_x*CHAR_W)+LEFTMOST, (pos_y-viewport)*CHAR_H,
      CHAR_W, CHAR_H
    );
    XSetForeground(
      dpy,
      DefaultGC(dpy, DefaultScreen(dpy)),
      (cell >> 32) & 0xffffff
    );
    XmbDrawString(
      dpy,
      win,
      fnt,
      DefaultGC(dpy, DefaultScreen(dpy)),
      (pos_x*CHAR_W)+LEFTMOST, ((pos_y-viewport)*CHAR_H)+TOPMOST,
      (char*)&c,
      (c <= 0xff ? 1 : (c <= 0xffff ? 2 : (c <= 0xffffff ? 3 : 4)))
    );
  }

  XFlush(dpy);
}

void term_draw_cursor(){
  if(cursor_style & TERM_CURSOR_NONE == 1){ return; }
 
  if(x_next > 0){ /* TODO: Hack for lingering cursor when printing empty line */
    term_draw(x_cur_prev, y_cur_prev);

    switch(cursor_style & ~TERM_CURSOR_NONE){
      case TERM_CURSOR_BLOCK:
        XSetForeground(
          dpy,
          DefaultGC(dpy, DefaultScreen(dpy)),
          fg
        );
        XFillRectangle(
          dpy,
          win,
          DefaultGC(dpy, DefaultScreen(dpy)),
          (x_next*CHAR_W)+LEFTMOST, y_next*CHAR_H,
          CHAR_W, CHAR_H
        );
        break;
      case TERM_CURSOR_LINE:
        XSetForeground(
          dpy,
          DefaultGC(dpy, DefaultScreen(dpy)),
          fg
        );
        XFillRectangle(
          dpy,
          win,
          DefaultGC(dpy, DefaultScreen(dpy)),
          (x_next*CHAR_W)+LEFTMOST, y_next*CHAR_H,
          2, CHAR_H
        );
        break;
    }

    x_cur_prev = x_next;
    y_cur_prev = y_next;

    XFlush(dpy);
  }
}

void term_redraw_line(int line){
  int x_i;

  XClearArea(
    dpy,
    win,
    0, ((line-viewport)*CHAR_H),
    term_width*CHAR_W, CHAR_H,
    False
  );

  for(x_i=0;x_i<term_width;x_i++){
    term_draw(x_i, line);
  }

  term_draw_cursor();
}

void term_redraw(){
  int y_i;

  XClearWindow(
    dpy,
    win
  );

  for(y_i=viewport;y_i<term_height+viewport;y_i++){
    term_redraw_line(y_i);
  }
}

void term_resize(){
  struct winsize ws;

  screen_buf = realloc(screen_buf, term_width*term_height*SCROLLBACK_SIZE*sizeof(uint128_t));

  ws.ws_col = term_width;
  ws.ws_row = term_height;
  ioctl(pty_m, TIOCSWINSZ, &ws);

  term_redraw();
}

void term_key(XKeyEvent key){
  char buf[32];
  int num;
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
      write(pty_m, buf, num);
      break;
    }
}

void term_putchar(wchar_t wc){
  int redraw = 1;

  switch(wc){
    case '\a':
      printf("Bell\n");
      redraw = 0;
      break;
    case '\x1b':
      esc_ind = -1;
      redraw = 0;
      break;
    case '\b':
      x_next--;
      if(x_next < 0){
        x_next = term_width-1;
        y_next--;
      }
      screen_buf[(y_next*term_width)+x_next] = 0;
      term_redraw_line(TERM_CURRENT_Y);
      break;
    case '\r':
      x_next = 0;
      redraw = 0;
      break;
    case '\n':
      y_next++;
/*      if(y_next > term_height){
        viewport++;
        term_redraw();
      } else {
*/        term_redraw_line(TERM_CURRENT_Y);
//      }
      redraw = 0;
      break;
    case '\t':
      x_next += TABWIDTH - (x_next % TABWIDTH);
      break;
    default:
      if(esc_ind >= 0){
        esc_seq[esc_ind++] = wc;
        if(ESC_IS_FUNCTION(wc)){
          esc_seq[esc_ind] = '\0';
          if(esc_parse(esc_seq) != ESC_SUCCESS){
            log_warn(TERM_WARN_ESC, esc_seq);
          }
          esc_ind = -2;
        }
        redraw = 0;
      } else if(esc_ind == -2){
        x = x_next;
        y = y_next;

        /* This could be optimized by compressing it into a one-liner
         *   (which was the case initially), but this would require
         *   changing mod, fg, and bg to uint128_t, using more memory
         *   than necessary.  Also, this is (slightly) clearer than
         *   the one-liner at first glance.
         */
        screen_buf[((y*term_width)+x)] = mod;
        screen_buf[((y*term_width)+x)] <<= 24;
        screen_buf[((y*term_width)+x)] |= bg;
        screen_buf[((y*term_width)+x)] <<= 24;
        screen_buf[((y*term_width)+x)] |= fg;
        screen_buf[((y*term_width)+x)] <<= 32;
        screen_buf[((y*term_width)+x)] |= wc;

        x_next++;
        if(x_next >= term_width){
          x_next = 0;
          y_next++;
        }
      } else {
        esc_ind++;
        redraw = 0;
      }
      break;
  }

  if(redraw){
    term_draw(TERM_CURRENT_X, TERM_CURRENT_Y);
    term_draw_cursor();
  }
}

void term_write(char *buf, int len){
  int n = 0;
  wchar_t wc;

  for(n=0;n<len;){
    wc = 0;
    if((buf[n] & 0xf0) == 0xf0){        /* 4-byte sequence */
      UTF8_PUSH_BYTE(3);
      UTF8_PUSH_BYTE(2);
      UTF8_PUSH_BYTE(1);
      UTF8_PUSH_BYTE_END(4);
    } else if((buf[n] & 0xe0) == 0xe0){ /* 3-byte sequence */
      UTF8_PUSH_BYTE(2);
      UTF8_PUSH_BYTE(1);
      UTF8_PUSH_BYTE_END(3);
    } else if((buf[n] & 0xc0) == 0xc0){ /* 2-byte sequence */
      UTF8_PUSH_BYTE(1);
      UTF8_PUSH_BYTE_END(2);
    } else {                            /* 1-byte sequence */
      UTF8_PUSH_BYTE_END(1);
    }

    term_putchar(wc);
  }
}

void term_loop(){
  XEvent evt;
  fd_set set;
  int maxfd,
      pty_len;
  char pty_buf[ESC_MAX];

  maxfd = (pty_m > ConnectionNumber(dpy) ? pty_m : ConnectionNumber(dpy));

  while(run){
    FD_ZERO(&set);
    FD_SET(pty_m, &set);
    FD_SET(ConnectionNumber(dpy), &set);

    select(maxfd+1, &set, NULL, NULL, NULL);

    if(FD_ISSET(pty_m, &set)){
      if((pty_len=read(pty_m, pty_buf, ESC_MAX)) <= 0) { return; }

      term_write(pty_buf, pty_len);

      memset(pty_buf, 0, pty_len);
    }

    if(FD_ISSET(ConnectionNumber(dpy), &set)){
      while(XPending(dpy)){
        XNextEvent(dpy, &evt);
        switch(evt.type){
          case ButtonPress:
            break;
          case KeyPress:
            term_key(evt.xkey);
            break;
          case ConfigureNotify:
            term_width = (evt.xconfigure.width / CHAR_W);
            term_height = (evt.xconfigure.height / CHAR_H) - 1; /* TODO: More robust solution using TOPMOST and CHAR_H */
            term_resize();
            break;
        }
      }
    }
  }
}

void term_shutdown(){
  free(screen_buf);

  log_info(TERM_LOG_SHUTDOWN);

  XFreeFontSet(dpy, fnt);
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
