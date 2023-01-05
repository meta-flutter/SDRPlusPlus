#pragma once
#include <string>

namespace backend {
    int init(const std::string& resDir = "", int width = 0, int height = 0, void *nativeWindow = nullptr);
    void beginFrame();
    void render(bool vsync = true);
    void getMouseScreenPos(double& x, double& y);
    void setMouseScreenPos(double x, double y);
    int renderLoop();
    void drawFrame();
    void resize(int width, int height);
    int end();
}