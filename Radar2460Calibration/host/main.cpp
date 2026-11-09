// Native entry point: runs the Arduino sketch unchanged against the shim.
#include "Arduino.h"

#include "../Radar2460Calibration.ino"

int main() {
    setup();
    for (;;) loop();
}
