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

    int winWidth, winHeight;
    int _winWidth, _winHeight;

    struct {
        double x;
        double y;
    } _mouse{};

    struct nativeWindow {
        EGLDisplay eglDisplay;
        EGLSurface eglSurface;
        EGLContext eglContext;
        struct wl_egl_window *eglWindow;
    } _egl{};

    int init(const std::string &resDir,
             int width,
             int height,
             void *nativeWindow) {

        (void) resDir;

        _winWidth = width;
        _winHeight = height;

        assert(width);
        assert(height);
        assert(nativeWindow);

        auto _egl = reinterpret_cast<struct nativeWindow *>(nativeWindow);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize.x = width;
        io.DisplaySize.y = height;
        (void) io;
        io.IniFilename = NULL;

        // Setup Platform/Renderer bindings
        if (!ImGui_ImplOpenGL3_Init()) {
            spdlog::error("Failed to initialize ImGui");
            return -1;
        }

        // Everything went according to plan
        return 0;
    }

    void beginFrame() {
        eglMakeCurrent(_egl.eglDisplay, _egl.eglSurface, _egl.eglSurface, _egl.eglContext);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
    }

    void render(bool vsync) {
        // Rendering
        ImGui::Render();
        glViewport(0, 0, winWidth, winHeight);
        glClearColor(gui::themeManager.clearColor.x, gui::themeManager.clearColor.y, gui::themeManager.clearColor.z,
                     gui::themeManager.clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        eglSwapBuffers(_egl.eglDisplay, _egl.eglSurface);
    }

    void getMouseScreenPos(double &x, double &y) {
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

        eglMakeCurrent(_egl.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
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
