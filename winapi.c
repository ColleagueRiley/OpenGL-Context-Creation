#include <windows.h>
#include <GL/gl.h>

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef uint8_t     u8;
typedef int8_t      i8;
typedef uint16_t   u16;
typedef int16_t    i16;
typedef uint32_t   u32;
typedef int32_t    i32;
typedef uint64_t   u64;
typedef int64_t    i64;
typedef u8 b8;

#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_CONTEXT_MAJOR_VERSION_ARB             0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB             0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB              0x9126

int main() {
	typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hdc, HGLRC hglrc, const int *attribList);
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;
	
    typedef HRESULT (APIENTRY* PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC hdc, const int* piAttribIList, const FLOAT* pfAttribFList, UINT nMaxFormats, int* piFormats, UINT* nNumFormats);
	static PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = NULL;

    typedef BOOL(APIENTRY* PFNWGLSWAPINTERVALEXTPROC)(int interval);
    static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;

    WNDCLASS wc = {0};
    wc.lpfnWndProc   = DefWindowProc; // Default window procedure
    wc.hInstance     = GetModuleHandle(NULL);
    wc.lpszClassName = "SampleWindowClass";

    RegisterClass(&wc);
    
    HWND dummyWin = CreateWindowA(wc.lpszClassName, "Sample Window", 0, 200, 200, 300, 300, 0, 0, wc.hInstance, 0);
    HDC dummy_dc = GetDC(dummyWin);

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

    HGLRC dummy_context = wglCreateContext(dummy_dc);
    wglMakeCurrent(dummy_dc, dummy_context);

    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC) (void*) wglGetProcAddress("wglCreateContextAttribsARB");
    wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC) (void*)wglGetProcAddress("wglChoosePixelFormatARB");

    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");

    wglMakeCurrent(dummy_dc, 0);
    wglDeleteContext(dummy_context);
    ReleaseDC(dummyWin, dummy_dc);
    DestroyWindow(dummyWin);


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

    size_t index = (sizeof(attribs) / sizeof(attribs[0])) - 13;
    #define RGFW_GL_ADD_ATTRIB(attrib, attVal) \
		if (attVal) { \
			attribs[index] = attrib;\
			attribs[index + 1] = attVal;\
			index += 2;\
		}

    i32 RGFW_STENCIL = 0, RGFW_SAMPLES = 0, RGFW_STEREO = GL_FALSE, RGFW_AUX_BUFFERS = 0, RGFW_DOUBLE_BUFFER = 1;

    if (RGFW_DOUBLE_BUFFER)
        RGFW_GL_ADD_ATTRIB(0x2011, 1);  // double buffer
    RGFW_GL_ADD_ATTRIB(0x2023, RGFW_STENCIL);
    RGFW_GL_ADD_ATTRIB(0x2012, RGFW_STEREO);
    RGFW_GL_ADD_ATTRIB(0x2024, RGFW_AUX_BUFFERS);
    RGFW_GL_ADD_ATTRIB(0x2042, RGFW_SAMPLES);
    RGFW_GL_ADD_ATTRIB(0, 0);

    typedef u8 RGFW_GL_profile; enum { RGFW_GL_CORE = 0,  RGFW_GL_COMPATIBILITY  };
	i32 RGFW_majorVersion = 0, RGFW_minorVersion = 0;
	b8 RGFW_profile = RGFW_GL_CORE;

    HWND hwnd = CreateWindowA(wc.lpszClassName, "Sample Window",
                                0,
                                400, 400, 300, 300,
                                NULL, NULL, wc.hInstance, NULL);

    HDC hdc = GetDC(hwnd);


    PIXELFORMATDESCRIPTOR pfd2 = (PIXELFORMATDESCRIPTOR){ sizeof(pfd2), 1, pfd_flags, PFD_TYPE_RGBA, 32, 8, PFD_MAIN_PLANE, 24, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int pixel_format2;
    UINT num_formats;
    wglChoosePixelFormatARB(hdc, attribs, 0, 1, &pixel_format2, &num_formats);
    if (!num_formats) {
        printf("Failed to create a pixel format for WGL.\n");
    }

    DescribePixelFormat(hdc, pixel_format2, sizeof(pfd2), &pfd2);
    if (!SetPixelFormat(hdc, pixel_format2, &pfd2)) {
        printf("Failed to set the WGL pixel format.\n");
    }

    #define SET_ATTRIB(a, v) { \
        assert(((size_t) index + 1) < sizeof(context_attribs) / sizeof(context_attribs[0])); \
        context_attribs[index++] = a; \
        context_attribs[index++] = v; \
    }

    /* create opengl/WGL context for the specified version */ 
    index = 0;
    i32 context_attribs[40];

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

    HGLRC ctx = (HGLRC)wglCreateContextAttribsARB(hdc, NULL, context_attribs);
    wglMakeCurrent(hdc, ctx);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;

    BOOL running = TRUE;
    while (running) {
        if (PeekMessageA(&msg, hwnd, 0u, 0u, PM_REMOVE)) {
			switch (msg.message) {
                case WM_CLOSE:
                case WM_QUIT:
                    running = FALSE;
                    break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        running = IsWindow(hwnd);
    

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        SwapBuffers(hdc);
    }
    
    DeleteDC(hdc);
    DestroyWindow(hwnd);
    return 0;
}
