// Unit test 3 (spec section 9): the built SDL has its Vulkan renderer. Without vcpkg's sdl3[vulkan]
// feature the Windows list would start at direct3d12 with nothing to say so. SDL lists the renderers
// it was built with, with no GPU and no SDL_Init, so this runs on every CI runner.
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "check.h"

int main(void)
{
    bool vulkan = false;
    int count = SDL_GetNumRenderDrivers();
    for (int i = 0; i < count; i++) {
        const char *name = SDL_GetRenderDriver(i);
        printf("render driver %d: %s\n", i, name != NULL ? name : "(none)");
        if (name != NULL && strcmp(name, "vulkan") == 0)
            vulkan = true;
    }
    CHECK(count > 0);
    CHECK(vulkan);
    return check_report();
}
