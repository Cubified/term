## term

A tiny terminal emulator.  Currently a work in progress, but reasonably functional.

### Demo

(With heavy 256-color GIF compression...)

![Demo](https://github.com/Cubified/term/blob/main/demo.gif)

### Current Features
- Implements a common subset of a VT-100 terminal's escape sequences (including truecolor graphics)
- Supports Unicode/UTF-8 character sets
- Depends upon standalone Xlib only

### Compiling, Running, and Configuring

Assuming Xlib is installed, `term` can be compiled with:

     $ make

And run with:

     $ ./term

There are several configuration options in `config.h` which affect the appearance and functioning of `term`, including fonts and color palettes.  To apply these changes, recompile `term`.

### To-Do
- More complete escape sequence support
- Rendering optimizations/alternative rendering engine
- Scrollback
