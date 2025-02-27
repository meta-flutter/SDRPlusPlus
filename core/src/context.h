#pragma once

#include <string>
#include <EGL/egl.h>
#include <GLES3/gl32.h>

#include <cstdint>

struct Context {
public:
    static uint32_t version();

    Context(int width,
            int height,
            void *nativeWindow,
            const char *rootPath,
            const char *modulesPath,
            const char *assetsPath);

    ~Context() = default;

    Context(const Context &) = delete;

    Context(Context &&) = delete;

    Context &operator=(const Context &) = delete;

    static void draw_frame(uint32_t time);

    void run_task();

    void resize(int width, int height);

    static void de_initialize();

private:
    std::string mRootPath;
    std::string mModulesPath;
    std::string mAssetsPath;
    int mWidth;
    int mHeight;

    struct {
        EGLDisplay eglDisplay;
        EGLSurface eglSurface;
        EGLContext eglContext;
        struct wl_egl_window *eglWindow;
    } mEgl{};

    static void *get_egl_proc_address(const char *address);

    static EGLSurface create_egl_surface(EGLDisplay &eglDisplay,
                                         EGLConfig &eglConfig,
                                         void *native_window,
                                         const EGLint *attrib_list);

    static void
    init_egl(void *nativeWindow, EGLDisplay &eglDisplay, EGLSurface &eglSurface, EGLContext &eglContext);

    static int init_sdrpp(int argc, char *argv[], int width, int height, void *nativeWindow);
};
