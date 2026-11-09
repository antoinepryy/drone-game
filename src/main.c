#include <stdio.h>

#include <SDL.h>

#include "command.h"
#include "drone.h"
#include "render.h"

static void print_help(void)
{
    printf("Drone Pilot v%s\n", APP_VERSION);
    printf("  T          takeoff\n");
    printf("  L          land\n");
    printf("  Z/S        forward / backward   (arrows Up/Down also work)\n");
    printf("  Q/D        left / right         (arrows Left/Right also work)\n");
    printf("  A/E        yaw left / yaw right\n");
    printf("  R/F        up / down\n");
    printf("  H          hover (stop)\n");
    printf("  SPACE      EMERGENCY stop\n");
    printf("  ESC        quit\n");
}

/* One-shot commands, triggered on key press. Uses physical key positions
 * (scancodes) so the layout works the same on AZERTY and QWERTY. */
static Command command_from_scancode(SDL_Scancode sc)
{
    switch (sc) {
    case SDL_SCANCODE_T:     return CMD_TAKEOFF;
    case SDL_SCANCODE_L:     return CMD_LAND;
    case SDL_SCANCODE_H:     return CMD_HOVER;
    case SDL_SCANCODE_SPACE: return CMD_EMERGENCY;
    default:                 return CMD_NONE;
    }
}

/* Continuous commands, applied every frame while the key is held.
 * Scancodes are named after QWERTY positions: SDL_SCANCODE_W is the AZERTY Z key. */
static void apply_held_keys(Drone *d)
{
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    if (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP])    drone_apply(d, CMD_FORWARD);
    if (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN])  drone_apply(d, CMD_BACKWARD);
    if (k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT])  drone_apply(d, CMD_LEFT);
    if (k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) drone_apply(d, CMD_RIGHT);
    if (k[SDL_SCANCODE_Q]) drone_apply(d, CMD_YAW_LEFT);
    if (k[SDL_SCANCODE_E]) drone_apply(d, CMD_YAW_RIGHT);
    if (k[SDL_SCANCODE_R]) drone_apply(d, CMD_UP);
    if (k[SDL_SCANCODE_F]) drone_apply(d, CMD_DOWN);
}

static void update_title(SDL_Window *w, const Drone *d)
{
    char title[160];
    snprintf(title, sizeof(title),
             "Drone Pilot v%s | %s | alt %.1f m | heading %3.0f deg | battery %.0f%%",
             APP_VERSION, drone_state_name(d->state), d->altitude, d->heading, d->battery);
    SDL_SetWindowTitle(w, title);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Drone Pilot", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                          WINDOW_HEIGHT, 0);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : NULL;
    if (!window || !renderer) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    print_help();

    Drone drone;
    drone_init(&drone);
    DroneState last_state = drone.state;

    Uint64 last = SDL_GetPerformanceCounter();
    double title_timer = 0.0;
    int running = 1;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    running = 0;
                    continue;
                }
                Command cmd = command_from_scancode(e.key.keysym.scancode);
                if (cmd != CMD_NONE) {
                    printf("> %s\n", command_name(cmd));
                    drone_apply(&drone, cmd);
                }
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / (double)SDL_GetPerformanceFrequency();
        last = now;
        if (dt > 0.1) dt = 0.1;   /* avoid big jumps after a pause */

        apply_held_keys(&drone);
        drone_update(&drone, dt);

        if (drone.state != last_state) {
            printf("  state: %s -> %s\n", drone_state_name(last_state),
                   drone_state_name(drone.state));
            last_state = drone.state;
        }

        title_timer += dt;
        if (title_timer > 0.2) {
            update_title(window, &drone);
            title_timer = 0.0;
        }

        render_frame(renderer, &drone);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
