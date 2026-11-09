#ifndef COMMAND_H
#define COMMAND_H

typedef enum {
    CMD_NONE = 0,
    CMD_TAKEOFF,
    CMD_LAND,
    CMD_UP,
    CMD_DOWN,
    CMD_FORWARD,
    CMD_BACKWARD,
    CMD_LEFT,
    CMD_RIGHT,
    CMD_YAW_LEFT,
    CMD_YAW_RIGHT,
    CMD_HOVER,
    CMD_EMERGENCY,
    CMD_COUNT
} Command;

/* Human-readable name of a command ("TAKEOFF", "LAND"...). */
const char *command_name(Command cmd);

/* Parse a command name (case-insensitive). Returns CMD_NONE if unknown. */
Command command_from_name(const char *name);

#endif
