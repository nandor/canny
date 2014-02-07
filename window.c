#include <stdio.h>
#include "window.h"

int
initWindow(struct window *wnd)
{
  XEvent evt;
  XVisualInfo *vi;
  XSetWindowAttributes swa;
  Colormap cmap;
  Window root;

  static int attr[] =
  {
    GLX_RGBA,
    GLX_RED_SIZE,     8,
    GLX_GREEN_SIZE,   8,
    GLX_BLUE_SIZE,    8,
    GLX_DOUBLEBUFFER,
    None
  };

  /* Open the X display */
  if (!(wnd->dpy = XOpenDisplay(0)) || !(root = DefaultRootWindow(wnd->dpy)))
  {
    return 0;
  }

  /* Retrieve visual info */
  if (!(vi = glXChooseVisual(wnd->dpy, 0, attr)))
  {
    return 0;
  }

  if (!(cmap = XCreateColormap(wnd->dpy, root, vi->visual, AllocNone)))
  {
    XFree(vi);
    return 0;
  }

  /* Create the window */
  swa.colormap = cmap;
  swa.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
  if (!(wnd->wnd = XCreateWindow(wnd->dpy, root, 0, 0, wnd->width, wnd->height,
                                 0, vi->depth, InputOutput, vi->visual,
                                 CWColormap | CWEventMask, &swa)))
  {
    XFree(vi);
    return 0;
  }

  /* Catch window close events */
  if (!(wnd->wndClose = XInternAtom(wnd->dpy, "WM_DELETE_WINDOW", False)))
  {
    XFree(vi);
    return 0;
  }

  XSetWMProtocols(wnd->dpy, wnd->wnd, &wnd->wndClose, 1);

  /* Map the window */
  XMapWindow(wnd->dpy, wnd->wnd);
  XSync(wnd->dpy, False);
  do {
    XNextEvent(wnd->dpy, &evt);
  } while (evt.type != MapNotify);

  /* Create the OpenGL context */
  if (!(wnd->ctx = glXCreateContext(wnd->dpy, vi, NULL, GL_TRUE)))
  {
    XFree(vi);
    return 0;
  }

  XFree(vi);
  glXMakeCurrent(wnd->dpy, wnd->wnd, wnd->ctx);
  return 1;
}

int
updateWindow(struct window *wnd)
{
  XEvent evt;

  while (XPending(wnd->dpy) > 0)
  {
    XNextEvent(wnd->dpy, &evt);
    switch (evt.type)
    {
      case ClientMessage:
      {
        if (evt.xclient.data.l[0] == wnd->wndClose)
        {
          return 0;
        }

        break;
      }
    }
  }

  return 1;
}

int
destroyWindow(struct window *wnd)
{
  if (wnd->ctx)
  {
    glXMakeCurrent(wnd->dpy, None, NULL);
    glXDestroyContext(wnd->dpy, wnd->ctx);
    wnd->ctx = 0;
  }

  if (wnd->wnd)
  {
    XDestroyWindow(wnd->dpy, wnd->wnd);
    wnd->wnd = 0;
  }

  if (wnd->dpy)
  {
    XCloseDisplay(wnd->dpy);
    wnd->dpy = NULL;
  }
}

void
displayFrame(struct window *wnd, uint8_t *data)
{
  glClear(GL_COLOR_BUFFER_BIT);
  glXSwapBuffers(wnd->dpy, wnd->wnd);
}
