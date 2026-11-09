#include "command.h"

#include <ctype.h>
#include <stddef.h>

static const char *NAMES[CMD_COUNT] = {
    [CMD_NONE]      = "NONE",
    [CMD_TAKEOFF]   = "TAKEOFF",
    [CMD_LAND]      = "LAND",
    [CMD_UP]        = "UP",
    [CMD_DOWN]      = "DOWN",
    [CMD_FORWARD]   = "FORWARD",
    [CMD_BACKWARD]  = "BACKWARD",
    [CMD_LEFT]      = "LEFT",
    [CMD_RIGHT]     = "RIGHT",
    [CMD_YAW_LEFT]  = "YAW_LEFT",
    [CMD_YAW_RIGHT] = "YAW_RIGHT",
    [CMD_HOVER]     = "HOVER",
    [CMD_EMERGENCY] = "EMERGENCY",
};

const char *command_name(Command cmd)
{
    if (cmd < 0 || cmd >= CMD_COUNT) {
        return "UNKNOWN";
    }
    return NAMES[cmd];
}

static int equals_ignore_case(const char *a, const char *b)
{
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

Command command_from_name(const char *name)
{
    if (name == NULL) {
        return CMD_NONE;
    }
    for (int i = 1; i < CMD_COUNT; i++) {
        if (equals_ignore_case(name, NAMES[i])) {
            return (Command)i;
        }
    }
    return CMD_NONE;
}
