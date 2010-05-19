/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2010 Scientific Computing and Imaging Institute,
   University of Utah.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

/**
  \file    context.cpp
  \author  Tom Fogal
           SCI Institute
           University of Utah
  \brief   Establishes an OpenGL context.
*/
#include "StdTuvokDefines.h"
#ifdef DETECTED_OS_WINDOWS
# include <GL/wgl.h>
#elif defined(DETECTED_OS_LINUX)
# include <GL/glx.h>
# include <X11/Xlib.h>
#endif
#include "Controller/Controller.h"

#include "context.h"

TvkContext::~TvkContext() { }

#ifdef DETECTED_OS_LINUX
struct xinfo {
  Display *display;
  XVisualInfo *visual;
  Window win;
  GLXContext ctx;
  Colormap cmap;
};
#endif

class TvkGLXContext: public TvkContext {
  public:
    TvkGLXContext();
    virtual ~TvkGLXContext();

  private:
    struct xinfo xi;
};

class TvkWGLContext: public TvkContext {
  public:
    TvkWGLContext() {}
    virtual ~TvkWGLContext() {}

  private:
};

TvkContext* TvkContext::Create() {
#ifdef DETECTED_OS_WINDOWS
  return new TvkWGLContext();
#else
  return new TvkGLXContext();
#endif
}

#ifdef DETECTED_OS_WINDOWS
// wgl init helpers..
#else

static struct xinfo x_connect();
static XVisualInfo* find_visual(Display*);
static void glx_init(Display*, XVisualInfo*, Window, GLXContext&);

TvkGLXContext::TvkGLXContext()
{
  this->xi = x_connect();
  glx_init(xi.display, xi.visual, xi.win, xi.ctx);
}

TvkGLXContext::~TvkGLXContext()
{
  glXDestroyContext(xi.display, xi.ctx);
  XDestroyWindow(xi.display, xi.win);
  XFree(xi.visual);
  XCloseDisplay(xi.display);
}

static struct xinfo
x_connect()
{
  struct xinfo rv;

  rv.display = XOpenDisplay(NULL);
  if(rv.display == NULL) {
    T_ERROR("Could not connect to display: '%s'!", XDisplayName(NULL));
    throw NoAvailableContext();
  }
  XSynchronize(rv.display, True);

  rv.visual = find_visual(rv.display);

  Window parent = RootWindow(rv.display, rv.visual->screen);

  XSetWindowAttributes xw_attr;
  xw_attr.override_redirect = False;
  xw_attr.background_pixel = 0;
  xw_attr.colormap = XCreateColormap(rv.display, parent, rv.visual->visual,
                                     AllocNone);
  xw_attr.event_mask = StructureNotifyMask | ExposureMask;

  rv.win = XCreateWindow(rv.display, parent, 0,0, 320,240, 0, rv.visual->depth,
                         InputOutput, rv.visual->visual,
                         CWBackPixel | CWBorderPixel | CWColormap |
                         CWOverrideRedirect | CWEventMask,
                         &xw_attr);
  XStoreName(rv.display, rv.win, "Tuvok testing");
  XSync(rv.display, False);

  return rv;
}

static void
glx_init(Display *disp, XVisualInfo *visual, Window win, GLXContext& ctx)
{
  if(!glXQueryExtension(disp, NULL, NULL)) {
    T_ERROR("Display does not support glX.");
    return;
  }

  ctx = glXCreateContext(disp, visual, 0, GL_TRUE);
  if(!ctx) {
    T_ERROR("glX Context creation failed.");
  }

  if(glXMakeCurrent(disp, win, ctx) == True) {
    MESSAGE("Make current succeeded: %p", glXGetCurrentContext());
  } else {
    T_ERROR("make current FAILED: %p", glXGetCurrentContext());
    return;
  }
}

static XVisualInfo *
find_visual(Display *d)
{
    XVisualInfo *ret_v;
    int attr[] = {
      GLX_RGBA,
      GLX_RED_SIZE,         5,
      GLX_GREEN_SIZE,       6,
      GLX_BLUE_SIZE,        5,
      GLX_ALPHA_SIZE,       8,
      GLX_DEPTH_SIZE,       8,
      GLX_ACCUM_RED_SIZE,   1,
      GLX_ACCUM_GREEN_SIZE, 1,
      GLX_ACCUM_BLUE_SIZE,  1,
      None
    };

    ret_v = glXChooseVisual(d, DefaultScreen(d), attr);
    MESSAGE("ChooseVisual got us %p", (const void*)ret_v);
    return ret_v;
}
#endif
