// Simulated LD2460 radar: emits "targets" frames in the format the sketch parses.
//   F4 F3 F2 F1 | type 0x04 | len (u16 LE, whole frame) | N x (x_dm i16, y_dm i16) | F8 F7 F6 F5
// Env: SIM_FRAMES (default 10, 0 = endless), SIM_PERIOD_MS (default 300),
//      SIM_TARGETS (default 2, max 3, or 8 with SIM_SCENARIO=walk), SIM_FAULTS=1 to
//      interleave malformed frames (see makeFaultyFrame), SIM_SCENARIO=walk for people
//      moving in and out of the field (see walkTargets) instead of the default
//      oscillating targets, SIM_SPEED (default 1) to speed walkers up or slow them down.
#include "Arduino.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
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

struct Target { int16_t x_dm, y_dm; };

// Default: targets oscillating in front of the radar; X can be negative (left of radar).
std::vector<Target> oscillatingTargets(int n, int targets) {
    std::vector<Target> out;
    for (int t = 0; t < targets; ++t) {
        double phase = n * 0.4 + t * 2.1;
        out.push_back({static_cast<int16_t>(std::lround(25.0 * std::sin(phase))),                    // X: -2.5 .. 2.5 m
                       static_cast<int16_t>(std::lround(30.0 + 15.0 * t + 10.0 * std::cos(phase)))}); // Y: 2 .. 7 m
    }
    return out;
}

// SIM_SCENARIO=walk: up to 8 people on a 60 s loop (at the nominal 10 frames/s,
// scaled by SIM_SPEED). Each person follows waypoints (time s, x m, y m) and is only
// reported between their first and last waypoint, so the target count varies.
// From 44 s to 65 s (5 s into the next loop) nobody is on the X > 0 side, so a
// monitored area covering X > 0 can stay clear for more than 10 s.
struct Waypoint { double t, x, y; };
struct Walker { std::vector<Waypoint> path; };

const std::vector<Walker>& walkers() {
    static const std::vector<Walker> all = {
        {{{0, -4.0, 3.0}, {8, 1.5, 3.0}, {12, 1.5, 3.2}, {18, -1.0, 5.0}, {24, -4.0, 5.0}}},  // walks in, stops at 1.5 m right, leaves
        {{{0, -3.0, 6.0}, {10, -1.5, 7.0}, {20, -2.5, 4.0}, {30, -1.0, 2.5},                  // wanders on the left side all the time
          {40, -3.0, 3.5}, {55, -2.0, 6.0}, {60, -3.0, 6.0}}},
        {{{5, 4.0, 6.0}, {20, -4.0, 6.5}}},                                                    // crosses right to left, far away
        {{{10, 2.0, 8.0}, {18, 0.8, 1.5}, {22, 0.8, 1.5}, {32, 3.0, 8.0}}},                    // comes close to the radar on the right
        {{{15, -3.0, 4.0}, {19, 1.0, 4.5}, {23, -1.0, 5.0}, {27, 1.5, 5.5},                   // zigzags across X = 0
          {31, -2.0, 6.0}, {36, -4.0, 6.0}}},
        {{{0, -1.5, 1.8}, {60, -1.5, 1.8}}},                                                   // stands still, left, close
        {{{25, -4.0, 2.0}, {31, 4.0, 3.0}}},                                                   // runs across, left to right
        {{{30, 3.0, 4.0}, {36, 2.0, 2.5}, {42, 3.5, 3.0}, {44, 4.0, 3.0}}},                    // lingers on the right
    };
    return all;
}

std::vector<Target> walkTargets(int n, int targets) {
    const char* speedEnv = std::getenv("SIM_SPEED");
    double speed = speedEnv ? std::atof(speedEnv) : 1.0;
    if (speed <= 0) speed = 1.0;
    double t = std::fmod(n * 0.1 * speed, 60.0);

    std::vector<Target> out;
    for (int i = 0; i < targets && i < static_cast<int>(walkers().size()); ++i) {
        const std::vector<Waypoint>& path = walkers()[i].path;
        if (t < path.front().t || t >= path.back().t) continue;
        size_t k = 1;
        while (path[k].t <= t) ++k;
        const Waypoint& a = path[k - 1];
        const Waypoint& b = path[k];
        double u = (t - a.t) / (b.t - a.t);
        u = u * u * (3 - 2 * u);                            // ease in/out between waypoints
        double sway = 0.06 * std::sin(t * 2.3 + i * 1.7);   // body sway and radar noise
        out.push_back({static_cast<int16_t>(std::lround((a.x + u * (b.x - a.x) + sway) * 10)),
                       static_cast<int16_t>(std::lround((a.y + u * (b.y - a.y) + sway) * 10))});
    }
    return out;
}

std::vector<uint8_t> makeFrame(int n, int targets) {
    std::vector<uint8_t> f = {0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0, 0};
    const char* scenario = std::getenv("SIM_SCENARIO");
    bool walk = scenario && std::strcmp(scenario, "walk") == 0;
    for (const Target& t : walk ? walkTargets(n, targets) : oscillatingTargets(n, targets)) {
        put16(f, t.x_dm);
        put16(f, t.y_dm);
    }
    f.insert(f.end(), {0xF8, 0xF7, 0xF6, 0xF5});
    uint16_t len = static_cast<uint16_t>(f.size());
    f[5] = len & 0xFF;
    f[6] = len >> 8;
    return f;
}

std::vector<uint8_t> makeValidFrame(int n) {
    int targets = envInt("SIM_TARGETS", 2);
    if (targets < 1) targets = 1;
    const char* scenario = std::getenv("SIM_SCENARIO");
    int maxTargets = scenario && std::strcmp(scenario, "walk") == 0 ? 8 : 3;
    if (targets > maxTargets) targets = maxTargets;
    return makeFrame(n, targets);
}

// Cycles through the faults a real UART line produces, each followed by a valid frame.
std::vector<uint8_t> makeFaultyFrame(int n) {
    std::vector<uint8_t> f;
    switch ((n / 2) % 5) {
    case 0:  // length field too small (noise): header + type + len=2
        f = {0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0x02, 0x00};
        std::fprintf(stderr, "[SIM] fault: length field = 2\n");
        break;
    case 1:  // length field larger than any real frame
        f = {0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0xFF, 0x7F};
        std::fprintf(stderr, "[SIM] fault: length field = 32767\n");
        break;
    case 2:  // frame with no target
        f = makeFrame(n, 0);
        std::fprintf(stderr, "[SIM] fault: frame without target\n");
        break;
    case 3:  // corrupted footer
        f = makeFrame(n, 1);
        f.back() = 0x00;
        std::fprintf(stderr, "[SIM] fault: corrupted footer\n");
        break;
    default:  // stray 0xF4 right before a valid header
        f = makeFrame(n, 1);
        f.insert(f.begin(), 0xF4);
        std::fprintf(stderr, "[SIM] fault: stray F4 before header (frame is valid)\n");
        break;
    }
    return f;
}
} // namespace

unsigned long millis() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return static_cast<unsigned long>(duration_cast<milliseconds>(steady_clock::now() - start).count());
}

void HardwareSerial::begin(unsigned long, uint32_t, int, int) {}

int HardwareSerial::available() {
    if (uart_ == 0) return 0;
    if (pos >= pending.size()) {
        int maxFrames = envInt("SIM_FRAMES", 10);
        if (maxFrames > 0 && framesSent >= maxFrames) {
            std::fflush(stdout);
            std::exit(0);  // simulation finished
        }
        if (framesSent > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(envInt("SIM_PERIOD_MS", 300)));
        bool faulty = envInt("SIM_FAULTS", 0) && framesSent % 2 == 1;
        pending = faulty ? makeFaultyFrame(framesSent) : makeValidFrame(framesSent);
        ++framesSent;
        pos = 0;
    }
    return static_cast<int>(pending.size() - pos);
}

int HardwareSerial::read() {
    if (uart_ == 0 || !available()) return -1;
    return pending[pos++];
}
