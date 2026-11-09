#include "render.h"

#include <math.h>

#define PI 3.14159265358979323846

#define MAP_SIZE   600                      /* px, square map on the left */
#define PANEL_X    MAP_SIZE
#define PX_PER_M   ((double)MAP_SIZE / (2.0 * DRONE_WORLD_HALF_SIZE))

static void set_color(SDL_Renderer *r, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca)
{
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
}

static void fill_circle(SDL_Renderer *r, int cx, int cy, int radius)
{
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrt((double)(radius * radius - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void world_to_screen(double wx, double wy, int *sx, int *sy)
{
    *sx = (int)(MAP_SIZE / 2.0 + wx * PX_PER_M);
    *sy = (int)(MAP_SIZE / 2.0 - wy * PX_PER_M);
}

static void draw_map(SDL_Renderer *r)
{
    set_color(r, 30, 60, 40, 255);
    SDL_Rect bg = {0, 0, MAP_SIZE, MAP_SIZE};
    SDL_RenderFillRect(r, &bg);

    set_color(r, 45, 85, 58, 255);
    for (int m = (int)-DRONE_WORLD_HALF_SIZE; m <= (int)DRONE_WORLD_HALF_SIZE; m += 5) {
        int p = (int)(MAP_SIZE / 2.0 + m * PX_PER_M);
        SDL_RenderDrawLine(r, p, 0, p, MAP_SIZE);
        SDL_RenderDrawLine(r, 0, p, MAP_SIZE, p);
    }

    /* Landing pad at the origin */
    int cx, cy;
    world_to_screen(0, 0, &cx, &cy);
    set_color(r, 200, 200, 200, 255);
    fill_circle(r, cx, cy, 22);
    set_color(r, 30, 60, 40, 255);
    fill_circle(r, cx, cy, 18);
    set_color(r, 200, 200, 200, 255);
    SDL_Rect h1 = {cx - 8, cy - 9, 4, 18}, h2 = {cx + 4, cy - 9, 4, 18}, h3 = {cx - 4, cy - 2, 8, 4};
    SDL_RenderFillRect(r, &h1);
    SDL_RenderFillRect(r, &h2);
    SDL_RenderFillRect(r, &h3);
}

static void state_color(DroneState s, Uint8 *cr, Uint8 *cg, Uint8 *cb)
{
    switch (s) {
    case DRONE_LANDED:     *cr = 150; *cg = 150; *cb = 150; break;
    case DRONE_TAKING_OFF: *cr = 240; *cg = 200; *cb = 60;  break;
    case DRONE_FLYING:     *cr = 70;  *cg = 200; *cb = 255; break;
    case DRONE_LANDING:    *cr = 240; *cg = 150; *cb = 50;  break;
    case DRONE_EMERGENCY:  *cr = 240; *cg = 50;  *cb = 50;  break;
    }
}

static void draw_drone(SDL_Renderer *r, const Drone *d)
{
    int sx, sy;
    world_to_screen(d->x, d->y, &sx, &sy);

    /* Shadow on the ground, drifting away as altitude grows */
    int off = (int)(d->altitude * 3.0);
    set_color(r, 0, 0, 0, 90);
    fill_circle(r, sx + off, sy + off, 14);

    /* The drone looks bigger the higher it flies */
    double scale = 1.0 + d->altitude / DRONE_MAX_ALTITUDE;
    int arm = (int)(18 * scale);
    int rotor = (int)(8 * scale);
    double h = d->heading * PI / 180.0;

    Uint8 cr, cg, cb;
    state_color(d->state, &cr, &cg, &cb);

    for (int i = 0; i < 4; i++) {
        double a = h + PI / 4.0 + i * PI / 2.0;
        int rx = sx + (int)(sin(a) * arm);
        int ry = sy - (int)(cos(a) * arm);
        set_color(r, 220, 220, 220, 255);
        SDL_RenderDrawLine(r, sx, sy, rx, ry);
        set_color(r, cr, cg, cb, 255);
        fill_circle(r, rx, ry, rotor);
    }

    set_color(r, 40, 40, 40, 255);
    fill_circle(r, sx, sy, (int)(7 * scale));

    /* Heading indicator */
    set_color(r, 255, 60, 60, 255);
    SDL_RenderDrawLine(r, sx, sy,
                       sx + (int)(sin(h) * arm * 1.6),
                       sy - (int)(cos(h) * arm * 1.6));
}

static void draw_gauge(SDL_Renderer *r, int x, int y, int w, int h, double ratio,
                       Uint8 cr, Uint8 cg, Uint8 cb)
{
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;
    set_color(r, 60, 60, 70, 255);
    SDL_Rect frame = {x, y, w, h};
    SDL_RenderFillRect(r, &frame);
    int fill_h = (int)(h * ratio);
    set_color(r, cr, cg, cb, 255);
    SDL_Rect fill = {x, y + h - fill_h, w, fill_h};
    SDL_RenderFillRect(r, &fill);
    set_color(r, 200, 200, 210, 255);
    SDL_RenderDrawRect(r, &frame);
}

static void draw_panel(SDL_Renderer *r, const Drone *d)
{
    set_color(r, 25, 25, 32, 255);
    SDL_Rect bg = {PANEL_X, 0, WINDOW_WIDTH - PANEL_X, WINDOW_HEIGHT};
    SDL_RenderFillRect(r, &bg);

    /* State light */
    Uint8 cr, cg, cb;
    state_color(d->state, &cr, &cg, &cb);
    set_color(r, cr, cg, cb, 255);
    fill_circle(r, PANEL_X + 150, 60, 25);

    /* Altitude gauge (left) */
    draw_gauge(r, PANEL_X + 60, 130, 60, 380, d->altitude / DRONE_MAX_ALTITUDE, 70, 200, 255);

    /* Battery gauge (right): green -> red */
    double b = d->battery / 100.0;
    draw_gauge(r, PANEL_X + 180, 130, 60, 380, b,
               (Uint8)(255 * (1.0 - b)), (Uint8)(220 * b), 60);

    /* Low battery threshold mark */
    int mark = 130 + (int)(380 * (1.0 - DRONE_LOW_BATTERY / 100.0));
    set_color(r, 255, 80, 80, 255);
    SDL_RenderDrawLine(r, PANEL_X + 170, mark, PANEL_X + 250, mark);

    /* Compass */
    int ccx = PANEL_X + 150, ccy = 555;
    set_color(r, 80, 80, 90, 255);
    fill_circle(r, ccx, ccy, 28);
    double h = d->heading * PI / 180.0;
    set_color(r, 255, 60, 60, 255);
    SDL_RenderDrawLine(r, ccx, ccy, ccx + (int)(sin(h) * 25), ccy - (int)(cos(h) * 25));
}

void render_frame(SDL_Renderer *r, const Drone *d)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_color(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    draw_map(r);
    draw_drone(r, d);
    draw_panel(r, d);
    SDL_RenderPresent(r);
}
