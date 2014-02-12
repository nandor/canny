#ifndef __HOG_WINDOW_H__
#define __HOG_WINDOW_H__

#include <stdint.h>
#include <X11/Xlib.h>
#include <GL/glew.h>
#include <GL/glxew.h>

struct process;

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
int updateWindow(struct window *);
void destroyWindow(struct window *);
void displayImage(struct window *, struct process *);

#endif /*__HOG_WINDOW_H__*/