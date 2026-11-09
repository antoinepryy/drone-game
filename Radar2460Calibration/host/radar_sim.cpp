// Simulated LD2460 radar: emits "targets" frames in the format the sketch parses.
//   F4 F3 F2 F1 | type 0x04 | len (u16 LE, whole frame) | N x (x_dm i16, y_dm i16) | F8 F7 F6 F5
// Env: SIM_FRAMES (default 10), SIM_PERIOD_MS (default 300), SIM_TARGETS (default 2, max 3).
#include "Arduino.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <vector>

HardwareSerial Serial(0);

namespace {
std::vector<uint8_t> pending;
size_t pos = 0;
int framesSent = 0;

int envInt(const char* name, int fallback) {
    const char* v = std::getenv(name);
    return v ? std::atoi(v) : fallback;
}

void put16(std::vector<uint8_t>& f, int16_t v) {
    f.push_back(static_cast<uint8_t>(v & 0xFF));
    f.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

std::vector<uint8_t> makeFrame(int n) {
    int targets = envInt("SIM_TARGETS", 2);
    if (targets < 1) targets = 1;
    if (targets > 3) targets = 3;

    std::vector<uint8_t> f = {0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0, 0};
    for (int t = 0; t < targets; ++t) {
        // Targets walking across the field of view; X can be negative (left of radar).
        double phase = n * 0.4 + t * 2.1;
        put16(f, static_cast<int16_t>(std::lround(25.0 * std::sin(phase))));        // X: -2.5 .. 2.5 m
        put16(f, static_cast<int16_t>(std::lround(30.0 + 15.0 * t + 10.0 * std::cos(phase)))); // Y: 2 .. 7 m
    }
    f.insert(f.end(), {0xF8, 0xF7, 0xF6, 0xF5});
    uint16_t len = static_cast<uint16_t>(f.size());
    f[5] = len & 0xFF;
    f[6] = len >> 8;
    return f;
}
} // namespace

void HardwareSerial::begin(unsigned long, uint32_t, int, int) {}

int HardwareSerial::available() {
    if (uart_ == 0) return 0;
    if (pos >= pending.size()) {
        if (framesSent >= envInt("SIM_FRAMES", 10)) {
            std::fflush(stdout);
            std::exit(0);  // simulation finished
        }
        if (framesSent > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(envInt("SIM_PERIOD_MS", 300)));
        pending = makeFrame(framesSent++);
        pos = 0;
    }
    return static_cast<int>(pending.size() - pos);
}

int HardwareSerial::read() {
    if (uart_ == 0 || !available()) return -1;
    return pending[pos++];
}
