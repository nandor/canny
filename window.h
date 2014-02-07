#ifndef __HOG_WINDOW_H__
#define __HOG_WINDOW_H__

#include <stdint.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

struct window
{
  Display * dpy;
  Window root;
  Window wnd;
  Atom wndClose;
  GLXContext ctx;
  int width;
  int height;
};

int initWindow(struct window *);
int destroyWindow(struct window *);
int updateWindow(struct window *);
void displayFrame(struct window *, uint8_t *);

#endif /*__HOG_WINDOW_H__*/