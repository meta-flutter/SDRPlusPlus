#include <core.h>
#include "context.h"

int main(int argc, char* argv[]) {
    return Context::init_sdrpp(argc, argv, 0, 0, nullptr);
}