#ifndef RENDER_H
#define RENDER_H

#include <SDL.h>

#include "drone.h"

#define WINDOW_WIDTH  900
#define WINDOW_HEIGHT 600

void render_frame(SDL_Renderer *r, const Drone *d);

#endif
