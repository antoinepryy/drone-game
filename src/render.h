#ifndef RENDER_H
#define RENDER_H

#include <SDL.h>

#include "drone.h"

#define WINDOW_WIDTH  900
#define WINDOW_HEIGHT 600

#define TRAIL_LENGTH 160

/* Visual state that is not part of the simulation (animations, trail). */
typedef struct {
    double time;              /* seconds since start */
    double rotor_angle;       /* radians, propeller animation */
    double rotor_speed;       /* rad/s, eases in and out */
    double trail_timer;
    float trail_x[TRAIL_LENGTH], trail_y[TRAIL_LENGTH];
    int trail_count, trail_head;
} View;

void view_init(View *v);
void view_update(View *v, const Drone *d, double dt);
void render_frame(SDL_Renderer *r, const Drone *d, const View *v);

#endif
