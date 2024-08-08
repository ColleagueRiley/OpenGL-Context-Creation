# RGFW Under the Hood: OpenGL context creation

NOTE: currently this article is a WIP and will be updated soon.



## Introduction
In my experience from working on RGFW, one of the most annoying parts of working with low-level APIs is creating an OpenGL context.
This is not because it's hard, but because there are many not-so obvious steps that you must do correctly in order to create your OpenGL context.

Although, it's a lot easier to create an OpenGL context when you know what goes into it. 

NOTE: MacOS code will be written with a Cocoa C Wrapper in mind (see the RGFW.h or Silicon.h)

## Overview
A quick overview of the steps required
- Load opengl context creation functions (if you need to)
- Create an OpenGL pixel format (or Visual on X11) using an attribute list 
- Create your OpenGL context using an attribute array to set the OpenGL version  
- Free the OpenGL context

On MacOS steps 2 and 3 are one step.\
EGL will not be included in this article because the setup is far easier. 

## Step 1 (Load OpenGL context creation functions)

First RGFW needs to load these functions

```c
X11 (GLX):
    glXCreateContextAttribsARB
    glXSwapIntervalEXT (optional)

Windows (WGL):
    wglCreateContextAttribsARB
    wglChoosePixelFormatARB
    wglSwapIntervalEXT (optional)

Cocoa (NSOpenGL)
    (none)
```

You need to load these functions because they're extension functions provided by your hardware vendor.

By default, `wglCreateContext` or `glXCreateContext` will create a OpenGL ~1.0 that probably uses software rendering. 

To load these functions you have to start by defining the functions.
```c
X11 (GLX):
    typedef GLXContext(*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    static glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    
    (optional)
    typedef void ( *PFNGLXSWAPINTERVALEXTPROC) (Display *dpy, GLXDrawable drawable, int interval);
    PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = NULL;

Windows (WGL):
	typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hdc, HGLRC hglrc, const int *attribList);
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;
	
    typedef HRESULT (APIENTRY* PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC hdc, const int* piAttribIList, const FLOAT* pfAttribFList, UINT nMaxFormats, int* piFormats, UINT* nNumFormats);
	static PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = NULL;

    (optional)
    typedef BOOL(APIENTRY* PFNWGLSWAPINTERVALEXTPROC)(int interval);
    static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
```

Once the functions are defined, RGFW loads the functions with these api functions:

```c
X11 (GLX): glXGetProcAddress aad glXGetProcAddressARB
Windows (WGL): wglGetProcAddress
```

Since RGFW doesn't need to load functions for NSOpenGL, I'm not going to explain the ProcAddress function. But it exists in RGFW.h and is more complicated than the other APIs.

```c
X11 (GLX):
    glXCreateContextAttribsARB = glXGetProcAddressARB((GLubyte*) "glXCreateContextAttribsARB");;
    
    (optional)
    glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC) glXGetProcAddress((GLubyte*) "glXSwapIntervalEXT");
```

(NOTE: for GLX the load code would go after step 2.)

WGL is a little bit more complicated because it needs to start by creating a dummy context.\
This dummy context is used by WGL to load the functions.

### WGL dummy 

First you have to create a dummy window, RGFW also uses this dummy window to get the height offset for the titlebar. 

Then get the device context from the dummy window.

```c
HWND dummyWin = CreateWindowA(Class.lpszClassName, name, window_style, win->r.x, win->r.y, win->r.w, win->r.h, 0, 0, inh, 0);

HDC dummy_dc = GetDC(dummyWin);
```

Afer that you need to create a dummy pixel format for the dummy window and dummy opengl context.

```c
u32 pfd_flags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;

PIXELFORMATDESCRIPTOR pfd = {
    sizeof(pfd),
    1, /* version */
    pfd_flags,
    PFD_TYPE_RGBA, /* ipixel type */
    24, /* color bits */
    0, 0, 0, 0, 0, 0,
    8, /* alpha bits */
    0, 0, 0, 0, 0, 0,
    32, /* depth bits */
    8, /* stencil bits */ 
    0,
    PFD_MAIN_PLANE, /* Layer type */
    0, 0, 0, 0
};

int pixel_format = ChoosePixelFormat(dummy_dc, &pfd);
SetPixelFormat(dummy_dc, pixel_format, &pfd);
```

Now you create a dummy context.\
This context will be using the default opengl context, probably opengl ~1.0. 

Then make it the current context

```c
HGLRC dummy_context = wglCreateContext(dummy_dc);
wglMakeCurrent(dummy_dc, dummy_context);
```

Now RGFW can load the functions and delete the dummy

```c
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC) (void*) wglGetProcAddress("wglCreateContextAttribsARB");
    wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC) (void*)wglGetProcAddress("wglChoosePixelFormatARB");

    (optional)
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");

    glMakeCurrent(dummy_dc, 0);
    wglDeleteContext(dummy_context);
    ReleaseDC(dummyWin, dummy_dc);
    DestroyWindow(dummyWin);
```


## Step 2 (Create an OpenGL pixel format (or Visual on X11) using an attribute list)

### Step 2.1: creating the attribute list

To create an OpenGL pixel format using an attribute list, RGFW has a function that creates an attribute list, `RGFW_initFormatAttribs` for the pixel format.

This function uses macros based on the target OS's API, supporting all of the APIs with a single-function. 

For this tutorial I'll be sepearting an array for each OS rather than using one big array. 

```c
linux:
    static u32 attribs[] = {
                            GLX_X_VISUAL_TYPE,      GLX_TRUE_COLOR,
                            GLX_DEPTH_SIZE,         24,                            
                            GLX_X_RENDERABLE,       1,
                            GLX_RED_SIZE,           8,
                            GLX_GREEN_SIZE,         8,
                            GLX_BLUE_SIZE,          8,
                            GLX_ALPHA_SIZE,         8,
                            GLX_RENDER_TYPE,        GLX_RGBA_BIT,
                            GLX_DRAWABLE_TYPE,      GLX_WINDOW_BIT,

                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

windows:

// windows makes you define the macros yourself, so I'll be using the hardcoded instead 
static u32 attribs[] = {
								0x2003, // WGL_ACCELERATION_ARB
								0x2027, // WGL_FULL_ACCELERATION_ARB
								0x201b, 8, // WGL_ALPHA_BITS_ARB
								0x2022, 24, // WGL_DEPTH_BITS_ARB
								0x2001, 1, // WGL_DRAW_TO_WINDOW_ARB
								0x2015, 8, // WGL_RED_BITS_ARB
								0x2017, 8, // WGL_GREEN_BITS_ARB
								0x2019, 8, // WGL_BLUE_BITS_ARB
								0x2013, 0x202B, // WGL_PIXEL_TYPE_ARB,  WGL_TYPE_RGBA_ARB
								0x2010,		1, // WGL_SUPPORT_OPENGL_ARB
								0x2014,	 32, // WGL_COLOR_BITS_ARB
								0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		};

macos:
		static u32 attribs[] = {
								11      , 8, // alpha size
								24      , 24, // depth size
								72, // NSOpenGLPFANoRecovery
								8, 24, // NSOpenGLPFAColorSize
								0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		};
```

You may notice the extra 0s at the bottom of the array, that's for optional arguments. 

To fill in the optional arguments, first RGFW sets the index to the first 0.

```c
size_t index = (sizeof(attribs) / sizeof(attribs[0])) - 13;
```

RGFW uses a macro to fill in optional arguments 

```c
#define RGFW_GL_ADD_ATTRIB(attrib, attVal) \
		if (attVal) { \
			attribs[index] = attrib;\
			attribs[index + 1] = attVal;\
			index += 2;\
		}
```

Then RGFW fills in the optional arguments 

I'm going to split this up between each platform again.

RGFW defines variables that can be changed by the user to fill in these arguments. 

```c
i32 RGFW_STENCIL = 0, RGFW_SAMPLES = 0, RGFW_STEREO = GL_FALSE, RGFW_AUX_BUFFERS = 0, RGFW_DOUBLE_BUFFER = 1;
```

```c
linux:
    if (RGFW_DOUBLE_BUFFER)
        RGFW_GL_ADD_ATTRIB(GLX_DOUBLEBUFFER, 1);
    RGFW_GL_ADD_ATTRIB(GLX_STENCIL_SIZE, RGFW_STENCIL);
    RGFW_GL_ADD_ATTRIB(GLX_STEREO, RGFW_STEREO);
    RGFW_GL_ADD_ATTRIB(GLX_AUX_BUFFERS, RGFW_AUX_BUFFERS);
    // samples are handled by GLX later

windows:
    if (RGFW_DOUBLE_BUFFER)
        RGFW_GL_ADD_ATTRIB(0x2011, 1);  // double buffer
    RGFW_GL_ADD_ATTRIB(0x2023, RGFW_STENCIL);
    RGFW_GL_ADD_ATTRIB(0x2012, RGFW_STEREO);
    RGFW_GL_ADD_ATTRIB(0x2024, RGFW_AUX_BUFFERS);
    RGFW_GL_ADD_ATTRIB(0x2042, RGFW_SAMPLES);

macOS:
    if (RGFW_DOUBLE_BUFFER)
        RGFW_GL_ADD_ATTRIB(5, 1); // double buffer
    RGFW_GL_ADD_ATTRIB(13, RGFW_STENCIL);
    RGFW_GL_ADD_ATTRIB(6, RGFW_STEREO);
    RGFW_GL_ADD_ATTRIB(7, RGFW_AUX_BUFFERS);
    RGFW_GL_ADD_ATTRIB(55, RGFW_SAMPLES);

    /* this is here because macOS has a specific way to handle using software rendering */

    if (useSoftware) {
        RGFW_GL_ADD_ATTRIB(70, kCGLRendererGenericFloatID);
    } else {
        attribs[index] = 73;
        index += 1;
    }
```

On macOS you'd also set the version here rather than later. 
```c
    attribs[index] = 99;
    attribs[index + 1] = 0x1000;

    if (RGFW_majorVersion >= 4 || RGFW_majorVersion >= 3) {
        attribs[index + 1] = (u32) ((RGFW_majorVersion >= 4) ? 0x4100 : 0x3200);
    }
```

Make sure the final two arguments are set to 0, this is how OpenGL/WGL/GLX/NSOpenGL knows to stop reading.

```
		RGFW_GL_ADD_ATTRIB(0, 0);
```


### step 2.2 creating the pixel format 

Now that the list is created, it can be used to create the pixel format.

#### GLX
GLX Handles this by creating an array of `GLXFBConfig` based on the attitudes.

```c
        i32 fbcount;
		GLXFBConfig* fbc = glXChooseFBConfig((Display*) display, DefaultScreen(display), (i32*) attribs, &fbcount);

		i32 best_fbc = -1;

		if (fbcount == 0) {
			printf("Failed to find any valid GLX visual configs\n");
			return NULL;
		}
```

Then it uses the generated array to find the closest matching FBConfig object. (This is where RGFW_SAMPLES comes in)

```c
		u32 i;
		for (i = 0; i < (u32)fbcount; i++) {
			XVisualInfo* vi = glXGetVisualFromFBConfig((Display*) display, fbc[i]);
                        if (vi == NULL)
				continue;
                        
			XFree(vi);

			i32 samp_buf, samples;
			glXGetFBConfigAttrib((Display*) display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
			glXGetFBConfigAttrib((Display*) display, fbc[i], GLX_SAMPLES, &samples);
			
			if ((best_fbc < 0 || samp_buf) && (samples == RGFW_SAMPLES || best_fbc == -1)) {
				best_fbc = i;
			}
		}

		if (best_fbc == -1) {
			printf("Failed to get a valid GLX visual\n");
			return NULL;
		}

		GLXFBConfig bestFbc = fbc[best_fbc];
```

Once it finds the closest matching object, it gets the X11 visual (or pixel format) from the array and frees the array.

```c
    /* Get a visual */
    XVisualInfo* vi = glXGetVisualFromFBConfig((Display*) display, bestFbc);
    
    XFree(fbc);
```

Now this Visual can be used to create a window and/or colormap.

```c
    XSetWindowAttributes swa;
    Colormap cmap;

    swa.colormap = cmap = XCreateColormap((Display*) display,
        DefaultRootWindow(display),
        vi->visual, AllocNone);

    swa.background_pixmap = None;
    swa.border_pixel = 0;
    swa.event_mask = event_mask;
    
    swa.background_pixel = 0;

    Window window = XCreateWindow((Display*) display, DefaultRootWindow((Display*) display), x, y, w, h,
        0, vi->depth, InputOutput, vi->visual,
        CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, &swa);
```

#### WGL

For WGL, you have to first create a win32 pixel format. 

```c
    PIXELFORMATDESCRIPTOR pfd = (PIXELFORMATDESCRIPTOR){ sizeof(pfd), 1, pfd_flags, PFD_TYPE_RGBA, 32, 8, PFD_MAIN_PLANE, 24, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    if (args & RGFW_OPENGL_SOFTWARE)
        pfd.dwFlags |= PFD_GENERIC_FORMAT | PFD_GENERIC_ACCELERATED;
```

```c
Then yo ucan create a WGL pixel format based on the attribs array.
    int pixel_format;
    UINT num_formats;
    wglChoosePixelFormatARB(hdc, attribs, 0, 1, &pixel_format, &num_formats);
    if (!num_formats) {
        printf("Failed to create a pixel format for WGL.\n");
    }
```

Now you can merge the two as one big format and set it as the format for your window.

```c
    DescribePixelFormat(hdc, pixel_format, sizeof(pfd), &pfd);
    if (!SetPixelFormat(hdc, pixel_format, &pfd)) {
        printf("Failed to set the WGL pixel format.\n");
    }
```


## NSOpenGL

For MacOS, you just have to create the pixel format then init a view based on the format for OpenGL.

```c
void* format = NSOpenGLPixelFormat_initWithAttributes(attribs);

/* the pixel format can be passed directly to opengl context creation to create a context 
    this is because the format also includes information about the opengl version (which may be a bad thing) */
view = NSOpenGLView_initWithFrame((NSRect){{0, 0}, {w, h}}, format);
objc_msgSend_void(view, sel_registerName("prepareOpenGL"));
ctx = objc_msgSend_id(view, sel_registerName("openGLContext"))
```

If you're following along for MacOS, you can skip step 3.


## Step 3 (Create your OpenGL context using an attribute array to set the OpenGL version)

NOTE RGFW defines this enum and these variables so the user can control the OpenGL version:

```c
    typedef RGFW_ENUM(u8, RGFW_GL_profile)  { RGFW_GL_CORE = 0,  RGFW_GL_COMPATIBILITY  };
	i32 RGFW_majorVersion = 0, RGFW_minorVersion = 0;
	b8 RGFW_profile = RGFW_GL_CORE;
```

### glx 
This is where GLX would load it's extension functions!!

```c
    /* load functions now that a context is created */
    glXCreateContextAttribsARB = glXGetProcAddressARB((GLubyte*) "glXCreateContextAttribsARB");;
    glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC) glXGetProcAddress((GLubyte*) "glXSwapIntervalEXT");
```

Now it's time to create the attribute for the GL version
```c
    i32 context_attribs[7] = { 0, 0, 0, 0, 0, 0, 0 };
    context_attribs[0] = GLX_CONTEXT_PROFILE_MASK_ARB;
    if (RGFW_profile == RGFW_GL_CORE) 
        context_attribs[1] = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
    else 
        context_attribs[1] = GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
    
    if (RGFW_majorVersion || RGFW_minorVersion) {
        context_attribs[2] = GLX_CONTEXT_MAJOR_VERSION_ARB;
        context_attribs[3] = RGFW_majorVersion;
        context_attribs[4] = GLX_CONTEXT_MINOR_VERSION_ARB;
        context_attribs[5] = RGFW_minorVersion;
    }
```


Now the context can be created:

```c
    win->src.ctx = glXCreateContextAttribsARB((Display*) win->src.display, bestFbc, NULL, True, context_attribs);
```

### WGL

First WGL needs to create an attribs array for setting the OpenGL Version

It also uses a helper macro called SET_ATTRIB

```c
    #define SET_ATTRIB(a, v) { \
        assert(((size_t) index + 1) < sizeof(attribs) / sizeof(attribs[0])); \
        attribs[index++] = a; \
        attribs[index++] = v; \
    }

    /* create opengl/WGL context for the specified version */ 
    u32 index = 0;
    i32 attribs[40];

    if (RGFW_profile == RGFW_GL_CORE) {
        SET_ATTRIB(WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB);
    }
    else {
        SET_ATTRIB(WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB);
    }

    if (RGFW_majorVersion || RGFW_minorVersion) {
        SET_ATTRIB(WGL_CONTEXT_MAJOR_VERSION_ARB, RGFW_majorVersion);
        SET_ATTRIB(WGL_CONTEXT_MINOR_VERSION_ARB, RGFW_minorVersion);
    }

    SET_ATTRIB(0, 0);
```

Now the context can be created:

```c
    win->src.ctx = (HGLRC)wglCreateContextAttribsARB(win->src.hdc, NULL, attribs);
```


## Step 4 (Free the OpenGL context)
Now that RGFW has created it's OpenGL context, it has to free the context when it's done using it.

This is the easy part.


```c
linux (GLX):
    glXDestroyContext((Display*) win->src.display, win->src.ctx);

windows (WGL):
    wglDeleteContext((HGLRC) win->src.ctx);

macOS (NSOpenGL):
    // I think macOS NSOpenGL stuff is freed automatically when everything else is freed 
```


## Full code examples

### X11
```c
#include <X11/Xlib.h>
#include <GL/glx.h>

#include <stdio.h>
#include <stdint.h>

typedef GLXContext(*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
typedef void ( *PFNGLXSWAPINTERVALEXTPROC) (Display *dpy, GLXDrawable drawable, int interval);

#define GL_ADD_ATTRIB(attrib, attVal) \
		if (attVal) { \
			attribs[index] = attrib;\
			attribs[index + 1] = attVal;\
			index += 2;\
		}

typedef uint8_t GL_profile; enum  { GL_CORE = 0,  GL_COMPATIBILITY  };
int32_t majorVersion = 0, minorVersion = 0;
Bool profile = GL_CORE;

int32_t STENCIL = 0, SAMPLES = 0, STEREO = GL_FALSE, AUX_BUFFERS = 0, DOUBLE_BUFFER = 1;

int main(void) {
    typedef void ( *PFNGLXSWAPINTERVALEXTPROC) (Display *dpy, GLXDrawable drawable, int interval);
    PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = NULL;

    static uint32_t attribs[] = {
                            GLX_X_VISUAL_TYPE,      GLX_TRUE_COLOR,
                            GLX_DEPTH_SIZE,         24,                            
                            GLX_X_RENDERABLE,       1,
                            GLX_RED_SIZE,           8,
                            GLX_GREEN_SIZE,         8,
                            GLX_BLUE_SIZE,          8,
                            GLX_ALPHA_SIZE,         8,
                            GLX_RENDER_TYPE,        GLX_RGBA_BIT,
                            GLX_DRAWABLE_TYPE,      GLX_WINDOW_BIT,

                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    
    size_t index = (sizeof(attribs) / sizeof(attribs[0])) - 13;

    if (DOUBLE_BUFFER)
        GL_ADD_ATTRIB(GLX_DOUBLEBUFFER, 1);
    GL_ADD_ATTRIB(GLX_STENCIL_SIZE, STENCIL);
    GL_ADD_ATTRIB(GLX_STEREO, STEREO);
    GL_ADD_ATTRIB(GLX_AUX_BUFFERS, AUX_BUFFERS);

    Display *display;
    XEvent event;
 
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        return -1;
    }
 
    int s = DefaultScreen(display);
 

    int32_t fbcount;
    GLXFBConfig* fbc = glXChooseFBConfig((Display*) display, DefaultScreen(display), (int32_t*) attribs, &fbcount);

    int32_t best_fbc = -1;

    if (fbcount == 0) {
        printf("Failed to find any valid GLX visual configs\n");
        return -1;
    }
  
    uint32_t i;
    for (i = 0; i < (uint32_t)fbcount; i++) {
        XVisualInfo* vi = glXGetVisualFromFBConfig((Display*) display, fbc[i]);
        if (vi == NULL)
            continue;
                    
        XFree(vi);

        int32_t samp_buf, samples;
        glXGetFBConfigAttrib((Display*) display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
        glXGetFBConfigAttrib((Display*) display, fbc[i], GLX_SAMPLES, &samples);
        
        if ((best_fbc < 0 || samp_buf) && (samples == SAMPLES || best_fbc == -1)) {
            best_fbc = i;
        }
    }

    if (best_fbc == -1) {
        printf("Failed to get a valid GLX visual\n");
        return -1;
    }

    GLXFBConfig bestFbc = fbc[best_fbc];
    
    XVisualInfo* vi = glXGetVisualFromFBConfig((Display*) display, bestFbc);
    
    XFree(fbc);

    XSetWindowAttributes swa;
    Colormap cmap;

    swa.colormap = cmap = XCreateColormap((Display*) display,
        DefaultRootWindow(display),
        vi->visual, AllocNone);

    swa.background_pixmap = None;
    swa.border_pixel = 0;
    swa.event_mask = CWColormap | CWBorderPixel | CWBackPixel | CWEventMask;
    
    swa.background_pixel = 0;

    Window window = XCreateWindow((Display*) display, DefaultRootWindow((Display*) display), 400, 400, 200, 200,
        0, vi->depth, InputOutput, vi->visual,
        CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, &swa);
    
    XSelectInput(display, window, ExposureMask | KeyPressMask);
    
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((GLubyte*) "glXCreateContextAttribsARB");
    glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC) glXGetProcAddress((GLubyte*) "glXSwapIntervalEXT");
    
    int32_t context_attribs[7] = { 0, 0, 0, 0, 0, 0, 0 };
    context_attribs[0] = GLX_CONTEXT_PROFILE_MASK_ARB;
    if (profile == GL_CORE) 
        context_attribs[1] = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
    else 
        context_attribs[1] = GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
    
    if (majorVersion || minorVersion) {
        context_attribs[2] = GLX_CONTEXT_MAJOR_VERSION_ARB;
        context_attribs[3] = majorVersion;
        context_attribs[4] = GLX_CONTEXT_MINOR_VERSION_ARB;
        context_attribs[5] = minorVersion;
    }
    
    GLXContext ctx = glXCreateContextAttribsARB((Display*) display, bestFbc, NULL, True, context_attribs);
    glXMakeCurrent(display, window, ctx);

    XMapWindow(display, window);
    
    for (;;) {
        XNextEvent(display, &event);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glXSwapBuffers(display, window);
    }
    
    glXDestroyContext((Display*) display, ctx);
    XCloseDisplay(display);
    
    return 0;
 }
```