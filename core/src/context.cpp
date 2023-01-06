
#include "context.h"

#include <cassert>

#include <EGL/eglext.h>
#include <spdlog/spdlog.h>

#include <server.h>
#include "imgui.h"
#include <gui/gui.h>
#include <gui/icons.h>
#include <gui/menus/theme.h>
#include <gui/style.h>
#include <gui/widgets/bandplan.h>
#include <version.h>
#include <stb_image.h>
#include <core.h>
#include <filesystem>
#include <backend.h>

#include <signal_path/signal_path.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#ifndef INSTALL_PREFIX
#ifdef __APPLE__
#define INSTALL_PREFIX "/usr/local"
#else
#define INSTALL_PREFIX "/usr"
#endif
#endif

__attribute__((visibility("default")))
uint32_t Context::version() {
    return 0x00010000;
}

__attribute__((visibility("default")))
Context::Context(int width,
                 int height,
                 void *nativeWindow,
                 const char *rootPath,
                 const char *modulesPath,
                 const char *assetsPath)
        : mWidth(width),
          mHeight(height),
          mRootPath(rootPath),
          mModulesPath(modulesPath),
          mAssetsPath(assetsPath) {

    spdlog::info("[sdrpp]");
    spdlog::info("Root Path: {0}", mRootPath);
    spdlog::info("Modules Path: {0}", mModulesPath);
    spdlog::info("Asset Path: {0}", mAssetsPath);

    typedef struct {
        struct wl_display *wl_display;
        struct wl_surface *wl_surface;
        EGLDisplay egl_display;
        struct wl_egl_window *egl_window;
    } wl;

    auto p = reinterpret_cast<wl *>(nativeWindow);
    assert(p);

    mEgl.eglDisplay = p->egl_display;
    mEgl.eglWindow = p->egl_window;

    assert(mEgl.eglDisplay);
    assert(mEgl.eglWindow);

    init_egl(mEgl.eglWindow, mEgl.eglDisplay, mEgl.eglSurface, mEgl.eglContext);

    auto ret = eglMakeCurrent(mEgl.eglDisplay, mEgl.eglSurface, mEgl.eglSurface, mEgl.eglContext);
    assert(ret == EGL_TRUE);

    std::vector<const char *> args;
    args.reserve(3);
    args.emplace_back("sdrpp");
    args.emplace_back("--root");
    args.emplace_back(mRootPath.c_str());
    init_sdrpp(args.size(), const_cast<char **>(args.data()), width, height, nativeWindow);

    // Don't block
    eglSwapInterval(mEgl.eglDisplay, 0);

    eglMakeCurrent(mEgl.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

//
// Driven by Frame callback
//
__attribute__((visibility("default")))
void Context::draw_frame(uint32_t time) {
    (void) time;

    backend::drawFrame();
}

void *Context::get_egl_proc_address(const char *address) {
    const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    if (extensions && (strstr(extensions, "EGL_EXT_platform_wayland") ||
                       strstr(extensions, "EGL_KHR_platform_wayland"))) {
        return (void *) eglGetProcAddress(address);
    }

    return nullptr;
}

EGLSurface Context::create_egl_surface(EGLDisplay &eglDisplay,
                                       EGLConfig &eglConfig,
                                       void *native_window,
                                       const EGLint *attrib_list) {
    static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC create_platform_window =
            nullptr;

    if (!create_platform_window) {
        create_platform_window =
                (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC) get_egl_proc_address(
                        "eglCreatePlatformWindowSurfaceEXT");
    }

    if (create_platform_window)
        return create_platform_window(eglDisplay, eglConfig, native_window,
                                      attrib_list);

    return eglCreateWindowSurface(
            eglDisplay, eglConfig, (EGLNativeWindowType) native_window, attrib_list);
}

void Context::init_egl(void *nativeWindow,
                       EGLDisplay &eglDisplay,
                       EGLSurface &eglSurface,
                       EGLContext &eglContext) {
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
    eglGetConfigs(eglDisplay, nullptr, 0, &count);
    assert(count);

    auto *configs =
            reinterpret_cast<EGLConfig *>(calloc(count, sizeof(EGLConfig)));
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

    eglContext =
            eglCreateContext(eglDisplay, egl_conf, EGL_NO_CONTEXT, context_attribs);
    eglSurface = create_egl_surface(eglDisplay, egl_conf, nativeWindow, nullptr);
    assert(eglSurface != EGL_NO_SURFACE);
}

__attribute__((visibility("default")))
void Context::run_task() {
}

__attribute__((visibility("default")))
void Context::resize(int width, int height) {
    mWidth = width;
    mHeight = height;
    backend::resize(mWidth, mHeight);
}

__attribute__((visibility("default")))
void Context::de_initialize() {

    // Shut down all modules
    for (auto &[name, mod]: core::moduleManager.modules) {
        mod.end();
    }

    // Terminate backend
    backend::end();

    sigpath::iqFrontEnd.stop();

    core::configManager.disableAutoSave();
    core::configManager.save();

    spdlog::info("Exiting successfully");
}

int Context::init_sdrpp(int argc, char *argv[], int width, int height, void *nativeWindow) {
    spdlog::info("SDR++ v" VERSION_STR);

#ifdef IS_MACOS_BUNDLE
    // If this is a MacOS .app, CD to the correct directory
    auto execPath = std::filesystem::absolute(argv[0]);
    chdir(execPath.parent_path().string().c_str());
#endif

    // Define command line options and parse arguments
    core::args.defineAll();
    if (core::args.parse(argc, argv) < 0) { return -1; }

    // Show help and exit if requested
    if (core::args["help"].b()) {
        core::args.showHelp();
        return 0;
    }

    bool serverMode = (bool) core::args["server"];

#ifdef _WIN32
    // Free console if the user hasn't asked for a console and not in server mode
    if (!core::args["con"].b() && !serverMode) { FreeConsole(); }

    // Set error mode to avoid abnoxious popups
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
#endif

    // Check root directory
    std::string root = (std::string) core::args["root"];
    if (!std::filesystem::exists(root)) {
        spdlog::warn("Root directory {0} does not exist, creating it", root);
        if (!std::filesystem::create_directories(root)) {
            spdlog::error("Could not create root directory {0}", root);
            return -1;
        }
    }

    // Check that the path actually is a directory
    if (!std::filesystem::is_directory(root)) {
        spdlog::error("{0} is not a directory", root);
        return -1;
    }

    // ======== DEFAULT CONFIG ========
    json defConfig;
    defConfig["bandColors"]["amateur"] = "#FF0000FF";
    defConfig["bandColors"]["aviation"] = "#00FF00FF";
    defConfig["bandColors"]["broadcast"] = "#0000FFFF";
    defConfig["bandColors"]["marine"] = "#00FFFFFF";
    defConfig["bandColors"]["military"] = "#FFFF00FF";
    defConfig["bandPlan"] = "General";
    defConfig["bandPlanEnabled"] = true;
    defConfig["bandPlanPos"] = 0;
    defConfig["centerTuning"] = false;
    defConfig["colorMap"] = "Classic";
    defConfig["fftHold"] = false;
    defConfig["fftHoldSpeed"] = 60;
    defConfig["fastFFT"] = false;
    defConfig["fftHeight"] = 300;
    defConfig["fftRate"] = 20;
    defConfig["fftSize"] = 65536;
    defConfig["fftWindow"] = 2;
    defConfig["frequency"] = 100000000.0;
    defConfig["fullWaterfallUpdate"] = false;
    defConfig["max"] = 0.0;
    defConfig["maximized"] = false;
    defConfig["fullscreen"] = false;

    // Menu
    defConfig["menuElements"] = json::array();

    defConfig["menuElements"][0]["name"] = "Source";
    defConfig["menuElements"][0]["open"] = true;

    defConfig["menuElements"][1]["name"] = "Radio";
    defConfig["menuElements"][1]["open"] = true;

    defConfig["menuElements"][2]["name"] = "Recorder";
    defConfig["menuElements"][2]["open"] = true;

    defConfig["menuElements"][3]["name"] = "Sinks";
    defConfig["menuElements"][3]["open"] = true;

    defConfig["menuElements"][3]["name"] = "Frequency Manager";
    defConfig["menuElements"][3]["open"] = true;

    defConfig["menuElements"][4]["name"] = "VFO Color";
    defConfig["menuElements"][4]["open"] = true;

    defConfig["menuElements"][6]["name"] = "Band Plan";
    defConfig["menuElements"][6]["open"] = true;

    defConfig["menuElements"][7]["name"] = "Display";
    defConfig["menuElements"][7]["open"] = true;

    defConfig["menuWidth"] = 300;
    defConfig["min"] = -120.0;

    // Module instances
    defConfig["moduleInstances"]["Airspy Source"]["module"] = "airspy_source";
    defConfig["moduleInstances"]["Airspy Source"]["enabled"] = true;
    defConfig["moduleInstances"]["AirspyHF+ Source"]["module"] = "airspyhf_source";
    defConfig["moduleInstances"]["AirspyHF+ Source"]["enabled"] = true;
    defConfig["moduleInstances"]["BladeRF Source"]["module"] = "bladerf_source";
    defConfig["moduleInstances"]["BladeRF Source"]["enabled"] = true;
    defConfig["moduleInstances"]["File Source"]["module"] = "file_source";
    defConfig["moduleInstances"]["File Source"]["enabled"] = true;
    defConfig["moduleInstances"]["HackRF Source"]["module"] = "hackrf_source";
    defConfig["moduleInstances"]["HackRF Source"]["enabled"] = true;
    defConfig["moduleInstances"]["Hermes Source"]["module"] = "hermes_source";
    defConfig["moduleInstances"]["Hermes Source"]["enabled"] = true;
    defConfig["moduleInstances"]["LimeSDR Source"]["module"] = "limesdr_source";
    defConfig["moduleInstances"]["LimeSDR Source"]["enabled"] = true;
    defConfig["moduleInstances"]["PlutoSDR Source"]["module"] = "plutosdr_source";
    defConfig["moduleInstances"]["PlutoSDR Source"]["enabled"] = true;
    defConfig["moduleInstances"]["RFspace Source"]["module"] = "rfspace_source";
    defConfig["moduleInstances"]["RFspace Source"]["enabled"] = true;
    defConfig["moduleInstances"]["RTL-SDR Source"]["module"] = "rtl_sdr_source";
    defConfig["moduleInstances"]["RTL-SDR Source"]["enabled"] = true;
    defConfig["moduleInstances"]["RTL-TCP Source"]["module"] = "rtl_tcp_source";
    defConfig["moduleInstances"]["RTL-TCP Source"]["enabled"] = true;
    defConfig["moduleInstances"]["SDRplay Source"]["module"] = "sdrplay_source";
    defConfig["moduleInstances"]["SDRplay Source"]["enabled"] = true;
    defConfig["moduleInstances"]["SDR++ Server Source"]["module"] = "sdrpp_server_source";
    defConfig["moduleInstances"]["SDR++ Server Source"]["enabled"] = true;
    defConfig["moduleInstances"]["SoapySDR Source"]["module"] = "soapy_source";
    defConfig["moduleInstances"]["SoapySDR Source"]["enabled"] = true;
    defConfig["moduleInstances"]["SpyServer Source"]["module"] = "spyserver_source";
    defConfig["moduleInstances"]["SpyServer Source"]["enabled"] = true;

    defConfig["moduleInstances"]["Audio Sink"] = "audio_sink";
    defConfig["moduleInstances"]["Network Sink"] = "network_sink";

    defConfig["moduleInstances"]["Radio"] = "radio";

    defConfig["moduleInstances"]["Frequency Manager"] = "frequency_manager";
    defConfig["moduleInstances"]["Recorder"] = "recorder";
    defConfig["moduleInstances"]["Rigctl Server"] = "rigctl_server";
    // defConfig["moduleInstances"]["Rigctl Client"] = "rigctl_client";
    // TODO: Enable rigctl_client when ready
    // defConfig["moduleInstances"]["Scanner"] = "scanner";
    // TODO: Enable scanner when ready


    // Themes
    defConfig["theme"] = "Dark";
#ifdef __ANDROID__
    defConfig["uiScale"] = 3.0f;
#else
    defConfig["uiScale"] = 1.0f;
#endif

    defConfig["modules"] = json::array();

    defConfig["offsetMode"] = (int) 0; // Off
    defConfig["offset"] = 0.0;
    defConfig["showMenu"] = true;
    defConfig["showWaterfall"] = true;
    defConfig["source"] = "";
    defConfig["decimationPower"] = 0;
    defConfig["iqCorrection"] = false;
    defConfig["invertIQ"] = false;

    defConfig["streams"]["Radio"]["muted"] = false;
    defConfig["streams"]["Radio"]["sink"] = "Audio";
    defConfig["streams"]["Radio"]["volume"] = 1.0f;

    defConfig["windowSize"]["h"] = 720;
    defConfig["windowSize"]["w"] = 1280;

    defConfig["vfoOffsets"] = json::object();

    defConfig["vfoColors"]["Radio"] = "#FFFFFF";

#ifdef __ANDROID__
    defConfig["lockMenuOrder"] = true;
#else
    defConfig["lockMenuOrder"] = false;
#endif

#if defined(_WIN32)
    defConfig["modulesDirectory"] = "./modules";
    defConfig["resourcesDirectory"] = "./res";
#elif defined(IS_MACOS_BUNDLE)
    defConfig["modulesDirectory"] = "../Plugins";
    defConfig["resourcesDirectory"] = "../Resources";
#elif defined(__ANDROID__)
    defConfig["modulesDirectory"] = root + "/modules";
    defConfig["resourcesDirectory"] = root + "/res";
#else
    defConfig["modulesDirectory"] = INSTALL_PREFIX "/lib/sdrpp/plugins";
    defConfig["resourcesDirectory"] = INSTALL_PREFIX "/share/sdrpp";
#endif

    // Load config
    spdlog::info("Loading config");
    core::configManager.setPath(root + "/config.json");
    core::configManager.load(defConfig);
    core::configManager.enableAutoSave();
    core::configManager.acquire();

    // Android can't load just any .so file. This means we have to hardcode the name of the modules
#ifdef __ANDROID__
    int modCount = 0;
    core::configManager.conf["modules"] = json::array();

    core::configManager.conf["modules"][modCount++] = "airspy_source.so";
    core::configManager.conf["modules"][modCount++] = "airspyhf_source.so";
    core::configManager.conf["modules"][modCount++] = "hackrf_source.so";
    core::configManager.conf["modules"][modCount++] = "hermes_source.so";
    core::configManager.conf["modules"][modCount++] = "plutosdr_source.so";
    core::configManager.conf["modules"][modCount++] = "rfspace_source.so";
    core::configManager.conf["modules"][modCount++] = "rtl_sdr_source.so";
    core::configManager.conf["modules"][modCount++] = "rtl_tcp_source.so";
    core::configManager.conf["modules"][modCount++] = "sdrpp_server_source.so";
    core::configManager.conf["modules"][modCount++] = "spyserver_source.so";

    core::configManager.conf["modules"][modCount++] = "network_sink.so";
    core::configManager.conf["modules"][modCount++] = "audio_sink.so";

    core::configManager.conf["modules"][modCount++] = "m17_decoder.so";
    core::configManager.conf["modules"][modCount++] = "meteor_demodulator.so";
    core::configManager.conf["modules"][modCount++] = "radio.so";

    core::configManager.conf["modules"][modCount++] = "frequency_manager.so";
    core::configManager.conf["modules"][modCount++] = "recorder.so";
    core::configManager.conf["modules"][modCount++] = "rigctl_server.so";
    core::configManager.conf["modules"][modCount++] = "scanner.so";
#endif

    // Fix missing elements in config
    for (auto const &item: defConfig.items()) {
        if (!core::configManager.conf.contains(item.key())) {
            spdlog::info("Missing key in config {0}, repairing", item.key());
            core::configManager.conf[item.key()] = defConfig[item.key()];
        }
    }

    // Remove unused elements
    auto items = core::configManager.conf.items();
    for (auto const &item: items) {
        if (!defConfig.contains(item.key())) {
            spdlog::info("Unused key in config {0}, repairing", item.key());
            core::configManager.conf.erase(item.key());
        }
    }

    // Update to new module representation in config if needed
    for (auto [_name, inst]: core::configManager.conf["moduleInstances"].items()) {
        if (!inst.is_string()) { continue; }
        std::string mod = inst;
        json newMod;
        newMod["module"] = mod;
        newMod["enabled"] = true;
        core::configManager.conf["moduleInstances"][_name] = newMod;
    }

    // Load UI scaling
    style::uiScale = core::configManager.conf["uiScale"];

    core::configManager.release(true);

    if (serverMode) { return server::main(); }

    core::configManager.acquire();
    std::string resDir = core::configManager.conf["resourcesDirectory"];
    json bandColors = core::configManager.conf["bandColors"];
    core::configManager.release();

    // Assert that the resource directory is absolute and check existence
    resDir = std::filesystem::absolute(resDir).string();
    if (!std::filesystem::is_directory(resDir)) {
        spdlog::error(
                "Resource directory doesn't exist! Please make sure that you've configured it correctly in config.json (check readme for details)");
        return 1;
    }

    // Initialize backend
    int biRes = backend::init("", width, height, nativeWindow);
    if (biRes < 0) { return biRes; }

    // Initialize SmGui in normal mode
    SmGui::init(false);

    if (!style::loadFonts(resDir)) { return -1; }
    thememenu::init(resDir);
    LoadingScreen::init();

    LoadingScreen::show("Loading icons");
    spdlog::info("Loading icons");
    if (!icons::load(resDir)) { return -1; }

    LoadingScreen::show("Loading band plans");
    spdlog::info("Loading band plans");
    bandplan::loadFromDir(resDir + "/bandplans");

    LoadingScreen::show("Loading band plan colors");
    spdlog::info("Loading band plans color table");
    bandplan::loadColorTable(bandColors);

    gui::mainWindow.init();

    spdlog::info("Ready.");
    return 0;
}
