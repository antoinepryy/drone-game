// Native entry point: runs the Arduino sketch unchanged against the shim.
#include "Arduino.h"

#include <cstdint>

// The Arduino IDE generates prototypes automatically; a plain C++ build needs them.
void processFrame(uint8_t* buf, uint16_t len);

#include "../Radar2460Calibration.ino"

int main() {
    setup();
    for (;;) loop();
}
