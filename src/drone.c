#include "drone.h"

#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846

#define MAX_SPEED      5.0    /* m/s */
#define ACCELERATION   3.0    /* how fast velocity reaches target, 1/s */
#define CLIMB_RATE     2.0    /* m/s */
#define FALL_RATE      6.0    /* m/s, emergency stop */
#define YAW_RATE       90.0   /* deg/s */
#define BATTERY_DRAIN  0.8    /* %/s while airborne */

static double clamp(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void drone_init(Drone *d)
{
    memset(d, 0, sizeof(*d));
    d->state = DRONE_LANDED;
    d->battery = 100.0;
}

void drone_apply(Drone *d, Command cmd)
{
    switch (cmd) {
    case CMD_TAKEOFF:
        if (d->state == DRONE_LANDED && d->battery > DRONE_LOW_BATTERY) {
            d->state = DRONE_TAKING_OFF;
        }
        return;
    case CMD_LAND:
        if (d->state == DRONE_FLYING || d->state == DRONE_TAKING_OFF) {
            d->state = DRONE_LANDING;
        }
        return;
    case CMD_EMERGENCY:
        if (d->state != DRONE_LANDED) {
            d->state = DRONE_EMERGENCY;
            d->vx = d->vy = 0.0;
        }
        return;
    default:
        break;
    }

    if (d->state != DRONE_FLYING) {
        return;
    }

    switch (cmd) {
    case CMD_FORWARD:   d->in_forward  += 1.0; break;
    case CMD_BACKWARD:  d->in_forward  -= 1.0; break;
    case CMD_RIGHT:     d->in_lateral  += 1.0; break;
    case CMD_LEFT:      d->in_lateral  -= 1.0; break;
    case CMD_UP:        d->in_vertical += 1.0; break;
    case CMD_DOWN:      d->in_vertical -= 1.0; break;
    case CMD_YAW_RIGHT: d->in_yaw      += 1.0; break;
    case CMD_YAW_LEFT:  d->in_yaw      -= 1.0; break;
    case CMD_HOVER:     d->vx = d->vy = 0.0;   break;
    default: break;
    }
}

static void reset_inputs(Drone *d)
{
    d->in_forward = d->in_lateral = d->in_vertical = d->in_yaw = 0.0;
}

static void update_flying(Drone *d, double dt)
{
    double fwd = clamp(d->in_forward, -1.0, 1.0);
    double lat = clamp(d->in_lateral, -1.0, 1.0);
    double ver = clamp(d->in_vertical, -1.0, 1.0);
    double yaw = clamp(d->in_yaw, -1.0, 1.0);

    d->heading = fmod(d->heading + yaw * YAW_RATE * dt + 360.0, 360.0);

    /* Body frame -> world frame (y axis points north/up on screen). */
    double h = d->heading * PI / 180.0;
    double target_vx = (fwd * sin(h) + lat * cos(h)) * MAX_SPEED;
    double target_vy = (fwd * cos(h) - lat * sin(h)) * MAX_SPEED;

    double k = clamp(ACCELERATION * dt, 0.0, 1.0);
    d->vx += (target_vx - d->vx) * k;
    d->vy += (target_vy - d->vy) * k;

    d->x = clamp(d->x + d->vx * dt, -DRONE_WORLD_HALF_SIZE, DRONE_WORLD_HALF_SIZE);
    d->y = clamp(d->y + d->vy * dt, -DRONE_WORLD_HALF_SIZE, DRONE_WORLD_HALF_SIZE);
    d->altitude = clamp(d->altitude + ver * CLIMB_RATE * dt,
                        DRONE_MIN_ALTITUDE, DRONE_MAX_ALTITUDE);
}

void drone_update(Drone *d, double dt)
{
    if (d->state != DRONE_LANDED) {
        d->battery = clamp(d->battery - BATTERY_DRAIN * dt, 0.0, 100.0);
    }

    switch (d->state) {
    case DRONE_LANDED:
        d->vx = d->vy = 0.0;
        break;

    case DRONE_TAKING_OFF:
        d->altitude += CLIMB_RATE * dt;
        if (d->altitude >= DRONE_TAKEOFF_ALTITUDE) {
            d->altitude = DRONE_TAKEOFF_ALTITUDE;
            d->state = DRONE_FLYING;
        }
        break;

    case DRONE_FLYING:
        update_flying(d, dt);
        if (d->battery <= DRONE_LOW_BATTERY) {
            d->state = DRONE_LANDING;   /* safety: auto-landing */
        }
        break;

    case DRONE_LANDING:
    case DRONE_EMERGENCY: {
        double rate = d->state == DRONE_EMERGENCY ? FALL_RATE : CLIMB_RATE;
        d->vx *= 0.9;
        d->vy *= 0.9;
        d->altitude -= rate * dt;
        if (d->altitude <= 0.0) {
            d->altitude = 0.0;
            d->state = DRONE_LANDED;
        }
        break;
    }
    }

    reset_inputs(d);
}

const char *drone_state_name(DroneState s)
{
    switch (s) {
    case DRONE_LANDED:     return "LANDED";
    case DRONE_TAKING_OFF: return "TAKING_OFF";
    case DRONE_FLYING:     return "FLYING";
    case DRONE_LANDING:    return "LANDING";
    case DRONE_EMERGENCY:  return "EMERGENCY";
    }
    return "UNKNOWN";
}
