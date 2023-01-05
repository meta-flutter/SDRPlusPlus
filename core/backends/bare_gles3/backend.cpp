#include <backend.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#include <spdlog/spdlog.h>
#include <utils/opengl_include_code.h>
#include <core.h>
#include <stb_image.h>
#include <gui/gui.h>

namespace backend {

    int winHeight;
    int winWidth;
    int _winWidth, _winHeight;

    struct {
        double x;
        double y;
    } _mouse;

    struct {
        EGLDisplay eglDisplay;
        EGLSurface eglSurface;
        EGLContext eglContext;
        struct wl_egl_window* eglWindow;
    } _egl{};

    void* get_egl_proc_address(const char* address) {
        const char* extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

        if (extensions && (strstr(extensions, "EGL_EXT_platform_wayland") ||
                           strstr(extensions, "EGL_KHR_platform_wayland"))) {
            return (void*)eglGetProcAddress(address);
        }

        return nullptr;
    }

    EGLSurface create_egl_surface(EGLDisplay& eglDisplay,
                                  EGLConfig& eglConfig,
                                  void* native_window,
                                  const EGLint* attrib_list) {
        static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC create_platform_window =
            nullptr;

        if (!create_platform_window) {
            create_platform_window =
                (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)get_egl_proc_address(
                    "eglCreatePlatformWindowSurfaceEXT");
        }

        if (create_platform_window)
            return create_platform_window(eglDisplay, eglConfig, native_window,
                                          attrib_list);

        return eglCreateWindowSurface(
            eglDisplay, eglConfig, (EGLNativeWindowType)native_window, attrib_list);
    }

    void init_egl(void* nativeWindow,
                  EGLDisplay& eglDisplay,
                  EGLSurface& eglSurface,
                  EGLContext& eglContext) {
        constexpr int kEglBufferSize = 24;

        EGLint config_attribs[] = {
            // clang-format off
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_STENCIL_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
            // clang-format on
        };

        static const EGLint context_attribs[] = {
            // clang-format off
            EGL_CONTEXT_MAJOR_VERSION, 3,
            EGL_CONTEXT_MAJOR_VERSION, 2,
            EGL_NONE
            // clang-format on
        };

        EGLint major, minor;
        EGLBoolean ret = eglInitialize(eglDisplay, &major, &minor);
        assert(ret == EGL_TRUE);

        ret = eglBindAPI(EGL_OPENGL_ES_API);
        assert(ret == EGL_TRUE);

        EGLint count;
        eglGetConfigs(eglDisplay, NULL, 0, &count);
        assert(count);

        auto* configs =
            reinterpret_cast<EGLConfig*>(calloc(count, sizeof(EGLConfig)));
        assert(configs);

        EGLint n;
        ret = eglChooseConfig(eglDisplay, config_attribs, configs, count, &n);
        assert(ret && n >= 1);

        EGLint size;
        EGLConfig egl_conf = nullptr;
        for (EGLint i = 0; i < n; i++) {
            eglGetConfigAttrib(eglDisplay, configs[i], EGL_BUFFER_SIZE, &size);
            if (kEglBufferSize <= size) {
                egl_conf = configs[i];
                break;
            }
        }
        free(configs);
        if (egl_conf == nullptr) {
            assert(false);
        }

        eglContext = eglCreateContext(eglDisplay, egl_conf, EGL_NO_CONTEXT, context_attribs);

        eglSurface = create_egl_surface(eglDisplay, egl_conf, nativeWindow, NULL);
        assert(eglSurface != EGL_NO_SURFACE);

        ret = eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
        assert(ret == EGL_TRUE);
    }

    int init(const std::string& resDir,
             int width,
             int height,
             void* nativeWindow) {

        (void)resDir;

        _winWidth = width;
        _winHeight = height;

        assert(width);
        assert(height);
        assert(nativeWindow);

        // Setup window

        typedef struct {
            struct wl_display* wl_display;
            struct wl_surface* wl_surface;
            EGLDisplay egl_display;
            struct wl_egl_window* egl_window;
        } wl;

        auto p = reinterpret_cast<wl*>(nativeWindow);

        _egl.eglDisplay = p->egl_display;
        _egl.eglWindow = p->egl_window;
        assert(_egl.eglDisplay);
        assert(_egl.eglWindow);

        init_egl(_egl.eglWindow, _egl.eglDisplay, _egl.eglSurface, _egl.eglContext);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize.x = width;
        io.DisplaySize.y = height;
        (void)io;
        io.IniFilename = NULL;

        // Setup Platform/Renderer bindings
        if (!ImGui_ImplOpenGL3_Init()) {
            spdlog::error("Failed to initialize OpenGL2");
            return -1;
        }

        // Everything went according to plan
        return 0;
    }

    void beginFrame() {
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
    }

    void render(bool vsync) {
        // Rendering
        ImGui::Render();
        int display_w = winWidth, display_h = winHeight;
        glViewport(0, 0, display_w, display_h);
        glClearColor(gui::themeManager.clearColor.x, gui::themeManager.clearColor.y, gui::themeManager.clearColor.z, gui::themeManager.clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        eglSwapInterval(_egl.eglDisplay, vsync);
        eglSwapBuffers(_egl.eglDisplay, _egl.eglSurface);
    }

    void getMouseScreenPos(double& x, double& y) {
        x = _mouse.x;
        y = _mouse.y;
    }

    void setMouseScreenPos(double x, double y) {
        _mouse.x = x;
        _mouse.y = y;
    }

    int renderLoop() {
        return 0;
    }

    void drawFrame() {

        beginFrame();

        if (_winWidth != winWidth || _winHeight != winHeight) {
            winWidth = _winWidth;
            winHeight = _winHeight;
            core::configManager.acquire();
            core::configManager.conf["windowSize"]["w"] = winWidth;
            core::configManager.conf["windowSize"]["h"] = winHeight;
            core::configManager.release(true);
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(_winWidth, _winHeight));
        gui::mainWindow.draw();

        render();
    }

    void resize(int width, int height) {
        _winWidth = width;
        _winHeight = height;
    }

    int end() {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();

        return 0; // TODO: Int really needed?
    }
}
