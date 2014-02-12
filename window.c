#include <stdio.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <GL/glew.h>
#include "window.h"
#include "process.h"

int
initWindow(struct window *wnd)
{
  XEvent evt;
  XVisualInfo *vi;
  XSetWindowAttributes swa;
  Colormap cmap;
  Atom atom;
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
  swa.border_pixel = 0;
  swa.override_redirect = 1;
  swa.event_mask = StructureNotifyMask | ExposureMask |
                   KeyPressMask | ResizeRedirectMask;
  if (!(wnd->wnd = XCreateWindow(wnd->dpy, root, 0, 0, wnd->width, wnd->height,
                                 0, vi->depth, InputOutput, vi->visual,
                                 CWColormap | CWEventMask, &swa)))
  {
    XFree(vi);
    return 0;
  }

  /* Catch window close events */
  wnd->wndClose = XInternAtom(wnd->dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(wnd->dpy, wnd->wnd, &wnd->wndClose, 1);

  /* Make this window a popup */
  if ((atom = XInternAtom(wnd->dpy, "_NET_WM_WINDOW_TYPE_DIALOG", True)) != 0)
  {
    XChangeProperty(wnd->dpy, wnd->wnd,
                    XInternAtom(wnd->dpy, "_NET_WM_WINDOW_TYPE", True),
                    XA_ATOM, 32, PropModeReplace, (uint8_t*)&atom, 1);
  }

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
  if (glewInit() != GLEW_OK)
  {
    return 0;
  }

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
      case ResizeRequest:
      {
        XResizeWindow(wnd->dpy, wnd->wnd, wnd->width, wnd->height);
        break;
      }
      case ClientMessage:
      {
        if (evt.xclient.data.l[0] == (int)wnd->wndClose)
        {
          return 0;
        }

        break;
      }
    }
  }

  return 1;
}

void
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
displayImage(struct window *wnd, struct process *proc)
{
  glViewport(0, 0, wnd->width, wnd->height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0f, wnd->width, wnd->height, 0.0f, -1.0, 1.0f);
  glMatrixMode(GL_MODELVIEW);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, proc->output);
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(0, 0);
    glTexCoord2f(1.0f, 0.0f); glVertex2i(wnd->width, 0);
    glTexCoord2f(1.0f, 1.0f); glVertex2i(wnd->width, wnd->height);
    glTexCoord2f(0.0f, 1.0f); glVertex2i(0, wnd->height);
  glEnd();
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);

  glXSwapBuffers(wnd->dpy, wnd->wnd);
}
