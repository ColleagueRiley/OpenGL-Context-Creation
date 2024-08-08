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

	glXCreateContextAttribsARBProc glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((GLubyte*) "glXCreateContextAttribsARB");
    glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC) glXGetProcAddress((GLubyte*) "glXSwapIntervalEXT");
 
	if (glXCreateContextAttribsARB == NULL)
		return -1;

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
