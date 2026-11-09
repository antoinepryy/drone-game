#ifndef SCRIPT_H
#define SCRIPT_H

#include <stddef.h>

#include "command.h"
#include "drone.h"

/* A command script (mission) is a text file, one step per line:
 *
 *     # comment
 *     TAKEOFF
 *     FORWARD 2.5      <- command + optional duration in seconds
 *     WAIT 1
 *     LAND
 *
 * Movement commands are held for their duration (default 1 s).
 * TAKEOFF waits until the drone is flying, LAND until it is on the ground. */

#define SCRIPT_MAX_STEPS 256
#define SCRIPT_MAX_DURATION 600.0

typedef struct {
    Command cmd;        /* CMD_NONE = WAIT */
    double duration;    /* seconds */
    int line;           /* line number in the source file */
} ScriptStep;

typedef enum {
    SCRIPT_IDLE,        /* loaded, not started */
    SCRIPT_RUNNING,
    SCRIPT_DONE,
    SCRIPT_ABORTED      /* stopped by an emergency */
} ScriptStatus;

typedef struct {
    ScriptStep steps[SCRIPT_MAX_STEPS];
    int count;
    int current;
    double elapsed;     /* time spent in the current step */
    int step_started;
    ScriptStatus status;
} Script;

/* Parse one line. Returns 1 if a step was read, 0 for a blank/comment line,
 * -1 on error (message written to err). */
int script_parse_line(const char *line, int line_no, ScriptStep *out, char *err, size_t err_len);

/* Load a whole script from a string or a file. Returns 0 on success. */
int script_load_string(Script *s, const char *text, char *err, size_t err_len);
int script_load_file(Script *s, const char *path, char *err, size_t err_len);

void script_start(Script *s);

/* Drive the drone: call once per frame, before drone_update(). */
void script_update(Script *s, Drone *d, double dt);

/* Name of a step's command, "WAIT" for CMD_NONE. */
const char *script_step_name(const ScriptStep *step);

#endif
