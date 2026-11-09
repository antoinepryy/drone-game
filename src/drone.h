#ifndef DRONE_H
#define DRONE_H

#include "command.h"

#define DRONE_WORLD_HALF_SIZE 20.0   /* meters, square flight area */
#define DRONE_TAKEOFF_ALTITUDE 1.5   /* meters */
#define DRONE_MIN_ALTITUDE 0.5
#define DRONE_MAX_ALTITUDE 30.0
#define DRONE_LOW_BATTERY 10.0       /* percent, triggers auto-landing */

typedef enum {
    DRONE_LANDED,
    DRONE_TAKING_OFF,
    DRONE_FLYING,
    DRONE_LANDING,
    DRONE_EMERGENCY
} DroneState;

typedef struct {
    DroneState state;
    double x, y;          /* position in meters, origin at center */
    double altitude;      /* meters */
    double heading;       /* degrees, 0 = north, clockwise */
    double vx, vy;        /* horizontal velocity, m/s */
    double battery;       /* percent, 0..100 */

    /* Pilot input accumulated for the current frame, each in [-1, 1]. */
    double in_forward, in_lateral, in_vertical, in_yaw;
} Drone;

void drone_init(Drone *d);

/* Apply a pilot command. Movement commands only act while flying. */
void drone_apply(Drone *d, Command cmd);

/* Advance the simulation by dt seconds. */
void drone_update(Drone *d, double dt);

const char *drone_state_name(DroneState s);

#endif
