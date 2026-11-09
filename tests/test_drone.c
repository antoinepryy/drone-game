#include <math.h>
#include <stdio.h>

#include "command.h"
#include "drone.h"
#include "script.h"

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

static void run(Drone *d, double seconds)
{
    for (double t = 0; t < seconds; t += 0.01) {
        drone_update(d, 0.01);
    }
}

static void test_takeoff_and_land(void)
{
    Drone d;
    drone_init(&d);
    CHECK(d.state == DRONE_LANDED);

    drone_apply(&d, CMD_TAKEOFF);
    CHECK(d.state == DRONE_TAKING_OFF);
    run(&d, 2.0);
    CHECK(d.state == DRONE_FLYING);
    CHECK(fabs(d.altitude - DRONE_TAKEOFF_ALTITUDE) < 1e-9);

    drone_apply(&d, CMD_LAND);
    run(&d, 2.0);
    CHECK(d.state == DRONE_LANDED);
    CHECK(d.altitude == 0.0);
}

static void test_no_move_when_landed(void)
{
    Drone d;
    drone_init(&d);
    for (int i = 0; i < 100; i++) {
        drone_apply(&d, CMD_FORWARD);
        drone_update(&d, 0.01);
    }
    CHECK(d.x == 0.0 && d.y == 0.0);
}

static void test_forward_goes_north(void)
{
    Drone d;
    drone_init(&d);
    drone_apply(&d, CMD_TAKEOFF);
    run(&d, 2.0);
    for (int i = 0; i < 100; i++) {
        drone_apply(&d, CMD_FORWARD);
        drone_update(&d, 0.01);
    }
    CHECK(d.y > 0.5);
    CHECK(fabs(d.x) < 1e-6);
}

static void test_emergency(void)
{
    Drone d;
    drone_init(&d);
    drone_apply(&d, CMD_TAKEOFF);
    run(&d, 2.0);
    drone_apply(&d, CMD_EMERGENCY);
    CHECK(d.state == DRONE_EMERGENCY);
    run(&d, 1.0);
    CHECK(d.state == DRONE_LANDED);
}

static void test_low_battery_auto_landing(void)
{
    Drone d;
    drone_init(&d);
    drone_apply(&d, CMD_TAKEOFF);
    run(&d, 2.0);
    d.battery = DRONE_LOW_BATTERY + 0.1;
    run(&d, 1.0);
    CHECK(d.state == DRONE_LANDING || d.state == DRONE_LANDED);

    drone_init(&d);
    d.battery = DRONE_LOW_BATTERY;
    drone_apply(&d, CMD_TAKEOFF);
    CHECK(d.state == DRONE_LANDED);   /* refuses to take off */
}

static void test_command_parsing(void)
{
    CHECK(command_from_name("takeoff") == CMD_TAKEOFF);
    CHECK(command_from_name("LAND") == CMD_LAND);
    CHECK(command_from_name("Yaw_Left") == CMD_YAW_LEFT);
    CHECK(command_from_name("fly_to_moon") == CMD_NONE);
    CHECK(command_from_name(NULL) == CMD_NONE);
}

static void test_script_parsing(void)
{
    ScriptStep st;
    char err[128];
    CHECK(script_parse_line("  # just a comment", 1, &st, err, sizeof(err)) == 0);
    CHECK(script_parse_line("", 1, &st, err, sizeof(err)) == 0);
    CHECK(script_parse_line("forward 2.5  # go", 3, &st, err, sizeof(err)) == 1);
    CHECK(st.cmd == CMD_FORWARD && st.duration == 2.5 && st.line == 3);
    CHECK(script_parse_line("WAIT", 1, &st, err, sizeof(err)) == 1);
    CHECK(st.cmd == CMD_NONE && st.duration == 1.0);
    CHECK(script_parse_line("TAKEOFF", 1, &st, err, sizeof(err)) == 1);
    CHECK(st.cmd == CMD_TAKEOFF && st.duration == 0.0);
    CHECK(script_parse_line("JUMP 2", 7, &st, err, sizeof(err)) == -1);
    CHECK(script_parse_line("UP -1", 1, &st, err, sizeof(err)) == -1);
    CHECK(script_parse_line("UP 2x", 1, &st, err, sizeof(err)) == -1);
    CHECK(script_parse_line("UP 1 2", 1, &st, err, sizeof(err)) == -1);

    Script s;
    CHECK(script_load_string(&s, "# empty\n\n", err, sizeof(err)) == -1);
    CHECK(script_load_string(&s, "TAKEOFF\nBOOM\n", err, sizeof(err)) == -1);
}

static void run_script(Script *s, Drone *d, double max_seconds)
{
    for (double t = 0; t < max_seconds && s->status == SCRIPT_RUNNING; t += 0.01) {
        script_update(s, d, 0.01);
        drone_update(d, 0.01);
    }
}

static void test_script_mission(void)
{
    static Script s;
    char err[128];
    Drone d;
    drone_init(&d);
    CHECK(script_load_string(&s, "TAKEOFF\nFORWARD 1\nHOVER\nLAND\n", err, sizeof(err)) == 0);
    CHECK(s.count == 4);
    script_start(&s);
    run_script(&s, &d, 30.0);
    CHECK(s.status == SCRIPT_DONE);
    CHECK(d.state == DRONE_LANDED);
    CHECK(d.y > 2.0);                 /* moved north */
}

static void test_script_aborts_on_emergency(void)
{
    static Script s;
    char err[128];
    Drone d;
    drone_init(&d);
    CHECK(script_load_string(&s, "TAKEOFF\nFORWARD 5\nLAND\n", err, sizeof(err)) == 0);
    script_start(&s);
    run_script(&s, &d, 2.0);
    drone_apply(&d, CMD_EMERGENCY);
    script_update(&s, &d, 0.01);
    CHECK(s.status == SCRIPT_ABORTED);
}

int main(void)
{
    test_takeoff_and_land();
    test_no_move_when_landed();
    test_forward_goes_north();
    test_emergency();
    test_low_battery_auto_landing();
    test_command_parsing();
    test_script_parsing();
    test_script_mission();
    test_script_aborts_on_emergency();

    if (failures == 0) {
        printf("All tests passed (v%s)\n", APP_VERSION);
        return 0;
    }
    printf("%d test(s) failed\n", failures);
    return 1;
}
