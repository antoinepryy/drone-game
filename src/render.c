#include "render.h"

#include <math.h>
#include <stdio.h>

#include "font.h"

#define PI 3.14159265358979323846f

#define MAP_SIZE   600.0f                   /* square map on the left */
#define PANEL_X    MAP_SIZE
#define PANEL_PAD  16.0f
#define PX_PER_M   (MAP_SIZE / (2.0f * (float)DRONE_WORLD_HALF_SIZE))

/* ---------------------------------------------------------------------- */
/* Palette                                                                 */
/* ---------------------------------------------------------------------- */

typedef struct { Uint8 r, g, b, a; } Rgba;

static Rgba rgba(Uint8 r, Uint8 g, Uint8 b, Uint8 a) { Rgba c = {r, g, b, a}; return c; }
static Rgba with_alpha(Rgba c, Uint8 a) { c.a = a; return c; }

#define COL_BG        rgba(9, 13, 20, 255)
#define COL_MAP       rgba(12, 19, 31, 255)
#define COL_PANEL     rgba(13, 18, 28, 255)
#define COL_CARD      rgba(20, 27, 40, 255)
#define COL_CARD_EDGE rgba(36, 47, 66, 255)
#define COL_TEXT      rgba(230, 236, 245, 255)
#define COL_DIM       rgba(120, 136, 160, 255)
#define COL_CYAN      rgba(61, 217, 235, 255)
#define COL_AMBER     rgba(245, 184, 65, 255)
#define COL_RED       rgba(255, 77, 94, 255)
#define COL_GREEN     rgba(74, 222, 128, 255)
#define COL_GREY      rgba(140, 150, 165, 255)

static Rgba state_color(DroneState s)
{
    switch (s) {
    case DRONE_LANDED:     return COL_GREY;
    case DRONE_TAKING_OFF: return COL_AMBER;
    case DRONE_FLYING:     return COL_CYAN;
    case DRONE_LANDING:    return COL_AMBER;
    case DRONE_EMERGENCY:  return COL_RED;
    }
    return COL_GREY;
}

static const char *state_label(DroneState s)
{
    switch (s) {
    case DRONE_LANDED:     return "AU SOL";
    case DRONE_TAKING_OFF: return "DECOLLAGE";
    case DRONE_FLYING:     return "EN VOL";
    case DRONE_LANDING:    return "ATTERRISSAGE";
    case DRONE_EMERGENCY:  return "URGENCE";
    }
    return "?";
}

/* ---------------------------------------------------------------------- */
/* Drawing primitives (SDL_RenderGeometry: sub-pixel, alpha, gradients)    */
/* ---------------------------------------------------------------------- */

#define MAX_SEGMENTS 96

static SDL_Vertex vtx(float x, float y, Rgba c)
{
    SDL_Vertex v;
    v.position.x = x;
    v.position.y = y;
    v.color.r = c.r;
    v.color.g = c.g;
    v.color.b = c.b;
    v.color.a = c.a;
    v.tex_coord.x = 0;
    v.tex_coord.y = 0;
    return v;
}

static void set_color(SDL_Renderer *r, Rgba c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

/* Filled disc; a transparent edge color gives a soft glow. */
static void disc(SDL_Renderer *r, float cx, float cy, float rad, Rgba center, Rgba edge)
{
    SDL_Vertex v[MAX_SEGMENTS + 2];
    int idx[MAX_SEGMENTS * 3];
    int n = rad < 12 ? 24 : (rad < 40 ? 48 : MAX_SEGMENTS);
    v[0] = vtx(cx, cy, center);
    for (int i = 0; i <= n; i++) {
        float a = 2.0f * PI * (float)i / (float)n;
        v[i + 1] = vtx(cx + cosf(a) * rad, cy + sinf(a) * rad, edge);
    }
    for (int i = 0; i < n; i++) {
        idx[i * 3] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = i + 2;
    }
    SDL_RenderGeometry(r, NULL, v, n + 2, idx, n * 3);
}

static void fill_disc(SDL_Renderer *r, float cx, float cy, float rad, Rgba c)
{
    disc(r, cx, cy, rad, c, c);
}

static void ring(SDL_Renderer *r, float cx, float cy, float rad, float width, Rgba c)
{
    SDL_Vertex v[(MAX_SEGMENTS + 1) * 2];
    int idx[MAX_SEGMENTS * 6];
    int n = MAX_SEGMENTS;
    float r0 = rad - width / 2, r1 = rad + width / 2;
    for (int i = 0; i <= n; i++) {
        float a = 2.0f * PI * (float)i / (float)n;
        v[i * 2] = vtx(cx + cosf(a) * r0, cy + sinf(a) * r0, c);
        v[i * 2 + 1] = vtx(cx + cosf(a) * r1, cy + sinf(a) * r1, c);
    }
    for (int i = 0; i < n; i++) {
        int b = i * 2;
        int *t = &idx[i * 6];
        t[0] = b; t[1] = b + 1; t[2] = b + 2;
        t[3] = b + 1; t[4] = b + 3; t[5] = b + 2;
    }
    SDL_RenderGeometry(r, NULL, v, (n + 1) * 2, idx, n * 6);
}

static void line(SDL_Renderer *r, float x1, float y1, float x2, float y2, float w, Rgba c1, Rgba c2)
{
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = -dy / len * w / 2, ny = dx / len * w / 2;
    SDL_Vertex v[4] = {
        vtx(x1 + nx, y1 + ny, c1), vtx(x1 - nx, y1 - ny, c1),
        vtx(x2 + nx, y2 + ny, c2), vtx(x2 - nx, y2 - ny, c2),
    };
    int idx[6] = {0, 1, 2, 1, 3, 2};
    SDL_RenderGeometry(r, NULL, v, 4, idx, 6);
}

static void solid_line(SDL_Renderer *r, float x1, float y1, float x2, float y2, float w, Rgba c)
{
    line(r, x1, y1, x2, y2, w, c, c);
}

static void rect(SDL_Renderer *r, float x, float y, float w, float h, Rgba c)
{
    SDL_FRect rc = {x, y, w, h};
    set_color(r, c);
    SDL_RenderFillRectF(r, &rc);
}

static void hgradient(SDL_Renderer *r, float x, float y, float w, float h, Rgba left, Rgba right)
{
    SDL_Vertex v[4] = {
        vtx(x, y, left), vtx(x + w, y, right),
        vtx(x, y + h, left), vtx(x + w, y + h, right),
    };
    int idx[6] = {0, 1, 2, 1, 3, 2};
    SDL_RenderGeometry(r, NULL, v, 4, idx, 6);
}

static void round_rect(SDL_Renderer *r, float x, float y, float w, float h, float rad, Rgba c)
{
    if (rad * 2 > h) rad = h / 2;
    if (rad * 2 > w) rad = w / 2;
    rect(r, x + rad, y, w - 2 * rad, h, c);
    rect(r, x, y + rad, rad, h - 2 * rad, c);
    rect(r, x + w - rad, y + rad, rad, h - 2 * rad, c);
    /* Corners: discs clipped to their quarter so alpha never overlaps */
    SDL_Rect clip;
    int had_clip = SDL_RenderIsClipEnabled(r);
    SDL_RenderGetClipRect(r, &clip);
    float cxs[4] = {x + rad, x + w - rad, x + rad, x + w - rad};
    float cys[4] = {y + rad, y + rad, y + h - rad, y + h - rad};
    float qx[4] = {x, x + w - rad, x, x + w - rad};
    float qy[4] = {y, y, y + h - rad, y + h - rad};
    for (int i = 0; i < 4; i++) {
        SDL_Rect q = {(int)floorf(qx[i]), (int)floorf(qy[i]), (int)ceilf(rad), (int)ceilf(rad)};
        SDL_RenderSetClipRect(r, &q);
        fill_disc(r, cxs[i], cys[i], rad, c);
    }
    SDL_RenderSetClipRect(r, had_clip ? &clip : NULL);
}

/* Card = rounded panel with a 1 px border. */
static void card(SDL_Renderer *r, float x, float y, float w, float h, Rgba border)
{
    round_rect(r, x, y, w, h, 10, border);
    round_rect(r, x + 1, y + 1, w - 2, h - 2, 9, COL_CARD);
}

static void text(SDL_Renderer *r, float x, float y, float scale, Rgba c, const char *s)
{
    set_color(r, c);
    font_draw(r, x, y, scale, s);
}

static void text_right(SDL_Renderer *r, float right, float y, float scale, Rgba c, const char *s)
{
    text(r, right - font_width(s, scale), y, scale, c, s);
}

static void text_center(SDL_Renderer *r, float cx, float y, float scale, Rgba c, const char *s)
{
    text(r, cx - font_width(s, scale) / 2, y, scale, c, s);
}

/* ---------------------------------------------------------------------- */
/* View (animations, trail)                                                */
/* ---------------------------------------------------------------------- */

void view_init(View *v)
{
    SDL_memset(v, 0, sizeof(*v));
}

void view_update(View *v, const Drone *d, double dt)
{
    v->time += dt;

    double target = d->state == DRONE_LANDED ? 0.0 : 40.0;
    v->rotor_speed += (target - v->rotor_speed) * (dt * 2.0 > 1.0 ? 1.0 : dt * 2.0);
    v->rotor_angle = fmod(v->rotor_angle + v->rotor_speed * dt, 2.0 * PI);

    v->trail_timer += dt;
    if (d->state != DRONE_LANDED && v->trail_timer > 0.05) {
        v->trail_timer = 0;
        v->trail_x[v->trail_head] = (float)d->x;
        v->trail_y[v->trail_head] = (float)d->y;
        v->trail_head = (v->trail_head + 1) % TRAIL_LENGTH;
        if (v->trail_count < TRAIL_LENGTH) v->trail_count++;
    }
}

/* ---------------------------------------------------------------------- */
/* Map                                                                     */
/* ---------------------------------------------------------------------- */

static void world_to_screen(float wx, float wy, float *sx, float *sy)
{
    *sx = MAP_SIZE / 2 + wx * PX_PER_M;
    *sy = MAP_SIZE / 2 - wy * PX_PER_M;
}

static void draw_map(SDL_Renderer *r, const Drone *d, const View *v)
{
    float c = MAP_SIZE / 2;
    rect(r, 0, 0, MAP_SIZE, MAP_SIZE, COL_MAP);
    disc(r, c, c, 430, rgba(22, 38, 62, 255), with_alpha(COL_MAP, 0));

    /* Grid: 1 m minor, 5 m major */
    for (int m = -20; m <= 20; m++) {
        float p = c + (float)m * PX_PER_M;
        Rgba g = (m % 5 == 0) ? rgba(120, 160, 220, 28) : rgba(120, 160, 220, 10);
        solid_line(r, p, 0, p, MAP_SIZE, 1, g);
        solid_line(r, 0, p, MAP_SIZE, p, 1, g);
    }

    /* Radar rings around the base + slow sweep */
    for (int m = 5; m <= 20; m += 5) {
        ring(r, c, c, (float)m * PX_PER_M, 1, with_alpha(COL_CYAN, 30));
    }
    float sweep = (float)fmod(v->time * 0.8, 2.0 * PI);
    for (int i = 0; i < 24; i++) {
        float a = sweep - (float)i * 0.025f;
        Uint8 alpha = (Uint8)(50 - i * 2);
        solid_line(r, c, c, c + cosf(a) * 300, c + sinf(a) * 300, 2, with_alpha(COL_CYAN, alpha));
    }

    /* Landing pad */
    float px, py;
    world_to_screen(0, 0, &px, &py);
    fill_disc(r, px, py, 26, rgba(16, 24, 38, 255));
    ring(r, px, py, 26, 2, with_alpha(COL_AMBER, 220));
    ring(r, px, py, 20, 1, with_alpha(COL_AMBER, 90));
    text_center(r, px, py - 7, 2, COL_AMBER, "H");

    /* Trail */
    if (v->trail_count > 1) {
        int start = (v->trail_head - v->trail_count + TRAIL_LENGTH) % TRAIL_LENGTH;
        for (int i = 1; i < v->trail_count; i++) {
            int a = (start + i - 1) % TRAIL_LENGTH, b = (start + i) % TRAIL_LENGTH;
            float x1, y1, x2, y2;
            world_to_screen(v->trail_x[a], v->trail_y[a], &x1, &y1);
            world_to_screen(v->trail_x[b], v->trail_y[b], &x2, &y2);
            Uint8 alpha = (Uint8)(150.0f * (float)i / (float)v->trail_count);
            solid_line(r, x1, y1, x2, y2, 2, with_alpha(COL_CYAN, alpha));
        }
    }

    /* Corner brackets */
    Rgba br = with_alpha(COL_CYAN, 140);
    float m = 12, l = 26;
    solid_line(r, m, m, m + l, m, 2, br);                       solid_line(r, m, m, m, m + l, 2, br);
    solid_line(r, MAP_SIZE - m, m, MAP_SIZE - m - l, m, 2, br); solid_line(r, MAP_SIZE - m, m, MAP_SIZE - m, m + l, 2, br);
    solid_line(r, m, MAP_SIZE - m, m + l, MAP_SIZE - m, 2, br); solid_line(r, m, MAP_SIZE - m, m, MAP_SIZE - m - l, 2, br);
    solid_line(r, MAP_SIZE - m, MAP_SIZE - m, MAP_SIZE - m - l, MAP_SIZE - m, 2, br);
    solid_line(r, MAP_SIZE - m, MAP_SIZE - m, MAP_SIZE - m, MAP_SIZE - m - l, 2, br);

    text(r, 24, 24, 1, COL_DIM, "ZONE DE VOL  40 X 40 M");
    text_right(r, MAP_SIZE - 24, 24, 1, COL_DIM, "N ^");

    /* Scale bar: 5 m */
    float sb = 5 * PX_PER_M;
    solid_line(r, 24, MAP_SIZE - 28, 24 + sb, MAP_SIZE - 28, 2, COL_DIM);
    solid_line(r, 24, MAP_SIZE - 33, 24, MAP_SIZE - 23, 2, COL_DIM);
    solid_line(r, 24 + sb, MAP_SIZE - 33, 24 + sb, MAP_SIZE - 23, 2, COL_DIM);
    text(r, 30 + sb, MAP_SIZE - 31, 1, COL_DIM, "5 M");

    /* Emergency: flashing red frame */
    if (d->state == DRONE_EMERGENCY && fmod(v->time, 0.5) < 0.25) {
        Rgba red = with_alpha(COL_RED, 200);
        rect(r, 0, 0, MAP_SIZE, 4, red);
        rect(r, 0, MAP_SIZE - 4, MAP_SIZE, 4, red);
        rect(r, 0, 0, 4, MAP_SIZE, red);
        rect(r, MAP_SIZE - 4, 0, 4, MAP_SIZE, red);
    }
}

static void draw_drone(SDL_Renderer *r, const Drone *d, const View *v)
{
    float sx, sy;
    world_to_screen((float)d->x, (float)d->y, &sx, &sy);
    float alt = (float)d->altitude;
    float scale = 1.0f + alt / (float)DRONE_MAX_ALTITUDE * 0.6f;
    float h = (float)d->heading * PI / 180.0f;
    Rgba sc = state_color(d->state);

    /* Shadow drifts and softens with altitude */
    float off = fminf(alt * 2.5f, 40.0f);
    disc(r, sx + off, sy + off, 26 + alt, rgba(0, 0, 0, (Uint8)(150 - fminf(alt * 3, 90))), rgba(0, 0, 0, 0));

    /* State glow */
    disc(r, sx, sy, 52 * scale, with_alpha(sc, 60), with_alpha(sc, 0));

    float arm = 20 * scale, rotor = 11 * scale;
    for (int i = 0; i < 4; i++) {
        float a = h + PI / 4 + (float)i * PI / 2;
        float rx = sx + sinf(a) * arm, ry = sy - cosf(a) * arm;
        solid_line(r, sx, sy, rx, ry, 4 * scale, rgba(52, 63, 82, 255));

        fill_disc(r, rx, ry, rotor, rgba(255, 255, 255, 18));
        ring(r, rx, ry, rotor, 1, rgba(255, 255, 255, 60));
        fill_disc(r, rx, ry, 2.5f * scale, rgba(90, 100, 120, 255));

        /* Propeller blades, alternate spin direction */
        float spin = (float)v->rotor_angle * (i % 2 ? -1.0f : 1.0f) + (float)i;
        float bl = rotor - 1;
        Rgba blade = v->rotor_speed > 20 ? rgba(220, 230, 240, 110) : rgba(220, 230, 240, 230);
        solid_line(r, rx - cosf(spin) * bl, ry - sinf(spin) * bl,
                   rx + cosf(spin) * bl, ry + sinf(spin) * bl, 2 * scale, blade);

        /* Navigation lights: front = cyan, rear = red */
        int front = (i == 0 || i == 3);
        fill_disc(r, rx, ry, 1.6f * scale, front ? COL_CYAN : COL_RED);
    }

    /* Body */
    fill_disc(r, sx, sy, 10 * scale, rgba(30, 38, 52, 255));
    ring(r, sx, sy, 10 * scale, 2, sc);
    fill_disc(r, sx, sy, 3 * scale, sc);

    /* Heading chevron */
    float tip = arm * 2.0f, base = arm * 1.5f, wing = 0.22f;
    SDL_Vertex t[3] = {
        vtx(sx + sinf(h) * tip, sy - cosf(h) * tip, sc),
        vtx(sx + sinf(h - wing) * base, sy - cosf(h - wing) * base, with_alpha(sc, 120)),
        vtx(sx + sinf(h + wing) * base, sy - cosf(h + wing) * base, with_alpha(sc, 120)),
    };
    SDL_RenderGeometry(r, NULL, t, 3, NULL, 0);

    /* Altitude tag next to the drone */
    if (d->state != DRONE_LANDED) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f M", d->altitude);
        float tw = font_width(buf, 1) + 10;
        float tx = sx + 28 * scale, ty = sy - 28 * scale;
        round_rect(r, tx, ty, tw, 15, 4, rgba(9, 13, 20, 200));
        text(r, tx + 5, ty + 4, 1, COL_TEXT, buf);
    }
}

/* ---------------------------------------------------------------------- */
/* Side panel                                                              */
/* ---------------------------------------------------------------------- */

static void progress(SDL_Renderer *r, float x, float y, float w, float ratio, Rgba from, Rgba to)
{
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;
    round_rect(r, x, y, w, 8, 4, rgba(255, 255, 255, 14));
    if (ratio > 0.01f) {
        float fw = w * ratio;
        hgradient(r, x + 2, y + 1, fw - 4 > 0 ? fw - 4 : 0, 6, from, to);
        fill_disc(r, x + 4, y + 4, 4, from);
        fill_disc(r, x + fw - 4, y + 4, 4, to);
    }
}

static void big_value(SDL_Renderer *r, float x, float y, const char *value, const char *unit)
{
    text(r, x, y, 4, COL_TEXT, value);
    text(r, x + font_width(value, 4) + 6, y + 14, 2, COL_DIM, unit);
}

static void draw_header(SDL_Renderer *r, const Drone *d, const View *v, float x, float w)
{
    text(r, x, 22, 2, COL_TEXT, "DRONE PILOT");
    text(r, x, 44, 1, COL_DIM, "V" APP_VERSION "  -  SIMULATEUR");

    const char *lbl = state_label(d->state);
    Rgba sc = state_color(d->state);
    float pw = font_width(lbl, 1) + 30;
    float px = x + w - pw;
    round_rect(r, px, 26, pw, 22, 11, with_alpha(sc, 40));
    float pulse = 0.5f + 0.5f * sinf((float)v->time * 5);
    Uint8 la = d->state == DRONE_LANDED ? 255 : (Uint8)(120 + 135 * pulse);
    fill_disc(r, px + 12, 37, 3.5f, with_alpha(sc, la));
    text(r, px + 20, 34, 1, sc, lbl);
}

static void draw_altitude(SDL_Renderer *r, const Drone *d, float x, float y, float w)
{
    card(r, x, y, w, 88, COL_CARD_EDGE);
    text(r, x + 14, y + 14, 1, COL_DIM, "ALTITUDE");
    text_right(r, x + w - 14, y + 14, 1, COL_DIM, "MAX 30 M");
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", d->altitude);
    big_value(r, x + 14, y + 30, buf, "M");
    progress(r, x + 14, y + 68, w - 28, (float)(d->altitude / DRONE_MAX_ALTITUDE),
             rgba(40, 120, 200, 255), COL_CYAN);
}

static void draw_battery(SDL_Renderer *r, const Drone *d, const View *v, float x, float y, float w)
{
    float b = (float)(d->battery / 100.0);
    int low = d->battery < 20.0;
    Rgba edge = (low && fmod(v->time, 0.8) < 0.4) ? COL_RED : COL_CARD_EDGE;
    card(r, x, y, w, 88, edge);
    text(r, x + 14, y + 14, 1, COL_DIM, "BATTERIE");
    text_right(r, x + w - 14, y + 14, 1, low ? COL_RED : COL_DIM, low ? "FAIBLE" : "OK");

    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", d->battery);
    big_value(r, x + 14, y + 30, buf, "%");

    Rgba tail = b > 0.5f ? COL_GREEN : (b > 0.2f ? COL_AMBER : COL_RED);
    progress(r, x + 14, y + 68, w - 28, b, with_alpha(tail, 160), tail);
    float mark = x + 14 + (w - 28) * (float)(DRONE_LOW_BATTERY / 100.0);
    solid_line(r, mark, y + 64, mark, y + 80, 1, with_alpha(COL_RED, 200));
}

static void draw_navigation(SDL_Renderer *r, const Drone *d, float x, float y, float w)
{
    card(r, x, y, w, 132, COL_CARD_EDGE);
    text(r, x + 14, y + 14, 1, COL_DIM, "CAP");

    /* Compass */
    float cx = x + 66, cy = y + 74, rad = 44;
    fill_disc(r, cx, cy, rad, rgba(12, 18, 28, 255));
    ring(r, cx, cy, rad, 1.5f, COL_CARD_EDGE);
    for (int deg = 0; deg < 360; deg += 30) {
        float a = (float)deg * PI / 180;
        float r0 = deg % 90 == 0 ? rad - 9 : rad - 5;
        solid_line(r, cx + sinf(a) * r0, cy - cosf(a) * r0,
                   cx + sinf(a) * (rad - 2), cy - cosf(a) * (rad - 2), 1.5f, COL_DIM);
    }
    text_center(r, cx, cy - rad + 12, 1, COL_RED, "N");
    text_center(r, cx, cy + rad - 19, 1, COL_DIM, "S");
    text(r, cx + rad - 20, cy - 3, 1, COL_DIM, "E");
    text(r, cx - rad + 15, cy - 3, 1, COL_DIM, "O");

    float h = (float)d->heading * PI / 180;
    SDL_Vertex needle[3] = {
        vtx(cx + sinf(h) * (rad - 14), cy - cosf(h) * (rad - 14), COL_RED),
        vtx(cx + sinf(h + 2.6f) * 8, cy - cosf(h + 2.6f) * 8, rgba(150, 40, 50, 255)),
        vtx(cx + sinf(h - 2.6f) * 8, cy - cosf(h - 2.6f) * 8, rgba(150, 40, 50, 255)),
    };
    SDL_RenderGeometry(r, NULL, needle, 3, NULL, 0);
    fill_disc(r, cx, cy, 3, COL_TEXT);

    /* Numbers on the right */
    float tx = x + 132;
    char buf[32];
    snprintf(buf, sizeof(buf), "%03.0f", d->heading);
    text(r, tx, y + 32, 2, COL_TEXT, buf);
    text(r, tx + font_width(buf, 2) + 4, y + 32, 1, COL_DIM, "DEG");

    text(r, tx, y + 58, 1, COL_DIM, "VITESSE");
    snprintf(buf, sizeof(buf), "%.1f M/S", sqrt(d->vx * d->vx + d->vy * d->vy));
    text(r, tx, y + 70, 1, COL_TEXT, buf);

    text(r, tx, y + 90, 1, COL_DIM, "POSITION");
    snprintf(buf, sizeof(buf), "X %+.1f", d->x);
    text(r, tx, y + 102, 1, COL_TEXT, buf);
    snprintf(buf, sizeof(buf), "Y %+.1f", d->y);
    text(r, tx + 60, y + 102, 1, COL_TEXT, buf);
}

static void keycap(SDL_Renderer *r, float x, float y, const char *key, const char *label)
{
    float kw = font_width(key, 1) + 12;
    if (kw < 18) kw = 18;
    round_rect(r, x, y, kw, 16, 4, rgba(52, 63, 82, 255));
    round_rect(r, x, y, kw, 14, 4, rgba(70, 84, 108, 255));
    text_center(r, x + kw / 2, y + 4, 1, COL_TEXT, key);
    text(r, x + kw + 7, y + 5, 1, COL_DIM, label);
}

static void draw_controls(SDL_Renderer *r, float x, float y, float w)
{
    card(r, x, y, w, WINDOW_HEIGHT - y - PANEL_PAD, COL_CARD_EDGE);
    text(r, x + 14, y + 14, 1, COL_DIM, "COMMANDES");
    static const char *keys[][2] = {
        {"T", "DECOLLER"},  {"L", "ATTERRIR"},
        {"ZQSD", "BOUGER"}, {"A E", "PIVOTER"},
        {"R F", "MONTER"},  {"H", "STOP"},
        {"ESP", "URGENCE"}, {"ESC", "QUITTER"},
    };
    for (int i = 0; i < 8; i++) {
        float kx = x + 14 + (float)(i % 2) * (w / 2 - 4);
        float ky = y + 32 + (float)(i / 2) * 22;
        keycap(r, kx, ky, keys[i][0], keys[i][1]);
    }
}

static void draw_panel(SDL_Renderer *r, const Drone *d, const View *v)
{
    rect(r, PANEL_X, 0, WINDOW_WIDTH - PANEL_X, WINDOW_HEIGHT, COL_PANEL);
    solid_line(r, PANEL_X + 0.5f, 0, PANEL_X + 0.5f, WINDOW_HEIGHT, 1, COL_CARD_EDGE);

    float x = PANEL_X + PANEL_PAD, w = WINDOW_WIDTH - PANEL_X - 2 * PANEL_PAD;
    draw_header(r, d, v, x, w);
    draw_altitude(r, d, x, 66, w);
    draw_battery(r, d, v, x, 164, w);
    draw_navigation(r, d, x, 262, w);
    draw_controls(r, x, 404, w);
}

void render_frame(SDL_Renderer *r, const Drone *d, const View *v)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_color(r, COL_BG);
    SDL_RenderClear(r);
    draw_map(r, d, v);
    draw_drone(r, d, v);
    draw_panel(r, d, v);
    SDL_RenderPresent(r);
}
