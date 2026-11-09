#include <math.h>
#include <stdio.h>

#include "command.h"
#include "drone.h"

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

int main(void)
{
    test_takeoff_and_land();
    test_no_move_when_landed();
    test_forward_goes_north();
    test_emergency();
    test_low_battery_auto_landing();
    test_command_parsing();

    if (failures == 0) {
        printf("All tests passed (v%s)\n", APP_VERSION);
        return 0;
    }
    printf("%d test(s) failed\n", failures);
    return 1;
}
