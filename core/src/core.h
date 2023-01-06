#pragma once
#include <config.h>
#include <module.h>
#include <module_com.h>
#include "command_args.h"

namespace core {
    SDRPP_EXPORT ConfigManager configManager;
    SDRPP_EXPORT ModuleManager moduleManager;
    SDRPP_EXPORT ModuleComManager modComManager;
    SDRPP_EXPORT CommandArgsParser args;

    void setInputSampleRate(double samplerate);
};

__attribute__((visibility("default")))
int sdrpp_main(int argc, char* argv[]);