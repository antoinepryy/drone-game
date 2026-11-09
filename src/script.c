#include "script.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_one_shot(Command c)
{
    return c == CMD_TAKEOFF || c == CMD_LAND || c == CMD_HOVER || c == CMD_EMERGENCY;
}

const char *script_step_name(const ScriptStep *step)
{
    return step->cmd == CMD_NONE ? "WAIT" : command_name(step->cmd);
}

int script_parse_line(const char *line, int line_no, ScriptStep *out, char *err, size_t err_len)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", line);

    char *hash = strchr(buf, '#');
    if (hash) *hash = '\0';

    char *word = strtok(buf, " \t\r\n");
    if (!word) return 0;   /* blank or comment */
    char *arg = strtok(NULL, " \t\r\n");
    char *extra = strtok(NULL, " \t\r\n");

    Command cmd = CMD_NONE;
    int is_wait = 1;
    for (const char *a = word, *b = "WAIT"; *a || *b; a++, b++) {
        if (toupper((unsigned char)*a) != *b) { is_wait = 0; break; }
    }
    if (!is_wait) {
        cmd = command_from_name(word);
        if (cmd == CMD_NONE) {
            snprintf(err, err_len, "line %d: unknown command '%s'", line_no, word);
            return -1;
        }
    }
    if (extra) {
        snprintf(err, err_len, "line %d: unexpected '%s'", line_no, extra);
        return -1;
    }

    double duration = is_one_shot(cmd) ? 0.0 : 1.0;
    if (arg) {
        char *end;
        duration = strtod(arg, &end);
        if (*end != '\0' || duration <= 0.0 || duration > SCRIPT_MAX_DURATION) {
            snprintf(err, err_len, "line %d: invalid duration '%s'", line_no, arg);
            return -1;
        }
    }

    out->cmd = cmd;
    out->duration = duration;
    out->line = line_no;
    return 1;
}

int script_load_string(Script *s, const char *text, char *err, size_t err_len)
{
    memset(s, 0, sizeof(*s));
    int line_no = 0;
    const char *p = text;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        char line[256];
        if (len >= sizeof(line)) {
            snprintf(err, err_len, "line %d: too long", line_no + 1);
            return -1;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        line_no++;

        ScriptStep step;
        int rc = script_parse_line(line, line_no, &step, err, err_len);
        if (rc < 0) return -1;
        if (rc > 0) {
            if (s->count >= SCRIPT_MAX_STEPS) {
                snprintf(err, err_len, "too many steps (max %d)", SCRIPT_MAX_STEPS);
                return -1;
            }
            s->steps[s->count++] = step;
        }
        p += len + (eol ? 1 : 0);
    }
    if (s->count == 0) {
        snprintf(err, err_len, "script is empty");
        return -1;
    }
    return 0;
}

int script_load_file(Script *s, const char *path, char *err, size_t err_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, err_len, "cannot open '%s'", path);
        return -1;
    }
    static char text[64 * 1024];
    size_t n = fread(text, 1, sizeof(text) - 1, f);
    int too_big = !feof(f);
    fclose(f);
    if (too_big) {
        snprintf(err, err_len, "'%s' is too large", path);
        return -1;
    }
    text[n] = '\0';
    return script_load_string(s, text, err, err_len);
}

void script_start(Script *s)
{
    s->current = 0;
    s->elapsed = 0.0;
    s->step_started = 0;
    s->status = s->count > 0 ? SCRIPT_RUNNING : SCRIPT_DONE;
}

static int step_finished(const ScriptStep *st, const Drone *d, double elapsed)
{
    if (elapsed < st->duration) return 0;
    switch (st->cmd) {
    case CMD_TAKEOFF: return d->state != DRONE_TAKING_OFF;
    case CMD_LAND:    return d->state == DRONE_LANDED;
    default:          return 1;
    }
}

void script_update(Script *s, Drone *d, double dt)
{
    if (s->status != SCRIPT_RUNNING) return;

    if (d->state == DRONE_EMERGENCY) {
        s->status = SCRIPT_ABORTED;
        return;
    }

    const ScriptStep *st = &s->steps[s->current];
    if (!s->step_started) {
        s->step_started = 1;
        s->elapsed = 0.0;
        if (is_one_shot(st->cmd)) drone_apply(d, st->cmd);
    }
    if (!is_one_shot(st->cmd) && st->cmd != CMD_NONE) {
        drone_apply(d, st->cmd);
    }

    s->elapsed += dt;
    if (step_finished(st, d, s->elapsed)) {
        s->current++;
        s->step_started = 0;
        if (s->current >= s->count) {
            s->current = s->count - 1;
            s->status = SCRIPT_DONE;
        }
    }
}
