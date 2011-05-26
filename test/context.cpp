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
# include <windows.h>
#else
# include <GL/glx.h>
# include <X11/Xlib.h>
#endif
#include "Controller/Controller.h"

#include "context.h"

TvkContext::~TvkContext() { }

#ifndef DETECTED_OS_WINDOWS
struct xinfo {
  Display *display;
  XVisualInfo *visual;
  Window win;
  GLXContext ctx;
  Colormap cmap;
};

class TvkGLXContext: public TvkContext {
  public:
    TvkGLXContext(uint32_t w, uint32_t h, uint8_t color_bits,
                  uint8_t depth_bits, uint8_t stencil_bits,
                  bool double_buffer);
    virtual ~TvkGLXContext();

    bool isValid() const;
    bool makeCurrent();
    bool swapBuffers();

  private:
    struct xinfo xi;
};
#endif

class TvkWGLContext: public TvkContext {
  public:
    TvkWGLContext(uint32_t w, uint32_t h, uint8_t color_bits,
                  uint8_t depth_bits, uint8_t stencil_bits,
                  bool double_buffer);
    virtual ~TvkWGLContext() {}

    bool isValid() const;
    bool makeCurrent();
    bool swapBuffers();

  private:
#ifdef DETECTED_OS_WINDOWS
    HDC deviceContext;
    HGLRC renderingContext;
    HWND window;
#endif
};

TvkContext* TvkContext::Create(uint32_t width, uint32_t height,
                               uint8_t color_bits, uint8_t depth_bits,
                               uint8_t stencil_bits, bool double_buffer)
{
#ifdef DETECTED_OS_WINDOWS
  return new TvkWGLContext(width, height, color_bits, depth_bits, stencil_bits,
                           double_buffer);
#else
  return new TvkGLXContext(width, height, color_bits, depth_bits, stencil_bits,
                           double_buffer);
#endif
}

#ifndef DETECTED_OS_WINDOWS
static struct xinfo x_connect(uint32_t, uint32_t, bool);
static XVisualInfo* find_visual(Display*, bool);
static void glx_init(Display*, XVisualInfo*, GLXContext&);

TvkGLXContext::TvkGLXContext(uint32_t w, uint32_t h, uint8_t,
                             uint8_t, uint8_t,
                             bool double_buffer)
{
  // if you *really* require a specific value... just hack this class
  // to your liking.  You probably want to add those parameters to x_connect
  // and use them there.
  WARNING("Ignoring color, depth, stencil bits.  For many applications, it "
          "is better to let the GLX library choose the \"best\" visual.");
  this->xi = x_connect(w, h, double_buffer);
  glx_init(xi.display, xi.visual, xi.ctx);
  this->makeCurrent();
  MESSAGE("Current context: %p", glXGetCurrentContext());
}

TvkGLXContext::~TvkGLXContext()
{
  glXDestroyContext(xi.display, xi.ctx);
  XDestroyWindow(xi.display, xi.win);
  XFree(xi.visual);
  XCloseDisplay(xi.display);
}

bool TvkGLXContext::isValid() const
{
  return this->xi.display != NULL && this->xi.ctx != NULL;
}

bool TvkGLXContext::makeCurrent()
{
  if(glXMakeCurrent(this->xi.display, this->xi.win, this->xi.ctx) != True) {
    T_ERROR("Could not make context current!");
    return false;
  }
  return true;
}

bool TvkGLXContext::swapBuffers()
{
  glXSwapBuffers(this->xi.display, this->xi.win);
  // SwapBuffers generates an X error if it fails.
  return true;
}

static struct xinfo
x_connect(uint32_t width, uint32_t height, bool dbl_buffer)
{
  struct xinfo rv;

  rv.display = XOpenDisplay(NULL);
  if(rv.display == NULL) {
    T_ERROR("Could not connect to display: '%s'!", XDisplayName(NULL));
    throw NoAvailableContext();
  }
  XSynchronize(rv.display, True);

  rv.visual = find_visual(rv.display, dbl_buffer);

  Window parent = RootWindow(rv.display, rv.visual->screen);

  XSetWindowAttributes xw_attr;
  xw_attr.override_redirect = False;
  xw_attr.background_pixel = 0;
  xw_attr.colormap = XCreateColormap(rv.display, parent, rv.visual->visual,
                                     AllocNone);
  xw_attr.event_mask = StructureNotifyMask | ExposureMask;

  rv.win = XCreateWindow(rv.display, parent, 0,0, width,height, 0,
                         rv.visual->depth,
                         InputOutput, rv.visual->visual,
                         CWBackPixel | CWBorderPixel | CWColormap |
                         CWOverrideRedirect | CWEventMask,
                         &xw_attr);
  XStoreName(rv.display, rv.win, "Tuvok testing");
  XSync(rv.display, False);

  return rv;
}

static void
glx_init(Display *disp, XVisualInfo *visual, GLXContext& ctx)
{
  if(!glXQueryExtension(disp, NULL, NULL)) {
    T_ERROR("Display does not support glX.");
    return;
  }

  ctx = glXCreateContext(disp, visual, 0, GL_TRUE);
  if(!ctx) {
    T_ERROR("glX Context creation failed.");
  }
}

static XVisualInfo *
find_visual(Display *d, bool double_buffered)
{
    XVisualInfo *ret_v;
    // GLX_USE_GL is basically a no-op, so this provides a convenient
    // way of specifying double buffering or not.
    int att_buf = double_buffered ? GLX_DOUBLEBUFFER : GLX_USE_GL;
    int attr[] = {
      GLX_RGBA,
      att_buf,
      GLX_RED_SIZE,         5,
      GLX_GREEN_SIZE,       6,
      GLX_BLUE_SIZE,        5,
      GLX_ALPHA_SIZE,       8,
      GLX_DEPTH_SIZE,       8,
      GLX_ACCUM_RED_SIZE,   0,
      GLX_ACCUM_GREEN_SIZE, 0,
      GLX_ACCUM_BLUE_SIZE,  0,
      None
    };

    ret_v = glXChooseVisual(d, DefaultScreen(d), attr);
    MESSAGE("ChooseVisual got us %p", (const void*)ret_v);
    return ret_v;
}
#endif

#ifdef DETECTED_OS_WINDOWS

static void outputLastError() {
  DWORD lastError = GetLastError();
  LPVOID msgBuffer;
  FormatMessageW(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    lastError,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR)&msgBuffer,
    0, NULL
  );
  T_ERROR("Win32 error: %s", std::string((LPSTR)msgBuffer));
  LocalFree(msgBuffer);
}

TvkWGLContext::TvkWGLContext(uint32_t w, uint32_t h, uint8_t color_bits,
                             uint8_t depth_bits, uint8_t stencil_bits,
                             bool double_buffer) :
  deviceContext(NULL),
  renderingContext(NULL),
  window(NULL)
{
  if(w == 0 || h == 0 ||
     !(color_bits==8 || color_bits==16 || color_bits==24 || color_bits==32) ||
     !(depth_bits==8 || depth_bits==16 || depth_bits==24 || depth_bits==32 ||
       depth_bits==0) ||
     !(stencil_bits == 0 || stencil_bits == 8)) {
    T_ERROR("Invalid parameters passed to constructor.");
    throw NoAvailableContext();
  }

  window = CreateWindowExW(WS_EX_TOOLWINDOW, L"Static", L"GLContextWindow",
                WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN ,
                0, 0, width, height, 0, 0, GetModuleHandle(NULL), 0);
  if (!window)
  {
    outputLastError();
    return;
  }
  ShowWindow(window, SW_HIDE);

  deviceContext = GetDC(window);
  if(!deviceContext) {
    // GetDC doesn't SetLastError, but this still works anyway...
    outputLastError();
    DestroyWindow(window);
    throw NoAvailableContext();
  }

  PIXELFORMATDESCRIPTOR pfd;
  pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_TYPE_RGBA;
  if (useDoubleBuffer) pfd.dwFlags |= PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = colorBits;
  pfd.cRedBits = 0;
  pfd.cRedShift = 0;
  pfd.cGreenBits = 0;
  pfd.cGreenShift = 0;
  pfd.cBlueBits = 0;
  pfd.cBlueShift = 0;
  pfd.cAlphaBits = 0;
  pfd.cAlphaShift = 0;
  pfd.cAccumBits = 0;
  pfd.cAccumRedBits = 0;
  pfd.cAccumGreenBits = 0;
  pfd.cAccumBlueBits = 0;
  pfd.cAccumAlphaBits = 0;
  pfd.cDepthBits = depthBits;
  pfd.cStencilBits = stencilBits;
  pfd.cAuxBuffers = 0;
  pfd.iLayerType = PFD_MAIN_PLANE;
  pfd.bReserved = 0;
  pfd.dwLayerMask = 0;
  pfd.dwVisibleMask = 0;
  pfd.dwDamageMask = 0;

  int pixelFormat = ChoosePixelFormat(deviceContext, &pfd);

  if (!pixelFormat)
  {
    outputLastError();
    ReleaseDC(window, deviceContext);
    DestroyWindow(window);
    throw NoAvailableContext();
  }

  PIXELFORMATDESCRIPTOR pfdResult;
  DescribePixelFormat(deviceContext, pixelFormat,
                      sizeof(PIXELFORMATDESCRIPTOR), &pfdResult);

  if (!(pfdResult.dwFlags & PFD_SUPPORT_OPENGL))
  {
    T_ERROR("No OpenGL support.");
    ReleaseDC(window, deviceContext);
    DestroyWindow(window);
    throw NoAvailableContext();
  }

  if (useDoubleBuffer && !(pfdResult.dwFlags & PFD_DOUBLEBUFFER)) {
    WARNING("No double buffer support!");
  }

  std::ostringstream ss;
  if (pfdResult.cColorBits != colorBits) {
    ss << "Color bits requested: " << colorBits << ", actual color bits: "
       << pfdResult.cColorBits << "\n";
  }
  if (pfdResult.cDepthBits != depthBits) {
    ss << "Depth bits requested " << depthBits << ", actual depth bits: "
       << pfdResult.cDepthBits << "\n";
  }
  if (pfdResult.cStencilBits != stencilBits) {
    ss << "Stencil bits requested " << stencilBits << ", actual stencil bits:"
       << pfdResult.cStencilBits << "\n";
  }
  MESSAGE("%s", ss.str().c_str());

  if (!SetPixelFormat(deviceContext, pixelFormat, &pfd)) {
    outputLastError();
    ReleaseDC(window, deviceContext);
    DestroyWindow(window);
    throw NoAvailableContext();
  }

  renderingContext = wglCreateContext(deviceContext);

  if (!renderingContext) {
    outputLastError();
    ReleaseDC(window, deviceContext);
    DestroyWindow(window);
    throw NoAvailableContext();
  }
}

TvkWGLContext::~TvkWGLContext()
{
  wglDeleteContext(renderingContext);
  ReleaseDC(window, deviceContext);
  DestroyWindow(window);
}

bool TvkWGLContext::isValid() const
{
  // Object would not be created otherwise (throw in constructor)
  return true;
}

bool TvkWGLContext::makeCurrent()
{
  if(!wglMakeCurrent(deviceContext, renderingContext)) {
    outputLastError();
    return false;
  }
  return true;
}

bool TvkWGLContext::swapBuffers()
{
  if(!isValid()) { return false; }

  if(!wglSwapBuffers(deviceContext, WGL_SWAP_MAIN_PLANE)) {
    outputLastError();
    return false;
  }
  return true;
}
#endif
