// The built SDL_ttf shapes text with HarfBuzz (spec section 9, test 5). Without it a Windows build
// (sdl3-ttf without [harfbuzz]) draws fallback Arabic as isolated letters in the wrong order, and
// TTF_GetHarfBuzzVersion() reports 0.0.0 (SDL_ttf.c's TTF_GetHarfBuzzVersion, 3.2.2).
#include <SDL3_ttf/SDL_ttf.h>
#include "check.h"

int main(void)
{
    int major = 0, minor = 0, patch = 0;
    TTF_GetHarfBuzzVersion(&major, &minor, &patch);
    printf("HarfBuzz %d.%d.%d\n", major, minor, patch);
    CHECK(major > 0);
    return check_report();
}
